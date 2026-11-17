/*
 * "BossStart" task: clears enemies and game flow, loads the boss overlay, runs its entry and lives
 * until the returned boss task ends. Also the boss-select debug variables.
 */

#include "task.h"
#include "types_game.h"
#include "types_gw.h"
#include "gw_varedit.h"
#include "boss_start.h"
#include "file.h"
#include "game_flow_editor.h"
#include "game_flow_enemy.h"
#include "module.h"
#include "stage.h"

typedef struct BossStartWork { int child; int state; Pair arg; } BossStartWork;

typedef struct BossStartWorkView {
    TaskDesc *child;
    int state;
    int idx;
    int arg;
} BossStartWorkView;

extern void *boss_start_work;
extern char boss_start_overlays[];
extern int boss_overlay_base;
extern struct TaskDesc boss_start_desc;
extern int boss_kind;
extern int boss_start_arg;
extern char gw_varedit_root[];
extern int boss_vars_group;
extern GwVarDef boss_vardef[];
extern GwVarDef boss_kind_vardefs[];
extern GwVarDef boss_vardefs[];

static void boss_start_vars_register(void);
static void boss_start_vars_unregister(void);

/* 0x001035d0 */
void boss_start_stage_init(void)
{
    boss_start_vars_register();
}

/* 0x001035d8 */
void boss_start_stage_release(void)
{
    boss_start_vars_unregister();
}

/* 1 while the task exists and is not marked for removal. */
/* 0x001035e0 */
int boss_start_alive(void)
{
    void *w = boss_start_work;

    if (!w)
        return 0;
    return (TASK_OF(w)->flags & TASK_F_END) ^ 1;
}

/* prm: {boss index, argument}. */
/* steer(cast) */
/* 0x00103608 */
static void boss_start_task_init(BossStartWork *w, Pair *prm)
{
    w->arg = *prm;
    ((void (*)(BossStartWork *))flow_enemy_kill_all)(w);
    GameFlowClear();
    boss_start_work = w;
}

/* 0x00103650 */
static void boss_start_task_release(void)
{
    boss_start_work = 0;
}

/* Overlay from boss_start_overlays at boss_overlay_base. Ends with the entry's task (or none). */
/* 0x00103660 */
static void boss_start_task_exec(BossStartWorkView *w)
{
    char *ent = boss_start_overlays + w->idx * 16;
    struct {
        int a;
        int b;
    } args;

    switch (w->state) {
    case 0:
        file_request_simple(ent + 8, 0);
        w->state = 1;
        break;
    case 1:
        if (file_request_status(ent + 8)) {
            file_load_sync(ent + 8, &boss_overlay_base, 0);
            module_request_pairs(&boss_overlay_base);
            w->state = 2;
        }
        break;
    case 2:
        if (file_request_all_done()) {
            args.a = w->idx + 0x1010000;
            args.b = w->arg;
            w->child = (TaskDesc *)((int (*)(void *, void *))boss_overlay_base)(w, &args);
            w->state = 3;
        }
        break;
    case 3:
        if (w->child == 0 || (TASK_OF(w->child)->flags & TASK_F_END)) {
            do {
                TASK_OF(w)->flags |= TASK_F_END;
            } while (0);
        }
        break;
    }
}

/* 0x001037a8 */
static void boss_start_task_disp(void)
{
}

/* prm: {boss index, argument}. No-op while a BossStart task runs. */
/* 0x001037b0 */
void *boss_start_create(void *prm)
{
    if (boss_start_work)
        return 0;
    return task_create_arg(stage_get_lockon(), &boss_start_desc, 16, prm);
}

/* 0x00103808 */
void boss_start_kill(void)
{
    void *w = boss_start_work;

    if (w) {
        do {
            TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
    }
}

/* From boss_kind, boss_start_arg. */
/* 0x00103830 */
static void boss_start_create_from_globals(void)
{
    struct { int a; int b; } args;

    args.a = boss_kind;
    args.b = boss_start_arg;
    boss_start_create(&args);
}

/* 0x00103868 */
static void boss_start_vars_register(void)
{
    boss_vars_group = gw_varedit_add(gw_varedit_root, 0, boss_vardef);
    gw_varedit_add_table(gw_varedit_root, boss_vars_group, boss_kind_vardefs);
    gw_varedit_add_table(gw_varedit_root, boss_vars_group, boss_vardefs);
}

/* 0x001038d8 */
static void boss_start_vars_unregister(void)
{
    gw_varedit_remove(gw_varedit_root, 0, boss_vars_group);
}
