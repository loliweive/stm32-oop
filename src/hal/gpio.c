/**
 * @file    gpio.c
 * @brief   GPIO vtable 的真实硬件实现 / Real Hardware Implementation
 *
 * 这个文件包含生产代码中使用的 vtable 实现。
 * 操作真实的 STM32F103 寄存器。
 *
 * ── STM32F103 GPIO 寄存器概览 ─────────────────────────────
 *
 *   CRL/CRH: 配置寄存器 (每 4 bits 控制一根引脚的模式)
 *   IDR:     输入数据寄存器 (只读引脚电平)
 *   ODR:     输出数据寄存器 (写入引脚电平)
 *   BSRR:    位设置/复位寄存器 (原子操作，不影响其他位)
 *   BRR:     位复位寄存器
 *
 *   使用 BSRR/BRR 而非 ODR 的原因：
 *     ODR 是读-改-写操作，非原子，ISR 不安全。
 *     BSRR 低 16 位写 1 置位，高 16 位写 1 复位，
 *     只写不读 → 天然原子，ISR 安全。
 *
 * ── GPIO 模式编码 (CNF[1:0] | MODE[1:0]) ─────────────────
 *
 *   00_00 = 模拟输入     01_00 = 浮空输入
 *   00_01 = 推挽输出10MHz 00_10 = 推挽输出2MHz  00_11 = 推挽输出50MHz
 *   01_01 = 开漏输出10MHz 01_10 = 开漏输出2MHz  01_11 = 开漏输出50MHz
 *   10_01 = 复用推挽10MHz 10_10 = 复用推挽2MHz  10_11 = 复用推挽50MHz
 *   11_01 = 复用开漏10MHz 11_10 = 复用开漏2MHz  11_11 = 复用开漏50MHz
 */

#include "gpio.h"
#include "stm32f103xb.h"

/* ═══════════════════════════════════════════════════════════════
 *  默认 vtable — 真实硬件操作
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief 初始化 GPIO 引脚
 * 设置为默认模式 (推挽输出 50MHz) 并标记已初始化。
 */
static void _gpio_init(GpioPin *self)
{
    self->_init = 1;
    /* 默认设置为推挽输出 — 最常见的初始模式 */
    gpio_set_mode(self, GPIO_MODE_OUT_PP);
}

/**
 * @brief 设置引脚输出电平
 *
 * 使用 BSRR 寄存器进行原子写操作：
 *   - level=1 → BSRR 低 16 位对应 pin 位写 1 → 引脚置高
 *   - level=0 →  BRR 对应 pin 位写 1 → 引脚置低
 *
 * BSRR/BRR 是只写寄存器，读操作无意义。
 * 写 0 的位不受影响，所以多任务/ISR 中安全。
 *
 * 为什么不用 ODR？
 *   ODR = (ODR & ~pin) | (level ? pin : 0);  // 读-改-写，非原子
 *   如果 ISR 在读取 ODR 后修改了某位，再写回 ODR 会覆盖 ISR 的修改。
 */
static void _gpio_set(GpioPin *self, uint8_t level)
{
    GPIO_Type *reg = (GPIO_Type *)self->port;  /* 转换为寄存器结构体 */
    if (level) {
        reg->BSRR = self->pin;   /* 置位：BSRR 低 16 位写 1 对应 pin 置高 */
    } else {
        reg->BRR = self->pin;    /* 复位：BRR 对应位写 1 置低 */
    }
}

/**
 * @brief 读取引脚当前电平
 *
 * 从 IDR 读取实际引脚电平，1 = 高电平，0 = 低电平。
 * 读的是实际引脚状态，不是上次写入的值。
 */
static uint8_t _gpio_get(GpioPin *self)
{
    GPIO_Type *reg = (GPIO_Type *)self->port;
    return (reg->IDR & self->pin) ? 1 : 0;
}

/**
 * @brief 翻转引脚电平
 *
 * 先读当前电平，再写相反值。
 * 注意：这不是原子操作 (读→判断→写有间隙)，
 * 但 ISR 中通常够用 (BSRR 保证写入是原子的)。
 */
static void _gpio_toggle(GpioPin *self)
{
    uint8_t cur = _gpio_get(self);
    _gpio_set(self, !cur);
}

/**
 * @brief 设置引脚模式
 *
 * STM32F103 用两个 32 位寄存器 CRL (pin 0-7) 和 CRH (pin 8-15)
 * 每根引脚占 4 bits (CNF[1:0] | MODE[1:0])。
 *
 * 这里用读-改-写操作配置。通常在初始化阶段调用，此时没有并发问题。
 */
static void _gpio_set_mode(GpioPin *self, uint8_t mode)
{
    /* 守卫: pin=0 时后续移位操作会导致未定义行为 */
    if (self->pin == 0) return;

    GPIO_Type *reg = (GPIO_Type *)self->port;
    uint32_t pos = 0;

    /* 找到 pin 的位置 (0..15) */
    for (pos = 0; pos < 16; pos++) {
        if (self->pin & (1 << pos)) break;
    }
    /* 未找到匹配位 → 退出 (不应发生, pin!=0 保证了至少有一个位) */
    if (pos >= 16) return;

    /* 根据 pin 的位置选择 CRL (0-7) 或 CRH (8-15) */
    if (pos < 8) {
        uint32_t shift = pos * 4;                    /* 每 pin 4 bits */
        reg->CRL = (reg->CRL & ~(0xFUL << shift))    /* 清零对应 4 bits */
                 | ((uint32_t)mode << shift);         /* 写入新模式 */
    } else {
        uint32_t shift = (pos - 8) * 4;
        reg->CRH = (reg->CRH & ~(0xFUL << shift))
                 | ((uint32_t)mode << shift);
    }
    self->mode = mode;  /* 记录模式，方便查询 */
}

/* ═══════════════════════════════════════════════════════════════
 *  虚函数表实例 — ROM 中共享一份
 * ═══════════════════════════════════════════════════════════════ */

/**
 * @brief 真实硬件的虚函数表 (const → 存放在 Flash 中)
 *
 * 所有 GPIO 引脚共享同一个 vtable。
 * 每个引脚的数据 (port, pin, mode) 存储在各自的 GpioPin 结构体中，
 * 行为 (如何 init/set/get/toggle/set_mode) 全部在这里。
 *
 * 这是 OOP 中"类"的概念 — 数据在实例，方法在类。
 */
static const GpioVtable _gpio_vtable = {
    .init     = _gpio_init,
    .set      = _gpio_set,
    .get      = _gpio_get,
    .toggle   = _gpio_toggle,
    .set_mode = _gpio_set_mode,
};

/**
 * @brief GPIO 构造函数
 *
 * 绑定真实硬件 vtable，设置引脚标识。
 * 不操作硬件寄存器 (初始化由 gpio_init/gpio_set_mode 完成)。
 *
 * 注意：port 参数接受 void* 是为了在 .h 中不暴露 GPIO_TypeDef，
 * 实际传入的是 GPIOA/GPIOB/GPIOC 等寄存器基地址。
 */
void GpioPin_ctor(GpioPin *self, void *port, uint16_t pin)
{
    self->port   = port;
    self->pin    = pin;
    self->mode   = 0;
    self->vtable = &_gpio_vtable;  /* 绑定真实硬件 vtable */
    self->_init  = 0;
}
