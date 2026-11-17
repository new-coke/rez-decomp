/*
 * Player status: level, items (eight per level), Overdrive stock, chain, score, analysis and item
 * counts per stage and game, flags, damage. "PlayerStatusMan" applies them each frame. Also
 * score/item pop-ups and the status kept across a stage restart.
 */

#include "types_game.h"
#include "types_gw.h"
#include "task.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "gw_varedit.h"
#include "nj_system.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "color.h"

typedef struct PlayerLevelParam { int max_locks, overdrive_max, disp_level; } PlayerLevelParam;
typedef struct PlayerStatusDispScoreWork { char text[32]; float x; float y; int timer; } PlayerStatusDispScoreWork;
typedef struct PlayerStatusDispScoreWorkView { char text[36]; float y; int timer; } PlayerStatusDispScoreWorkView;

extern void player_status_apply_level(void);
extern char gw_varedit_root[];
extern GwVarDef player_status_vardef[];
extern GwVarDef player_status_vardefs[];
extern GwVarDef player_status_flag_vardef[];
extern GwVarDef player_status_flag_vardefs[];
extern struct GwVarNode *player_status_vars_group;
extern void listener_add(int, void (*)(void));
extern void player_status_level_listener(void);
extern int player_status_level;
extern PlayerLevelParam player_status_level_params[];
extern void lockon_set_max_locks(int);
extern void player_status_set_overdrive_max(int);
extern void console_set_disp_level(int);
extern int game_type_get(void);
extern int ConfigGetSelect(int);
extern int player_status_save[3];
extern int player_status_get_start_level(void);
extern PlayerStatus player_status;
extern int player_status_disp_flags;
extern int od_time;
extern void player_status_save_reset(void);
extern int stage_get_num(void);
extern int stage_get_analyze_total(int);
extern void player_status_taken(void);
extern void player_status_resume(void);
extern int player_status_get_level(void);
extern void SoundRhythmSe_StgCommon(int);
extern void player_status_next_level_down(void);
extern int player_status_chain_time;
extern int boss_start_alive(void);
extern unsigned int player_status_level_limits[];
extern void player_status_set_next_level(unsigned int);
extern float fmt_get_count;
extern float game_pass_count_spd_get(void);
extern void game_pass_count_spd_set(float);
extern int player_status_man_work;
extern void player_status_add_overdrive(int);
extern void player_set_draw(int);
extern struct TaskDesc player_status_man_desc;
extern unsigned int player_status_level_req;
extern int player_status_shot_cnt;
extern int player_status_hit_cnt;
extern int player_status_cap_cnt;
extern int player_status_cap_end_cnt;
extern int player_status_shot_frm;
extern int player_status_hit_frm;
extern int player_status_hit_dmg_frm;
extern int player_status_cap_frm;
extern int player_status_cap_end_frm;
extern int player_status_cap_dmg_frm;
extern int player_status_progress;
extern int player_status_od_stock;
extern int player_status_od_stock_max;
extern int player_status_best_chain;
extern int player_status_chain;
extern int player_status_chain_timer;
extern int player_status_chain_min;
extern int lockon_get_lock_count(void);
extern int player_status_score;
extern int player_status_score_boss;
extern int player_status_score_stage;
extern void listener_dispatch(int, void *);
extern int cs_g;
extern int cs_s;
extern int cs_get_g;
extern int cs_get_s;
extern int item_ap_g;
extern int item_ap_s;
extern int item_get_g;
extern int item_get_s;
extern void *ulMatrixCur;
extern int stage_get_work(void);
extern void draw_set_blend(int, int);
extern void font_set(int);
extern void font_print(float, float, float, unsigned int, void *);
extern int game_flags;
extern struct TaskDesc player_status_disp_score_desc;
extern int player_status_test_flags(int);
extern int player_status_add_score(int);
extern void console_cell_pos_right(int, float *, float *);
extern void console_cell_pos_left(int, float *, float *);
extern void player_status_add_items(int);
extern int system_ptm;
extern void tex_rect_draw_rot(int, int, int, float, float, float, float, float);
extern struct TaskDesc ps_sub_add_item_desc;
extern int player_status_get_overdrive(void);
extern int player_status_get_overdrive_max(void);
extern int player_status_is_max_level(void);
extern int player_status_get_progress(void);
extern void player_status_chain_step(PlayerStatusView *ps);
extern void player_status_clamp_overdrive(PlayerStatusView *ps);
extern void player_status_damage_step(PlayerStatusView *ps);
extern void player_status_items_step(PlayerStatusView *ps);
extern void player_status_score_step(PlayerStatusView *ps);
extern int player_status_set_flags(int bits);
extern void player_status_stats_step(PlayerStatusView *ps);
extern void player_status_update(int w, PlayerStatusView *ps);
extern void ps_sub_add_item_fade_exec(PsSubAddItemWork *w);
extern void ps_sub_add_item_init(PsSubAddItemWork *w, int mode, float x, float y, int idx, int delay);

