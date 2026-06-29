/**
 * @file    state_machine.c
 * @brief   状态机引擎实现
 *
 * ── 转移执行流程 / Transition Execution Flow ─────────────
 *
 *   sm_dispatch(fsm, event):
 *     1. 在当前状态的转移表中查找匹配事件
 *     2. 如果找到守卫函数，调用它 → 不通过则跳过
 *     3. 调用转移动作 (如果存在)
 *     4. 调用转移钩子 (用于日志)
 *     5. 调用旧状态的 on_exit
 *     6. 切换 current 指针到新状态
 *     7. 调用新状态的 on_entry
 *     8. 返回 true
 *
 * ── 重入保护 / Re-entry Protection ──────────────────────
 *   transitioning 标志防止在 on_exit/on_entry 中
 *   递归调用 sm_dispatch 导致的状态混乱。
 *   转移期间新事件会被忽略。
 *
 * ── 表终止标记 / Sentinel ───────────────────────────────
 *   转移表以 event=0 的条目作为终止标记。
 *   事件 0 是保留事件，不应用于正常转移。
 *
 * ── 层次状态机支持 ─────────────────────────────────────
 *   当前实现是扁平状态机。层次状态机 (嵌套状态)
 *   需要扩展 sm_dispatch 在未匹配时向上查找父状态的转移表。
 *   sm_is_in 已预留层次支持 — 沿 parent 链向上查找。
 */

#include "state_machine.h"

/**
 * @brief 退出一个状态
 *
 * 调用状态的 on_exit 回调。
 * 在层次状态机中，退出应沿 parent 链向上传播。
 */
static void exit_state(StateMachine *fsm, State *s)
{
    if (s->on_exit) {
        s->on_exit(fsm);
    }
}

/**
 * @brief 进入一个状态
 *
 * 调用状态的 on_entry 回调。
 * 在层次状态机中，进入应从最顶层 parent 开始向下传播。
 */
static void enter_state(StateMachine *fsm, State *s)
{
    if (s->on_entry) {
        s->on_entry(fsm);
    }
}

/**
 * @brief 初始化状态机
 *
 * 设置起始状态并调用其 on_entry。
 * ctx 初始为 NULL，调用者可在初始化后设置。
 *
 * 示例：
 *   StateMachine fsm;
 *   sm_init(&fsm, &state_idle);
 *   fsm.ctx = &my_app_context;  // 设置应用上下文
 */
void sm_init(StateMachine *fsm, State *start_state)
{
    fsm->current       = start_state;
    fsm->previous      = NULL;
    fsm->last_event    = 0;
    fsm->transitioning = false;
    fsm->ctx           = NULL;
    fsm->on_transition = NULL;

    /* 进入起始状态层次结构 */
    if (start_state->on_entry) {
        start_state->on_entry(fsm);
    }
}

/**
 * @brief 派发事件到当前状态
 *
 * 遍历当前状态的转移表，寻找匹配的事件。
 * 第一个匹配且守卫通过的转移被执行。
 * 转移表中事件顺序决定优先级 — 排在前面的优先匹配。
 *
 * 使用 trans_count 而非遍历到 sentinel (event=0)，
 * 这样可以在表中使用 event=0 作为内部标记而不被误判为终止。
 *
 * @return true 发生了转移，false 未匹配或守卫阻止或正在转移中
 */
bool sm_dispatch(StateMachine *fsm, uint32_t event)
{
    /* 转移中拒绝新事件 — 防止递归转移导致状态不一致 */
    if (fsm->transitioning || !fsm->current) {
        return false;
    }

    fsm->last_event = event;

    /* 在当前状态的转移表中查找匹配事件 */
    if (fsm->current->transitions) {
        for (size_t i = 0; i < fsm->current->trans_count; i++) {
            const Transition *t = &fsm->current->transitions[i];
            if (t->event == 0) {
                break; /* 到达表终止标记 */
            }
            if (t->event == event) {
                /* 守卫检查 */
                if (t->guard && !t->guard(fsm, event)) {
                    continue; /* 守卫阻止，尝试下一个 */
                }

                /* 执行转移动作 (在状态切换之前) */
                if (t->action) {
                    t->action(fsm);
                }

                /* 转移钩子 — 日志/调试 */
                if (fsm->on_transition) {
                    fsm->on_transition(fsm, fsm->current, t->target, event);
                }

                /* 退出现态 → 进入新态 (target=NULL = self-transition, 仅执行action) */
                if (t->target != NULL) {
                    State *from = fsm->current;
                    fsm->transitioning = true;      /* 设置重入保护 */
                    exit_state(fsm, from);
                    fsm->previous = from;           /* 记录上一个状态 */
                    fsm->current  = t->target;
                    enter_state(fsm, t->target);
                    fsm->transitioning = false;     /* 解除重入保护 */
                }

                return true;
            }
        }
    }

    return false; /* 无匹配转移 */
}

/**
 * @brief 每帧调用 — 执行当前状态的 on_run
 *
 * 应该在主循环的每次迭代中调用。
 * 如果当前状态没有 on_run，此函数几乎是空操作。
 *
 * 典型的主循环模式：
 *   while (1) {
 *       sm_tick(&fsm);           // 状态机心跳
 *       sm_dispatch(&fsm, poll_events()); // 处理事件
 *   }
 */
void sm_tick(StateMachine *fsm)
{
    if (fsm->current && fsm->current->on_run) {
        fsm->current->on_run(fsm);
    }
}

/**
 * @brief 强制跳转到目标状态
 *
 * 绕过转移表，直接跳转。用于：
 *   - 系统初始化后的强制状态设置
 *   - 异常/错误恢复
 *   - 外部信号触发的无条件跳转
 *
 * 仍会正确调用 on_exit/on_entry。
 */
void sm_goto(StateMachine *fsm, State *target)
{
    if (!target || fsm->transitioning) {
        return;
    }

    if (fsm->on_transition) {
        fsm->on_transition(fsm, fsm->current, target, 0);
    }

    fsm->transitioning = true;
    exit_state(fsm, fsm->current);
    fsm->previous = fsm->current;
    fsm->current  = target;
    enter_state(fsm, target);
    fsm->transitioning = false;
}

/** @brief 返回当前最内层活跃状态 */
State *sm_current(const StateMachine *fsm)
{
    return fsm->current;
}

/**
 * @brief 层次包含检查
 *
 * 沿 parent 链向上查找。如果当前状态是 s，或 s 是
 * 当前状态的某一级祖先，返回 true。
 *
 * 扁平状态机中 (parent 全为 NULL)，等价于 current == s。
 *
 * 示例：状态层次为 UI → Menu → Settings
 *   sm_is_in(fsm, &state_UI)       → true (祖先)
 *   sm_is_in(fsm, &state_Menu)     → true (祖先)
 *   sm_is_in(fsm, &state_Settings) → true (当前)
 *   sm_is_in(fsm, &state_Game)     → false (无关)
 */
bool sm_is_in(const StateMachine *fsm, const State *s)
{
    for (const State *cursor = fsm->current; cursor; cursor = cursor->parent) {
        if (cursor == s) {
            return true;
        }
    }
    return false;
}
