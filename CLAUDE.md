# CLAUDE.md — STM32-oop 项目状态 & 接力文件

> **最后更新**: 2026-06-28
> **当前会话工作**: 移植江协科技 OLED 驱动 → OOP vtable 模式, 20 个 host 单元测试
> **下一次会话**: 阅读此文件即可接续, 无需重复询问上下文。

---

## 1. 项目身份

| 项 | 值 |
|------|-----|
| **名称** | stm32-oop |
| **芯片** | STM32F103C8T6 (Blue Pill, Cortex-M3, 72MHz) |
| **Flash / SRAM** | 64KB / 20KB |
| **工具链** | ARM GCC 14.2 (`brew install arm-none-eabi-gcc`) |
| **构建** | CMake 4.0 双/四模式 |
| **测试** | Unity + CMock (host x86 运行) |
| **RTOS** | FreeRTOS V11 (git submodule: `lib/FreeRTOS`) |
| **GitHub** | https://github.com/loliweive/stm32-oop |
| **许可** | MIT |

---

## 2. 快速开始 (新会话复制粘贴即可)

```bash
cd /Users/vivi/AI/ClaudeCode/stm32-oop
git submodule update --init  # 首次需要

# 运行所有测试
cmake -B build/test -DBUILD_MODE=host && cmake --build build/test
cd build/test && ctest --output-on-failure

# App (默认 0x08002000, 配合 bootloader)
cmake -B build/freertos -DBUILD_MODE=freertos && cmake --build build/freertos

# 独立 App (0x08000000, 无 bootloader)
cmake -B build/standalone -DBUILD_MODE=freertos -DSTANDALONE=ON && cmake --build build/standalone

# Bootloader (8KB @ 0x08000000)
cmake -B build/bootloader -DBUILD_MODE=bootloader && cmake --build build/bootloader

# 烧录 — bootloader + app (推荐)
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/bootloader/stm32-oop.bin 0x08000000 verify" \
  -c "program build/freertos/stm32-oop.bin 0x08002000 verify" \
  -c "reset run" -c "shutdown"

# 烧录 — 独立 App (调试用)
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program build/standalone/stm32-oop.bin 0x08000000 verify reset exit"
```

---

## 3. 硬件接线 (IO 完整分配)

```
STM32F103C8T6 Blue Pill — 13/48 引脚已用, 5 个外设

  PA1  = M2 DS18B20 DQ / M10 DHT11 DAT (共用, 编译切换, 4.7kΩ上拉)
  PA3  = M7 光敏 AO (ADC1_IN3)
  PA5  = M12 SPI Flash SCK  (SPI1)
  PA6  = M12 SPI Flash MISO (SPI1)
  PA7  = M12 SPI Flash MOSI (SPI1)
  PA9  = USART1 TX → 串口终端 CLI (115200-8N1)
  PA10 = USART1 RX ← 串口终端 CLI
  PA13 = SWDIO (调试, 保留)
  PA14 = SWCLK (调试, 保留)

  PB6  = M1 SSD1306 OLED SCL (I2C1)
  PB7  = M1 SSD1306 OLED SDA (I2C1)
  PB9  = M12 SPI Flash CS (GPIO 片选)
  PB11 = M7 光敏 DO (数字阈值, GPIO 输入)
  PB14 = 上拉按键 (按下=LOW)

  PC13 = 板载 LED (低电平亮)
  PC14 = OSC32_IN (保留)
  PC15 = OSC32_OUT (保留)
```

### 模块编号

| 编号 | 模块 | 协议 | 关键引脚 |
|:----:|------|------|---------|
| M1 | SSD1306 OLED 128×64 | I2C @ 0x3C | PB6, PB7 |
| M2 | DS18B20 温度 | OneWire | PA1 |
| M7 | 光敏电阻 | ADC + GPIO | PA3, PB11 |
| M10 | DHT11 温湿度 | 单总线 | PA1 |
| M12 | W25Qxx SPI Flash | SPI | PA5-7, PB9 |