/* Event 20: re-applies the level's limits. */
/* 0x001557c0 */
void player_status_level_listener(void)
{
    player_status_apply_level();
}

/* 0x001557c8 */
void player_status_vars_register(void)
{
    struct GwVarNode *grp;

    listener_add(20, player_status_level_listener);
    player_status_vars_group = gw_varedit_add(gw_varedit_root, 0, player_status_vardef);
    gw_varedit_add_table(gw_varedit_root, player_status_vars_group, player_status_vardefs);
    grp = gw_varedit_add(gw_varedit_root, player_status_vars_group, player_status_flag_vardef);
    gw_varedit_add_table(gw_varedit_root, grp, player_status_flag_vardefs);
}

/* Lock-on count, Overdrive stock limit, console display level. */
/* 0x00155858 */
void player_status_apply_level(void)
{
    PlayerLevelParam *param = &player_status_level_params[player_status_level];

    lockon_set_max_locks(param->max_locks);
    player_status_set_overdrive_max(param->overdrive_max);
    console_set_disp_level(param->disp_level);
}

/* Game type 5: the options' level, else 1. */
/* 0x001558a8 */
int player_status_get_start_level(void)
{
    if (game_type_get() == 5)
        return ConfigGetSelect(4);
    return 1;
}

/* Starting level, no items, no Overdrive. */
/* 0x001558e0 */
void player_status_save_reset(void)
{
    nj_fill8(player_status_save, 0, 12);
    player_status_save[0] = player_status_get_start_level();
    player_status_save[1] = 0;
    player_status_save[2] = 0;
}

/* Level, items, Overdrive stock (player_status_resume restores). */
/* 0x00155930 */
void player_status_taken(void)
{
    player_status_save[0] = player_status.level;
    player_status_save[1] = player_status.items;
    player_status_save[2] = player_status.overdrive;
    DEBUG_PRINT("PlayerStatus Taken\n");
}

/* 0x00155968 */
void player_status_resume(void)
{
    player_status.level = player_status_save[0];
    player_status.next_level = player_status_save[0];
    player_status.items = player_status_save[1];
    player_status.overdrive = player_status_save[2];
    DEBUG_PRINT("PlayerStatus Resume\n");
}

/* Flags from player_status_disp_flags, starting level, counts zero, Overdrive time 270 frames. */
/* 0x001559a0 */
void player_status_init_game(void)
{
    PlayerStatus *ps = &player_status;
    int level;

    DEBUG_PRINTF("PlayerStatusInitGame\n");
    nj_fill8(ps, 0, 164);
    ps->no_damage_cnt = 0;
    ps->flags = player_status_disp_flags;
    level = player_status_get_start_level();
    od_time = 270;
    ps->level = level;
    ps->next_level = level;
    ps->shot_cnt = 0;
    ps->hit_cnt = 0;
    ps->cap_cnt = 0;
    ps->cap_end_cnt = 0;
    ps->shot_add = 0;
    ps->hit_add = 0;
    ps->hit_dmg_add = 0;
    ps->cap_add = 0;
    ps->cap_end_add = 0;
    ps->cap_dmg_add = 0;
    ps->shot_frm = 0;
    ps->hit_frm = 0;
    ps->hit_dmg_frm = 0;
    ps->cap_frm = 0;
    ps->cap_end_frm = 0;
    ps->cap_dmg_frm = 0;
    ps->items_add = 0;
    ps->items = 0;
    ps->overdrive = 0;
    ps->overdrive_max = 0;
    ps->best_chain = 0;
    ps->chain = 0;
    ps->chain_timer = 0;
    ps->score = 0;
    ps->score_add = 0;
    ps->score_stage = 0;
    ps->score_boss = 0;
    ps->game_analyze_total = 0;
    ps->stage_analyze_total = 0;
    ps->game_analyzed = 0;
    ps->stage_analyzed = 0;
    ps->game_item_total = 0;
    ps->stage_item_total = 0;
    ps->game_items_got = 0;
    ps->stage_items_got = 0;
    player_status_apply_level();
    player_status_save_reset();
}

