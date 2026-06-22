/**
 * @file    ota_flash.c
 * @brief   STM32F103 内部 Flash 操作实现
 *
 * STM32F103 Flash 写入流程:
 *   1. 解锁 FLASH_KEYR
 *   2. 擦除目标页 (页擦除 ~20ms)
 *   3. 编程: 逐半字 (16-bit) 写入
 *   4. 锁定 FLASH_CR
 *
 * 注意: 写 Flash 时 CPU 停顿 (STALL), 中断会被延迟。
 * 在 FreeRTOS 中, 擦除操作应短暂挂起调度器或确保任务不被抢占。
 */
#include "ota_flash.h"
#include "ota_config.h"
#include "stm32f103xb.h"
#include <string.h>

#define FLASH_KEY1  0x45670123UL
#define FLASH_KEY2  0xCDEF89ABUL

/* Flash 控制位 — 最小化 CMSIS 依赖 */
#define FLASH_CR_PG       (1 << 0)
#define FLASH_CR_PER      (1 << 1)
#define FLASH_CR_STRT     (1 << 6)
#define FLASH_CR_LOCK     (1 << 7)
#define FLASH_SR_BSY      (1 << 0)
#define FLASH_SR_PGERR    (1 << 2)

void ota_flash_init(void)
{
    if (FLASH->CR & FLASH_CR_LOCK) {
        FLASH->KEYR = FLASH_KEY1;
        FLASH->KEYR = FLASH_KEY2;
    }
    /* 等待解锁生效 */
    while (FLASH->CR & FLASH_CR_LOCK) {}
}

void ota_flash_lock(void)
{
    FLASH->CR |= FLASH_CR_LOCK;
}

bool ota_flash_erase_page(uint32_t page_addr)
{
    /* 等待 Flash 空闲 */
    while (FLASH->SR & FLASH_SR_BSY) {}

    /* 设置页擦除模式 */
    FLASH->CR |= FLASH_CR_PER;
    FLASH->AR  = page_addr;
    FLASH->CR |= FLASH_CR_STRT;

    /* 等待操作完成 */
    while (FLASH->SR & FLASH_SR_BSY) {}

    /* 检查错误 */
    bool ok = !(FLASH->SR & FLASH_SR_PGERR);
    FLASH->CR &= ~FLASH_CR_PER;
    return ok;
}

bool ota_flash_erase_range(uint32_t start, size_t size)
{
    /* 对齐到页边界 */
    uint32_t page_start = start & ~(OTA_FLASH_PAGE_SIZE - 1);
    uint32_t end = start + size;
    uint32_t page_end = (end + OTA_FLASH_PAGE_SIZE - 1) & ~(OTA_FLASH_PAGE_SIZE - 1);

    for (uint32_t addr = page_start; addr < page_end; addr += OTA_FLASH_PAGE_SIZE) {
        if (!ota_flash_erase_page(addr)) return false;
    }
    return true;
}

bool ota_flash_write(uint32_t addr, const uint8_t *data, size_t len)
{
    if (!data || len == 0 || (addr & 1)) return false;

    /* 设置编程模式 */
    FLASH->CR |= FLASH_CR_PG;

    for (size_t i = 0; i < len; i += 2) {
        /* 等待空闲 */
        while (FLASH->SR & FLASH_SR_BSY) {}

        /* 构建半字 (小端序) */
        uint16_t half = (uint16_t)data[i];
        if (i + 1 < len) {
            half |= (uint16_t)(data[i + 1] << 8);
        } else {
            half |= 0xFF00; /* 最后一个字节填充 0xFF (Flash 擦除态) */
        }

        *(volatile uint16_t *)addr = half;
        addr += 2;

        /* 检查编程错误 */
        if (FLASH->SR & FLASH_SR_PGERR) {
            FLASH->SR |= FLASH_SR_PGERR; /* 清除标志 */
            FLASH->CR &= ~FLASH_CR_PG;
            return false;
        }
    }

    FLASH->CR &= ~FLASH_CR_PG;
    return true;
}

bool ota_flash_verify(uint32_t addr, const uint8_t *data, size_t len)
{
    if (!data) return false;
    for (size_t i = 0; i < len; i++) {
        if (*(volatile uint8_t *)(addr + i) != data[i]) return false;
    }
    return true;
}

void ota_flash_read(uint32_t addr, uint8_t *buf, size_t len)
{
    if (buf) memcpy(buf, (const void *)addr, len);
}

/* CRC32 — 软件实现 (避免依赖 zlib) */
static uint32_t crc32_table[256];
static bool crc32_table_ready = false;

static void crc32_init_table(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_table_ready = true;
}

uint32_t ota_flash_crc32(uint32_t addr, size_t len)
{
    if (!crc32_table_ready) crc32_init_table();

    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; i++) {
        uint8_t byte = *(volatile uint8_t *)(addr + i);
        crc = (crc >> 8) ^ crc32_table[(crc ^ byte) & 0xFF];
    }
    return crc ^ 0xFFFFFFFFUL;
}