**M2 和 M10 共用 PA1** — 物理可插拔替换。编译时通过 `#define SENSOR_TYPE` 切换。

---

## 4. 架构总览

```
app/           ds18b20_freertos (当前主线) | blinky | uart_echo | ota_demo | oled_test
middleware/    cli/ | ota/ | sensor/ (OOP 接口) | display/ (OLED OOP) | storage/
hal/           gpio(vtable) | uart(依赖注入) | i2c | spi | timer(vtable) | adc | rcc
core/          ring_buffer | list | state_machine | object
lib/           CMSIS | STM32F103 regs | FreeRTOS V11 (submodule)
```

### 核心设计模式

1. **vtable 多态**: GPIO, Timer, Sensor, OLED 使用虚函数表 → 测试时可注入 mock
2. **依赖注入**: UART 的 `SerialInterface` → host 测试不需要真实串口
3. **Sensor OOP**: `DS18B20`, `DHT11`, `LightSensor` 都实现 `Sensor` 接口
4. **OLED OOP**: `OledDisplay` 抽象接口 → `SSD1306` 具体实现 (vtable + 帧缓冲)
   - 移植自江协科技 OLED 驱动 V2.0, 完整保留所有绘图/文本/中文功能
   - 应用层通过 `OledDisplay*` 统一操作, 不关心底层硬件
   - 支持: 点/线/矩形/三角/圆/椭圆/圆弧 + 8x16/6x8字体 + UTF-8中文 + printf

### FreeRTOS 任务

| 任务 | 优先级 | 栈 | 职责 |
|------|:------:|:-----|------|
| Sensor Task | 2 | 512B | 周期采样 → Queue → 刷新 OLED |
| CLI Task | 1 | 2KB | UART RX/TX, 命令派发, OTA 流程 |
| IDLE | 0 | FreeRTOS | 自动创建 |

---

## 5. 构建系统

### CMake 四模式

| BUILD_MODE | 编译器 | 输出 | 大小 |
|------------|--------|------|------|
| `host` | Apple Clang | x86 可执行 + ctest | — |
| `stm32f103` | arm-none-eabi-gcc | 裸机固件 .elf/.bin/.hex | ~7KB |
| `freertos` | arm-none-eabi-gcc | FreeRTOS + CLI + 全功能 | ~26KB |
| `bootloader` | arm-none-eabi-gcc | OTA 启动器 (8KB 分区) | ~5.4KB |

### 注意事项
- **Toolchain 必须在 `project()` 之前设置** — `CMakeLists.txt` 第 14-18 行处理
- FreeRTOSConfig.h 位于 `src/app/freertos_demo/` (include path 依赖)
- `delay.c` 使用 `#ifndef USE_FREERTOS` 条件编译 — FreeRTOS 下 SysTick 由内核管理
- `test/mocks/` 目录有 host 兼容的 `stm32f103xb.h` 和 `core_cm3.h` stub

---

## 6. 测试

```bash
cmake -B build/test -DBUILD_MODE=host && cmake --build build/test
cd build/test && ctest --output-on-failure
```

| # | 套件 | 内容 |
|:--|------|------|
| 1 | test_ring_buffer | 11 用例 — 空/满/绕回 |
| 2 | test_list | 7 用例 — 增删/遍历 |
| 3 | test_state_machine | 7 用例 — 转移/守卫 |
| 4 | test_assert | 4 用例 |
| 5 | test_gpio | 6 用例 — vtable mock |
| 6 | test_uart | 7 用例 — 依赖注入 |
| 7 | test_timer | 5 用例 |
| 8 | test_ota_protocol | 12 用例 — CRC16 + 帧编解码 |
| 9 | test_ssd1306 | 20 用例 — vtable/帧缓冲/绘图/文本 |

---

## 7. Git 工作流

### 推送 (⚠️ 需要特殊处理)

用户的 git 全局配置了 `ghproxy.net` 代理, 但代理需要认证。推送命令：

```bash
# 不能直接用 git push — 会被代理拦截要求用户名
# 正确方式:
TOKEN=$(gh auth token) && \
  git push "https://loliweive:${TOKEN}@ghproxy.net/https://github.com/loliweive/stm32-oop.git" main
```

