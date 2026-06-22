# CLI 命令处理流程

## 字符输入 → 命令执行

```
UART RX (PA10)
  │
  ▼
CLI Task (FreeRTOS, prio 1)
  │ uart_recv() 轮询
  ▼
cli_feed(&cli, byte)
  │
  ├─ '\\x1B' → 转义序列解析
  │   ├─ ESC[A  ↑ 历史向上
  │   ├─ ESC[B  ↓ 历史向下
  │   ├─ ESC[C  → 光标右移
  │   ├─ ESC[D  ← 光标左移
  │   └─ ESC[3~ Del 删除
  │
  ├─ '\\b' / 0x7F → 退格删除
  ├─ '\\t' → Tab 补全 (前缀匹配)
  ├─ '\\r' / '\\n' → 执行命令
  │   │
  │   ├─ Tokenize (按空格分割)
  │   ├─ cli_find_command(argv[0])
  │   │   └─ 遍历命令表, strcmp 匹配
  │   ├─ cmd->handler(cli, argc, argv)
  │   ├─ 保存到历史
  │   └─ cli_prompt()
  │
  └─ 可打印 → 插入到 line[cursor]
      └─ _redraw_line()  (发送 ANSI 刷新)
```

## 命令派发表

```
CLI.commands[] (NULL 终止)
  │
  ├─ "help"        → cmd_help()        cli_show_help()
  ├─ "temp"        → cmd_temp()        xQueueReceive(temp_queue)
  ├─ "temp-stream" → cmd_temp_stream() stream_mode = true
  ├─ "temp-stop"   → cmd_temp_stop()   stream_mode = false
  ├─ "led"         → cmd_led()         gpio_set/gpio_toggle
  ├─ "btn"         → cmd_btn()         gpio_get(PB14)
  ├─ "info"        → cmd_info()        系统信息
  ├─ "uptime"      → cmd_uptime()      xTaskGetTickCount
  ├─ "reset"       → cmd_reset()       SCB AIRCR 软复位
  ├─ "ota-start"   → cmd_ota_start()   ota_client_start() + poll
  ├─ "ota-status"  → cmd_ota_status()  ota_client_get_state()
  └─ "flash-id"    → cmd_flash_id()    spi_flash_get_info()
```

## ANSI 终端兼容

| 按键 | 序列 | 行为 |
|------|------|------|
| ↑ | ESC [ A | 浏览上一条历史 |
| ↓ | ESC [ B | 浏览下一条历史 |
| → | ESC [ C | 光标右移 |
| ← | ESC [ D | 光标左移 |
| Home | ESC [ H | 光标到行首 |
| End | ESC [ F | 光标到行尾 |
| Delete | ESC [ 3 ~ | 删除光标处字符 |
| Tab | \\t | 自动补全 |
