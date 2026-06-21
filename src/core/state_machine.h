/**
 * @file    state_machine.h
 * @brief   轻量级层次状态机引擎 / Lightweight Hierarchical State Machine
 *
 * 为嵌入式系统设计的低开销状态机。支持：
 *   · 扁平状态和层次 (嵌套) 状态
 *   · 进入 / 退出 动作 (on_entry / on_exit)
 *   · 事件驱动的状态转移，带守卫条件 (guard)
 *   · 零动态内存分配
 *
 * 状态机是一种设计模式，用于管理具有明确状态的系统 —
 * 按键去抖、通信协议解析、设备状态管理等。
 *
 * ── 为什么不用 switch-case？ ────────────────────────────
 *   switch-case 状态机在小规模 (<5 状态) 时够用，但：
 *   1. 状态和转移分散在 switch 中，难以独立测试
 *   2. 新增状态需要修改 switch，违反开闭原则
 *   3. 不能复用转移逻辑
 *   4. 不支持层次状态 (嵌套状态是 switch 的噩梦)
 *
 *   表驱动状态机把"状态有哪些"和"怎么转移"分开，
 *   每个状态是一个独立的数据结构，转移规则集中在一张表里。
 *
 * ── 使用示例 / Usage Example ────────────────────────────
 *
 *   // 1. 定义状态
 *   static State state_idle   = { .name="idle",   .on_entry=entry_idle };
 *   static State state_active = { .name="active", .on_exit=exit_active };
 *
 *   // 2. 定义转移表 (以 {event=0} 终止)
 *   static Transition idle_trans[] = {
 *       { .event=EV_START, .target=&state_active, .guard=is_ready },
 *       { .event=0 }  // sentinel
 *   };
 *
 *   // 3. 绑定转移表到状态
 *   // (在初始化时: state_idle.transitions = idle_trans;)
 *
 *   // 4. 运行
 *   StateMachine fsm;
 *   sm_init(&fsm, &state_idle);
 *   sm_dispatch(&fsm, EV_START);   // idle → active
 *   sm_tick(&fsm);                 // 每帧调用
 *
 * ── 内存开销 / Memory Overhead ──────────────────────────
 *   每个 State: ~40 bytes
 *   每个 Transition: ~16 bytes
 *   5 状态 + 15 转移 ≈ 440 bytes → 适合 STM32F103 的 20KB SRAM
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 前向声明 */
typedef struct StateMachine StateMachine;
typedef struct State State;

/**
 * @brief 转移守卫函数 / Guard Function
 *
 * 在状态转移发生前调用。返回 true 允许转移，false 阻止转移。
 * 典型用法：
 *   - 检查条件是否满足 (如"是否收到完整数据包")
 *   - 检查资源是否可用
 *
 * @param fsm   状态机实例
 * @param event 触发事件
 * @return true 允许转移 / false 阻止转移
 */
typedef bool (*GuardFn)(StateMachine *fsm, uint32_t event);

/**
 * @brief 状态动作函数 / Action Function
 *
 * 在进入状态、退出状态、或转移过程中执行。
 * 可以修改 fsm->ctx 中的用户上下文数据。
 */
typedef void (*ActionFn)(StateMachine *fsm);

/**
 * @brief 转移定义 / Transition Definition
 *
 * 描述从一个状态到另一个状态的转移规则。
 * 转移按表格顺序匹配，第一个匹配的转移被执行 (first-match wins)。
 *
 * @field event   触发事件 ID (0 = 表终止标记)
 * @field target  目标状态 (NULL = 自转移，不改变状态但执行 action)
 * @field guard   守卫函数 (NULL = 无条件通过)
 * @field action  转移动作 (NULL = 无动作)
 */
typedef struct {
    uint32_t  event;           /**< 触发事件 ID */
    State    *target;          /**< 目标状态指针 */
    GuardFn   guard;           /**< 守卫条件 (可选) */
    ActionFn  action;          /**< 转移动作 (可选) */
} Transition;

