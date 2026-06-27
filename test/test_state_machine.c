#include "unity.h"
#include "state_machine.h"
#include <stdio.h>

/* ── Test states and helpers ────────────────────────────────── */
static StateMachine fsm;
static int entry_count;
static int exit_count;
static int run_count;
static int action_count;

static void reset_counters(void) {
    entry_count = exit_count = run_count = action_count = 0;
}

static void on_entry(StateMachine *sm) { (void)sm; entry_count++; }
static void on_exit(StateMachine *sm)  { (void)sm; exit_count++; }
static void on_run(StateMachine *sm)   { (void)sm; run_count++; }
static void on_action(StateMachine *sm) { (void)sm; action_count++; }

static bool always_false(StateMachine *sm, uint32_t ev) {
    (void)sm; (void)ev; return false;
}

/* ── States ─────────────────────────────────────────────────── */
static Transition idle_trans[] = {
    { .event = 1, .target = NULL, .guard = NULL, .action = on_action },
    { .event = 2, .target = NULL, .guard = NULL, .action = on_action },
    { 0, NULL, NULL, NULL }
};

static State state_idle = {
    .name = "idle", .on_entry = on_entry, .on_exit = on_exit,
    .on_run = on_run, .transitions = idle_trans
};

static State state_active = {
    .name = "active", .on_entry = on_entry, .on_exit = on_exit,
    .on_run = on_run, .transitions = NULL
};

/* ── Tests ──────────────────────────────────────────────────── */
TEST(sm_init_enters_start_state)
{
    reset_counters();
    idle_trans[0].target = &state_active;
    idle_trans[0].guard  = NULL;
    idle_trans[0].event  = 1;
    idle_trans[0].action = NULL;
    idle_trans[1].event  = 0;  /* terminate */
    state_idle.trans_count = 1;
    state_active.transitions = NULL;
    state_active.trans_count = 0;

    sm_init(&fsm, &state_idle);
    TEST_ASSERT_EQUAL_U8(1, entry_count); /* on_entry called */
    TEST_ASSERT_EQUAL_PTR(&state_idle, sm_current(&fsm));
}

TEST(sm_dispatch_no_match)
{
    reset_counters();
    idle_trans[0].event = 1; idle_trans[0].target = &state_active;
    idle_trans[0].guard = NULL; idle_trans[0].action = NULL;
    idle_trans[1].event = 0;
    state_idle.trans_count = 1;

    sm_init(&fsm, &state_idle);
    reset_counters();

    /* Event 99 has no match → no transition */
    bool moved = sm_dispatch(&fsm, 99);
    TEST_ASSERT_FALSE(moved);
    TEST_ASSERT_EQUAL_PTR(&state_idle, sm_current(&fsm));
    TEST_ASSERT_EQUAL_U8(0, entry_count);
    TEST_ASSERT_EQUAL_U8(0, exit_count);
}

TEST(sm_dispatch_transitions)
{
    reset_counters();
    idle_trans[0].event = 1; idle_trans[0].target = &state_active;
    idle_trans[0].guard = NULL; idle_trans[0].action = NULL;
    idle_trans[1].event = 0;
    state_idle.trans_count = 1;
    state_active.transitions = NULL;
    state_active.trans_count = 0;

    sm_init(&fsm, &state_idle);
    reset_counters();

    bool moved = sm_dispatch(&fsm, 1);
    TEST_ASSERT_TRUE(moved);
    TEST_ASSERT_EQUAL_PTR(&state_active, sm_current(&fsm));
    TEST_ASSERT_EQUAL_U8(1, entry_count); /* active on_entry */
    TEST_ASSERT_EQUAL_U8(1, exit_count);  /* idle on_exit */
}

TEST(sm_guard_blocks)
{
    reset_counters();
    idle_trans[0].event = 1; idle_trans[0].target = &state_active;
    idle_trans[0].guard = always_false; idle_trans[0].action = NULL;
    idle_trans[1].event = 0;
    state_idle.trans_count = 1;

    sm_init(&fsm, &state_idle);
    reset_counters();

    bool moved = sm_dispatch(&fsm, 1);
    TEST_ASSERT_FALSE(moved);
    TEST_ASSERT_EQUAL_PTR(&state_idle, sm_current(&fsm));
}

TEST(sm_tick_runs_current_state)
{
    reset_counters();
    idle_trans[0].event = 0;
    state_idle.trans_count = 0;
    state_idle.on_run = on_run;

    sm_init(&fsm, &state_idle);
    reset_counters();

    sm_tick(&fsm);
    TEST_ASSERT_EQUAL_U8(1, run_count);

    sm_tick(&fsm);
    TEST_ASSERT_EQUAL_U8(2, run_count);
}

TEST(sm_goto_direct)
{
    reset_counters();
    idle_trans[0].event = 0;
    state_idle.trans_count = 0;
    state_active.transitions = NULL;
    state_active.trans_count = 0;

    sm_init(&fsm, &state_idle);
    reset_counters();

    sm_goto(&fsm, &state_active);
    TEST_ASSERT_EQUAL_PTR(&state_active, sm_current(&fsm));
    TEST_ASSERT_EQUAL_U8(1, exit_count);  /* exited idle */
    TEST_ASSERT_EQUAL_U8(1, entry_count); /* entered active */
}

TEST(sm_is_in_hierarchy)
{
    /* Set up a simple parent/child relationship */
    static State parent = {
        .name = "parent", .on_entry = NULL, .on_exit = NULL,
        .on_run = NULL, .transitions = NULL, .parent = NULL
    };
    static State child = {
        .name = "child", .on_entry = NULL, .on_exit = NULL,
        .on_run = NULL, .transitions = NULL, .parent = &parent
    };

    reset_counters();
    sm_init(&fsm, &child);

    TEST_ASSERT_TRUE(sm_is_in(&fsm, &child));
    TEST_ASSERT_TRUE(sm_is_in(&fsm, &parent));
}

/* ── Runner ─────────────────────────────────────────────────── */
int main(void)
{
    unity_init();
    RUN_TEST(sm_init_enters_start_state);
    RUN_TEST(sm_dispatch_no_match);
    RUN_TEST(sm_dispatch_transitions);
    RUN_TEST(sm_guard_blocks);
    RUN_TEST(sm_tick_runs_current_state);
    RUN_TEST(sm_goto_direct);
    RUN_TEST(sm_is_in_hierarchy);
    unity_summary();
    return Unity.failed ? 1 : 0;
}
