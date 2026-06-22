# OTA 固件更新流程

## 整体架构

```
┌──────────────┐         UART          ┌─────────────────────┐
│  Host PC     │◄─────────────────────►│  STM32F103           │
│  ota_sender  │   115200-8N1          │                     │
│  .py         │   协议帧              │  Bootloader (8KB)   │
└──────────────┘                       │  or CLI Task (App)   │
                                       └─────────────────────┘
```

## 正常启动流程

```
上电 / 复位
    │
    ▼
┌─────────────┐
│ Bootloader   │ 0x08000000 (8KB)
│ main()      │
└──────┬──────┘
       │
       ├─ 读 Metadata @ 0x0800F800
       │
       ├─ magic==OTA && state==VERIFIED?
       │   ├─ YES → jump_to_application(0x08002000)
       │   │         │
       │   │         ▼
       │   │    ┌──────────────┐
       │   │    │ Application  │ 0x08002000 (54KB)
       │   │    │ CLI + Sensor │
       │   │    └──────────────┘
       │   │
       │   └─ NO → 等待 OTA (3s 超时)
       │
       └─ 3s 内收到 OTA_HELLO?
           ├─ YES → 进入 OTA 接收
           │         │
           │         ├─ Erase App area
           │         ├─ Receive DATA frames
           │         ├─ CRC verify
           │         ├─ Write metadata (state=VERIFIED)
           │         └─ Soft reset → back to Bootloader
           │
           └─ NO → 检查旧固件
               ├─ 有 → jump_to_application()
               └─ 无 → 等待 OTA forever
```

## OTA 协议帧流

```
Host (PC)                        STM32 (Bootloader)
    │                                    │
    │── HELLO(ver, max_payload, size) ──►│  握手
    │◄────── HELLO_ACK(max_size) ──────│
    │                                    │
    │── DATA[0..255] seq=0 ────────────►│  × N 帧
    │◄────── DATA_ACK seq=0 ───────────│
    │── DATA[256..511] seq=1 ──────────►│
    │◄────── DATA_ACK seq=1 ───────────│
    │         ... (重复 N 次) ...        │
    │                                    │
    │── VERIFY(total_size, crc32) ─────►│  校验
    │◄────── VERIFY_ACK ───────────────│
    │                                    │
    │── BOOT ──────────────────────────►│  启动
    │◄────── BOOT_ACK ─────────────────│
    │                                    │── 软复位 ──► 新固件
```

## Flash 分区

```
0x08000000 ┌──────────────┐
           │  Bootloader  │  8KB  (~5.4KB used)
0x08002000 ├──────────────┤
           │              │
           │ Application  │  54KB (CLI + FreeRTOS + 传感器 ~26KB)
           │              │
0x0800F800 ├──────────────┤
           │  Metadata    │  2KB  (magic + size + crc32 + state)
0x08010000 └──────────────┘
```

## CLI 内 OTA 命令

```
> ota-start
Starting OTA firmware update...
CLI suspended — send firmware now.

[上位机运行: python3 tools/ota_sender.py firmware.bin --port /dev/tty.usbserial-*]

OTA complete! Booting new firmware...
──── 软复位 ──→ Bootloader → 新 App
```

## 协议帧格式

```
┌──────┬────────┬──────┬──────┬───────────┬────────┐
│ STX  │  LEN   │ TYPE │ SEQ  │  PAYLOAD  │ CRC16  │
│ 0xAA │ uint16 │ u8   │ u8   │  0..256B  │ uint16 │
└──────┴────────┴──────┴──────┴───────────┴────────┘
```

## 命令类型

| 命令 | 值 | 方向 | 说明 |
|------|:--:|:----:|------|
| HELLO | 0x01 | PC→MCU | 握手, 携带版本+容量 |
| HELLO_ACK | 0x02 | MCU→PC | 应答, 最大固件大小 |
| DATA | 0x03 | PC→MCU | 固件数据块 (≤256B) |
| DATA_ACK | 0x04 | MCU→PC | 确认 |
| VERIFY | 0x05 | PC→MCU | CRC32 校验请求 |
| VERIFY_ACK | 0x06 | MCU→PC | 校验通过 |
| BOOT | 0x07 | PC→MCU | 启动新固件 |
| ERROR | 0x0E | MCU→PC | 错误报告 |
