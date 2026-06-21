# stm32-oop

**面向对象的 C 语言嵌入式微框架** | STM32F103C8T6 (Blue Pill)

一套可复用、可测试、可移植的嵌入式软件基础，融合 OOP in C + TDD + 依赖注入。

## 快速开始

```bash
# 1. 运行所有 host 端单元测试
cmake -B build/test -DBUILD_MODE=host
cmake --build build/test
cd build/test && ctest --output-on-failure

# 2. 编译 STM32 固件
cmake -B build/target -DBUILD_MODE=stm32f103
cmake --build build/target

# 3. 烧录 (需要 ST-Link)
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/target/stm32-oop.elf verify reset exit"
```

## 项目结构

```
src/core/      运行时：Object · RingBuffer · List · StateMachine
src/hal/       硬件抽象：GPIO · UART · Timer · I2C · SPI · ADC · RCC
src/utils/     工具：Assert · Log · Delay
src/app/       实验：blinky · breathing · uart_echo
test/          单元测试：7 套件 · Unity · Mock (vtable 注入)
lib/           CMSIS + STM32F103 寄存器定义
```

## 核心设计

| 模式 | 说明 |
|------|------|
| **vtable 派发** | C 语言模拟虚函数 — GPIO/Timer 等外设通过虚函数表实现多态 |
| **依赖注入** | UART 通过 `SerialInterface` 注入 mock，host 端可直接测试 |
| **双模式 CMake** | `-DBUILD_MODE=host` 运行 x86 测试，`=stm32f103` 交叉编译 |
| **零动态分配** | 所有对象静态分配，适配 20KB SRAM |
| **FreeRTOS 就绪** | 调度器接口已定义，裸机协作式 → RTOS 抢占式一键切换 |

## OOP in C 示例

```c
// "类" → 虚函数表 + 内联派发
GpioPin led;
GpioPin_ctor(&led, GPIOC, GPIO_PIN_13);  // 构造
gpio_set_mode(&led, GPIO_MODE_OUT_PP);    // 设置模式
gpio_set(&led, 1);                        // 点亮 LED

// 测试时注入 mock vtable → 零硬件依赖
```

## 测试覆盖

| 测试套件 | 内容 |
|---------|------|
| `test_ring_buffer` | 11 用例 — 空/满/绕回/批量/压力 |
| `test_list` | 7 用例 — 增删/遍历/容器宏 |
| `test_state_machine` | 7 用例 — 转移/守卫/层次/跳转 |
| `test_assert` | 4 用例 — 处理器/NDEBUG |
| `test_gpio` | 6 用例 — vtable 派发/mock 记录 |
| `test_uart` | 7 用例 — 依赖注入/收发/缓冲 |
| `test_timer` | 5 用例 — 启停/PWM/周期 |

## 环境要求

- **macOS**: `brew install arm-none-eabi-gcc openocd cmake`
- **Linux**: `apt install gcc-arm-none-eabi openocd cmake`
- **Windows**: 推荐 WSL2 + 同上 Linux 安装

## 实验列表

| 实验 | 文件 | 说明 |
|------|------|------|
| LED 闪烁 | `src/app/blinky/main.c` | 板载 PC13，1Hz |
| 呼吸灯 | `src/app/blinky/breathing.c` | 软件 PWM 渐变 |
| UART 回显 | `src/app/uart_echo/main.c` | USART1, 115200-8N1 |

## License

MIT
