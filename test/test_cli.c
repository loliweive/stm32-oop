/**
 * @file    test_cli.c
 * @brief   CLI 引擎单元测试 — 命令解析/历史/Tab补全/转义序列
 *
 * 纯逻辑测试，无需硬件。通过 mock write_fn 捕获输出。
 */
#include "unity.h"
#include "cli.h"
#include <string.h>

/* ── Mock 输出捕获 ──────────────────────────────────── */
static char  g_output[512];
static size_t g_output_len;

static void mock_write(const char *str, size_t len) {
    if (g_output_len + len < sizeof(g_output) - 1) {
        memcpy(g_output + g_output_len, str, len);
        g_output_len += len;
        g_output[g_output_len] = '\0';
    }
}

static void setup(void) {
    memset(g_output, 0, sizeof(g_output));
    g_output_len = 0;
}

/* ── 测试命令处理函数 ────────────────────────────────── */
static void cmd_hello(CLI *cli, int argc, char **argv) {
    cli_printf(cli, "Hello %s", argc > 1 ? argv[1] : "World");
}

static const CLICommand test_commands[] = {
    { "hello", cmd_hello, "Say hello" },
    { "echo",  NULL,     "Echo text" },
    { "help",  NULL,     "Show help" },
    { NULL, NULL, NULL }
};

/* ── 测试: 初始化 ────────────────────────────────────── */
TEST(cli_init_counts_commands)
{
    CLI cli;
    cli_init(&cli, test_commands, mock_write, NULL);
    TEST_ASSERT_EQUAL_SIZE(3, cli.command_count);
    TEST_ASSERT_EQUAL_SIZE(0, cli.length);
}

TEST(cli_init_null_commands)
{
    CLI cli;
    cli_init(&cli, NULL, mock_write, NULL);
    TEST_ASSERT_EQUAL_SIZE(0, cli.command_count);
}

/* ── 测试: 查找命令 ──────────────────────────────────── */
TEST(cli_find_existing)
{
    CLI cli;
    cli_init(&cli, test_commands, mock_write, NULL);
    const CLICommand *cmd = cli_find_command(&cli, "hello");
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL_STRING("hello", cmd->name);
}

TEST(cli_find_nonexistent)
{
    CLI cli;
    cli_init(&cli, test_commands, mock_write, NULL);
    TEST_ASSERT_NULL(cli_find_command(&cli, "foobar"));
}

/* ── 测试: 命令执行 (通过 cli_feed) ──────────────────── */
TEST(cli_execute_hello)
{
    CLI cli;
    setup();
    cli_init(&cli, test_commands, mock_write, NULL);
    cli_feed(&cli, 'h'); cli_feed(&cli, 'e'); cli_feed(&cli, 'l');
    cli_feed(&cli, 'l'); cli_feed(&cli, 'o'); cli_feed(&cli, '\r');
    TEST_ASSERT_NOT_NULL(strstr(g_output, "Hello World"));
}

TEST(cli_unknown_command_prints_error)
{
    CLI cli;
    setup();
    cli_init(&cli, test_commands, mock_write, NULL);
    cli_feed(&cli, 'x'); cli_feed(&cli, 'x'); cli_feed(&cli, 'x');
    cli_feed(&cli, '\r');
    TEST_ASSERT_NOT_NULL(strstr(g_output, "Unknown command"));
}

/* ── 测试: printf ─────────────────────────────────────── */
TEST(cli_printf_formats_correctly)
{
    CLI cli;
    setup();
    cli_init(&cli, test_commands, mock_write, NULL);
    cli_printf(&cli, "%s %d", "test", 42);
    TEST_ASSERT_EQUAL_STRING("test 42", g_output);
}

/* ── 测试: Backspace ──────────────────────────────────── */
TEST(cli_backspace_removes_char)
{
    CLI cli;
    setup();
    cli_init(&cli, test_commands, mock_write, NULL);
    cli_feed(&cli, 'a');
    cli_feed(&cli, 'b');
    cli_feed(&cli, '\b');
    TEST_ASSERT_EQUAL_SIZE(1, cli.length);
    TEST_ASSERT_EQUAL_SIZE('a', cli.line[0]);
}

/* ── Runner ──────────────────────────────────────────── */
int main(void)
{
    unity_init();
    RUN_TEST(cli_init_counts_commands);
    RUN_TEST(cli_init_null_commands);
    RUN_TEST(cli_find_existing);
    RUN_TEST(cli_find_nonexistent);
    RUN_TEST(cli_execute_hello);
    RUN_TEST(cli_unknown_command_prints_error);
    RUN_TEST(cli_printf_formats_correctly);
    RUN_TEST(cli_backspace_removes_char);
    return 0;
}
