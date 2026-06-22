# FreeRTOS 任务架构

## 任务拓扑

```
┌──────────────────────────────────────────────────────────┐
│                    FreeRTOS Scheduler                     │
│                  configTICK_RATE_HZ = 1000                │
│                  configTOTAL_HEAP_SIZE = 6KB              │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  ┌─────────────────────┐    Queue     ┌────────────────┐ │
│  │  Sensor Task        │── TempRead ─→│  CLI Task      │ │
│  │  prio 2, 512B stack │  (depth=1)  │  prio 1, 2KB   │ │
│  │                     │             │                │ │
│  │  每 2s:             │             │  串口 RX/TX    │ │
│  │  · LED 亮           │             │  命令行交互    │ │
│  │  · sensor_read()    │             │  命令派发      │ │
│  │  · 刷新 OLED        │             │  OTA 流程      │ │
│  │  · xQueueOverwrite  │             │                │ │
│  │  · LED 灭           │             │                │ │
│  │  · vTaskDelay(2s)   │             │                │ │
│  └─────────────────────┘             └────────────────┘ │
│                                                          │
│  ┌─────────────────────┐                                │
│  │  IDLE Task           │ ← FreeRTOS 自动创建             │
│  │  prio 0 (最低)       │    栈溢出检测                  │
│  └─────────────────────┘                                │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

## 队列通信

```
Sensor Task                    CLI Task
    │                             │
    │ TempReading r;              │
    │ sensor_read(s, &r.temp_c,   │
    │             &r.humidity);   │
    │ r.timestamp = tick;         │
    │                             │
    │ xQueueOverwrite(queue, &r)  │
    │ ───────────────────────────→│
    │                             │ xQueueReceive(queue, &r, 0)
    │                             │ cmd_temp() 读取最新数据
    │                             │
```

## 任务栈监控

```c
// 运行时检查栈使用
UBaseType_t highwater;
highwater = uxTaskGetStackHighWaterMark(xTaskHandle);

// Sensor task: 128 words (512B) → 通常用 ~60%
// CLI task:    512 words (2KB)  → 通常用 ~40%
```

## 外设访问

| 外设 | 访问任务 | 保护方式 |
|------|---------|---------|
| UART (PA9/10) | CLI Task only | 独占 |
| OLED (I2C1) | Sensor Task only | 独占 |
| SPI Flash (SPI1) | CLI Task (on-demand) | 独占 (非 RTOS 上下文) |
| OneWire (PA1) | Sensor Task | 独占 + 关中断 |
| LED (PC13) | Sensor Task | 独占 |
| Button (PB14) | CLI Task (on-demand) | 只读 |
