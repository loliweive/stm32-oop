/**
 * @file    ota_config.h
 * @brief   OTA 模块配置 — Flash 分区、协议参数、超时设置
 *
 * STM32F103C8T6 Flash 布局 (64KB total):
 * ┌──────────────────┐ 0x08000000
 * │  Bootloader      │  8KB — 固件更新逻辑 (只读)
 * │  0x08000000      │
 * ├──────────────────┤ 0x08002000
 * │  Application     │ 54KB — 用户固件
 * │  0x08002000      │
 * ├──────────────────┤ 0x0800F800
 * │  OTA Metadata    │  2KB — 固件版本、校验和、状态标志
 * │  0x0800F800      │
 * └──────────────────┘ 0x08010000 (64KB end)
 *
 * SRAM: 20KB — 接收缓冲区从应用栈中动态使用
 *
 * 可扩展性：
 *   - 修改 OTA_APP_START 适配不同 Flash 大小
 *   - 修改 transport 后端支持 WiFi/BLE/CAN 等不同通信方式
 *   - 替换 storage 后端支持外部 Flash (SPI Flash)
 */

#ifndef OTA_CONFIG_H
#define OTA_CONFIG_H

#include <stdint.h>

/* ── Flash 分区 ──────────────────────────────────────────────── */
#define OTA_FLASH_PAGE_SIZE     1024UL      /* STM32F103: 1KB per page */
#define OTA_FLASH_TOTAL_SIZE    (64 * 1024) /* 64KB total */

#define OTA_BOOTLOADER_START    0x08000000UL
#define OTA_BOOTLOADER_SIZE     (8 * 1024)  /* 8KB for bootloader */

#define OTA_APP_START           0x08002000UL
#define OTA_APP_SIZE            (54 * 1024) /* 54KB for application */

#define OTA_METADATA_START      0x0800F800UL
#define OTA_METADATA_SIZE       (2 * 1024)  /* 2KB for metadata */
#define OTA_METADATA_PAGES      2           /* 2 pages at end of Flash */

/* ── 协议参数 ────────────────────────────────────────────────── */
#define OTA_PROTOCOL_VERSION    1
#define OTA_MAX_PAYLOAD_SIZE    256         /* 每帧最大负载 */
#define OTA_FRAME_OVERHEAD      6           /* STX(1) + LEN(2) + TYPE(1) + SEQ(1) + CRC(2) 已包含,LEN不计入payload本身 */
#define OTA_FRAME_MAX_SIZE      (OTA_MAX_PAYLOAD_SIZE + 6)  /* STX+LEN+TYPE+SEQ+PAYLOAD+CRC */

#define OTA_ACK_TIMEOUT_MS      500         /* 等待 ACK 超时 */
#define OTA_MAX_RETRIES         3           /* 最大重试次数 */

/* ── 帧定界符 ────────────────────────────────────────────────── */
#define OTA_STX                 0xAA        /* 帧起始标志 */

/* ── 命令类型 ────────────────────────────────────────────────── */
typedef enum {
    OTA_CMD_HELLO    = 0x01,  /* 握手: 交换版本和能力 */
    OTA_CMD_HELLO_ACK= 0x02,  /* 握手应答 */
    OTA_CMD_DATA     = 0x03,  /* 固件数据块 */
    OTA_CMD_DATA_ACK = 0x04,  /* 数据块确认 */
    OTA_CMD_VERIFY   = 0x05,  /* 请求校验 */
    OTA_CMD_VERIFY_ACK=0x06,  /* 校验结果 */
    OTA_CMD_BOOT     = 0x07,  /* 完成更新, 启动新固件 */
    OTA_CMD_BOOT_ACK = 0x08,  /* 确认启动 */
    OTA_CMD_ERROR    = 0x0E,  /* 错误报告 */
    OTA_CMD_RESET    = 0x0F,  /* 软复位 */
} OtaCommand;

/* ── 错误码 ──────────────────────────────────────────────────── */
typedef enum {
    OTA_ERR_NONE        = 0,
    OTA_ERR_CRC         = 1,   /* CRC 校验失败 */
    OTA_ERR_FLASH_ERASE = 2,   /* Flash 擦除失败 */
    OTA_ERR_FLASH_WRITE = 3,   /* Flash 写入失败 */
    OTA_ERR_FLASH_VERIFY= 4,   /* Flash 校验失败 */
    OTA_ERR_TIMEOUT     = 5,   /* 通信超时 */
    OTA_ERR_SEQUENCE    = 6,   /* 序列号错乱 */
    OTA_ERR_SIZE        = 7,   /* 固件过大 */
    OTA_ERR_STATE       = 8,   /* 状态机错误 */
} OtaError;

/* ── 元数据结构 (存储在 Flash 末尾) ────────────────────────────── */
typedef struct __attribute__((packed)) {
    uint32_t magic;          /* 魔数: 0x4F5441 "OTA\0" */
    uint32_t firmware_size;  /* 固件字节数 */
    uint32_t firmware_crc;   /* 固件 CRC32 */
    uint16_t version_major;
    uint16_t version_minor;
    uint32_t update_counter; /* 更新计数 (递增) */
    uint8_t  state;          /* 0=无固件 1=已验证 2=已启动 */
    uint8_t  reserved[11];   /* 对齐 32 bytes */
} OtaMetadata;

#define OTA_MAGIC               0x4F5441UL  /* "OTA" */
#define OTA_STATE_EMPTY         0
#define OTA_STATE_VERIFIED      1
#define OTA_STATE_BOOTED        2

#endif /* OTA_CONFIG_H */
