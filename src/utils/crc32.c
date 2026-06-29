/**
 * @file    crc32.c
 * @brief   CRC32 统一实现 (Ethernet/zip polynomial 0xEDB88320)
 */
#include "crc32.h"
#include <stdbool.h>

static uint32_t crc32_table[256];
static bool crc32_ready = false;

static void crc32_init(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320UL : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_ready = true;
}

uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len)
{
    if (!crc32_ready) crc32_init();

    for (size_t i = 0; i < len; i++) {
        uint8_t idx = (uint8_t)(crc ^ data[i]);
        crc = (crc >> 8) ^ crc32_table[idx];
    }
    return crc;
}
