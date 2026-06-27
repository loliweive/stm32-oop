/**
 * @file    spi_flash.c
 * @brief   SPI Flash W25Qxx 驱动实现
 */
#include "spi_flash.h"
#include "spi.h"
#include "gpio.h"
#include "stm32f103xb.h"
#include <string.h>

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

/* ── CS 控制 ────────────────────────────────────────────── */
static void _cs_low(SpiFlash *f) {
    GpioPin cs; GpioPin_ctor(&cs, f->cs_port, f->cs_pin);
    gpio_set_mode(&cs, GPIO_MODE_OUT_PP); gpio_set(&cs, 0);
}
static void _cs_high(SpiFlash *f) {
    GpioPin cs; GpioPin_ctor(&cs, f->cs_port, f->cs_pin);
    gpio_set(&cs, 1);
}

/* ── SPI 传输 ───────────────────────────────────────────── */
static uint8_t _spi_xfer(SpiFlash *f, uint8_t byte) {
    SpiPort sp; SpiPort_ctor(&sp, f->spi, 1000000, 72000000, 0);
    return spi_transfer(&sp, byte);
}

static void _spi_read(SpiFlash *f, uint8_t *buf, size_t len) {
    SpiPort sp; SpiPort_ctor(&sp, f->spi, 1000000, 72000000, 0);
    for (size_t i = 0; i < len; i++) buf[i] = spi_transfer(&sp, 0xFF);
}

/* ── API ────────────────────────────────────────────────── */

void spi_flash_wait_busy(SpiFlash *flash)
{
    _cs_low(flash);
    _spi_xfer(flash, CMD_READ_STATUS);
    while (_spi_xfer(flash, 0xFF) & STATUS_BUSY) {}
    _cs_high(flash);
}

uint8_t spi_flash_read_status(SpiFlash *flash)
{
    _cs_low(flash);
    _spi_xfer(flash, CMD_READ_STATUS);
    uint8_t s = _spi_xfer(flash, 0xFF);
    _cs_high(flash);
    return s;
}

static void _write_enable(SpiFlash *flash)
{
    _cs_low(flash);
    _spi_xfer(flash, CMD_WRITE_ENABLE);
    _cs_high(flash);
}

uint32_t spi_flash_read_jedec_id(SpiFlash *flash)
{
    uint32_t id = 0;
    _cs_low(flash);
    _spi_xfer(flash, CMD_JEDEC_ID);
    id  = (uint32_t)_spi_xfer(flash, 0xFF) << 16;
    id |= (uint32_t)_spi_xfer(flash, 0xFF) << 8;
    id |= (uint32_t)_spi_xfer(flash, 0xFF);
    _cs_high(flash);
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

    switch (id >> 8) {  /* 高 16 bits = manufacturer + type */
        case 0xEF4015: flash->info.capacity = 2*1024*1024;  strcpy(flash->info.name,"W25Q16"); break;
        case 0xEF4016: flash->info.capacity = 4*1024*1024;  strcpy(flash->info.name,"W25Q32"); break;
        case 0xEF4017: flash->info.capacity = 8*1024*1024;  strcpy(flash->info.name,"W25Q64"); break;
        case 0xEF4018: flash->info.capacity = 16*1024*1024; strcpy(flash->info.name,"W25Q128");break;
        default:       flash->info.capacity = 4*1024*1024;  strcpy(flash->info.name,"Unknown"); flash->ready=false; return false;
    }
    flash->ready = true;
    return true;
}

bool spi_flash_init(SpiFlash *flash, void *spi, void *cs_port, uint16_t cs_pin)
{
    memset(flash, 0, sizeof(*flash));
    flash->spi     = spi;
    flash->cs_port = cs_port;
    flash->cs_pin  = cs_pin;

    /* 初始化 CS 为高 (不选中) */
    _cs_high(flash);

    /* 初始化 SPI */
    SpiPort sp;
    SpiPort_ctor(&sp, spi, 1000000, 72000000, 0);  /* 1MHz, mode 0 */
    spi_init(&sp);

    return _detect_flash(flash);
}

const SpiFlashInfo *spi_flash_get_info(const SpiFlash *flash) { return &flash->info; }

bool spi_flash_read(SpiFlash *flash, uint32_t addr, uint8_t *buf, size_t len)
{
    if (!flash->ready || addr + len > flash->info.capacity) return false;

    _cs_low(flash);
    _spi_xfer(flash, CMD_READ_DATA);
    _spi_xfer(flash, (uint8_t)(addr >> 16));
    _spi_xfer(flash, (uint8_t)(addr >> 8));
    _spi_xfer(flash, (uint8_t)(addr));
    _spi_read(flash, buf, len);
    _cs_high(flash);
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
        _cs_low(flash);
        _spi_xfer(flash, CMD_PAGE_PROGRAM);
        _spi_xfer(flash, (uint8_t)(cur_addr >> 16));
        _spi_xfer(flash, (uint8_t)(cur_addr >> 8));
        _spi_xfer(flash, (uint8_t)(cur_addr));

        size_t chunk = flash->info.page_size - (cur_addr & (flash->info.page_size - 1));
        if (chunk > remaining) chunk = remaining;

        for (size_t i = 0; i < chunk; i++) _spi_xfer(flash, src[i]);
        _cs_high(flash);

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
    _cs_low(flash);
    _spi_xfer(flash, CMD_SECTOR_ERASE);
    _spi_xfer(flash, (uint8_t)(addr >> 16));
    _spi_xfer(flash, (uint8_t)(addr >> 8));
    _spi_xfer(flash, (uint8_t)(addr));
    _cs_high(flash);
    spi_flash_wait_busy(flash);
    return true;
}

bool spi_flash_erase_block(SpiFlash *flash, uint32_t addr)
{
    if (!flash->ready) return false;
    _write_enable(flash);
    _cs_low(flash);
    _spi_xfer(flash, CMD_BLOCK_ERASE);
    _spi_xfer(flash, (uint8_t)(addr >> 16));
    _spi_xfer(flash, (uint8_t)(addr >> 8));
    _spi_xfer(flash, (uint8_t)(addr));
    _cs_high(flash);
    spi_flash_wait_busy(flash);
    return true;
}

bool spi_flash_erase_chip(SpiFlash *flash)
{
    if (!flash->ready) return false;
    _write_enable(flash);
    _cs_low(flash);
    _spi_xfer(flash, CMD_CHIP_ERASE);
    _cs_high(flash);
    spi_flash_wait_busy(flash);  /* 全片擦除可能需要 40-80 秒! */
    return true;
}
