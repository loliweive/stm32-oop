# STM32-oop 项目总结 & 开发路线图

> 2026-06-28 | 当前版本: 裸机 CLI 开发中

---

## 1. 项目概览

基于 STM32F103C8T6 (Blue Pill) 的嵌入式 OOP 框架，使用 vtable 多态实现硬件抽象。

```
Flash 布局:
0x08000000  Bootloader (4.5KB) — 5级恢复 + OLED报错 + HMAC验签
0x08002000  App (47KB)         — FreeRTOS + CLI + 传感器 + OTA
0x0800F800  Metadata (2KB)

外设:
PA1  = DHT11 温湿度 (OneWire)     PA3  = 光敏 ADC
PA5-7 = SPI1 Flash (W25Qxx)       PA9-10 = USART1 CLI
PB6-7 = I2C1 OLED (SSD1306)       PB11 = 光敏 DO
PB9  = SPI CS                     PB14 = 按键
PC13 = LED

软件栈:
hal/  → vtable 多态 (GPIO/UART/I2C/SPI/ADC/Timer/IWDG)
sensor/ → Sensor 抽象接口 (DHT11/DS18B20/Light/BMP280)
display/ → OledDisplay 抽象 (SSD1306 128×64)
cli/  → 命令行引擎 (ANSI, Tab补全, 历史)
ota/  → OTA 固件升级 (HMAC签名, 外部Flash缓冲)
```

## 2. 当前功能清单

| 模块 | 功能 | 状态 |
|------|------|:--:|
| CLI | 14个命令 (help/btn/flash-id/info/iwdg/led/light/oled/ota-*/reset/runtime/temp/temp-stream) | ✅ |
| OLED | SSD1306 128×64, OOP vtable, 8x16+6x8+中文 | ✅ |
| DHT11 | 温湿度, DWT µs 精确定时 | ✅ |
| Light | M7 光敏 (ADC+GPIO), 常驻 | ✅ |
| SPI Flash | W25Qxx JEDEC 自动识别 | ✅ |
| Button | PB14 上拉输入 | ✅ |
| OTA | HMAC-SHA256签名, 外部Flash缓冲, 60s超时, ESC取消 | ✅ |
| Bootloader | 4.5KB, 5级恢复, OLED报错 | ✅ |
| IWDG | 软件定时器喂狗, ~10-26s超时 | ✅ |
| 外设测试 | 7个独立测试固件 + CLI综合 | ✅ |
| 系统时钟 | 72MHz PLL, APB1=/2修复 | ✅ |
| Bare CLI | 裸机版CLI (UART调试中) | WIP |
| BMP280 | I2C温压传感器 (缺硬件) | 代码已备 |
| LittleFS | SPI Flash文件系统 | 待做 |
| 低功耗 | Sleep/Stop/Standby | 待做 |

## 3. Bug 记录 & 预防

### 3.1 CRITICAL: `#if` guard 被 sed 全局替换破坏
- **现象**: DHT11 OLED不显示, temp命令返回"Sensor error"
- **根因**: sed `s/SENSOR_DS18B20/SENSOR_DHT11/` 替换了 `#if` 条件
- **预防**: 永远不要对 `#if`/`#ifdef` guard 做全局替换。用精确行匹配或手动修改。

### 3.2 CRITICAL: APB1 预分频器从未配置
- **现象**: I2C/SPI2/USART2-3 时序不稳定, OLED 偶尔花屏
- **根因**: `rcc_set_sysclk` 不配置 PPRE1, APB1跑72MHz (超限2倍)
- **修复**: `cfgr |= (0x4 << 8)` → APB1 = 36MHz
- **预防**: 审查 `rcc.c` 的第3步配置了 AHB/APB prescaler

### 3.3 HIGH: `abs()` crash on newlib-nano
- **现象**: `cli_printf` 中 `abs()` 导致 HardFault 复位
- **根因**: newlib-nano 的 `abs()` 在某些路径下异常
- **修复**: `if (x < 0) x = -x;`
- **预防**: 禁止使用 `abs()`, `%f`, `%lf`

### 3.4 HIGH: `%f` 浮点格式化 crash
- **现象**: `cli_printf` 格式化 crash
- **修复**: 全部用 `%d.%d` 整数拆分
- **预防**: 铁律: newlib-nano 禁止浮点格式化

### 3.5 HIGH: GPIO 输入模式错误
- **现象**: PB14 按键读不到
- **根因**: `GPIO_CNF_PP | GPIO_MODE_IN` = 0x00 = 模拟模式
- **修复**: `0x8 | GPIO_MODE_IN` = 上拉输入
- **预防**: 铁律: 输入用 `0x8|MODE_IN`, 不是 `GPIO_CNF_PP`

