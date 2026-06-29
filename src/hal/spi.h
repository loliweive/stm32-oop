/**
 * @file    spi.h
 * @brief   OOP SPI 主机驱动 — vtable 多态模式
 */

#ifndef SPI_HAL_H
#define SPI_HAL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前置声明 + Vtable ──────────────────────────────────────── */
typedef struct SpiPort SpiPort;

typedef struct {
    void    (*init)(SpiPort *self);
    uint8_t (*transfer)(SpiPort *self, uint8_t byte);
    void    (*transfer_buf)(SpiPort *self, const uint8_t *tx, uint8_t *rx, size_t len);
} SpiVtable;

/* ── 实例结构体 ──────────────────────────────────────────────── */
struct SpiPort {
    void            *spi;       /* SPI_TypeDef*, void* 避免头文件依赖 */
    const SpiVtable *vtable;
    uint32_t         speed_hz;
    uint32_t         pclk_hz;
    uint8_t          mode;      /* 0..3 (CPOL << 1 | CPHA) */
    uint8_t          _init;
};

/* ── 构造函数 ────────────────────────────────────────────────── */
void SpiPort_ctor(SpiPort *self, void *spi, uint32_t speed_hz, uint32_t pclk_hz, uint8_t mode);

/* ── 公共 API (inline 分发到 vtable) ─────────────────────────── */
static inline void    spi_init(SpiPort *self)  { self->vtable->init(self); }
static inline uint8_t spi_transfer(SpiPort *self, uint8_t byte) { return self->vtable->transfer(self, byte); }
static inline void    spi_transfer_buf(SpiPort *self, const uint8_t *tx, uint8_t *rx, size_t len) { self->vtable->transfer_buf(self, tx, rx, len); }

#ifdef __cplusplus
}
#endif

#endif /* SPI_HAL_H */
