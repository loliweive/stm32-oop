/**
 * @file    spi_flash.h
 * @brief   SPI NOR Flash 驱动 (W25Qxx 兼容)
 *
 * 接线:
 *   SCK  → PA5  (SPI1_SCK)
 *   MISO → PA6  (SPI1_MISO)
 *   MOSI → PA7  (SPI1_MOSI)
 *   CS   → PB9  (GPIO 片选)
 *
 * 支持型号: W25Q16/32/64/128 (JEDEC ID 自动识别)
 * 特性: 4KB 扇区擦除 / 64KB 块擦除 / 256B 页编程
 */

#ifndef SPI_FLASH_H
#define SPI_FLASH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "gpio.h"
#include "spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Flash 信息 ─────────────────────────────────────────── */
typedef struct {
    uint32_t jedec_id;       /**< JEDEC ID (3 bytes) */
    uint32_t capacity;       /**< 总容量 (bytes) */
    uint16_t page_size;      /**< 页大小 (通常 256) */
    uint16_t sector_size;    /**< 扇区大小 (通常 4096) */
    uint32_t block_size;     /**< 块大小 (通常 65536) */
    char     name[12];       /**< 型号名 */
} SpiFlashInfo;

/* ── Flash 对象 ─────────────────────────────────────────── */
typedef struct {
    GpioPin      cs;         /**< CS 引脚 (持久化, 避免每次构造) */
    SpiPort      spi_port;   /**< SPI 端口 (持久化, 避免每次构造) */
    SpiFlashInfo info;       /**< 检测到的 Flash 信息 */
    bool         ready;      /**< 初始化完成 */
} SpiFlash;

/**
 * @brief 初始化 SPI Flash (检测型号 + 容量)
 * @param flash   Flash 对象
 * @param spi     SPI 外设 (SPI1)
 * @param cs_port CS GPIO 端口 (GPIOB)
 * @param cs_pin  CS GPIO 引脚 (GPIO_PIN_9)
 * @return true 检测到支持的 Flash
 */
bool spi_flash_init(SpiFlash *flash, void *spi, void *cs_port, uint16_t cs_pin);

/** @brief 获取 Flash 信息 */
const SpiFlashInfo *spi_flash_get_info(const SpiFlash *flash);

/** @brief 读取数据 */
bool spi_flash_read(SpiFlash *flash, uint32_t addr, uint8_t *buf, size_t len);

/**
 * @brief 写入数据 (自动处理跨页 + 擦除)
 *
 * 注意: Flash 必须先擦除再写入。
 * 此函数不自动擦除 — 调用者需先调用 spi_flash_erase_sector/block。
 *
 * @return true 成功
 */
bool spi_flash_write(SpiFlash *flash, uint32_t addr, const uint8_t *data, size_t len);

/** @brief 擦除一个扇区 (4KB) */
bool spi_flash_erase_sector(SpiFlash *flash, uint32_t addr);

/** @brief 擦除一个块 (64KB) */
bool spi_flash_erase_block(SpiFlash *flash, uint32_t addr);

/** @brief 全片擦除 (慢 — 可能几十秒) */
bool spi_flash_erase_chip(SpiFlash *flash);

/** @brief 读取 JEDEC ID */
uint32_t spi_flash_read_jedec_id(SpiFlash *flash);

/** @brief 读取状态寄存器 */
uint8_t spi_flash_read_status(SpiFlash *flash);

/** @brief 等待 Flash 空闲 (BUSY bit clear) */
void spi_flash_wait_busy(SpiFlash *flash);

#ifdef __cplusplus
}
#endif

#endif