/* 0x00155aa0 */
void player_status_init_score(void)
{
    PlayerStatus *ps = &player_status;

    DEBUG_PRINTF("PlayerStatusInitScore\n");
    ps->score = 0;
    ps->score_add = 0;
    ps->score_stage = 0;
    ps->score_boss = 0;
    ps->game_analyze_total = 0;
    ps->stage_analyze_total = 0;
    ps->game_analyzed = 0;
    ps->stage_analyzed = 0;
    ps->game_item_total = 0;
    ps->stage_item_total = 0;
    ps->game_items_got = 0;
    ps->stage_items_got = 0;
}

/* Adds the stage's analysis targets to the game total; saves for a restart except in stage 5. */
/* 0x00155b00 */
void player_status_init_stage(void)
{
    PlayerStatus *ps = &player_status;
    int stage;
    int total;

    DEBUG_PRINTF("PlayerStatusInitStage\n");
    stage = stage_get_num();
    ps->score_stage = 0;
    ps->score_boss = 0;
    total = stage_get_analyze_total(stage);
    ps->stage_analyze_total = total;
    ps->game_analyze_total += total;
    ps->stage_analyzed = 0;
    ps->stage_item_total = 0;
    ps->stage_items_got = 0;
    player_status_apply_level();
    if (stage != 5)
        player_status_taken();
}

/* 0x00155b90 */
void player_status_logout(void)
{
    player_status_resume();
}

/* Fresh status: starting level, no items, no Overdrive. */
/* 0x00155b98 */
void player_status_restart(void)
{
    player_status_save_reset();
    player_status_resume();
}

/* Adds this frame's shots, hits, captures to the totals, keeps them as last frame's, clears. */
/* 0x00155bb8 */
void player_status_stats_step(PlayerStatusView *ps)
{
    ps->shot_cnt += ps->shot_add;
    ps->shot_frm = ps->shot_add;
    ps->hit_cnt += ps->hit_add;
    ps->hit_frm = ps->hit_add;
    ps->hit_dmg_frm = ps->hit_dmg_add;
    ps->cap_cnt += ps->cap_add;
    ps->cap_frm = ps->cap_add;
    ps->cap_end_cnt += ps->cap_end_add;
    ps->cap_end_frm = ps->cap_end_add;
    ps->cap_dmg_frm = ps->cap_dmg_add;
    ps->shot_add = 0;
    ps->hit_add = 0;
    ps->hit_dmg_add = 0;
    ps->cap_add = 0;
    ps->cap_end_add = 0;
    ps->cap_dmg_add = 0;
}

/* Ignored while flags 0x180000. Hit sound, flag 2, level - 1; at level 0 or 7 fatal (flag 1). */
/* 0x00155c38 */
void player_status_damage_step(PlayerStatusView *ps)
{
    int level;
    unsigned int flags;

    if (ps->flags & 0x180000)
        ps->damage = 0;
    if (fabsf(ps->damage) < FLT_EPSILON) {
        ps->damage = 0;
        return;
    }
    level = player_status_get_level();
    SoundRhythmSe_StgCommon(5);
    flags = ps->flags;
    ps->flags = flags | 2;
    if (level == 7 || level == 0)
        ps->flags = flags | 3;
    else
        player_status_next_level_down();
    ps->damage = 0;
}

