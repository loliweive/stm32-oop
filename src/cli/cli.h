/**
 * @file    cli.h
 * @brief   嵌入式命令行接口 / Embedded CLI Engine
 *
 * 可扩展的命令行框架，通过串口交互。
 *
 * 特性:
 *   · 命令注册/注销 (运行时扩展)
 *   · 行编辑 (Backspace 支持)
 *   · 命令历史 (可配置深度)
 *   · Tab 补全 (通过最长公共前缀匹配)
 *   · 非阻塞 — 适合裸机和 FreeRTOS
 *   · 零动态分配
 *
 * ── 使用示例 ─────────────────────────────────────────────
 *
 *   // 1. 定义命令
 *   static void cmd_hello(CLI *cli, int argc, char **argv) {
 *       cli_printf(cli, "Hello, %s!\r\n", argc>1 ? argv[1] : "world");
 *   }
 *
 *   // 2. 创建 CLI (静态分配)
 *   static CLI cli;
 *   static CLICommand cmds[] = {
 *       { "hello", cmd_hello, "Say hello" },
 *       { NULL }
 *   };
 *
 *   // 3. 初始化
 *   cli_init(&cli, cmds, uart_write_fn);
 *
 *   // 4. 在主循环中喂字符
 *   uint8_t byte;
 *   while (uart_recv(&uart, &byte)) {
 *       cli_feed(&cli, (char)byte);
 *   }
 */

#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ──────────────────────────────────────────────────── */
typedef struct CLI CLI;

/* ── 输出函数类型 ──────────────────────────────────────────────── */
typedef void (*CLIWrite)(const char *str, size_t len);

/* ── 命令处理函数 ──────────────────────────────────────────────── */
typedef void (*CLIHandler)(CLI *cli, int argc, char **argv);

/* ── 命令定义 ──────────────────────────────────────────────────── */
typedef struct {
    const char *name;        /**< 命令名 (小写) */
    CLIHandler  handler;     /**< 处理函数 */
    const char *help;        /**< 帮助文本 (一行) */
} CLICommand;

/* ── 配置 ──────────────────────────────────────────────────────── */
#define CLI_MAX_LINE_LENGTH   80    /* 最大输入行长度 */
#define CLI_MAX_ARGS          8     /* 最大参数数量 */
#define CLI_HISTORY_DEPTH     4     /* 命令历史深度 (0=禁用) */
#define CLI_PROMPT            "> "  /* 提示符 */
#define CLI_ANSI              1     /* 0=哑终端模式 1=ANSI终端 */
#define CLI_LOCAL_ECHO        0     /* 1=终端本地回显 0=CLI回显 */

/* ── CLI 实例 ──────────────────────────────────────────────────── */
struct CLI {
    /* 输出 */
    CLIWrite    write_fn;           /**< 输出函数 */
    void       *write_ctx;          /**< 输出上下文 (如 UartPort*) */

    /* 命令表 */
    const CLICommand *commands;     /**< 注册的命令表 (NULL 终止) */
    size_t      command_count;      /**< 命令数量 */

    /* 行编辑缓冲 */
    char        line[CLI_MAX_LINE_LENGTH + 1];
    size_t      cursor;             /**< 当前光标位置 */
    size_t      length;             /**< 当前行长度 */

    /* 历史 */
#if CLI_HISTORY_DEPTH > 0
    char        history[CLI_HISTORY_DEPTH][CLI_MAX_LINE_LENGTH + 1];
    int         history_count;
    int         history_index;      /**< 当前浏览的历史索引 (-1 = 当前行) */
#endif

    /* 转义序列解析 */
    uint8_t     esc_state;          /**< 0=正常, 1=收到ESC, 2=收到[ */
};

/* ── API ──────────────────────────────────────────────────────── */

/**
 * @brief 初始化 CLI
 * @param cli      CLI 实例 (调用者分配)
 * @param commands 命令表 (NULL 终止数组)
 * @param write_fn 输出函数
 * @param write_ctx 输出上下文 (传给 write_fn)
 */
void cli_init(CLI *cli, const CLICommand *commands,
              CLIWrite write_fn, void *write_ctx);

/**
 * @brief 喂入一个字符 (从串口接收)
 *
 * 处理行编辑、回车执行、Backspace 等。
 * 当用户按下回车时，自动解析并执行命令。
 *
 * @param cli CLI 实例
 * @param c   接收的字符
 */
void cli_feed(CLI *cli, char c);

/**
 * @brief 注册单个命令
 *
 * 如果命令名已存在则覆盖。
 * 命令表的最后一个条目必须 name=NULL。
 *
 * @param cli CLI 实例
 * @param cmd 命令定义
 */
void cli_register_command(CLI *cli, const CLICommand *cmd);

/**
 * @brief 查找命令
 * @return 找到的命令指针, 或 NULL
 */
const CLICommand *cli_find_command(const CLI *cli, const char *name);

/**
 * @brief 输出格式化文本 (printf 格式)
 *
 * 输出到 CLI 绑定的 write 函数。
 * 缓冲区限制 128 字节, 超出截断。
 */
void cli_printf(const CLI *cli, const char *fmt, ...);

/**
 * @brief 显示所有已注册命令的帮助
 */
void cli_show_help(const CLI *cli);

/**
 * @brief 打印提示符
 */
void cli_prompt(const CLI *cli);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H */
