# stm32-oop

**面向对象的 C 语言嵌入式微框架** — STM32F103C8T6 (Blue Pill)

可复用 · 可测试 (TDD) · OOP in C (vtable) · 依赖注入 · FreeRTOS · CLI · OTA · OLED

---

## 快速开始

```bash
git clone --recurse-submodules https://github.com/loliweive/stm32-oop.git
cd stm32-oop

# 1. 运行所有 host 端单元测试 (8 suites, 60+ assertions)
cmake -B build/test -DBUILD_MODE=host && cmake --build build/test
cd build/test && ctest --output-on-failure

# 2. 交叉编译裸机固件 (ds18b20 reader, ~7KB)
cmake -B build/target -DBUILD_MODE=stm32f103 && cmake --build build/target

# 3. 交叉编译 FreeRTOS + CLI + 传感器 + OLED + OTA (~26KB)
cmake -B build/freertos -DBUILD_MODE=freertos && cmake --build build/freertos

# 4. 交叉编译 OTA Bootloader (~5.4KB)
cmake -B build/bootloader -DBUILD_MODE=bootloader && cmake --build build/bootloader

# 5. 烧录
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/freertos/stm32-oop.elf verify reset exit"
```

---

## 硬件接线 (当前 IO 分配)

```
STM32F103C8T6 (Blue Pill)
       ┌──────────────────────────┐
 M2/10 │ PA1   DS18B20/DHT11 数据  │ OneWire / 单总线
 M7    │ PA3   光敏 AO (模拟)      │ ADC1_IN3
 M12   │ PA5   SPI1_SCK           │ Flash 时钟
 M12   │ PA6   SPI1_MISO          │ Flash 主入从出
 M12   │ PA7   SPI1_MOSI          │ Flash 主出从入
 串口  │ PA9   USART1_TX           │ CLI 输出 (115200)
 串口  │ PA10  USART1_RX           │ CLI 输入
  SWD  │ PA13  SWDIO               │ 调试 (保留)
  SWD  │ PA14  SWCLK               │ 调试 (保留)
       ├──────────────────────────┤
 M1    │ PB6   I2C1_SCL            │ OLED 时钟
 M1    │ PB7   I2C1_SDA            │ OLED 数据
 M12   │ PB9   SPI_CS              │ Flash 片选
 M7    │ PB11  光敏 DO (数字)       │ 阈值输出
 按键  │ PB14  上拉按键 (按下=LOW)   │
       ├──────────────────────────┤
 LED   │ PC13  板载 LED (低电平亮) │
       └──────────────────────────┘

总计: 13/48 引脚已用 | 5 个外设 (USART1 SPI1 I2C1 ADC1 SWD)
```

### 模块列表

| 编号 | 模块 | 协议 | 引脚 |
|:----:|------|------|------|
| M1 | SSD1306 OLED 128×64 | I2C | PB6, PB7 |
| M2 | DS18B20 温度 (-55~125°C) | OneWire | PA1 |
| M7 | 光敏电阻 | ADC + GPIO | PA3, PB11 |
| M10 | DHT11 温湿度 (0~50°C + RH) | 单总线 | PA1 |
| M12 | W25Qxx SPI Flash (8MB) | SPI | PA5-7, PB9 |

> M2 和 M10 共用 PA1，编译宏 `SENSOR_TYPE` 切换，物理可插拔替换。

---

## 项目结构