### 分支
- `main` — 唯一分支, 所有开发在此

### 提交规范
- `feat:` 新功能
- `fix:` 修复
- `refactor:` 重构
- `docs:` 文档
- 中英文混合 commit message

---

## 8. 当前状态

### ✅ 已完成
- [x] 完整的 HAL 层 (GPIO/UART/I2C/SPI/Timer/ADC/RCC/NVIC) — 全部 OOP vtable
- [x] 核心运行时 (RingBuffer, List, StateMachine, Object, Assert, Log, Delay)
- [x] TDD 单元测试 (9 套件, 80+ 断言, 100% pass)
- [x] FreeRTOS 集成 (V11, ARM_CM3 port, heap_4, 6KB heap)
- [x] OneWire 总线驱动 (DS18B20)
- [x] DHT11 温湿度驱动
- [x] 光敏传感器驱动 (ADC + GPIO)
- [x] Sensor OOP 接口 (3 个实现)
- [x] **OLED OOP 驱动** — 移植江协科技 V2.0 → vtable 模式
  - 完整绘图: 点/线/矩形/三角/圆/椭圆/圆弧
  - 双字体: 8x16 + 6x8 ASCII
  - UTF-8 中文支持 (16x16 字模)
  - 格式化输出 (printf)
  - 20 个 host 单元测试
- [x] SPI Flash W25Qxx 驱动 (JEDEC 自动识别)
- [x] CLI 引擎 (行编辑, 历史, Tab 补全, ANSI)
- [x] OTA 固件升级 (bootloader + 协议 + Python 发送工具)
- [x] 代码审查 + 安全修复 (CRITICAL 5 + HIGH 6)
- [x] 完整文档 + 6 个流程图
- [x] CLAUDE.md 接力文件 (本文件)

### 🔜 待实现
- [ ] 将裸机版也接入 CLI + Sensor OOP
- [ ] BMP280 / SHT30 等 I2C 传感器 (验证 OOP 可扩展性)
- [ ] OTA 安全加固 (固件签名, 版本回滚)
- [ ] LittleFS on SPI Flash
- [ ] 看门狗 (IWDG)
- [ ] 低功耗模式实验

## 13. 外设测试固件

每个外设一份独立测试固件, 用于快速验证硬件是否正常。
位于 `tests/peripheral/`, 预编译 `.bin` 在 `tests/peripheral/bin/`。

```bash
# 构建单个测试
cmake -B build/peripheral_<name> -DBUILD_MODE=peripheral -DPERIPHERAL=<name>
cmake --build build/peripheral_<name>

# 烧录 (替换 <name>)
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
  -c "program tests/peripheral/bin/test_<name>.bin 0x08000000 verify reset exit"
```

| 测试 | 文件 | 大小 | 正常表现 |
|------|------|:--:|------|
| LED | `test_led.bin` | 684B | PC13 亮1秒灭1秒 (SysTick 精确定时 @72MHz) |
| UART Echo | `test_uart_echo.bin` | 376B | 串口 115200, 敲键原样回显, 回车补换行 |
| OLED | `test_oled.bin` | 13.7KB | 第1屏: 文本"OLED Test/SSD1306 OK" → 第2屏: 几何图形 → 第3屏: 计数器 |
| Button | `test_button.bin` | 748B | 启动3快闪→就绪。按 PB14→LED亮, 松→LED灭 |
| SPI Flash | `test_spi_flash.bin` | 1KB | 启动3快闪→就绪。LED常亮=JEDEC ID OK, LED慢闪=未检测到 |
| DS18B20 | `test_ds18b20.bin` | 1.2KB | 启动1快闪→就绪。LED常亮=传感器OK, LED慢闪=未检测到(检查PA1+4.7kΩ上拉) |
| CLI+OLED | `test_cli_oled.bin` | 30KB | 串口115200: help/hello/uptime/led/oled, OLED显示"OLED Ready" |

