# IO 引脚分配图

## 完整引脚状态

```
STM32F103C8T6 (Blue Pill) — 64KB Flash / 20KB SRAM
       ┌──────────────────────────┐
       │   PA0  (free)            │
 M2/10 │   PA1  DS18B20/DHT11     │ OneWire / DHT11 数据
       │   PA2  (free)            │
 M7    │   PA3  Light AO          │ ADC1_IN3 模拟光照
       │   PA4  (free)            │
 M12   │   PA5  SPI1_SCK          │ SPI Flash 时钟
 M12   │   PA6  SPI1_MISO         │ SPI Flash 主入从出
 M12   │   PA7  SPI1_MOSI         │ SPI Flash 主出从入
       │   PA8  (free)            │
 串口  │   PA9  USART1_TX         │ CLI 输出
 串口  │   PA10 USART1_RX         │ CLI 输入
       │   PA11 (free)            │
       │   PA12 (free)            │
  SWD  │   PA13 SWDIO             │ 调试接口 (保留)
  SWD  │   PA14 SWCLK             │ 调试接口 (保留)
       │   PA15 (free)            │
       ├──────────────────────────┤
       │   PB0  (free)            │
       │   PB1  (free)            │
       │   PB2  (free)            │
       │   PB3  (free, JTDO)      │
       │   PB4  (free, JNTRST)    │
       │   PB5  (free)            │
 M1    │   PB6  I2C1_SCL          │ OLED 时钟
 M1    │   PB7  I2C1_SDA          │ OLED 数据
       │   PB8  (free)            │
 M12   │   PB9  SPI_CS            │ SPI Flash 片选
       │   PB10 (free)            │
 M7    │   PB11 Light DO          │ 光敏数字阈值
       │   PB12 (free)            │
       │   PB13 (free)            │
 按键  │   PB14 Button            │ 上拉按键 (按下=LOW)
       │   PB15 (free)            │
       ├──────────────────────────┤
       │   PC13 LED               │ 板载 (低电平亮)
       │   PC14 OSC32_IN          │ 低速晶振 (保留)
       │   PC15 OSC32_OUT         │ 低速晶振 (保留)
       └──────────────────────────┘
```

## 引脚统计

| 端口 | 总数 | 已用 | 空闲 | 占用率 |
|------|:----:|:----:|:----:|:------:|
| GPIOA | 16 | 7 | 9 | 44% |
| GPIOB | 16 | 5 | 11 | 31% |
| GPIOC | 16 | 1 | 15 | 6% |
| **合计** | **48** | **13** | **35** | **27%** |

## 外设占用

| 外设 | 用途 | 引脚组合 |
|------|------|---------|
| USART1 | CLI 交互 | PA9(TX) + PA10(RX) |
| SPI1 | Flash 存储 | PA5(SCK) + PA6(MISO) + PA7(MOSI) + PB9(CS) |
| I2C1 | OLED 显示 | PB6(SCL) + PB7(SDA) |
| ADC1 | 光敏采样 | PA3 (CH3) |
| SWD | 调试 | PA13 + PA14 |

## 模块编号

| 编号 | 模块 | 协议 | 数据引脚 |
|:----:|------|------|---------|
| M1 | SSD1306 OLED 128x64 | I2C | PB6, PB7 |
| M2 | DS18B20 温度 | OneWire | PA1 |
| M7 | 光敏电阻 | ADC + GPIO | PA3, PB11 |
| M10 | DHT11 温湿度 | 单总线 | PA1 |
| M12 | W25Qxx SPI Flash | SPI | PA5-7, PB9 |

> M2 和 M10 共用 PA1，通过编译宏 `SENSOR_TYPE` 切换。物理上可插拔替换。
