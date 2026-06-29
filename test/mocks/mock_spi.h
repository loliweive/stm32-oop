/**
 * @file    mock_spi.h
 * @brief   SPI Mock — host 测试用, vtable 注入替换真实 SPI
 */
#ifndef MOCK_SPI_H
#define MOCK_SPI_H

#include <stdint.h>
#include <stdbool.h>
#include "spi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    bool     initialized;
    uint32_t speed_hz;
    uint32_t pclk_hz;
    uint8_t  mode;
    int      transfer_count;

    /* 可编程响应: mock 按索引返回预设字节 */
    uint8_t  rx_data[128];
    int      rx_len;
    int      rx_idx;

    /* 记录所有发送的字节 (最多 128 个) */
    uint8_t  tx_log[128];
    int      tx_count;
} MockSpiState;

void mock_spi_reset(MockSpiState *s);
void mock_spi_inject(SpiPort *spi, MockSpiState *state);

/** 设置全局 mock state (在 spi_flash_init 之前调用) */
void mock_spi_set_global(MockSpiState *s);

/** 便捷: 预设 JEDEC ID 响应 (command-byte dummy + 3 ID bytes) */
static inline void mock_spi_set_jedec_id(MockSpiState *s, uint8_t mfr, uint8_t type, uint8_t cap) {
    s->rx_data[0] = 0x00;  /* 0x9F 命令字节的响应 (don't care) */
    s->rx_data[1] = mfr;   /* 第一个 dummy 返回 manufacturer */
    s->rx_data[2] = type;  /* 第二个 dummy 返回 type */
    s->rx_data[3] = cap;   /* 第三个 dummy 返回 capacity */
    s->rx_len = 4; s->rx_idx = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* MOCK_SPI_H */
