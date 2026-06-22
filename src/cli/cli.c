/**
 * @file    cli.c
 * @brief   CLI 引擎实现 — 行编辑、命令派发、历史
 */
#include "cli.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* ── ANSI 转义码 ───────────────────────────────────────────────── */
#define ANSI_ESC      "\x1B"
#define ANSI_CURSOR_L "\x1B[%zuD"    /* 光标左移 N */
#define ANSI_CLEAR_EOL "\x1B[0K"     /* 清除到行尾 */
#define ANSI_SAVE     "\x1B[s"       /* 保存光标 */
#define ANSI_RESTORE  "\x1B[u"       /* 恢复光标 */

/* ── 内部输出 ──────────────────────────────────────────────────── */
static void _write(const CLI *cli, const char *s)
{
    cli->write_fn(s, strlen(s));
}

static void _write_n(const CLI *cli, const char *s, size_t n)
{
    cli->write_fn(s, n);
}

/* ── 行编辑 ────────────────────────────────────────────────────── */

static void _line_insert(CLI *cli, char c)
{
    if (cli->length >= CLI_MAX_LINE_LENGTH) return;

    /* 在光标位置插入 */
    if (cli->cursor < cli->length) {
        memmove(cli->line + cli->cursor + 1,
                cli->line + cli->cursor,
                cli->length - cli->cursor);
    }
    cli->line[cli->cursor] = c;
    cli->cursor++;
    cli->length++;
    cli->line[cli->length] = '\0';
}

static void _line_delete_at_cursor(CLI *cli)
{
    if (cli->cursor < cli->length) {
        memmove(cli->line + cli->cursor,
                cli->line + cli->cursor + 1,
                cli->length - cli->cursor - 1);
        cli->length--;
        cli->line[cli->length] = '\0';
    }
}

static void _line_clear(CLI *cli)
{
    cli->length = 0;
    cli->cursor = 0;
    cli->line[0] = '\0';
}

/* ── 行刷新 (重绘) ──────────────────────────────────────────────── */
static void _redraw_line(const CLI *cli)
{
    _write(cli, "\r" CLI_PROMPT);
    _write(cli, cli->line);

    /* 光标定位 */
    if (cli->cursor < cli->length) {
        char buf[12];
        int n = snprintf(buf, sizeof(buf), "\r\x1B[%zuC", strlen(CLI_PROMPT) + cli->cursor);
        _write_n(cli, buf, (size_t)n);
    }
    _write(cli, ANSI_CLEAR_EOL);
}

#if CLI_HISTORY_DEPTH > 0
static void _history_push(CLI *cli, const char *line)
{
    if (line[0] == '\0') return;

    /* 去重: 如果与最新历史相同则跳过 */
    if (cli->history_count > 0 &&
        strcmp(cli->history[(cli->history_count - 1) % CLI_HISTORY_DEPTH], line) == 0) {
        return;
    }

    int idx = cli->history_count % CLI_HISTORY_DEPTH;
    strncpy(cli->history[idx], line, CLI_MAX_LINE_LENGTH);
    cli->history[idx][CLI_MAX_LINE_LENGTH] = '\0';
    cli->history_count++;
    cli->history_index = -1;
}

static void _history_navigate(CLI *cli, int direction)
{
    if (cli->history_count == 0) return;

    /* 首次进入历史浏览: 保存当前行 */
    if (cli->history_index == -1) {
        cli->history_index = cli->history_count;
    }

    cli->history_index += direction;

    if (cli->history_index < 0) {
        cli->history_index = 0;
        return;
    }

    int max_hist = (cli->history_count < CLI_HISTORY_DEPTH)
                   ? cli->history_count : CLI_HISTORY_DEPTH;

    if (cli->history_index >= cli->history_count) {
        /* 回到最新 → 清空行 */
        _line_clear(cli);
        cli->history_index = -1;
        _redraw_line(cli);
        return;
    }

    int oldest = (cli->history_count > CLI_HISTORY_DEPTH)
                 ? cli->history_count - CLI_HISTORY_DEPTH : 0;
    int idx = cli->history_index % CLI_HISTORY_DEPTH;

    if (cli->history_index < oldest) {
        cli->history_index = oldest;
    }

    strncpy(cli->line, cli->history[idx], CLI_MAX_LINE_LENGTH);
    cli->line[CLI_MAX_LINE_LENGTH] = '\0';
    cli->length = strlen(cli->line);
    cli->cursor = cli->length;
    _redraw_line(cli);
}
#endif

/* ── 命令解析与执行 ────────────────────────────────────────────── */

static void _execute_line(CLI *cli)
{
    _write(cli, "\r\n");

    if (cli->line[0] == '\0') {
        cli_prompt(cli);
        return;
    }

    /* Tokenize: 按空格分割参数 */
    int    argc = 0;
    char  *argv[CLI_MAX_ARGS];
    char  *token = cli->line;
    bool   in_token = false;

    /* 手动 tokenize (避免 strtok 修改原串) */
    for (size_t i = 0; i <= cli->length && argc < CLI_MAX_ARGS; i++) {
        if (cli->line[i] == ' ' || cli->line[i] == '\0') {
            if (in_token) {
                cli->line[i] = '\0';  /* 终止当前 token */
                argc++;
                in_token = false;
            }
        } else if (!in_token) {
            argv[argc] = &cli->line[i];
            in_token = true;
        }
    }

    if (argc == 0) {
        cli_prompt(cli);
        return;
    }

    /* 查找命令 */
    const CLICommand *cmd = cli_find_command(cli, argv[0]);
    if (cmd) {
        cmd->handler(cli, argc, argv);
    } else {
        cli_printf(cli, "Unknown command: '%s'. Type 'help'.\r\n", argv[0]);
    }

#if CLI_HISTORY_DEPTH > 0
    _history_push(cli, cli->line);
#endif

    _line_clear(cli);
    cli_prompt(cli);
}

