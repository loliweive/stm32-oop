/**
 * @file    spi_flash.c
 * @brief   SPI Flash W25Qxx 驱动实现 — 持久化 HAL 对象
 */
#include "spi_flash.h"
#include "gpio.h"
#include "spi.h"
#if defined(BAREMETAL)
#include "stm32f103xb.h"
#else
#include "stm32f1xx_hal.h"
#endif
#include <string.h>
#include <stdio.h>

/* ── SPI Flash 命令 ─────────────────────────────────────── */
#define CMD_WRITE_ENABLE    0x06
#define CMD_WRITE_DISABLE   0x04
#define CMD_READ_STATUS     0x05
#define CMD_READ_DATA       0x03
#define CMD_PAGE_PROGRAM    0x02
#define CMD_SECTOR_ERASE    0x20
#define CMD_BLOCK_ERASE     0xD8
#define CMD_CHIP_ERASE      0xC7
#define CMD_JEDEC_ID        0x9F
#define STATUS_BUSY         0x01

/* ── SPI 传输 (复用持久化 SpiPort) ─────────────────────────── */
static uint8_t _spi_xfer(SpiFlash *f, uint8_t byte) {
    return spi_transfer(&f->spi_port, byte);
}

static void _spi_read(SpiFlash *f, uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; i++) buf[i] = spi_transfer(&f->spi_port, 0xFF);
}

/* ── API ────────────────────────────────────────────────── */

void spi_flash_wait_busy(SpiFlash *flash)
{
    gpio_set(&flash->cs, 0);  /* CS low */
    _spi_xfer(flash, CMD_READ_STATUS);
    while (_spi_xfer(flash, 0xFF) & STATUS_BUSY) {}
    gpio_set(&flash->cs, 1);  /* CS high */
}

uint8_t spi_flash_read_status(SpiFlash *flash)
{
    gpio_set(&flash->cs, 0);
    _spi_xfer(flash, CMD_READ_STATUS);
    uint8_t s = _spi_xfer(flash, 0xFF);
    gpio_set(&flash->cs, 1);
    return s;
}

static void _write_enable(SpiFlash *flash)
{
    gpio_set(&flash->cs, 0);
    _spi_xfer(flash, CMD_WRITE_ENABLE);
    gpio_set(&flash->cs, 1);
}

uint32_t spi_flash_read_jedec_id(SpiFlash *flash)
{
    uint32_t id = 0;
    gpio_set(&flash->cs, 0);
    _spi_xfer(flash, CMD_JEDEC_ID);
    id  = (uint32_t)_spi_xfer(flash, 0xFF) << 16;
    id |= (uint32_t)_spi_xfer(flash, 0xFF) << 8;
    id |= (uint32_t)_spi_xfer(flash, 0xFF);
    gpio_set(&flash->cs, 1);
    return id;
}

/* ── 型号识别表 ─────────────────────────────────────────── */
static bool _detect_flash(SpiFlash *flash)
{
    uint32_t id = spi_flash_read_jedec_id(flash);
    flash->info.jedec_id = id;
    flash->info.page_size   = 256;
    flash->info.sector_size = 4096;
    flash->info.block_size  = 65536;

    /* 高 16 bits = manufacturer + type (e.g. 0xEF40 = Winbond W25Q) */
    switch (id >> 8) {
        case 0xEF40: {  /* Winbond W25Q series */
            uint8_t cap = (uint8_t)(id & 0xFF);
            flash->info.capacity = (uint32_t)1 << cap;  /* 2^cap bytes */
            switch (cap) {
                case 0x15: strncpy(flash->info.name,"W25Q16", sizeof(flash->info.name)); break;
                case 0x16: strncpy(flash->info.name,"W25Q32", sizeof(flash->info.name)); break;
                case 0x17: strncpy(flash->info.name,"W25Q64", sizeof(flash->info.name)); break;
                case 0x18: strncpy(flash->info.name,"W25Q128",sizeof(flash->info.name)); break;
                default:   snprintf(flash->info.name,sizeof(flash->info.name),"W25Q%lu",(unsigned long)(flash->info.capacity/(1024*1024))); break;
            }
            flash->ready = true;
            return true;
        }
        default:
            flash->info.capacity = 0;
            strncpy(flash->info.name, "Unknown", sizeof(flash->info.name));
            flash->ready = false;
            return false;
    }
    flash->ready = true;
    return true;
}