```text
stm32-oop/
├── src/
│   ├── core/                运行时: Object, RingBuffer, List, StateMachine
│   ├── hal/                 硬件抽象: GPIO, UART, I2C, SPI, Timer, ADC, RCC
│   ├── sensor/              传感器: Sensor(interface), DS18B20, DHT11, Light
│   ├── display/             OLED: SSD1306 128×64 I2C
│   ├── storage/             存储: SPI Flash W25Qxx
│   ├── cli/                 命令行: 行编辑, 历史, Tab 补全, ANSI 终端
│   ├── ota/                 固件升级: 协议, Flash 操作, 传输层, 客户端
│   ├── utils/               工具: Assert, Log, Delay
│   ├── startup/             启动: 汇编向量表, SystemInit
│   ├── app/                 实验:
│   │   ├── ds18b20_freertos/  FreeRTOS + CLI + Sensor + OTA + OLED (当前)
│   │   ├── ds18b20_reader/    裸机温度读取
│   │   ├── blinky/            LED 闪烁
│   │   └── uart_echo/         串口回显
│   └── bootloader/          OTA 启动加载器
├── test/                    8 个测试套件 + Unity + Mock
├── lib/
│   ├── FreeRTOS/            FreeRTOS Kernel V11 (git submodule)
│   ├── cmsis/               CMSIS Cortex-M3 头文件
│   └── stm32f1/             STM32F103 寄存器定义
├── tools/                   ota_sender.py (上位机 OTA 工具)
├── docs/                    文档 + 流程图
│   ├── io-map.md            IO 引脚分配图
│   ├── architecture.md      系统架构
│   ├── sensor-oop.md        Sensor OOP 类继承图
│   ├── cli-flow.md          CLI 命令处理流程
│   ├── freertos-tasks.md    FreeRTOS 任务架构
│   └── ota-flow.md          OTA 更新流程
├── linker/                  链接脚本
├── cmake/                   工具链文件
└── scripts/                 flash.sh, gdb.sh, run_tests.sh
```

---

## 构建模式

| 模式 | 命令 | Flash | SRAM | 说明 |
|------|------|:-----|:-----|------|
| **host** | `-DBUILD_MODE=host` | — | — | x86 测试, ctest |
| **stm32f103** | `-DBUILD_MODE=stm32f103` | ~7KB | ~0.5KB | 裸机 DS18B20 reader |
| **freertos** | `-DBUILD_MODE=freertos` | ~26KB | ~10KB | CLI + Sensor + OLED + OTA |
| **bootloader** | `-DBUILD_MODE=bootloader` | ~5.4KB | ~1.8KB | OTA 启动加载器 |

---

## 核心设计模式

### OOP in C: vtable 多态

```c
// HAL 层: GPIO, Timer
typedef struct { const GpioVtable *vtable; ... } GpioPin;
gpio_set(&led, 1);  // → self->vtable->set(self, 1)  ← 多态派发

// Sensor 层: DS18B20, DHT11, Light
Sensor *s = ds18b20_create(&obj, &bus);
sensor_read(s, &temp, &hum);   // ← 一行代码, 三种传感器行为!
sensor_name(s);                // → "DS18B20" / "DHT11" / "Light(M7)"
```

### 依赖注入 (UART)

```c
// 运行时: io=NULL → 真实 USART 寄存器
// 测试时: io=&mock → 内存队列, host 端可测试
typedef struct { SerialInterface *io; ... } UartPort;
```

---

## CLI 命令

```
> help
  help          Show this help
  temp          Read sensor once
  temp-stream   Start continuous output
  temp-stop     Stop continuous output
  led           Control LED: on|off|toggle
  btn           Read button state (PB14)
  info          Show system information
  uptime        Show system uptime
  reset         Software reset MCU
  ota-start     Start OTA firmware update
  ota-status    Show OTA update status
  flash-id      Read SPI Flash JEDEC ID
```

### OTA 更新

```bash
# CLI 侧:
> ota-start
CLI suspended — send firmware now.

# PC 侧:
python3 tools/ota_sender.py firmware.bin --port /dev/tty.usbserial-*
```

---

## 测试

| 套件 | 用例 | 类型 |
|------|:----:|------|
| test_ring_buffer | 11 | 核心数据结构 |
| test_list | 7 | 侵入式链表 |
| test_state_machine | 7 | 状态机引擎 |
| test_assert | 4 | 断言系统 |
| test_gpio | 6 | vtable mock |
| test_uart | 7 | 依赖注入 |
| test_timer | 5 | Timer mock |
| test_ota_protocol | 12 | CRC16 + 帧编解码 |
| **合计** | **60+** | — |

---

## 要求

- **arm-none-eabi-gcc** (ARM GCC 14.2+)
- **CMake** 3.20+
- **OpenOCD** + ST-Link (烧录/调试)
- **pyserial** (OTA 上位机工具)

## License

MIT