/* 0x00155ce0 */
void player_status_clamp_overdrive(PlayerStatusView *ps)
{
    if (ps->max < ps->cur)
        ps->cur = ps->max;
}

/*
 * A chain hit extends the chain (and best), sets 0x40000, restarts player_status_chain_time; the
 * chain ends when the timer runs out.
 */
/* 0x00155d00 */
void player_status_chain_step(PlayerStatusView *ps)
{
    if (ps->chain_timer > 0)
        ps->chain_timer--;
    if (ps->chain_hit & 1) {
        ps->chain_hit &= ~1;
        ps->flags |= 0x40000;
        ps->chain++;
        ps->chain_timer = player_status_chain_time;
        if (ps->best_chain < ps->chain)
            ps->best_chain = ps->chain;
    }
    if (ps->chain_timer == 0) {
        ps->chain = 0;
        ps->flags &= ~0x40000;
    }
}

/* Also to the boss score while a boss is alive, else the stage score; flag 0x2000: scored. */
/* 0x00155d98 */
void player_status_score_step(PlayerStatusView *ps)
{
    ps->score += ps->score_add;
    if (boss_start_alive())
        ps->score_boss += ps->score_add;
    else
        ps->score_stage += ps->score_add;
    if (ps->score_add > 0)
        ps->flags |= 0x2000;
    else
        ps->flags &= ~0x2000;
    ps->score_add = 0;
}

/* Every 8 items raise the level, up to player_status_level_limits (then none kept); items 0..8. */
/* 0x00155e20 */
void player_status_items_step(PlayerStatusView *ps)
{
    unsigned int n;
    unsigned int lim;
    int items;
    int i;

    ps->items += ps->items_add;
    ps->items_add = 0;
    if (ps->items >= 8) {
        i = stage_get_num();
        n = player_status_get_level();
        lim = player_status_level_limits[i];
        if (n < lim) {
            n++;
            player_status_set_next_level(n);
            ps->items -= 8;
            if (n >= lim)
                ps->items = 0;
        } else {
            ps->items = 0;
        }
    }
    items = ps->items;
    if (items < 0)
        items = 0;
    ps->items = items < 9 ? items : 8;
}

/*
 * Flag 4: pass clock sped up by fmt_get_count. Clears per-frame flags (low 8 bits), runs the
 * steps, takes a requested level change, raising 0x100 until the player handles it.
 */
/* 0x00155ee0 */
void player_status_update(int w, PlayerStatusView *ps)
{
    if (ps->flags & 4)
        game_pass_count_spd_set(game_pass_count_spd_get() + fmt_get_count);
    ps->flags &= 0xffffff00;
    player_status_stats_step(ps);
    player_status_damage_step(ps);
    player_status_clamp_overdrive(ps);
    player_status_chain_step(ps);
    player_status_score_step(ps);
    player_status_items_step(ps);
    if (!(ps->flags & 0x100) && ps->level != ps->next_level) {
        ps->level = ps->next_level;
        ps->flags |= 0x100;
    }
}

/* 0x00155f98 */
void player_status_man_init(int w)
{
    player_status_man_work = w;
}

/* 0x00155fa8 */
void player_status_man_release(void)
{
    player_status_man_work = 0;
}

/*
 * Game type rules: no damage (0x100000) in type 2 or by type 5 option; type 5 also has Overdrive
 * refill each frame, and camera option 4 hides the avatar. Then the update.
 */
/* 0x00155fb8 */
void player_status_man_exec(int work)
{
    PlayerStatus *ps = &player_status;

    switch (game_type_get()) {
    case 2:
        player_status_set_flags(0x100000);
        player_set_draw(1);
        break;
    case 5:
        if (ConfigGetSelect(5))
            player_status_set_flags(0x100000);
        if (ConfigGetSelect(7))
            player_status_add_overdrive(1);
        if (ConfigGetSelect(8) == 4) {
            player_set_draw(0);
            break;
        }
    default:
        player_set_draw(1);
        break;
    }
    player_status_update(work, ps);
}

/* 0x00156088 */
void player_status_man_disp(void)
{
}

