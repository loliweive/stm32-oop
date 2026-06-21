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
    if (level < min_level) return;

    static const char *level_str[] = {
        [LOG_DEBUG] = "D",
        [LOG_INFO]  = "I",
        [LOG_WARN]  = "W",
        [LOG_ERROR] = "E"
    };

    char buf[128];
    int n = snprintf(buf, sizeof(buf), "[%s] %s:%d ", level_str[level], file, line);

    va_list args;
    va_start(args, fmt);
    n += vsnprintf(buf + n, sizeof(buf) - n, fmt, args);
    va_end(args);

    if (writer_fn) {
        writer_fn(buf, (size_t)n);
    } else {
        printf("%s\n", buf);
    }
}