**规则: 新增/修改外设驱动后, 必须更新对应的测试固件并重新生成 .bin**

---

## 9. 已知问题 & 注意事项 & 铁律

### 铁律 (违反必出 bug)
1. **禁止 `%f`**: newlib-nano 不支持，用 `%d.%d` 整数拆分
2. **禁止 `abs()`**: newlib-nano 会 crash，用 `if(x<0)x=-x`
3. **禁止 `GPIO_CNF_PP \| GPIO_MODE_IN`**: =0x00(模拟模式), 输入上拉用 `0x8 \| GPIO_MODE_IN`
4. **禁止 busy-wait 定时**: volatile loop ~7cyc/iter, 不准。用 SysTick/DWT/FreeRTOS tick
5. **禁止传感器任务调 `cli_printf`**: 跨任务重入 crash。传感器只写队列，CLI 任务读队列输出
6. **禁止在 `#if` guard 上做 sed 全局替换**: 会破坏预处理分支

### 已知问题

1. **Git 推送**: 必须用 token-in-URL 方式, 见 §7
2. **I2C/SPI/ADC 驱动**: 使用了 CMSIS 位定义 (代码审查后修复), 但仍缺少超时保护
3. **RCC 时钟配置**: 有 HSE 超时 + HSI fallback (审查后添加)
4. **ring_buffer**: head/tail/full 已声明 volatile, ISR 安全
5. **SSD1306**: 使用低层 `i2c_write_raw()` 绕过 I2cPort 封装 (性能优化, 每页刷新 129 字节连续写)
   - OOP vtable 模式: `OledDisplay` 抽象接口 → `SSD1306` 具体实现
   - 支持多实例 (framebuf 在对象内, 非全局变量)
   - `atan2` 用于圆弧绘制, 需链接 `-lm`
6. **DHT11**: 读取时关中断 (~25ms), 会影响 FreeRTOS tick 精度
7. **DS18B20**: 12-bit 转换需 750ms, sensor task 在此期间阻塞
8. **OTA**: 使用 `ota_transport_shared` 共享 CLI 的 UART, OTA 期间 CLI 暂停
9. **FreeRTOSConfig.h**: 位于 `src/app/freertos_demo/` (不在预期位置, 需在 include path 中)

---

## 10. 关键文件索引

| 文件 | 作用 | 修改频率 |
|------|------|:---:|
| `src/app/ds18b20_freertos/main.c` | 主固件入口, 传感器切换宏, 命令表 | 🔴 高 |
| `CMakeLists.txt` | 构建配置, 四模式, 源文件列表 | 🔴 高 |
| `lib/stm32f1/stm32f103xb.h` | 寄存器定义 (添加外设时更新) | 🟡 中 |
| `src/hal/*.h` | HAL 接口 (vtable 定义) | 🟢 低 |
| `src/display/oled.h` | OLED 抽象接口 (vtable) | 🟢 低 |
| `src/display/ssd1306.h` | SSD1306 具体驱动头 | 🟢 低 |
| `src/display/ssd1306.c` | SSD1306 完整实现 (~550行) | 🟡 中 |
| `src/display/font8x16.c` | ASCII 8x16 字模 | 🟢 低 |
| `src/display/font6x8.c` | ASCII 6x8 字模 | 🟢 低 |
| `src/display/font16x16.c` | 中文 16x16 字模 (UTF-8) | 🟢 低 |
| `src/sensor/sensor.h` | Sensor OOP 接口 | 🟢 低 |
| `src/cli/cli.h` | CLI 引擎接口 | 🟢 低 |
| `src/ota/ota_config.h` | OTA Flash 分区 & 协议参数 | 🟢 低 |
| `test/test_*.c` | 单元测试 (9 套件, 80+ 断言) | 🟡 中 |
| `docs/*.md` | 文档 & 流程图 | 🟡 中 |
| `CLAUDE.md` | 本文件 — 每次大改动后更新 | 🔴 高 |

---

## 11. 文件更新清单 (每次写代码后执行)