/* 0x00156090 */
void *player_status_man_create(void *parent)
{
    return task_create(parent, &player_status_man_desc, 4);
}

/* Returns the previous flags. */
/* 0x001560b8 */
int player_status_set_flags(int bits)
{
    int old = (*(int *)&player_status); /* steer(cast) */
    (*(int *)&player_status) = old | bits; /* steer(cast) */
    return old;
}

/* Returns the previous flags. */
/* 0x001560d0 */
int player_status_clear_flags(int bits)
{
    int old = (*(int *)&player_status); /* steer(cast) */
    (*(int *)&player_status) = old & ~bits; /* steer(cast) */
    return old;
}

/* 0x001560e8 */
int player_status_test_flags(int bits)
{
    return (*(int *)&player_status) & bits; /* steer(cast) */
}

/* 0x001560f8 */
int player_status_get_flags(void)
{
    return (*(int *)&player_status); /* steer(cast) */
}

/* Disp flags: the status flags a new game starts with. */
/* 0x00156108 */
int player_status_set_disp_flags(int bits)
{
    int old = player_status_disp_flags;
    player_status_disp_flags = old | bits;
    return old;
}

/* 0x00156120 */
int player_status_clear_disp_flags(int bits)
{
    int old = player_status_disp_flags;
    player_status_disp_flags = old & ~bits;
    return old;
}

/* 0x00156138 */
int player_status_test_disp_flags(int bits)
{
    return player_status_disp_flags & bits;
}

/* Flag 0x80000; periods nest. */
/* 0x00156148 */
void player_status_no_damage_start(void)
{
    player_status.flags |= 0x80000;
    player_status.no_damage_cnt += 1;
}

/* The flag clears when the last period ends. */
/* 0x00156170 */
void player_status_no_damage_tick(void)
{
    if (player_status.no_damage_cnt > 0) {
        player_status.no_damage_cnt--;
        if (player_status.no_damage_cnt > 0)
            return;
    }
    player_status.flags &= ~0x80000;
}

/* Highest level the current stage allows. */
/* 0x001561a8 */
int player_status_is_max_level(void)
{
    unsigned int i = player_status_level;
    unsigned int *lim = player_status_level_limits;

    return i >= lim[stage_get_num()];
}

/* At most 7. */
/* 0x001561f8 */
void player_status_set_next_level(unsigned int level)
{
    if (level > 7)
        level = 7;
    player_status_level_req = level;
}

/* 0x00156210 */
void player_status_set_level(unsigned int level)
{
    PlayerStatus *ps = &player_status;

    if (level > 7)
        level = 7;
    ps->level = level;
    ps->next_level = level;
}

/* 0x00156230 */
int player_status_get_level(void)
{
    return player_status_level;
}

/* At most 7. */
/* 0x00156240 */
void player_status_next_level_up(void)
{
    int level = player_status.next_level + 1;

    if (level >= 8)
        level = 7;
    player_status.next_level = level;
}

/* At least 0. */
/* 0x00156268 */
void player_status_next_level_down(void)
{
    int level = player_status.next_level - 1;

    if (level < 0)
        level = 0;
    player_status.next_level = level;
}

/* 0x00156290 */
int player_status_get_shot_cnt(void)
{
    return player_status_shot_cnt;
}

/* 0x001562a0 */
int player_status_get_hit_cnt(void)
{
    return player_status_hit_cnt;
}

/* 0x001562b0 */
int player_status_get_cap_cnt(void)
{
    return player_status_cap_cnt;
}

/* 0x001562c0 */
int player_status_get_cap_end_cnt(void)
{
    return player_status_cap_end_cnt;
}

/* 0x001562d0 */
int player_status_get_shot_frm(void)
{
    return player_status_shot_frm;
}

/* 0x001562e0 */
int player_status_get_hit_frm(void)
{
    return player_status_hit_frm;
}

/* 0x001562f0 */
int player_status_get_hit_dmg_frm(void)
{
    return player_status_hit_dmg_frm;
}

/* 0x00156300 */
int player_status_get_cap_frm(void)
{
    return player_status_cap_frm;
}