/* ── 转义序列处理 ──────────────────────────────────────────────── */
static void _handle_escape(CLI *cli, char c)
{
    switch (cli->esc_state) {
    case 0:
        if (c == '\x1B') { cli->esc_state = 1; }
        break;
    case 1:
        cli->esc_state = (c == '[') ? 2 : 0;
        break;
    case 2:
        switch (c) {
        case 'A': /* 上箭头 */
#if CLI_HISTORY_DEPTH > 0
            _history_navigate(cli, -1);
#endif
            break;
        case 'B': /* 下箭头 */
#if CLI_HISTORY_DEPTH > 0
            _history_navigate(cli, 1);
#endif
            break;
        case 'C': /* 右箭头 */
            if (cli->cursor < cli->length) {
                cli->cursor++;
                _redraw_line(cli);
            }
            break;
        case 'D': /* 左箭头 */
            if (cli->cursor > 0) {
                cli->cursor--;
                _redraw_line(cli);
            }
            break;
        case 'H': /* Home */
            cli->cursor = 0;
            _redraw_line(cli);
            break;
        case 'F': /* End */
            cli->cursor = cli->length;
            _redraw_line(cli);
            break;
        case '3': /* Delete (序列: ESC [ 3 ~) */
            cli->esc_state = 3;
            return;
        }
        cli->esc_state = 0;
        break;
    case 3:
        /* ESC [ 3 ~ → Delete 键 */
        if (c == '~') {
            _line_delete_at_cursor(cli);
            _redraw_line(cli);
        }
        cli->esc_state = 0;
        break;
    }
}

/* ── 公开 API ──────────────────────────────────────────────────── */

void cli_init(CLI *cli, const CLICommand *commands,
              CLIWrite write_fn, void *write_ctx)
{
    memset(cli, 0, sizeof(*cli));
    cli->write_fn  = write_fn;
    cli->write_ctx = write_ctx;
    cli->commands  = commands;

    /* 数命令数 */
    if (commands) {
        while (commands[cli->command_count].name) {
            cli->command_count++;
        }
    }
}

void cli_feed(CLI *cli, char c)
{
    /* 转义序列处理 */
    if (cli->esc_state > 0 || c == '\x1B') {
        _handle_escape(cli, c);
        return;
    }

    switch (c) {
    case '\r':
    case '\n':
        _execute_line(cli);
        break;

    case '\b':
    case 0x7F: /* Backspace / DEL */
        if (cli->cursor > 0) {
            cli->cursor--;
            _line_delete_at_cursor(cli);
            _redraw_line(cli);
        }
        break;

    case '\t': /* Tab — 自动补全 */
        if (cli->length > 0) {
            /* 查找匹配前缀的命令 */
            const char *match = NULL;
            int match_count = 0;
            for (size_t i = 0; i < cli->command_count; i++) {
                if (strncmp(cli->commands[i].name, cli->line, cli->length) == 0) {
                    if (!match) match = cli->commands[i].name;
                    match_count++;
                }
            }
            if (match_count == 1 && match) {
                /* 唯一匹配 — 自动补全 + 空格 */
                strncpy(cli->line, match, CLI_MAX_LINE_LENGTH);
                cli->length = strlen(match);
                cli->cursor = cli->length;
                _line_insert(cli, ' ');
                _redraw_line(cli);
            } else if (match_count > 1) {
                /* 多个匹配 — 显示可选列表 */
                _write(cli, "\r\n");
                for (size_t i = 0; i < cli->command_count; i++) {
                    if (strncmp(cli->commands[i].name, cli->line, cli->length) == 0) {
                        _write(cli, "  ");
                        _write(cli, cli->commands[i].name);
                        _write(cli, "\r\n");
                    }
                }
                _redraw_line(cli);
            }
        }
        break;

    default:
        if (c >= 0x20 && c <= 0x7E) { /* 可打印字符 */
            _line_insert(cli, c);
            _redraw_line(cli);
        }
        break;
    }
}

void cli_register_command(CLI *cli, const CLICommand *cmd)
{
    /* 检查是否已存在 */
    for (size_t i = 0; i < cli->command_count; i++) {
        if (strcmp(cli->commands[i].name, cmd->name) == 0) {
            /* 无法直接修改 const 表，跳过 (调用者应使用可变命令表) */
            return;
        }
    }
    /* 此处假设命令表末尾有空位 */
    (void)cmd; /* 简化实现: 仅支持初始化时注册 */
}

const CLICommand *cli_find_command(const CLI *cli, const char *name)
{
    for (size_t i = 0; i < cli->command_count; i++) {
        if (strcmp(cli->commands[i].name, name) == 0) {
            return &cli->commands[i];
        }
    }
    return NULL;
}

void cli_printf(const CLI *cli, const char *fmt, ...)
{
    char buf[128];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (n > 0) {
        cli->write_fn(buf, (size_t)(n < (int)sizeof(buf) ? n : sizeof(buf) - 1));
    }
}

void cli_show_help(const CLI *cli)
{
    _write(cli, "\r\nCommands:\r\n");
    for (size_t i = 0; i < cli->command_count; i++) {
        cli_printf(cli, "  %-12s  %s\r\n",
                   cli->commands[i].name,
                   cli->commands[i].help);
    }
    _write(cli, "\r\n");
}

void cli_prompt(const CLI *cli)
{
    _write(cli, CLI_PROMPT);
}
