/**
 * @file    ota_client.h
 * @brief   OTA 客户端 — 应用侧 API (接收固件并写入 Flash)
 *
 * OTA 客户端提供应用侧的固件更新接口。
 * 可以运行在裸机或 FreeRTOS 上下文中。
 *
 * 使用流程 (Application Side):
 *   1. ota_client_init(&client, &transport);
 *   2. ota_client_start(&client);        // 开始在后台接收
 *   3. 轮询 ota_client_poll(&client);    // 驱动接收状态机
 *   4. ota_client_get_state(&client);    // 查询进度
 *   5. ota_client_boot(&client);         // 验证完成 → 跳转到新固件
 *
 * 接收过程中, 每次调用 ota_client_poll 处理一个接收到的帧。
 * 在 FreeRTOS 中, 可以将 poll 放在一个低优先级任务中。
 */

#ifndef OTA_CLIENT_H
#define OTA_CLIENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ota_config.h"
#include "ota_protocol.h"
#include "ota_transport.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 客户端状态 ───────────────────────────────────────────────── */
typedef enum {
    OTA_STATE_IDLE       = 0,  /* 等待开始 */
    OTA_STATE_HANDSHAKE  = 1,  /* 握手中 */
    OTA_STATE_RECEIVING  = 2,  /* 接收固件数据 */
    OTA_STATE_VERIFYING  = 3,  /* 校验中 */
    OTA_STATE_COMPLETE   = 4,  /* 完成 */
    OTA_STATE_ERROR      = 5,  /* 错误 */
} OtaClientState;

/* ── 客户端结构 ───────────────────────────────────────────────── */
typedef struct {
    OtaTransport  *transport;       /* 传输后端 */
    OtaClientState state;           /* 当前状态 */
    uint8_t        seq;             /* 当前序列号 */
    uint32_t       current_addr;    /* 当前写入地址 */
    uint32_t       total_size;      /* 总固件大小 */
    uint32_t       received_bytes;  /* 已接收字节数 */
    uint8_t        error_code;      /* 最后一次错误码 */
    uint8_t        retry_count;     /* 当前重试次数 */

    /* 回调 */
    void (*on_progress)(uint32_t received, uint32_t total); /* 进度回调 */
    void (*on_error)(uint8_t error_code);                   /* 错误回调 */
    void (*on_complete)(void);                              /* 完成回调 */
} OtaClient;

/* ── API ──────────────────────────────────────────────────────── */

/** @brief 初始化 OTA 客户端 */
void ota_client_init(OtaClient *client, OtaTransport *transport);

/** @brief 发送 HELLO, 等待握手应答, 开始接收流程 */
void ota_client_start(OtaClient *client);

/**
 * @brief 驱动接收状态机 — 每次调用处理一个接收周期
 *
 * 应在主循环或任务中周期性调用。
 * 如果 transport->recv 超时返回 0, 此函数立即返回 (非阻塞)。
 *
 * @return true 状态仍在进行, false 已完成或出错 (检查 state)
 */
bool ota_client_poll(OtaClient *client);

/** @brief 获取当前状态 */
OtaClientState ota_client_get_state(const OtaClient *client);

/** @brief 获取进度百分比 (0..100) */
uint8_t ota_client_get_progress(const OtaClient *client);

/**
 * @brief 验证固件完整性并请求跳转
 *
 * 发送 VERIFY → 等待确认 → 发送 BOOT → 等待确认 → 软复位
 * 软复位后, bootloader 检测到已验证的固件并跳转。
 */
void ota_client_boot(OtaClient *client);

#ifdef __cplusplus
}
#endif

#endif /* OTA_CLIENT_H */