/* 0x00156310 */
int player_status_get_cap_end_frm(void)
{
    return player_status_cap_end_frm;
}

/* 0x00156320 */
int player_status_get_cap_dmg_frm(void)
{
    return player_status_cap_dmg_frm;
}

/* Counters below collect this frame's shots, hits, captures (player_status_stats_step). */
/* 0x00156330 */
void player_status_count_shot(int n)
{
    player_status.shot_add += n;
}

/* 0x00156348 */
void player_status_count_hit(int n, int dmg)
{
    player_status.hit_add += n;
    player_status.hit_dmg_add += dmg;
}

/* 0x00156370 */
void player_status_count_capture(int n)
{
    player_status.cap_add += n;
}

/* 0x00156388 */
void player_status_count_capture_end(int n, int dmg)
{
    player_status.cap_end_add += n;
    player_status.cap_dmg_add += dmg;
}

/* Applied by player_status_damage_step. Returns 0. */
/* 0x001563b0 */
float player_status_add_damage(float v)
{
    player_status.damage += v;
    return 0.0f;
}

/* 0x001563d0 */
int player_status_get_progress(void)
{
    return player_status_progress;
}

/* Not at the stage's highest level. */
/* 0x001563e0 */
void player_status_add_items(int n)
{
    PlayerStatus *ps = &player_status;
    unsigned int i = player_status_get_level();
    unsigned int *lim = player_status_level_limits;

    if (i < lim[stage_get_num()])
        ps->items_add += n;
}

/* 0x00156458 */
void player_status_clear_items(void)
{
    player_status.items = 0;
    player_status.items_add = 0;
}

/* If any. */
/* 0x00156470 */
void player_status_use_overdrive(void)
{
    if (player_status.overdrive > 0)
        player_status.overdrive--;
}

/* 0x00156490 */
int player_status_has_overdrive(void)
{
    return player_status_od_stock > 0;
}

/* Up to its limit. */
/* 0x001564a0 */
void player_status_add_overdrive(int n)
{
    int total = player_status.overdrive + n;

    player_status.overdrive = total < player_status.overdrive_max ? total : player_status.overdrive_max;
}

/* 0x001564c8 */
void player_status_set_overdrive(int stock)
{
    player_status_od_stock = stock;
}

/* 0x001564d8 */
int player_status_get_overdrive(void)
{
    return player_status_od_stock;
}

/* 0x001564e8 */
void player_status_set_overdrive_max(int stock_max)
{
    player_status_od_stock_max = stock_max;
}

/* 0x001564f8 */
int player_status_get_overdrive_max(void)
{
    return player_status_od_stock_max;
}

/* 0x00156508 */
int player_status_get_od_time(void)
{
    return od_time;
}

/* 0x00156518 */
int player_status_get_best_chain(void)
{
    return player_status_best_chain;
}

/* 0x00156528 */
int player_status_get_chain(void)
{
    return player_status_chain;
}

/* 0x00156538 */
int player_status_get_chain_timer(void)
{
    return player_status_chain_timer;
}

/* Needs player_status_chain_min targets locked on; returns whether counted. */
/* 0x00156548 */
int player_status_chain_check(void)
{
    if (lockon_get_lock_count() < player_status_chain_min)
        return 0;
    player_status.chain_hit |= 1;
    return 1;
}

/* 0x00156590 */
int player_status_get_score(void)
{
    return player_status_score;
}

/* 0x001565a0 */
int player_status_get_boss_score(void)
{
    return player_status_score_boss;
}

/* 0x001565b0 */
int player_status_get_stage_score(void)
{
    return player_status_score_stage;
}

/* Returns the score including points not yet applied. */
/* 0x001565c0 */
int player_status_add_score(int n)
{
    player_status.score_add += n;
    return player_status.score + player_status.score_add;
}

/* Sends event 23 with (id, score). */
/* 0x001565e0 */
void player_status_add_score_event(int id, int score)
{
    int msg[2];

    msg[0] = id;
    msg[1] = score;
    player_status.score_add += score;
    listener_dispatch(23, msg);
}

/* 0x00156620 */
int player_status_get_game_analyze_total(void)
{
    return cs_g;
}

