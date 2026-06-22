/**
 * @file    ota_flash.h
 * @brief   Flash 操作接口 — 可替换的存储后端
 *
 * 定义了 OTA 模块所需的 Flash 操作抽象。
 * 默认实现针对 STM32F103 内部 Flash。
 *
 * 可扩展性：
 *   实现 ota_flash_spi.c 即可支持外部 SPI Flash (如 W25Q64)。
 *   只需提供相同的函数签名。
 *
 * ── STM32F103 Flash 限制 ─────────────────────────────────
 *   · 页大小: 1KB (小容量产品) 或 2KB (大容量)
 *   · 写入: 必须 16-bit (半字) 对齐
 *   · 擦除: 页擦除 (~20ms) 或 全片擦除
 *   · 寿命: ~10K 擦写周期
 *   · 写 Flash 时 CPU 停顿 (Flash 总线被占用)
 */

#ifndef OTA_FLASH_H
#define OTA_FLASH_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Flash 操作接口 ──────────────────────────────────────────── */

/**
 * @brief 初始化 Flash 子系统 (解锁 Flash 控制寄存器)
 *
 * STM32F103 的 Flash 默认是写保护的，必须先解锁才能擦写。
 */
void ota_flash_init(void);

/**
 * @brief 锁定 Flash (写保护)
 */
void ota_flash_lock(void);

/**
 * @brief 擦除一个 Flash 页
 *
 * @param page_addr 页起始地址 (必须在页边界上)
 * @return true 成功
 */
bool ota_flash_erase_page(uint32_t page_addr);

/**
 * @brief 擦除指定范围的 Flash 页
 *
 * @param start 起始地址 (自动对齐到页边界)
 * @param size  要擦除的字节数 (自动扩展到包含的页)
 * @return true 成功
 */
bool ota_flash_erase_range(uint32_t start, size_t size);

/**
 * @brief 写入数据到 Flash (半字对齐)
 *
 * STM32F103 Flash 必须按 16-bit 写入。
 * 地址必须是半字对齐 (bit 0 = 0)。
 *
 * @param addr  目标地址 (Flash 内, 半字对齐)
 * @param data  数据指针
 * @param len   字节数 (必须是偶数)
 * @return true 成功
 */
bool ota_flash_write(uint32_t addr, const uint8_t *data, size_t len);

/**
 * @brief 验证 Flash 内容与数据是否一致
 *
 * @param addr  起始地址
 * @param data  预期数据
 * @param len   长度
 * @return true 完全一致
 */
bool ota_flash_verify(uint32_t addr, const uint8_t *data, size_t len);

/**
 * @brief 读取 Flash (简单的 memcpy 封装)
 *
 * @param addr Flash 地址
 * @param buf  输出缓冲区
 * @param len  长度
 */
void ota_flash_read(uint32_t addr, uint8_t *buf, size_t len);

/**
 * @brief 计算 Flash 区域的 CRC32
 *
 * @param addr 起始地址
 * @param len  长度
 * @return CRC32 校验值
 */
uint32_t ota_flash_crc32(uint32_t addr, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* OTA_FLASH_H */
