#include "log.h"
#include <stdio.h>
#include <stdarg.h>

static LogLevel   min_level  = LOG_DEBUG;
static LogWriter  writer_fn  = NULL;

void log_set_writer(LogWriter writer)
{
    writer_fn = writer;
}

void log_set_level(LogLevel level)
{
    min_level = level;
}

void _log_emit(LogLevel level, const char *file, int line, const char *fmt, ...)
{
    /* 守卫: 无效的日志级别 */
    if (level > LOG_ERROR) return;

    if (level < min_level) return;

    static const char *level_str[] = {
        [LOG_DEBUG] = "D",
        [LOG_INFO]  = "I",
        [LOG_WARN]  = "W",
        [LOG_ERROR] = "E"
    };

    char buf[128];

    /* 写入前缀 "[L] file:line " — 如果截断, 留出末尾空间 */
    int written = snprintf(buf, sizeof(buf), "[%s] %s:%d ",
                           level_str[level], file, line);
    if (written < 0 || written >= (int)sizeof(buf)) {
        written = (int)sizeof(buf) - 1; /* 确保后续不会越界 */
    }

    /* 安全地追加格式化消息 */
    int remaining = (int)sizeof(buf) - written;
    if (remaining > 1) {
        va_list args;
        va_start(args, fmt);
        int n2 = vsnprintf(buf + written, (size_t)remaining, fmt, args);
        va_end(args);
        if (n2 > 0) {
            written += (n2 < remaining ? n2 : remaining - 1);
        }
    }

    if (writer_fn) {
        writer_fn(buf, (size_t)written);
    } else {
        printf("%.*s\n", written, buf);
    }
}