/* 0x00156630 */
int player_status_get_stage_analyze_total(void)
{
    return cs_s;
}

/* 0x00156640 */
int player_status_get_game_analyzed(void)
{
    return cs_get_g;
}

/* 0x00156650 */
int player_status_get_stage_analyzed(void)
{
    return cs_get_s;
}

/* Game and stage. */
/* 0x00156660 */
void player_status_count_analyzed(void)
{
    player_status.game_analyzed += 1;
    player_status.stage_analyzed += 1;
}

/* 0x00156688 */
int player_status_get_game_item_total(void)
{
    return item_ap_g;
}

/* 0x00156698 */
int player_status_get_stage_item_total(void)
{
    return item_ap_s;
}

/* 0x001566a8 */
int player_status_get_game_items_got(void)
{
    return item_get_g;
}

/* 0x001566b8 */
int player_status_get_stage_items_got(void)
{
    return item_get_s;
}

/* Game and stage. */
/* 0x001566c8 */
void player_status_count_item(void)
{
    player_status.game_item_total += 1;
    player_status.stage_item_total += 1;
}

/* Game and stage. */
/* 0x001566f0 */
void player_status_count_item_got(void)
{
    player_status.game_items_got += 1;
    player_status.stage_items_got += 1;
}

/*
 * Text "score" or "scorexmultiplier" at the scoring point's screen position (or the corner);
 * life 60 frames.
 */
/* 0x00156718 */
void player_status_disp_score_init(PlayerStatusDispScoreWork *w, PlayerStatusDispScoreArg *prm)
{
    float pos[3];

    if (prm->mul == 1)
        sprintf(w, "%d", prm->score);
    else
        sprintf(w, "%dx%d", prm->score, prm->mul);
    if (prm->pos == 0) {
        w->x = 0;
        w->y = 0;
    } else {
        ulMatrixApplyPoint(ulMatrixCur, prm->pos, pos);
        project_point(pos, pos);
        w->x = ulRoundAway(pos[0]);
        w->y = ulRoundAway(pos[1]);
    }
    w->timer = 60;
}

/* 0x001567d0 */
void player_status_disp_score_release(void)
{
}

/* Rises half a pixel a frame; ends with its life or the stage task. */
/* 0x001567d8 */
void player_status_disp_score_exec(PlayerStatusDispScoreWorkView *w)
{
    if (w->timer-- <= 0) {
        if (w)
            TASK_OF(w)->flags |= TASK_F_END;
    }
    w->y = w->y - 0.5f;
    if (!stage_get_work() || (((int *)stage_get_work())[-9] & 1)) {
        do {
            if (w)
                TASK_OF(w)->flags |= TASK_F_END;
        } while (0);
    }
}

/* Disp: green text, skipped far off screen. */
#ifdef VERSION_US
/* 0x00156870 */
void player_status_disp_score_disp(PlayerStatusDispScoreWork *w)
{
    float x = w->x;
    float y = ulRoundAway(w->y);

    if (x <= -1500.0f || x >= 1500.0f || y <= -1500.0f || y >= 1500.0f)
        return;
    draw_set_blend(4, 5);
    font_set(2);
    font_print(x, y, 1.0526316f, RGBA(0, 0x80, 0, 0x80), w);
}
#else
/* 0x00156870 */
void player_status_disp_score_disp(PlayerStatusDispScoreWork *w)
{
    draw_set_blend(4, 5);
    font_set(2);
    font_print(w->x, ulRoundAway(w->y), 1.0526316f, RGBA(0, 0x80, 0, 0x80), w);
}
#endif

/* score x mul points; pop-up only when game_flags 0x40 and status flag 0x40000000. */
/* 0x00156950 */
void player_status_disp_score_create(void *parent, PlayerStatusDispScoreArg *prm)
{
    if ((game_flags & 0x40) && player_status_test_flags(0x40000000))
        task_create_arg(parent, &player_status_disp_score_desc, 44, prm);
    player_status_add_score(prm->score * prm->mul);
}

