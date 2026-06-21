/**
 * Minimal logging utility.
 *
 * On target: outputs via a user-provided write function (UART by default).
 * On host: uses printf.
 *
 * Log levels: DEBUG, INFO, WARN, ERROR.
 * Compile-time filtering via LOG_LEVEL.
 */

#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_NONE
} LogLevel;

/** Set the output function (NULL = printf on host, UART on target). */
typedef void (*LogWriter)(const char *str, size_t len);
void log_set_writer(LogWriter writer);

/** Set minimum level to emit. Messages below this are dropped. */
void log_set_level(LogLevel level);

/** Core log function. Use LOG_* macros instead. */
void _log_emit(LogLevel level, const char *file, int line, const char *fmt, ...);

/* ── Convenience macros ─────────────────────────────────────── */
#define LOG_D(fmt, ...) _log_emit(LOG_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) _log_emit(LOG_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) _log_emit(LOG_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) _log_emit(LOG_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