### 3.6 HIGH: busy-wait 延时不可靠
- **现象**: LED 闪烁/OTA 超时/时序计算全部偏差
- **根因**: volatile loop ≈7 cycles/iter, 不是1 cycle
- **计算**: `for(volatile int i=0;i<N;i++)__asm__("nop")` → 实际耗时为 N×7/72M 秒
- **修复**: 全部改用 SysTick/DWT/FreeRTOS tick
- **预防**: 铁律: 禁止 busy-wait 定时

### 3.7 MEDIUM: 传感器任务跨任务调 cli_printf
- **现象**: `temp-stream` 导致复位
- **根因**: 传感器任务(优先级1)调用 `cli_printf`, 与CLI任务(优先级2)冲突
- **修复**: 传感器直接 raw UART 输出, 或走队列
- **预防**: 只在单一任务上下文调用 `cli_printf`

### 3.8 MEDIUM: IWDG 喂狗不可靠
- **现象**: 系统不定期复位
- **根因**: `vApplicationIdleHook` 是 weak 函数, idle 任务可能永远不运行
- **修复**: FreeRTOS 软件定时器(优先级最高)强制喂狗
- **预防**: IWDG 喂狗必须用内核级机制(定时器), 不依赖 idle/任务

### 3.9 MEDIUM: PA10 浮空导致 UART 噪声
- **现象**: bootloader 卡死在 UART 接收循环
- **根因**: PA10 浮空输入, 噪声触发 RXNE → 超时永久重置
- **修复**: `0x8 | GPIO_MODE_IN` 上拉输入
- **预防**: 所有 UART RX 必须配上拉

## 4. 开发铁律

1. **禁止 `%f`** — 用 `%d.%d` 整数拆分
2. **禁止 `abs()`** — 用 `if(x<0)x=-x`
3. **禁止 `GPIO_CNF_PP|MODE_IN`** — 输入用 `0x8|MODE_IN`
4. **禁止 busy-wait 定时** — 用 SysTick/DWT/FreeRTOS tick
5. **禁止传感器任务调 `cli_printf`** — 走队列或 raw UART
6. **禁止 `#if` guard 上 sed 全局替换**
7. **新功能 → 新分支 → 测试通过 → merge main → push**

## 5. 开发流程计划

```
Phase 0: 研究 ─── GitHub搜索 + 库文档 + 硬件手册
Phase 1: 设计 ─── OOP接口 + 内存预算 + IO分配
Phase 2: 驱动 ─── TDD: host测试(Mock) → 实现 → 目标验证
Phase 3: 应用 ─── OOP模式(State/Strategy/Observer) + host测试
Phase 4: 编译 ─── 全目标交叉编译 + 静态分析
Phase 5: HIL  ─── 目标硬件测试 + 外设测试固件验证
Phase 6: 安全 ─── 代码审查(agent) + 铁律检查
Phase 7: 交付 ─── 提交 → 分支merge → 推送远程
```

### 每个 Phase 的检查点

| Phase | 检查 |
|-------|------|
| 0 研究 | 硬件文档已读, SDK版本已定, 冲突引脚已确认 |
| 1 设计 | OOP接口定义, 内存预算 < 80%, 无IO冲突 |
| 2 驱动 | host测试 100% pass, 覆盖率 ≥ 80% |
| 3 应用 | code-reviewer agent 无 CRITICAL/HIGH |
| 4 编译 | -Wall -Wextra 零警告, 所有目标编译通过 |
| 5 HIL  | 外设测试 .bin 通过, 保存到 tests/peripheral/bin/ |
| 6 安全 | 铁律检查通过, 无硬编码密钥, 无 busy-wait |
| 7 交付 | 分支 merge, 推送到 main, 更新 CLAUDE.md |

### 后续开发方向

| 优先级 | 任务 | 工作量 | 说明 |
|:--:|------|:--:|------|
| 1 | **Bare CLI** | 中 | UART调试 + 全功能迁移 |
| 2 | **LittleFS** | 中 | SPI Flash 文件系统 |
| 3 | **低功耗** | 中 | Sleep/Stop/Standby + 唤醒 |
| 4 | **中文字库** | 低 | CLI 中文提示 |
| 5 | **OTA 生产密钥** | 低 | 替换 dev key, 密钥管理工具 |
| 6 | **BMP280** | 低 | 需要硬件, 代码已备 |
| 7 | **DS18B20/DHT11** | 低 | 外设测试修复, 时序验证 |