/* Item flying to its HUD slot: full size and opacity, spinning 8 degrees a frame. */
/* 0x001569c0 */
void ps_sub_add_item_init(PsSubAddItemWork *w, int mode, float x, float y, int idx, int delay)
{
    w->mode = mode;
    w->idx = idx;
    w->delay = delay;
    w->x = x;
    w->y = y;
    w->scl = 1.0f;
    w->alpha = 255;
    w->ang = 0;
    w->spd = 1456;
    task_set_dlist(w, 6);
}

/* 0x00156a00 */
void ps_sub_add_item_release(void)
{
}

/* After arrival: fades out (ends when gone), grows 4% a frame, spins faster. */
/* 0x00156a08 */
void ps_sub_add_item_fade_exec(PsSubAddItemWork *w)
{
    w->alpha -= 8;
    if (w->alpha <= 0) {
        w->alpha = 0;
        if (w)
            TASK_OF(w)->flags |= TASK_F_END;
    }
    w->scl = w->scl * 1.04f;
    w->ang = w->ang + w->spd;
    w->spd = w->spd + 182;
}

/*
 * After the delay flies up to 12 pixels a frame to its slot (mode 0 Overdrive, right; 2 item,
 * left); on arrival adds it and starts fading.
 */
/* 0x00156a68 */
void ps_sub_add_item_exec(PsSubAddItemWork *w)
{
    float tgt[2];
    float dx;
    float dy;
    float dis;
    float scale;

    if (w->delay >= 0) {
        w->delay--;
        return;
    }
    w->ang += w->spd;
    switch (w->mode) {
    case 0:
        console_cell_pos_right(w->idx, &tgt[0], &tgt[1]);
        tgt[0] = tgt[0] + 18.0f;
        tgt[1] = tgt[1] + 22.0f;
        break;
    case 2:
        console_cell_pos_left(w->idx, &tgt[0], &tgt[1]);
        tgt[0] = tgt[0] + 9.0f;
        tgt[1] = tgt[1] + 6.0f;
        break;
    default:
        tgt[1] = tgt[0] = 0.0f;
        break;
    }
    dx = tgt[0] - w->x;
    dy = tgt[1] - w->y;
    dis = sqrtf(dx * dx + dy * dy);
    if (12.0f <= dis) {
        scale = 12.0f / dis;
        dx *= scale;
        dy *= scale;
    } else {
        switch (w->mode) {
        case 0:
            player_status_add_overdrive(1);
            break;
        case 2:
            player_status_add_items(1);
            break;
        }
        TASK_OF(w)->exec = ps_sub_add_item_fade_exec;
    }
    w->x += dx;
    w->y += dy;
}

/* Additive sprite at its size, angle and opacity. */
/* 0x00156c08 */
void ps_sub_add_item_disp(PsSubAddItemWork *w)
{
    float size;
    int col;

    draw_set_blend(4, 1);
    ulGxSetTextureList(system_ptm);
    size = w->scl * 32.0f;
    col = ((w->alpha + 1) >> 1) << 24;
    col |= 0x808080;
    tex_rect_draw_rot(10, col, w->ang, w->x, w->y, 2.2222223f, size, size);
}

/*
 * Mode 0 Overdrives (stock not full), 2 items (not at the highest level), into the next free
 * slots, 15 frames apart.
 */
/* 0x00156c90 */
void ps_sub_add_item_spawn(float x, float y, int mode, int count)
{
    int idx;
    int ang;
    void *owner;
    void *w;

    if (count <= 0)
        return;
    owner = stage_get_work();
    /* Always 0. */
    ang = (count & 1) >> 1;
    switch (mode) {
    case 0:
        idx = player_status_get_overdrive();
        if (!(idx < player_status_get_overdrive_max()))
            return;
        break;
    case 2:
        if (player_status_is_max_level())
            return;
        idx = player_status_get_progress();
        break;
    default:
        idx = ang;
        break;
    }
    do {
        w = task_create(owner, &ps_sub_add_item_desc, 36);
        if (w) {
            ps_sub_add_item_init(w, mode, x, y, idx, ang);
            idx++;
            ang += 15;
            if (!(idx < 8))
                idx = 0;
            count--;
        } else {
            count--;
        }
    } while (count);
}
