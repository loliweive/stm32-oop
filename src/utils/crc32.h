/**
 * @file    crc32.h
 * @brief   CRC32 统一实现 (Ethernet/zip polynomial 0xEDB88320)
 *
 * 替代 ota_client.c 和 ota_flash.c 中的两份独立实现。
 * 256 条目查找表，约 1KB ROM。
 */
#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 增量 CRC32 计算（可多次调用追加数据） */
uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len);

/** @brief 单次 CRC32 计算 */
static inline uint32_t crc32_compute(const uint8_t *data, size_t len) {
    return crc32_update(0xFFFFFFFFUL, data, len) ^ 0xFFFFFFFFUL;
}

#ifdef __cplusplus
}
#endif

#endif