/**
 * @brief 状态定义 / State Definition
 *
 * 每个状态可以指定：
 *   - on_entry: 进入时调用一次
 *   - on_exit:  离开时调用一次
 *   - on_run:   停留期间每帧调用
 *   - transitions: 从该状态出发的转移表
 *   - parent:   父状态 (用于层次状态机，NULL = 顶层)
 *   - initial:  子状态的初始状态 (用于层次状态机)
 *
 * @field name         状态名 (调试用，建议有意义的名字)
 * @field on_entry     进入状态时调用
 * @field on_exit      离开状态时调用
 * @field on_run       停留在状态期间每 tick 调用
 * @field transitions  转移表 (NULL 终止数组)
 * @field trans_count  转移表条目数
 * @field parent       父状态 (层次状态机用)
 * @field initial      子状态初始态 (层次状态机用)
 */
struct State {
    const char   *name;           /**< 调试用状态名 */
    ActionFn      on_entry;       /**< 进入回调 */
    ActionFn      on_exit;        /**< 退出回调 */
    ActionFn      on_run;         /**< 停留回调 (每帧) */
    Transition    *transitions;   /**< 转移表指针 */
    size_t         trans_count;   /**< 转移表长度 */
    State         *parent;        /**< 父状态 (层次) */
    State         *initial;       /**< 子初始态 (层次) */
};

/**
 * @brief 状态机实例 / State Machine Instance
 *
 * 一个状态机实例持有当前状态和转移上下文。
 * 每个需要独立状态管理的对象应该有自己的实例。
 *
 * @field current       当前状态 (最内层活跃状态)
 * @field previous      上一个状态 (转移前)
 * @field last_event    最后触发的事件 ID
 * @field transitioning 是否正在转移中 (防止重入)
 * @field ctx           用户上下文 (任意指针，由应用定义)
 * @field on_transition 转移回调钩子 (调试/日志用)
 */
struct StateMachine {
    State       *current;        /**< 当前活跃状态 */
    State       *previous;       /**< 转移前状态 */
    uint32_t     last_event;     /**< 最后触发的事件 */
    bool         transitioning;  /**< 转移中标志 (防重入) */
    void        *ctx;            /**< 用户上下文指针 */

    /** @brief 转移钩子 — 每次转移时调用，用于日志/调试 */
    void (*on_transition)(StateMachine *fsm, State *from, State *to, uint32_t event);
};

/**
 * @brief 初始化状态机，进入起始状态
 *
 * 调用 start_state 的 on_entry (如果存在)。
 * 所有字段被初始化，ctx 初始为 NULL。
 *
 * @param fsm         状态机实例
 * @param start_state 起始状态
 */
void sm_init(StateMachine *fsm, State *start_state);

/**
 * @brief 向状态机派发事件
 *
 * 在当前状态的转移表中查找匹配事件。
 * 如果找到且守卫通过 → 执行转移 (exit → action → enter)
 * 如果找不到或守卫阻止 → 无操作
 *
 * @param fsm   状态机实例
 * @param event 事件 ID
 * @return true 发生了转移，false 未匹配或守卫阻止
 */
bool sm_dispatch(StateMachine *fsm, uint32_t event);

/**
 * @brief 执行当前状态的 on_run 动作
 *
 * 在主循环中周期性调用，驱动状态的持续行为。
 * 如果没有 on_run，此调用是空操作 (几乎零开销)。
 *
 * @param fsm 状态机实例
 */
void sm_tick(StateMachine *fsm);

/**
 * @brief 直接跳转到目标状态 (绕过转移表)
 *
 * 用于初始化阶段的强制切换，或外部条件触发的无条件跳转。
 * 仍会正确调用 on_exit / on_entry。
 *
 * @param fsm    状态机实例
 * @param target 目标状态
 */
void sm_goto(StateMachine *fsm, State *target);

/** @brief 获取当前最内层活跃状态 */
State *sm_current(const StateMachine *fsm);

/**
 * @brief 检查状态机当前是否在指定状态 (或其子状态) 中
 *
 * 用于层次状态机：如果当前状态是 s 的子状态，也会返回 true。
 * 扁平状态机中等价于 sm_current(fsm) == s。
 *
 * @param fsm 状态机实例
 * @param s   要检查的状态
 * @return true 当前在 s 或其子状态中
 */
bool sm_is_in(const StateMachine *fsm, const State *s);

#ifdef __cplusplus
}
#endif

#endif /* STATE_MACHINE_H */
