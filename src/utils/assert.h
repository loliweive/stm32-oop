/**
 * @file    assert.h
 * @brief   轻量级断言系统 / Lightweight Assertion System
 *
 * 为嵌入式系统设计的可定制断言。
 *
 * ── 设计目标 ─────────────────────────────────────────────
 *
 *   1. 在开发阶段捕获逻辑错误 (通过断言)
 *   2. 在发布阶段零开销 (通过 NDEBUG 禁用)
 *   3. 可自定义失败行为：
 *      - Host: 打印并中止
 *      - Target: 闪烁 LED、通过 UART 输出、进入安全状态
 *   4. 不依赖 printf (嵌入式友好)
 *
 * ── 使用示例 ────────────────────────────────────────────
 *
 *   ASSERT(ptr != NULL);                         // 简单断言
 *   ASSERT_MSG(count < MAX, "count overflow");   // 带消息
 *
 *   // 自定义处理器 (目标上)
 *   void my_handler(const char *file, int line, const char *msg) {
 *       blink_led_fast();  // 视觉报警
 *       while(1);          // 停止 (或软复位)
 *   }
 *   assert_set_handler(my_handler);
 *
 * ── NDEBUG 的作用 ───────────────────────────────────────
 *
 *   当定义 NDEBUG (通常通过 -DNDEBUG 编译选项)：
 *     ASSERT(cond) → ((void)0)  (完全消除，零开销)
 *   用于发布构建。
 */

#ifndef ASSERT_H
#define ASSERT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 断言处理器类型 — 失败时调用 */
typedef void (*AssertHandler)(const char *file, int line, const char *msg);

/** @brief 设置自定义断言处理器 (NULL = 恢复默认) */
void assert_set_handler(AssertHandler handler);

/** @brief 底层断言失败函数 (由宏调用，不直接使用) */
void _assert_fail(const char *file, int line, const char *msg);

/**
 * @brief 断言宏
 *
 * 如果 condition 为 false，调用断言处理器。
 * 当 NDEBUG 定义时，这些宏展开为空。
 */
#ifdef NDEBUG
    #define ASSERT(condition)            ((void)0)
    #define ASSERT_MSG(condition, msg)   ((void)0)
#else
    #define ASSERT(condition) \
        do { if (!(condition)) _assert_fail(__FILE__, __LINE__, #condition); } while(0)
    #define ASSERT_MSG(condition, msg) \
        do { if (!(condition)) _assert_fail(__FILE__, __LINE__, msg); } while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* ASSERT_H */
