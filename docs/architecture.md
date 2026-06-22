# 系统架构

## 分层架构

```
┌──────────────────────────────────────────────────────┐
│                  Application Layer                    │
│    ds18b20_freertos (CLI + Sensor + OLED + OTA)      │
│    ds18b20_reader (裸机温度)                          │
│    bootloader (OTA 启动加载)                          │
├──────────────────────────────────────────────────────┤
│              Middleware / Services                    │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌────────────────┐ │
│  │  CLI   │ │  OTA   │ │Sensor  │ │  Display/Storage│ │
│  │ 命令行 │ │ 固件升级│ │ 接口   │ │  OLED  SPI Flash│ │
│  └────────┘ └────────┘ └────────┘ └────────────────┘ │
├──────────────────────────────────────────────────────┤
│               HAL (OOP vtable)                        │
│  ┌─────┐ ┌─────┐ ┌─────┐ ┌──────┐ ┌─────┐ ┌──────┐  │
│  │GPIO │ │UART │ │ I2C │ │ SPI  │ │Timer│ │ ADC  │  │
│  │vtab │ │注入 │ │     │ │      │ │vtab │ │      │  │
│  └─────┘ └─────┘ └─────┘ └──────┘ └─────┘ └──────┘  │
│  ┌─────┐ ┌─────┐                                    │
│  │ RCC │ │NVIC │                                    │
│  └─────┘ └─────┘                                    │
├──────────────────────────────────────────────────────┤
│                Core Runtime                           │
│  ┌────────┐ ┌──────────┐ ┌──────┐ ┌──────────────┐  │
│  │ Object │ │RingBuffer│ │ List │ │StateMachine  │  │
│  └────────┘ └──────────┘ └──────┘ └──────────────┘  │
│  ┌────────┐ ┌────────┐ ┌───────┐                    │
│  │ Assert │ │  Log   │ │ Delay │                    │
│  └────────┘ └────────┘ └───────┘                    │
├──────────────────────────────────────────────────────┤
│            Hardware Abstraction                       │
│  ┌──────────────────────────────────────────────┐    │
│  │  CMSIS Core (Cortex-M3) + STM32F103 headers  │    │
│  └──────────────────────────────────────────────┘    │
├──────────────────────────────────────────────────────┤
│         STM32F103C8T6 (Cortex-M3, 72MHz)             │
│         64KB Flash / 20KB SRAM                        │
└──────────────────────────────────────────────────────┘
```

## 模块依赖

```
app (main.c)
 ├── cli (命令行)
 │    └── uart (依赖注入 SerialInterface)
 ├── sensor (多态接口)
 │    ├── ds18b20 → onewire → gpio
 │    ├── dht11 → gpio (位操作)
 │    └── light_sensor → adc + gpio
 ├── ota (固件升级)
 │    ├── ota_client → transport + flash + protocol
 │    ├── ota_transport_shared → uart
 │    └── ota_flash → stm32f103xb (寄存器)
 ├── display/ssd1306 → i2c
 ├── storage/spi_flash → spi + gpio
 ├── FreeRTOS (tasks, queue)
 │    └── port (ARM_CM3, SysTick, PendSV)
 └── hal (gpio, uart, rcc, timer, i2c, spi, adc)
      └── stm32f103xb (寄存器) + cmsis (内核)
```

## OOP 模式分布

| 层次 | 模块 | 模式 |
|------|------|------|
| HAL | GPIO, Timer | vtable 虚函数表 |
| HAL | UART | 依赖注入 (SerialInterface) |
| Sensor | DS18B20, DHT11, LightSensor | vtable (Sensor 接口) |
| OTA | Transport | vtable (OtaTransportVtable) |
| CLI | — | 纯 C (单实例, 不需要多态) |
| Core | RingBuffer, List, StateMachine | 纯 C (数据结构) |

## 构建系统

CMake 双模式/四模式:
- `BUILD_MODE=host`: x86 host 单元测试
- `BUILD_MODE=stm32f103`: 裸机固件交叉编译
- `BUILD_MODE=freertos`: FreeRTOS 固件交叉编译
- `BUILD_MODE=bootloader`: OTA bootloader 交叉编译

ARM GCC 工具链通过 `cmake/toolchain-arm-none-eabi.cmake` 配置。
