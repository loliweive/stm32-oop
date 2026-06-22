/**
 * @file    ota_client.c
 * @brief   OTA 客户端状态机实现
 *
 * 状态机流转:
 *   IDLE → HANDSHAKE → RECEIVING → VERIFYING → COMPLETE
 *                                                ↓ (boot)
 *                                              复位 → Bootloader 跳转到新固件
 */
#include "ota_client.h"
#include "ota_flash.h"
#include <string.h>

/* ── 内部辅助 ─────────────────────────────────────────────────── */
static uint8_t  rx_buf[OTA_FRAME_MAX_SIZE];
static uint8_t  tx_buf[OTA_FRAME_MAX_SIZE];

static void set_state(OtaClient *c, OtaClientState s) { c->state = s; }

static void send_frame(OtaClient *c, const uint8_t *data, size_t len)
{
    ota_xport_send(c->transport, data, len);
}

/* ── 初始化 ──────────────────────────────────────────────────── */
void ota_client_init(OtaClient *client, OtaTransport *transport)
{
    memset(client, 0, sizeof(*client));
    client->transport = transport;
    client->state     = OTA_STATE_IDLE;
    client->seq       = 0;
    ota_flash_init();
}

void ota_client_start(OtaClient *client)
{
    /* 擦除应用区域 */
    ota_flash_erase_range(OTA_APP_START, OTA_APP_SIZE);
    client->current_addr  = OTA_APP_START;
    client->received_bytes = 0;
    client->total_size     = 0;
    client->retry_count    = 0;

    /* 发送 HELLO */
    size_t len = ota_build_hello(tx_buf, sizeof(tx_buf), client->seq++);
    send_frame(client, tx_buf, len);
    set_state(client, OTA_STATE_HANDSHAKE);
}

bool ota_client_poll(OtaClient *client)
{
    if (client->state == OTA_STATE_IDLE ||
        client->state == OTA_STATE_COMPLETE ||
        client->state == OTA_STATE_ERROR) {
        return false;
    }

    /* 尝试接收一帧 */
    size_t n = ota_xport_recv(client->transport, rx_buf, sizeof(rx_buf), OTA_ACK_TIMEOUT_MS);
    if (n == 0) return true; /* 超时, 继续轮询 */

    uint8_t  type, seq;
    const uint8_t *payload;
    size_t   plen;

    if (!ota_frame_decode(rx_buf, n, &type, &seq, &payload, &plen)) {
        /* CRC 失败 → 发送 NAK */
        size_t elen = ota_build_error(tx_buf, sizeof(tx_buf), seq, OTA_ERR_CRC);
        send_frame(client, tx_buf, elen);
        return true;
    }

    switch (client->state) {

    case OTA_STATE_HANDSHAKE:
        if (type == OTA_CMD_HELLO_ACK && plen >= 4) {
            client->total_size = (uint32_t)payload[0]
                               | ((uint32_t)payload[1] << 8)
                               | ((uint32_t)payload[2] << 16)
                               | ((uint32_t)payload[3] << 24);

            if (client->total_size > OTA_APP_SIZE) {
                set_state(client, OTA_STATE_ERROR);
                client->error_code = OTA_ERR_SIZE;
                return false;
            }
            set_state(client, OTA_STATE_RECEIVING);
        }
        break;

    case OTA_STATE_RECEIVING:
        if (type == OTA_CMD_DATA && plen > 0) {
            /* 写入 Flash */
            if (!ota_flash_write(client->current_addr, payload, plen)) {
                client->error_code = OTA_ERR_FLASH_WRITE;
                set_state(client, OTA_STATE_ERROR);
                return false;
            }
            client->current_addr  += plen;
            client->received_bytes += plen;

            /* 进度回调 */
            if (client->on_progress) {
                client->on_progress(client->received_bytes, client->total_size);
            }

            /* 发送 ACK */
            size_t alen = ota_build_ack(tx_buf, sizeof(tx_buf), OTA_CMD_DATA, seq);
            send_frame(client, tx_buf, alen);

            /* 接收完成? */
            if (client->received_bytes >= client->total_size) {
                set_state(client, OTA_STATE_VERIFYING);
            }
        } else if (type == OTA_CMD_VERIFY) {
            /* 上位机主动请求校验 */
            set_state(client, OTA_STATE_VERIFYING);
        }
        break;

    case OTA_STATE_VERIFYING: {
        /* 计算 CRC32 并发送校验结果 */
        uint32_t crc = ota_flash_crc32(OTA_APP_START, client->received_bytes);
        size_t vlen = ota_build_verify(tx_buf, sizeof(tx_buf), seq,
                                       client->received_bytes, crc);
        send_frame(client, tx_buf, vlen);

        /* 等待上位机确认 */
        size_t nr = ota_xport_recv(client->transport, rx_buf, sizeof(rx_buf), 2000);
        if (nr > 0 && ota_frame_decode(rx_buf, nr, &type, &seq, &payload, &plen)) {
            if (type == OTA_CMD_VERIFY_ACK) {
                /* 写入元数据 */
                OtaMetadata meta;
                memset(&meta, 0, sizeof(meta));
                meta.magic         = OTA_MAGIC;
                meta.firmware_size = client->received_bytes;
                meta.firmware_crc  = crc;
                meta.state         = OTA_STATE_VERIFIED;
                meta.update_counter = 1;

                ota_flash_erase_range(OTA_METADATA_START, sizeof(meta));
                ota_flash_write(OTA_METADATA_START, (const uint8_t *)&meta, sizeof(meta));

                set_state(client, OTA_STATE_COMPLETE);
                if (client->on_complete) client->on_complete();
                return false;
            }
        }
        break;
    }

    default:
        break;
    }

    return true;
}

void ota_client_boot(OtaClient *client)
{
    /* 发送 BOOT 命令 → 等待确认 → 软复位 */
    size_t len = ota_build_boot(tx_buf, sizeof(tx_buf), client->seq++);
    send_frame(client, tx_buf, len);

    /* 软复位 — bootloader 将检测已验证固件并跳转 */
    #define AIRCR_ADDR         ((volatile uint32_t *)0xE000ED0CUL)
    #define AIRCR_VECTKEY      (0x05FAUL << 16)
    #define AIRCR_SYSRESETREQ  (1UL << 2)
    *AIRCR_ADDR = AIRCR_VECTKEY | AIRCR_SYSRESETREQ;
    while (1) {}
}

OtaClientState ota_client_get_state(const OtaClient *client) { return client->state; }

uint8_t ota_client_get_progress(const OtaClient *client)
{
    if (client->total_size == 0) return 0;
    return (uint8_t)((client->received_bytes * 100) / client->total_size);
}