- [ ] 如果改了 IO 引脚 → 更新 `docs/io-map.md` + `CLAUDE.md §3` + `README.md`
- [ ] 如果加了新模块 → 更新 `CMakeLists.txt` + `CLAUDE.md §4` + `README.md` + `docs/architecture.md`
- [ ] 如果加了新 CLI 命令 → 更新 `CLAUDE.md §4` 命令表
- [ ] 如果改了构建 → 更新 `CLAUDE.md §5`
- [ ] 每次会话结束 → 更新 `CLAUDE.md §8` 状态 + `CLAUDE.md §12` 工作日志
- [ ] 重大架构变化 → 更新对应 `docs/*.md` 流程图

---

## 12. 工作日志

### 2026-06-27 (本次会话)
1. **OLED 移植**: 从江协科技 OLED 驱动 V2.0 移植到项目
2. **OOP 重构**: 创建 `OledDisplay` 抽象接口 (vtable 模式, 类似 Sensor/GPIO)
3. **SSD1306 重写**: 完整实现 22 个 vtable 方法
   - 硬件控制: init/clear/flush/flush_area/reverse
   - 几何绘图: point/line/rect/triangle/circle/ellipse/arc
   - 文本: char/string/num/hex/bin/float/image/printf
   - UTF-8 中文 + GB2312 fallback
4. **字模数据**: 替换旧 ascii_font.c → 3 个新文件
   - `font8x16.c` — 95 个 ASCII 8x16 字模
   - `font6x8.c` — 95 个 ASCII 6x8 字模
   - `font16x16.c` — 中文 16x16 UTF-8 字模 + Diode 图像
5. **单元测试**: 20 个 host 测试 (framebuf 操作, 无需 I2C)
6. **构建更新**: stm32f103 + freertos + host 三模式全部通过
7. **CLAUDE.md 接力**: 更新架构/文件索引/状态/已知问题

### 2026-06-22 (历史会话)
1. **项目骨架**: CMake 双模式 + CMSIS + STM32F103 寄存器 + 链接脚本 + 启动汇编
2. **核心运行时**: RingBuffer, List, StateMachine, Object, Assert, Log, Delay
3. **HAL 层**: GPIO(vtable), UART(依赖注入), Timer(vtable), I2C, SPI, ADC, RCC, NVIC
4. **TDD 测试**: 7 个测试套件 (Unity/CMock), mock 框架
5. **FreeRTOS 集成**: V11 kernel, ARM_CM3 port, heap_4, FreeRTOSConfig, 多任务
6. **DS18B20**: OneWire 总线, DS18B20 驱动, 裸机 + FreeRTOS 实验
7. **DHT11**: 自有协议驱动, 编译宏传感器切换
8. **Sensor OOP**: Sensor 抽象接口 + vtable 多态重构 (3 实现)
9. **CLI 引擎**: 行编辑, 历史, Tab 补全, ANSI, 10 个命令
10. **OTA**: 协议(CRC16), Flash 操作, 传输层, 客户端, bootloader, Python 发送工具
11. **CLI OTA 集成**: `ota-start`, `ota-status` 命令, 共享 UART
12. **代码审查**: 修复 5 CRITICAL + 6 HIGH + 魔法数字→CMSIS + 超时保护 + volatile
13. **新模块**: M7 光敏 (ADC+GPIO), M1 OLED (I2C SSD1306), M12 SPI Flash (W25Qxx), 按键 (PB14)
14. **文档**: README 重写, 6 个流程图, CLAUDE.md 接力文件
15. **Git**: 初始化, 6 次提交, 推送到 GitHub (通过 ghproxy 代理)

---

> **给下一个会话的提示**: 
> 1. 先读此文件和 `docs/` 下的流程图
> 2. 运行 `cmake -B build/test -DBUILD_MODE=host && cmake --build build/test && cd build/test && ctest` 确认环境正常
> 3. 要改传感器: 修改 `main.c` 顶部 `#define SENSOR_TYPE`
> 4. 要加新模块: 参考 `src/sensor/light_sensor.c` 的 Sensor OOP 模式
> 5. 推送用 §7 的特殊命令
