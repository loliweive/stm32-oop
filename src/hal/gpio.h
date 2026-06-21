/**
 * @file    gpio.h
 * @brief   OOP GPIO 驱动 / Object-Oriented GPIO Driver
 *
 * 这是 OOP in C 的核心示范。通过 vtable (虚函数表) 模式，
 * 实现对硬件寄存器的面向对象封装。每根引脚是一个 GpioPin "实例"，
 * 方法通过 vtable 派发，可以注入 mock 用于测试。
 *
 * ── 关键设计：vtable 模式 ─────────────────────────────────
 *
 *   在 C++ 中你会写：
 *     class GpioPin { virtual void set(uint8_t level); };
 *     led.set(1);  // 编译器通过 vptr 找到实际函数
 *
 *   在 C 中我们手动实现 vtable：
 *     struct GpioPin { const GpioVtable *vtable; ... };
 *     gpio_set(&led, 1);  // 内联函数委托到 vtable->set()
 *
 * ── 为什么用内联调度函数而不是直接调用 vtable？ ─────────
 *   static inline void gpio_set(GpioPin *self, uint8_t level) {
 *       self->vtable->set(self, level);
 *   }
 *
 *   好处：
 *   1. 调用者不需要知道 vtable 的存在
 *   2. 与 C++ 的 pin.set() 语法接近，可读
 *   3. 编译器在 vtable 已知时常量传播直接调用，零开销
 *   4. 可以像接口一样统一调用 (多态)
 *
 * ── 测试策略 ──────────────────────────────────────────────
 *   在目标上：GpioPin_ctor 绑定真实 vtable → 操作寄存器
 *   在 host 上：mock_gpio_inject 注入 mock vtable → 记录调用
 *   测试验证"调用了什么"而非"寄存器写了什么值"
 *
 * ── 内存开销 / Memory ─────────────────────────────────────
 *   GpioPin: 32 bytes (4 ptrs + 3 uint16/8 + vtable ptr)
 *   GpioVtable: 5 function ptrs = 40 bytes (共享, 只有一份)
 *
 *   典型 10 个 GPIO 引脚: 10 × 32 + 40 = 360 bytes
 *   在 20KB SRAM 中完全可以接受
 */

#ifndef GPIO_HAL_H
#define GPIO_HAL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 — 允许在 vtable 定义中引用 GpioPin */
typedef struct GpioPin GpioPin;

/**
 * @brief GPIO 虚函数表 / Virtual Function Table
 *
 * 定义了 GPIO 引脚的所有操作。在真实硬件上绑定到寄存器操作，
 * 在测试中绑定到 mock 实现 (记录调用、模拟行为)。
 *
 * 每个函数指针接收 GpioPin* 作为第一个参数 (类似 C++ 的 this)。
 */
typedef struct {
    /** @brief 初始化引脚 (设置默认模式) */
    void    (*init)(GpioPin *self);

    /** @brief 设置输出电平 (1=高, 0=低) */
    void    (*set)(GpioPin *self, uint8_t level);

    /** @brief 读取当前输入电平 */
    uint8_t (*get)(GpioPin *self);

    /** @brief 翻转输出电平 */
    void    (*toggle)(GpioPin *self);

    /** @brief 设置引脚模式 (输入/输出/复用/模拟) */
    void    (*set_mode)(GpioPin *self, uint8_t mode);
} GpioVtable;

/**
 * @brief GPIO 引脚 "实例"
 *
 * 每根物理引脚对应一个 GpioPin 结构体。
 * 包含引脚标识 (port, pin)、当前模式和虚函数表指针。
 *
 * 内存布局设计：
 *   port 是 void* — 避免在 .h 中引入 GPIO_TypeDef 依赖
 *   vtable 是 const* — 指向 ROM 中的共享虚函数表
 *   _init 是私有标记 — 外部不应直接访问
 */
struct GpioPin {
    void         *port;     /**< GPIO 端口 (GPIO_TypeDef*) */
    uint16_t      pin;      /**< 引脚位掩码 (GPIO_PIN_0..GPIO_PIN_15) */
    uint8_t       mode;     /**< 当前引脚模式 */
    const GpioVtable *vtable; /**< 虚函数表指针 (生产=真实, 测试=mock) */
    uint8_t       _init;    /**< 私有：初始化标记 */
};

/**
 * @brief 构造函数 / Constructor
 *
 * 初始化 GpioPin 实例，绑定默认 vtable (真实硬件操作)。
 * 在测试中需调用 mock_gpio_inject() 覆盖 vtable。
 *
 * @param self 未初始化的 GpioPin 指针
 * @param port GPIO 端口基地址 (GPIOA, GPIOB, GPIOC)
 * @param pin  引脚位掩码 (GPIO_PIN_0 .. GPIO_PIN_15)
 *
 * 示例：
 *   GpioPin led;
 *   GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);  // Blue Pill 板载 LED
 *   gpio_set_mode(&led, GPIO_MODE_OUT_PP);
 *   gpio_set(&led, 1);  // 点亮
 */
void GpioPin_ctor(GpioPin *self, void *port, uint16_t pin);

/**
 * @brief 便捷调度函数 / Inline Dispatchers
 *
 * 这些内联函数将调用委托给 vtable 中的对应函数。
 * 编译器在编译时已知 vtable 的情况下会直接内联跳转，
 * 开销等价于一个间接函数调用 (2-3 条 ARM 指令)。
 *
 * 为什么不直接用 self->vtable->set(self, level)？
 *   封装。调用者不需要知道 vtable 的存在。
 *   如果以后改变派发机制 (如加入 tracing)，只改这里即可。
 */
static inline void gpio_init(GpioPin *self)     { self->vtable->init(self); }
static inline void gpio_set(GpioPin *self, uint8_t level) { self->vtable->set(self, level); }
static inline uint8_t gpio_get(GpioPin *self)   { return self->vtable->get(self); }
static inline void gpio_toggle(GpioPin *self)   { self->vtable->toggle(self); }
static inline void gpio_set_mode(GpioPin *self, uint8_t mode) { self->vtable->set_mode(self, mode); }

#ifdef __cplusplus
}
#endif

#endif /* GPIO_HAL_H */
