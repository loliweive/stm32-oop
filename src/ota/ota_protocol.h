/**
 * @file    ota_protocol.h
 * @brief   OTA 协议层 — 帧编解码、CRC16、序列号管理
 *
 * 协议帧格式 (小端序):
 * ┌──────┬────────┬──────┬──────┬──────────────────┬────────┐
 * │ STX  │  LEN   │ TYPE │ SEQ  │    PAYLOAD        │ CRC16  │
 * │ 1B   │  2B    │ 1B   │ 1B   │   0..256B         │ 2B     │
 * │ 0xAA │ uint16 │ enum │ uint8│                   │ uint16 │
 * └──────┴────────┴──────┴──────┴──────────────────┴────────┘
 *
 * LEN = TYPE(1) + SEQ(1) + PAYLOAD(N) + CRC16(2) 的总长度 (不含 STX 和 LEN 自身)
 *
 * 设计原则：
 *   - 帧定界用 STX + LEN，简单高效 (无需字节填充)
 *   - CRC16 保护整个数据区域
 *   - 序列号检测丢包和重传
 *
 * 可扩展性：
 *   - 替换 ota_protocol.c 即可更换成 COBS/SLIP 等其他帧格式
 *   - CRC 实现可替换为 CRC32 或其他校验算法
 */

#ifndef OTA_PROTOCOL_H
#define OTA_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ota_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 帧结构 ──────────────────────────────────────────────────── */

/**
 * @brief OTA 协议帧
 *
 * 内存布局对应协议帧格式。
 * 使用 packed 属性确保无填充字节。
 */
typedef struct __attribute__((packed)) {
    uint8_t  stx;           /**< 帧起始标志 (0xAA) */
    uint16_t len;           /**< 后续字节长度 (TYPE+SEQ+PAYLOAD+CRC) */
    uint8_t  type;          /**< 命令类型 */
    uint8_t  seq;           /**< 序列号 (用于丢包检测) */
    /* payload follows — up to OTA_MAX_PAYLOAD_SIZE bytes */
    /* crc16 follows — 2 bytes at end of frame */
} OtaFrameHeader;

#define OTA_FRAME_HEADER_SIZE   sizeof(OtaFrameHeader)
#define OTA_CRC_SIZE            2

/* ── 帧构建/解析 API ─────────────────────────────────────────── */

/**
 * @brief 将帧编码到缓冲区
 *
 * @param buf     输出缓冲区 (至少 OTA_FRAME_MAX_SIZE bytes)
 * @param buf_size 缓冲区大小
 * @param type    命令类型
 * @param seq     序列号
 * @param payload 负载数据 (可为 NULL)
 * @param payload_len 负载长度
 * @return 编码后的帧长度 (字节数), 0 = 参数错误
 */
size_t ota_frame_encode(uint8_t *buf, size_t buf_size,
                        uint8_t type, uint8_t seq,
                        const uint8_t *payload, size_t payload_len);

/**
 * @brief 从字节流中解析帧
 *
 * @param buf     输入字节流
 * @param len     字节流长度
 * @param type    输出: 命令类型
 * @param seq     输出: 序列号
 * @param payload 输出: 负载数据指针 (指向 buf 内部, 不分配新内存)
 * @param payload_len 输出: 负载长度
 * @return true 解析成功且 CRC 校验通过, false 格式错误或 CRC 不匹配
 */
bool ota_frame_decode(const uint8_t *buf, size_t len,
                      uint8_t *type, uint8_t *seq,
                      const uint8_t **payload, size_t *payload_len);

/* ── CRC16 ────────────────────────────────────────────────────── */

/**
 * @brief 计算 CRC16-CCITT (多项式 0x1021, 初始值 0xFFFF)
 *
 * 工业标准 CRC，广泛用于嵌入式通信协议。
 * 查表实现，在 Cortex-M3 上约 2 cycles/byte。
 *
 * @param data 数据指针
 * @param len  数据长度
 * @return CRC16 校验值
 */
uint16_t ota_crc16(const uint8_t *data, size_t len);

/* ── 便捷构建函数 ──────────────────────────────────────────────── */

/** 构建 HELLO 请求帧 */
size_t ota_build_hello(uint8_t *buf, size_t buf_size, uint8_t seq);

/** 构建 DATA 帧 (固件数据块) */
size_t ota_build_data(uint8_t *buf, size_t buf_size, uint8_t seq,
                      const uint8_t *data, size_t data_len);

/** 构建 VERIFY 请求帧 */
size_t ota_build_verify(uint8_t *buf, size_t buf_size, uint8_t seq,
                        uint32_t total_size, uint32_t crc32);

/** 构建 BOOT 命令帧 */
size_t ota_build_boot(uint8_t *buf, size_t buf_size, uint8_t seq);

/** 构建 ACK 帧 */
size_t ota_build_ack(uint8_t *buf, size_t buf_size, uint8_t cmd, uint8_t seq);

/** 构建 ERROR 帧 */
size_t ota_build_error(uint8_t *buf, size_t buf_size, uint8_t seq,
                       uint8_t error_code);

#ifdef __cplusplus
}
#endif

#endif /* OTA_PROTOCOL_H */