bool spi_flash_init(SpiFlash *flash, void *spi, void *cs_port, uint16_t cs_pin)
{
    memset(flash, 0, sizeof(*flash));

    /* 构造持久化 HAL 对象 — 整个生命周期只构造一次 */
    GpioPin_ctor(&flash->cs, cs_port, cs_pin);
    gpio_set_mode(&flash->cs, GPIO_MODE_OUT_PP);
    gpio_set(&flash->cs, 1);  /* CS 初始为高 */

    SpiPort_ctor(&flash->spi_port, spi, 1000000, 72000000, 0);
    spi_init(&flash->spi_port);

    return _detect_flash(flash);
}

const SpiFlashInfo *spi_flash_get_info(const SpiFlash *flash) { return &flash->info; }

bool spi_flash_read(SpiFlash *flash, uint32_t addr, uint8_t *buf, size_t len)
{
    if (!flash->ready || addr + len > flash->info.capacity) return false;

    gpio_set(&flash->cs, 0);
    _spi_xfer(flash, CMD_READ_DATA);
    _spi_xfer(flash, (uint8_t)(addr >> 16));
    _spi_xfer(flash, (uint8_t)(addr >> 8));
    _spi_xfer(flash, (uint8_t)(addr));
    _spi_read(flash, buf, len);
    gpio_set(&flash->cs, 1);
    return true;
}

bool spi_flash_write(SpiFlash *flash, uint32_t addr, const uint8_t *data, size_t len)
{
    if (!flash->ready || addr + len > flash->info.capacity) return false;

    size_t remaining = len;
    uint32_t cur_addr = addr;
    const uint8_t *src = data;

    while (remaining > 0) {
        _write_enable(flash);
        gpio_set(&flash->cs, 0);
        _spi_xfer(flash, CMD_PAGE_PROGRAM);
        _spi_xfer(flash, (uint8_t)(cur_addr >> 16));
        _spi_xfer(flash, (uint8_t)(cur_addr >> 8));
        _spi_xfer(flash, (uint8_t)(cur_addr));

        size_t chunk = flash->info.page_size - (cur_addr & (flash->info.page_size - 1));
        if (chunk > remaining) chunk = remaining;

        for (size_t i = 0; i < chunk; i++) _spi_xfer(flash, src[i]);
        gpio_set(&flash->cs, 1);

        spi_flash_wait_busy(flash);
        src       += chunk;
        cur_addr  += chunk;
        remaining -= chunk;
    }
    return true;
}

bool spi_flash_erase_sector(SpiFlash *flash, uint32_t addr)
{
    if (!flash->ready) return false;
    _write_enable(flash);
    gpio_set(&flash->cs, 0);
    _spi_xfer(flash, CMD_SECTOR_ERASE);
    _spi_xfer(flash, (uint8_t)(addr >> 16));
    _spi_xfer(flash, (uint8_t)(addr >> 8));
    _spi_xfer(flash, (uint8_t)(addr));
    gpio_set(&flash->cs, 1);
    spi_flash_wait_busy(flash);
    return true;
}

bool spi_flash_erase_block(SpiFlash *flash, uint32_t addr)
{
    if (!flash->ready) return false;
    _write_enable(flash);
    gpio_set(&flash->cs, 0);
    _spi_xfer(flash, CMD_BLOCK_ERASE);
    _spi_xfer(flash, (uint8_t)(addr >> 16));
    _spi_xfer(flash, (uint8_t)(addr >> 8));
    _spi_xfer(flash, (uint8_t)(addr));
    gpio_set(&flash->cs, 1);
    spi_flash_wait_busy(flash);
    return true;
}

bool spi_flash_erase_chip(SpiFlash *flash)
{
    if (!flash->ready) return false;
    _write_enable(flash);
    gpio_set(&flash->cs, 0);
    _spi_xfer(flash, CMD_CHIP_ERASE);
    gpio_set(&flash->cs, 1);
    spi_flash_wait_busy(flash);  /* 全片擦除可能需要 40-80 秒! */
    return true;
}
