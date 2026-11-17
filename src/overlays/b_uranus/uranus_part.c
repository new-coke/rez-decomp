/*
 * Uranus boss part manager: 200 cubes steered by weighted behaviours, split when shot, moved
 * between two form slots (model, snake, orbits, wall, core pit). Raises the damage level on the
 * beat as cubes are lost, exposes the core, and scatters the body when beaten.
 */

#include "types_gw.h"
#include "b_uranus.h"
#include "mathdef.h"
#include "color.h"
#include "console.h"
#include "debug.h"
#include "enemy_status.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "uranus_part.h"

#define FLARE_COL RGBA(0x80, 0x80, 0x80, 0x7f)
#include "uranus.h"
#include "uranus_bullet.h"
#include "uranus_core.h"
#include "uranus_map.h"
#include "uranus_model.h"
#include "uranus_orbit.h"
#include "uranus_snake.h"
#include "uranus_wall.h"
#include "../common/boss_common.h"
#include "../common/boss_demo_mes.h"
#include "../common/boss_sound.h"
#include "../common/eff_circle.h"

extern float uranus_pm_hit_sec;
extern int cmd_deb_hit_count;
extern struct TaskDesc uranus_pm_desc;
extern int cmd_deb_seq_time;
extern float uranus_pm_pit_sec;
extern float uranus_pm_core_sec;
extern int uranus_pm_hit_min;
extern float uranus_pm_hit_circle_scl;
extern int cm_uranus_col[];
extern char uranus_pm_vardef[];
extern int cmd_cube_life_x;
extern float cmd_p1_mv;
extern float cmd_p2_mvr;
extern int uranus_pm_level_mul;
extern float uranus_pm_level_scl[];
extern int uranus_pm_level_forms[][2];
extern int uranus_pm_level_types[][2];
extern float cmd_p1_mvr;
extern int cmd_sp_frm;
extern float uranus_pm_fire_sec;
extern float uranus_pm_fire_near;
extern float uranus_pm_fire_far;
extern float uranus_pb_drag;
extern float uranus_pb_grav;
extern float cmd_p_r;
extern float cmd_frate_1, cmd_frate_2, cmd_frate_3, cmd_frate_4;
extern float cmd_frate_5, cmd_frate_6, cmd_frate_7, cmd_frate_8;
extern float cmd_rate_fade_time;
extern float cmd_p2_mv;
extern char str_uranus_part_c[];
extern int uranus_pm_vars_group;
extern GwVarDef uranus_pm_vardefs[];
extern float cmd_p2_r;
extern float cmd_player_r;
extern float cmd_player_dv;

extern int cmd_deb_disp;
extern int uranus_pm_deb_x;
extern int uranus_pm_deb_y;
extern int uranus_pm_deb_g3;
extern char *uranus_pm_form_names[];
extern char str_uranus_pm_num_fmt[];
extern char str_uranus_pm_tree_fmt[];
extern char str_uranus_pm_seq_fmt[];
extern char str_uranus_pm_rate_fmt[];
extern int cmd_test_fast;
extern float uranus_pm_obj_scl;
extern float cmd_p4_mv;
extern float cmd_col_r;
extern float cmd_col_r2;
extern float cmd_cube_r;
extern struct TaskDesc uranus_pb_desc;
extern float uranus_pb_sec;
extern float uranus_pb_sec_rnd;
extern float uranus_pb_kick;
extern float uranus_pb_kick_rnd;
extern int cmd_deb_pos_trap;
extern int cmd_set_sw;
extern int cmd_auto_break_sw;
extern int cmd_auto_break_loop_frm;
extern float uranus_pm_sw_sec;
extern float uranus_pm_sw_rate_max;
extern float uranus_seq_time[];
extern char str_uranus_pm_name[];
extern float cmd_do_core_v;
extern float uranus_pm_core_frm_step;
extern unsigned int cm_uranus_col3;
extern BossCfKey uranus_pm_hit_keys[];
extern int uranus_pm_set_course;
extern float uranus_pm_sel_sec;
extern float uranus_pm_spd_rate;
extern unsigned int system_flags;
extern float cmd_p3_mv;
extern float cmd_hit_col_t;
extern float cmd_do_mf;
extern float cmd_do_kf;
extern const float uranus_pm_glow_min;
extern float uranus_pm_glow_scl;
extern float uranus_pm_glow_rate;
extern float uranus_pm_glow_damp;
extern float uranus_pm_core_spin;

enum {
    PM_RESET = 8,
    PM_START,                   /* waits, then builds the first forms */
    PM_PIT,                     /* the core waits in its pit for the player */
    PM_PIT_IN,                  /* the cubes leave the pit for the second form */
    PM_FORMS,                   /* switching between the two forms */
    PM_CORE,                    /* the core is exposed */
    PM_NEXT,                    /* damage level up: scatter and rebuild */
    PM_KILL,
    PM_DEAD
};

/* broken cube piece, 0x4c bytes */
typedef struct UranusPbWork {
    char pad_00[4];
    void *textures;             /* 0x04 */
    ModelPart *model;
    Vec3 pos;
    Vec3 vel;
    int ang[3];
    int spin[3];
    int life;
    int life_max;
    struct UranusMapWork *map;
    float r;
} UranusPbWork;

/* steer(cast) */
/* 0x00cad0a0 */
void uranus_pm_init(UranusPartWork *w, UranusWork *boss)
{
    Vec3 *pool;
    int n;
    ModelPart **parts;
    ModelPart *body;
    float z;

    w->boss = boss;
    w->objs = heap_block_tag(game_calloc(200 * sizeof(UranusObj)), str_uranus_part_c, 410);
    boss = w->boss;
    parts = boss->parts;
    body = (ModelPart *)((int *)parts)[32]; /* steer(alias) */
    z = -1.0f;
    w->dir.z = z;
    *(int *)&w->tc = *(int *)&boss->tc; /* steer(alias) */
    w->unk_ac = w->dir.x = w->dir.y = 0.0f;
    *(int *)&w->map = *(int *)&boss->map; /* steer(alias) */
    w->unk_c4 = 0;
    w->unk_b4 = body->frames;
    z = 1.0f;
    w->unk_b0 = z;
    w->nobjs = 200;
    w->parts = parts;
    w->body = body;
    pool = heap_block_tag(game_calloc(7 * 200 * sizeof(Vec3)), str_uranus_part_c, 426);
    w->cube_life = cmd_cube_life_x;
    n = w->nobjs;
    w->pool = pool;
    w->home = pool;
    w->slot_pts[0][0] = pool + n;
    w->slot_pts[0][1] = pool + n * 2;
    w->slot_pts[0][2] = pool + n * 3;
    w->slot_pts[1][0] = pool + n * 4;
    w->slot_pts[1][1] = pool + n * 5;
    w->slot_pts[1][2] = pool + n * 6;
    w->boss_arg = w->boss->arg0;
    w->state = PM_RESET;
    uranus_pm_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, uranus_pm_vardef);
    gw_varedit_add_table(&gw_varedit_root, uranus_pm_vars_group, uranus_pm_vardefs);
    uranus_core_vars_add();
    uranus_model_vars_add();
    uranus_snake_vars_add();
    uranus_wall_vars_add();
    uranus_orbit_vars_add();
}

/* 0x00cad258 */
void uranus_pm_blend_mtx(UranusPartWork *w)
{
    float rate, rate2;
    float *dst, *from, *to;
    int i;

    if (w->wgt_cnt) {
        w->wgt_cnt--;
        rate = (float)w->wgt_cnt / (float)w->wgt_total;
        rate2 = 1.0f - rate;
        from = w->wgt_from.wgts;
        dst = w->wgt.wgts;
        to = w->wgt_to.wgts;
        for (i = 0; i < 16; i++)
            dst[i] = from[i] * rate + to[i] * rate2;
    }
}

/* 0x00cad2d8 */
void uranus_pm_col_step(UranusPartWork *w)
{
    float rate;

    if (w->col_cnt) {
        if (--w->col_cnt <= 0)
            w->col_kind = w->col_kind_to;
        rate = (float)w->col_cnt / (float)w->col_total;
        w->width = w->width_from * rate + w->width_to * (1.0f - rate);
        w->col = boss_color_blend(w->col_from, w->col_to, rate);
    }
}

/* 0x00cad368 */
void uranus_pm_set_col(UranusPartWork *w, int kind, float width, float secs)
{
    w->col_cnt = w->col_total = (int)(secs * FPS);
    if (kind == 1)
        w->col_kind_to = kind;
    else
        w->col_kind_to = 0;
    if (!w->col_cnt) {
        w->col_kind = w->col_kind_to;
        w->col = w->col_from = w->col_to = cm_uranus_col[kind];
        w->width = w->width_from = w->width_to = width;
        w->col_cnt = w->col_total = 1;
        uranus_pm_col_step(w);
    } else {
        w->width_to = width;
        w->width_from = w->width;
        w->col_from = w->col;
        w->col_to = cm_uranus_col[kind];
    }
}

/* 0x00cad420 */
void uranus_pm_hit_reset(UranusPartWork *w)
{
    w->sw_delay = 0;
    w->sw_req[1] = 0;
    w->sw_req[0] = 0;
    w->sw_wait = uranus_pm_hit_sec * FPS;
}

/* 0x00cad450 */
int uranus_pm_is_end(UranusPartWork *w)
{
    return w->end;
}

/* 0x00cad458 */
int uranus_pm_fire(UranusPartWork *w, int type)
{
    Vec3 to_pl, core;
    float dist;

    if (w->state == PM_CORE && type < 2)
        return 0;
    if ((unsigned int)(*(volatile int *)&w->state - PM_KILL) < 2) /* steer(volatile): killed or dead */
        return 0;
    player_get_frame_vel(&core);
    v3_addto(&core, &w->core->pos);
    player_get_pos(&to_pl);
    v3_sub(&to_pl, &core, &to_pl);
    dist = vec3_length(&to_pl);
    if (dist <= uranus_pm_fire_far && uranus_pm_fire_near <= dist) {
        boss_se_play(13, 0, (int)&w->core->pos);
        uranus_mb_man_create(w, w->map, &w->core->pos, w->parts, (int)(uranus_pm_fire_sec * FPS), type);
        if (!w->core_state)
            w->core_state = 1;
    }
    return 1;
}

/* 0x00cad5c0 */
int uranus_pm_get_sec(UranusPartWork *w)
{
    return w->frm / 60;
}

/* 0x00cad5e0 */
void uranus_pm_clear_hits(UranusPartWork *w)
{
    w->hits = 0;
}

/* 0x00cad5e8 */
int uranus_pm_get_hits(UranusPartWork *w)
{
    if (!cmd_deb_hit_count)
        return w->hits;
    return 1000;
}

/* 0x00cad608 */
void uranus_pm_set_gauge(UranusPartWork *w, int on)
{
    if (on) {
        boss_gage_create(0);
        w->gauge = 1;
    } else {
        console2_swap_boss_on(0);
        w->gauge = 0;
    }
}

/* 0x00cad650 */
void uranus_pm_to_core(UranusPartWork *w)
{
    if (w->level == 3 && w->state == PM_FORMS) {
        w->state = PM_CORE;
        w->blend_spd = 0.01f;
        w->seq_frm = 0;
        uranus_pm_break_objs(w);
    }
}

/* 0x00cad698 */
int uranus_pm_is_core(UranusPartWork *w)
{
    return w->state == PM_CORE;
}

/* 0x00cad6a8 */
int uranus_pm_can_core(UranusPartWork *w)
{
    if (!cmd_deb_seq_time && w->state == PM_FORMS && uranus_pm_core_sec * FPS < (float)w->level_frm && w->level == 3)
        return 1;
    return 0;
}

/* 0x00cad708 */
void uranus_pm_pit_reached(UranusPartWork *w, int id)
{
    int frames;

    if (w->slot_task[0] == id && w->state == PM_PIT) {
        w->state = PM_PIT_IN;
        w->end = 0;
        frames = uranus_pm_pit_sec * FPS;
        w->wait = w->wait_max = frames;
        if (w->sw_cb[1])
            w->sw_cb[1](w->slot_task[1]);
    }
}

/* 0x00cad778 */
int uranus_pm_get_models(UranusPartWork *w)
{
    return w->form;
}

/* 0x00cad780 */
void uranus_pm_break_objs(UranusPartWork *w)
{
    Vec3 scroll;
    UranusObj *obj = w->objs;
    int i;

    player_get_frame_vel(&scroll);
    for (i = 200; i; obj++, i--) {
        if (obj->flags && obj->lvl == 1) {
            v3_add(&obj->pos, &scroll);
            uranus_pm_obj_split_at(w, obj);
        }
    }
    if (w->level == 1)
        boss_se_play(11, 0, (int)&w->core->pos);
    else
        boss_se_play(12, 0, (int)&w->core->pos);
}

/* 0x00cad878 */
int uranus_pm_sw_request(UranusPartWork *w, int id, int delay)
{
    int slot;

    if (w->slot_task[0] == id)
        slot = 0;
    else if (w->slot_task[1] == id)
        slot = 1;
    else
        return 0;
    if (w->cur != slot)
        return 0;
    if (w->sw_req[slot])
        return 0;
    if (w->sw_wait)
        return 0;
    w->sw_req[slot] = 1;
    w->sw_delay = delay;
    if (w->sw_cb[slot ^ 1])
        w->sw_cb[slot ^ 1](w->slot_task[slot ^ 1]);
    return 1;
}

/* 0x00cad920 */
int uranus_pm_sw_force(UranusPartWork *w, int id, int delay)
{
    int slot;

    if (w->slot_task[0] == id)
        slot = 0;
    else if (w->slot_task[1] == id)
        slot = 1;
    else
        return 0;
    if (w->cur != slot)
        return 0;
    if (w->sw_req[slot])
        return 0;
    w->sw_req[slot] = 1;
    w->sw_delay = delay;
    w->sw_wait = 0;
    if (w->sw_cb[slot ^ 1])
        w->sw_cb[slot ^ 1](w->slot_task[slot ^ 1]);
    return 1;
}

/* 0x00cad9c0 */
void uranus_pm_set_cb(UranusPartWork *w, int owner, void (*cb)(int), int slot)
{
    w->sw_cb[slot] = cb;
}

/* 0x00cad9d0 */
int uranus_pm_sw_is_live(UranusPartWork *w, int id)
{
    int slot;

    if (w->slot_task[0] == id)
        slot = 0;
    else if (w->slot_task[1] == id)
        slot = 1;
    else
        return 0;
    if (w->cur != slot)
        return 0;
    return w->core->hp > 0;
}

/* 0x00cada10 */
int uranus_pm_sw_is_ready(UranusPartWork *w, int id)
{
    int slot;

    if (w->slot_task[0] == id)
        slot = 0;
    else if (w->slot_task[1] == id)
        slot = 1;
    else
        return 0;
    if (w->cur != slot)
        return 0;
    if (w->core->hp <= 0)
        return 0;
    if (!slot) {
        if (fabsf(w->blend) < FLT_EPSILON)
            return 1;
    } else if (fabsf(w->blend - 1.0f) < FLT_EPSILON)
        return 1;
    return 0;
}

/* 0x00cadab0 */
int uranus_pm_sw_get(UranusPartWork *w, int id, int *cnt, Vec3 *pos)
{
    int slot;
    Vec3 *core;

    if (w->slot_task[0] == id)
        slot = 0;
    else if (w->slot_task[1] == id)
        slot = 1;
    else
        return 0;
    if (w->cur == slot)
        return 0;
    if (cnt)
        *cnt = w->sw_delay;
    if (pos) {
        core = &w->core->pos;
        pos->x = core->x;
        pos->y = core->y;
        pos->z = core->z;
    }
    return 1;
}

/* 0x00cadb20 */
int uranus_pm_slot_create(UranusPartWork *w, int type, int slot, UranusNode **out)
{
    Vec3 **pts = !slot ? w->slot_pts[0] : w->slot_pts[1];
    UranusObj *core;
    Vec3 pos, scroll;
    Vec3 *start_pos;
    int other;

    w->slot_type[slot] = type;
    core = w->core;
    if (core) {
        v3_copy(&pos, &core->pos);
        player_get_frame_vel(&scroll);
        v3_add(&pos, &scroll);
        start_pos = &pos;
    } else
        start_pos = 0;
    w->sw_cb[slot] = 0;
    switch (type) {
    case FORM_WALL:
        w->slot_task[slot] = (int)uranus_wall_create(w->tc, w, pts, &w->node[slot], slot, w->parts);
        break;
    case FORM_PIT:
        w->slot_task[slot] = (int)uranus_pit_create(w->tc, w, pts, &w->node[slot], start_pos, w->node[slot ^ 1], w->parts);
        break;
    case FORM_MODEL:
        w->slot_task[slot] = (int)uranus_model_create(w->tc, w, pts, &w->node[slot], (YNode *)w->body->root, 0, slot);
        break;
    case FORM_MODEL2:
        w->slot_task[slot] = (int)uranus_model_create(w->tc, w, pts, &w->node[slot], (YNode *)w->body->root, 1, slot);
        break;
    case FORM_SNAKE:
        other = w->slot_type[slot ^ 1];
        if (other == FORM_MODEL || other == FORM_MODEL2)
            w->slot_task[slot] = (int)uranus_snake_create_from(w->tc, w, pts, &w->node[slot], slot, w->node[slot ^ 1], w->parts);
        else
            w->slot_task[slot] = (int)uranus_snake_create(w->tc, w, pts, &w->node[slot], slot, w->parts);
        break;
    case FORM_ORBIT:
        w->slot_task[slot] = (int)u_or_create(w->tc, w, pts, &w->node[slot], slot);
        break;
    case FORM_ORBIT2:
        {
            w->slot_task[slot] = (int)u_or2_create(w->tc, w, pts, &w->node[slot], slot);
        }
        break;
    }
    if (out)
        *out = w->node[slot];
    return w->slot_task[slot];
}

/* 0x00caddc8 */
void uranus_pm_set_level(UranusPartWork *w)
{
    int level = w->level;
    int mul = uranus_pm_level_mul;
    int lv_idx;

    w->form = level % 4;
    lv_idx = level % 8;
    w->cube_hp[0] = ((int *)uranus_pm_level_forms)[w->form * 2] * mul;
    w->cube_hp[1] = ((int *)uranus_pm_level_forms)[(w->form << 1) | 1] * mul;
    w->map_spd = uranus_pm_level_scl[lv_idx];
    w->level_type[0] = ((int *)uranus_pm_level_types)[lv_idx * 2];
    w->level_type[1] = ((int *)uranus_pm_level_types)[(lv_idx << 1) | 1];
    w->level_frm = 0;
}

/* 0x00cade88 */
void uranus_pm_damage_cb(UranusPartWork *w)
{
    UranusObj *core;
    int hp;

    peri_act_play_boss_break();
    boss_vol_play(w->level + 2, 0);
    if (w->dmg_score)
        lock_score_at(w->level, &w->core->pos, 1);
    switch (w->dmg_kind) {
    case 1:
        hp = cmd_cube_life_x / 4 * (3 - w->level);
        w->state = PM_NEXT;
        core = w->core;
        if (hp < core->lock.life)
            core->lock.life = hp;
        break;
    case 2:
        w->state = PM_NEXT;
        break;
    }
    w->dmg_kind = 0;
    w->dmg_score = 0;
}

/* 0x00cadf58 */
void uranus_pm_damage_req(UranusPartWork *w, int kind, int score)
{
    boss_bgm_switch((int)uranus_pm_damage_cb, (int)w, w->level + 1);
    w->dmg_kind = kind;
    w->dmg_score = score;
}

/* 0x00cadfb0 */
int uranus_pm_check_kill(UranusPartWork *w)
{
    if (beat_grid_hit(0) && !(beat_clock_get_haku() & 3)) {
        if (uranus_pm_hit_min >= w->live || w->core->hp <= 0) {
            w->state = PM_KILL;
            boss_pcm_load(0);
            boss_se_stop_all();
            return 1;
        }
    }
    return 0;
}

/* 0x00cae038 */
int uranus_pm_check_damage(UranusPartWork *w, int score)
{
    int level;

    if (w->dmg_kind > 0)
        return 0;
    level = w->level;
    if (level >= 3)
        return uranus_pm_check_kill(w);
    if (!(uranus_pm_hit_min < w->live)) {
        uranus_pm_damage_req(w, 1, score);
        return 0;
    }
    if (w->core->hp < cmd_cube_life_x / 4 * (3 - level))
        uranus_pm_damage_req(w, 2, score);
    return 0;
}

/* 0x00cae0f0 */
void uranus_pm_reset(UranusPartWork *w)
{
    float ofs[8];
    float z;
    void *task;
    int i;

    if (w->slot_task[0]) {
        task_end_with_exit((void *)w->slot_task[0]);
        w->sw_cb[0] = 0;
    }
    if (w->slot_task[1]) {
        task_end_with_exit((void *)w->slot_task[1]);
        w->sw_cb[1] = 0;
    }
    for (i = 0; i < 200; i++)
        uranus_pm_obj_free(w, &w->objs[i]);
    memset(w->objs, 0, 200 * sizeof(UranusObj));
    z = 0.0f;
    uranus_pm_set_col(w, 1, cmd_p_r, z);
    w->core = 0;
    w->blend_spd = z;
    w->sw_wait = 0;
    w->level = 0;
    w->form = 0;
    w->frm = 0;
    w->seq_frm = 0;
    w->dmg_kind = 0;
    w->dmg_score = 0;
    w->unk_c4 = -1;
    uranus_pm_set_level(w);
    uranus_pm_hit_reset(w);
    ofs[0] = cmd_frate_1;
    ofs[1] = cmd_frate_2;
    ofs[2] = cmd_frate_3;
    ofs[3] = cmd_frate_4;
    ofs[4] = cmd_frate_5;
    ofs[5] = cmd_frate_6;
    ofs[6] = cmd_frate_7;
    ofs[7] = cmd_frate_8;
    uranus_pm_set_layout(w, (UranusPmLayout *)ofs, cmd_rate_fade_time);
    w->core_frm = z;
    w->core_frm_max = w->parts[24]->frames;
    w->core_state = 0;
    task = w->unk_564;
    if (task) {
        do {
            FINISH(task);
        } while (0);
        w->unk_564 = 0;
    }
}

/* 0x00cae290 */
void uranus_pm_update(UranusPartWork *w)
{
    Vec3 v;
    Vec3 *acc;

    w->unk_c4 = (w->unk_c4 + cmd_sp_frm) % 200;
    uranus_pm_get_centre(w, &w->centre);
    uranus_map_get_vel_z(&v);
    v3_add(&w->centre, &v);
    uranus_map_get_dir(&w->dir);
    uranus_map_get_drift(&w->drift);
    player_get_pos(&w->pl_pos);
    w->kill_cnt_prev = w->kill_cnt;
    if (w->kill_cnt)
        v3_mul(&w->kill_ctr, &w->kill_sum, 1.0f / (float)w->kill_cnt);
    acc = &w->kill_sum;
    acc->x = 0.0f;
    w->kill_cnt = 0;
    acc->y = 0.0f;
    acc->z = 0.0f;
    uranus_pm_blend_mtx(w);
    uranus_pm_col_step(w);
}

/* 0x00cae3b8 */
void uranus_pm_exec_core(UranusPartWork *w)
{
    UranusNode *node;
    void (*fn)(int);
    float rate;
    int i;

    switch (w->state) {
    case PM_CORE:
        i = 0;
        rate = w->blend + w->blend_spd;
        w->blend = rate;
        if (1.0f < rate) {
            w->blend = 1.0f;
            w->blend_spd = 0.0f;
            w->sw_req[w->cur] = 0;
            w->cur = 1;
            if (w->live) {
                float lim = -uranus_pm_sw_rate_max;
                float rate = -1.0f / (uranus_pm_sw_sec * (w->live / 100.0f) * FPS);

                w->blend_spd = rate;
                if (rate < lim)
                    w->blend_spd = lim;
            } else
                w->blend_spd = -uranus_pm_sw_rate_max;
            uranus_pm_objs_bind(w, 0, 0, 0);
            if (w->slot_task[0])
                task_end_with_exit((void *)w->slot_task[0]);
            if (!(int)(RAND_FLOAT_A() * 2.0f))
                uranus_pm_slot_create(w, FORM_ORBIT2, 0, &node);
            else
                uranus_pm_slot_create(w, FORM_WALL, 0, &node);
            uranus_pm_objs_bind_tree(w->objs, i, node);
            w->core->node[i] = node;
            fn = w->sw_cb[i];
            if (fn)
                fn(w->slot_task[i]);
        } else if (rate < 0.0f) {
            if (w->live) {
                float lim = uranus_pm_sw_rate_max;
                float rate = 1.0f / (uranus_pm_sw_sec * (w->live / 100.0f) * FPS);

                w->blend_spd = rate;
                if (lim < rate)
                    w->blend_spd = lim;
            } else
                w->blend_spd = uranus_pm_sw_rate_max;
            w->blend_spd = 1.0f / (uranus_pm_sw_sec * FPS);
            w->blend = 0.0f;
            w->sw_req[w->cur] = 0;
            w->cur = 0;
        }
        if (!cmd_deb_seq_time) {
            w->seq_frm++;
            if (uranus_seq_time[w->level + 1] * FPS <= (float)w->seq_frm) {
                w->live = 0;
                boss_scoring_on(0);
            }
        }
        uranus_pm_check_kill(w);
        break;
    case PM_KILL:
        if (!beat_grid_hit(0) || (beat_clock_get_haku() & 3))
            break;
        uranus_pm_objs_scatter(w);
        uranus_pm_objs_bind(w, -1, 0, 0);
        w->core->lock.life = 0;
        boss_pcm_start();
        if (boss_get_scoring())
            boss_demo_mes2_start(w, str_uranus_pm_name, 0);
        else
            boss_demo_mes2_start(w, str_uranus_pm_name, 2);
        w->state = PM_DEAD;
    case PM_DEAD:
        w->end = 1;
        break;
    }
}

/* 0x00cae780 */
void uranus_pm_step(UranusPartWork *w)
{
    UranusNode *nodes[2][2];
    UranusNode *node;
    int slot;

    w->level_frm++;
    switch (w->state) {
    case PM_START: {
        if (--w->wait > 0)
            break;
        uranus_pm_slot_create(w, w->level_type[0], 1, &nodes[0][0]);
        uranus_pm_slot_create(w, FORM_PIT, 0, &nodes[0][1]);
        uranus_pm_objs_attach(w, 0, 0, nodes[0][1]);
        uranus_pm_objs_bind(w, 1, 1, nodes[0][0]);
        w->blend = 0.0f;
        w->cur = 0;
        w->state = PM_PIT;
        uranus_pm_set_lockable(w, 0);
        uranus_pm_set_gauge(w, 0);
        break;
    }
    case PM_RESET:
        uranus_pm_reset(w);
        if (uranus_pm_set_course) {
            w->level = uranus_pm_set_course;
            uranus_pm_set_course = 0;
            uranus_pm_set_level(w);
        }
        uranus_map_reset(w->map, 0);
        uranus_map_build_course(w->map, w->level);
        uranus_map_set_spd(w->map, 0.5f);
        w->wait_max = 3;
        w->wait = 3;
        w->state = PM_START;
        break;
    case PM_NEXT: {
        w->sw_wait = 0;
        w->blend_spd = 0.0f;
        w->sw_req[1] = 0;
        w->sw_req[0] = 0;
        w->end = 1;
        uranus_pm_objs_scatter(w);
        boss_se_play(2, 0, (int)&w->core->pos);
        uranus_pm_objs_bind(w, -1, 0, 0);
        if (w->slot_task[0])
            task_end_with_exit((void *)w->slot_task[0]);
        if (w->slot_task[1])
            task_end_with_exit((void *)w->slot_task[1]);
        w->sw_cb[1] = 0;
        w->sw_cb[0] = 0;
        w->level++;
        uranus_pm_set_level(w);
        uranus_pm_slot_create(w, w->level_type[0], 1, &nodes[1][0]);
        uranus_pm_slot_create(w, FORM_PIT, 0, &nodes[1][1]);
        uranus_pm_objs_attach(w, 0, 1, nodes[1][1]);
        w->core->node[0] = nodes[1][1];
        uranus_pm_objs_bind(w, 1, 1, nodes[1][0]);
        w->blend = 0.0f;
        w->cur = 0;
        w->state = PM_PIT;
        uranus_map_build_course(w->map, w->level);
        uranus_pm_hit_reset(w);
        break;
    }
    case PM_PIT_IN: {
        uranus_map_blend_spd(w->map, w->map_spd, uranus_pm_spd_rate);
        if (--w->wait <= 0) {
            uranus_pm_objs_bind(w, 0, 0, 0);
            if (w->slot_task[0]) {
                task_end_with_exit((void *)w->slot_task[0]);
                w->sw_cb[0] = 0;
            }
            uranus_pm_slot_create(w, w->level_type[1], 0, &node);
            uranus_pm_objs_bind(w, 0, 1, node);
            w->blend = 1.0f;
            w->cur = 1;
            w->state = PM_FORMS;
            w->seq_frm = 0;
            if (!w->slot_type[1])
                uranus_pm_break_objs(w);
        } else {
            w->blend = 1.0f - (float)w->wait / (float)w->wait_max;
        }
        break;
    }
    case PM_FORMS:
        uranus_map_blend_spd(w->map, w->map_spd, uranus_pm_spd_rate);
        slot = w->cur;
        if (w->sw_req[slot]) {
            if (w->sw_delay)
                w->sw_delay--;
            else if (!slot)
                w->blend_spd = 1.0f / (uranus_pm_sel_sec * FPS);
            else
                w->blend_spd = -1.0f / (uranus_pm_sel_sec * FPS);
        }
        w->blend += w->blend_spd;
        if (w->sw_wait)
            w->sw_wait--;
        if (1.0f < w->blend) {
            w->sw_req[w->cur] = 0;
            w->blend = 1.0f;
            w->cur = 1;
            w->blend_spd = 0.0f;
            w->sw_wait = uranus_pm_hit_sec * FPS;
        } else if (w->blend < 0.0f) {
            w->blend = 0.0f;
            w->blend_spd = 0.0f;
            w->sw_req[w->cur] = 0;
            w->cur = 0;
            w->sw_wait = uranus_pm_hit_sec * FPS;
        }
        if (!cmd_deb_seq_time && uranus_seq_time[w->level] * FPS <= (float)++w->seq_frm) {
            w->live = 0;
            uranus_pm_check_damage(w, 0);
            break;
        }
        uranus_pm_check_damage(w, 1);
        break;
    case PM_PIT:
        break;
    default:
        uranus_pm_exec_core(w);
        break;
    }
}

/* 0x00caec88 */
void uranus_pm_exec(UranusPartWork *w)
{
    float ofs[8];
    UranusObj *obj;
    unsigned int bits;
    int i;

    w->frm++;
    if (w->hit_fade && (TASK_OF(w->hit_fade)->flags & TASK_F_END))
        w->hit_fade = 0;
    if (cmd_set_sw) {
        ofs[0] = cmd_frate_1;
        ofs[1] = cmd_frate_2;
        ofs[2] = cmd_frate_3;
        ofs[3] = cmd_frate_4;
        ofs[4] = cmd_frate_5;
        ofs[5] = cmd_frate_6;
        ofs[6] = cmd_frate_7;
        ofs[7] = cmd_frate_8;
        uranus_pm_set_layout(w, (UranusPmLayout *)ofs, 0.0f);
    }
    uranus_pm_step(w);
    uranus_pm_update(w);
    if (debug_flags_test(4) && (kbd_state[7] & 0x44) && kbd_state[5] == 17)
        uranus_pm_objs_scatter(w);
    obj = w->objs;
    w->live = 0;
    w->live_a = 0;
    w->live_b = 0;
    w->live_ab = 0;
    for (i = 200; i; i--, obj++) {
        if (!obj->flags)
            continue;
        if (!(obj->flags & 0x10))
            uranus_pm_obj_exec(w, obj);
        if (!obj->hp)
            continue;
        w->live++;
        if (obj->node[0])
            w->live_a++;
        if (obj->node[1])
            w->live_b++;
        if (obj->node[0] && obj->node[1])
            w->live_ab++;
    }
    {
        UranusObj *cur;
        int n;

        if (cmd_deb_pos_trap) {
            UranusObj *core = w->core;

            bits = *(unsigned int *)&core->pos.x;
            if (bits - 0x800000 > 0x7effffff && bits + 0x7f800000 > 0x7effffff)
                for (;;)
                    ;
            bits = *(unsigned int *)&core->pos.y;
            if (bits - 0x800000 > 0x7effffff && bits + 0x7f800000 > 0x7effffff)
                for (;;)
                    ;
            bits = *(unsigned int *)&core->pos.z;
            if (bits - 0x800000 > 0x7effffff && bits + 0x7f800000 > 0x7effffff)
                for (;;)
                    ;
        }
        if (w->core && w->gauge)
            console2_set_boss_rate((float)w->core->lock.life / (float)cmd_cube_life_x);
        cur = w->objs;
        for (n = 200; n; n--, cur++)
            if (cur->flags & 0x10)
                cur->flags &= ~0x10;
    }
    if (debug_flags_test(4)) {
        if (cmd_auto_break_sw && uranus_map_check_event(w->map, cmd_auto_break_loop_frm)) {
            uranus_pm_objs_scatter(w);
            cmd_auto_break_sw = 0;
        }
        if (kbd_state[7] & 0x44) {
            switch (kbd_state[5]) {
            case 8:
                sound_debug_toggle();
                break;
            case 21:
            case 22:
                w->state = PM_RESET;
                break;
            }
        }
    }
}

/* 0x00caf040 */
void uranus_pm_release(UranusPartWork *w)
{
    int i;

    console2_swap_boss_on(0);
    heap_free(w->pool, 0);
    for (i = 0; i < 200; i++)
        uranus_pm_obj_free(w, &w->objs[i]);
    heap_free(w->objs, 0);
    uranus_core_vars_remove();
    uranus_snake_vars_remove();
    uranus_model_vars_remove();
    uranus_wall_vars_remove();
    uranus_orbit_vars_remove();
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)uranus_pm_vardef);
}

/* steer(cast) */
/* 0x00caf0f8 */
void uranus_pm_disp(UranusPartWork *w)
{
    int cnt0[4], cnt1[4];
    UranusObj *obj;
    float old_lw;
    int old_list, old_blend, i, marked0, marked1;

    ulGxSetTextureList(w->textures);
    old_list = RenderSetCurrentDisplayList(6);
    ulGxSetTextureList(w->textures);
    RenderSetCurrentDisplayList(old_list);
    old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE);
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(-1, 2048);
    ulMatrixPush();
    old_lw = line_width_set(2.0f);
    draw_set_color(color_lerp(cm_uranus_col[0], cm_uranus_col[1], beat_curve(1)));
    if (!cmd_deb_disp) {
        obj = w->objs;
        for (i = 199; i; i--, obj++)
            if (obj->flags)
                uranus_pm_obj_disp(w, obj);
    }
    line_width_set(old_lw);
    ulMatrixPop();
    draw_pop_strip_flags();
    draw_set_override_flags(old_blend);
    if (debug_flags_test(4) && (cmd_deb_disp & 1)) {
        debug_set_origin(uranus_pm_deb_x, uranus_pm_deb_y);
        debug_printf(str_uranus_pm_num_fmt, w->live, w->live_a, w->live_b, w->live_ab);
        marked0 = uranus_node_count_tree(w->node[0], cnt0);
        marked1 = uranus_node_count_tree(w->node[1], cnt1);
        debug_set_origin(uranus_pm_deb_x, uranus_pm_deb_y + 1);
        debug_printf(str_uranus_pm_tree_fmt, cnt0[0], cnt0[1], cnt0[2], cnt1[0], cnt1[1], cnt1[2], marked0, marked1);
        debug_set_origin(uranus_pm_deb_x, uranus_pm_deb_y + 2);
        debug_printf(str_uranus_pm_seq_fmt, w->level, uranus_pm_form_names[w->slot_type[0] % 6], uranus_pm_form_names[w->slot_type[1] % 6], w->hits);
        debug_set_origin(uranus_pm_deb_x, uranus_pm_deb_y + 3);
        debug_printf(str_uranus_pm_rate_fmt, (int)(w->blend * 100.0f));
        duos_plot_debug();
        uranus_pm_deb_g3 = enemy_status_get_ticks();
    }
    if ((cmd_deb_disp & 2) && w->node[0])
        uranus_node_draw_debug(w->node[0]);
    if ((cmd_deb_disp & 4) && w->node[1])
        uranus_node_draw_debug(w->node[1]);
}

/* 0x00caf3d0 */
UranusPartWork *uranus_pm_create(UranusWork *boss)
{
    return task_create_inherit_arg(boss, &uranus_pm_desc, 1436, (int)boss);
}

/* 0x00caf3f8 */
void uranus_pm_get_centre(UranusPartWork *w, Vec3 *out)
{
    UranusObj *obj = w->objs;
    int i;

    out->x = 0.0f;
    out->y = 0.0f;
    out->z = 0.0f;
    for (i = 200; i; i--, obj++)
        v3_sum(out, &obj->pos, out);
    v3_scale(out, 0.005f);
}

/* 0x00caf478 */
void uranus_pm_obj_kick(UranusPartWork *w, UranusObj *obj)
{
    Vec3 v;
    float sc[4];
    int ang;
    UranusBeh *push = &obj->beh[BEH_PUSH];

    v.x = v.y = 0.0f;
    v.z = -cmd_p2_mv;
    ang = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    ROT_X(ang, sc, v.y, v.z);
    ang = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    ROT_Y(ang, (sc + 2), v.x, v.z);
    v3_add(&push->push, &v);
}

/* 0x00caf620 */
void uranus_pm_obj_push_apart(UranusPartWork *w, UranusObj *self, Vec3 *pos)
{
    UranusObj *obj = w->objs;
    float r_sq = cmd_p2_r * cmd_p2_r;
    float len_sq;
    Vec3 dir;
    int i;

    for (i = 200; i; i--, obj++) {
        if (!obj->flags || obj == self)
            continue;
        do {
            v3_sub(&dir, &obj->pos, pos);
        } while (0);
        len_sq = dir.x * dir.x + dir.y * dir.y + dir.z * dir.z;
        if (!(len_sq < r_sq) || fabsf(len_sq) < FLT_EPSILON)
            continue;
        len_sq = 1.0f / sqrtf(len_sq) * (cmd_p2_r - sqrtf(len_sq)) * cmd_p2_mv / cmd_p2_r;
        v3_scale(&dir, len_sq);
        v3_add(&obj->beh[BEH_PUSH].push, &dir);
    }
}

/* 0x00caf810 */
void uranus_pm_beh_home(UranusPartWork *w, UranusObj *obj, UranusBeh *beh)
{
    Vec3 pull;
    float rate;

    beh->ease = beh->ease * (1.0f - cmd_p1_mvr) + cmd_p1_mvr;
    rate = cmd_p1_mv * beh->ease * beh->weight / obj->weight;
    v3_copy(&pull, &w->home[obj->index]);
    v3_sub(&pull, &pull, &obj->base_pos);
    v3_scale(&pull, rate);
    v3_add(&obj->vel, &pull);
}

/* 0x00caf8f8 */
void uranus_pm_beh_wander(UranusPartWork *w, UranusObj *obj, UranusBeh *beh)
{
    Vec3 to_pl;
    float sc[4];
    float dist, rate;
    float y, z;
    Vec3 *v;

    switch (beh->on) {
    case 1:
        y = 0.0f;
        z = cmd_p1_mv;
        beh->push.y = y;
        beh->push.x = y;
        beh->push.z = z;
        ROT_X(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), sc, beh->push.y, beh->push.z);
        ROT_Y(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), (sc + 2), beh->push.x, beh->push.z);
        beh->on++;
    case 2:
        break;
    default:
        return;
    }
    v3_sub(&to_pl, &obj->pos, &w->pl_pos);
    dist = vec3_length(&to_pl);
    if (cmd_col_r2 <= dist) {
        v = &beh->push;
        v->x = -to_pl.x;
        v->y = -to_pl.y;
        v->z = -to_pl.z;
        vec_set_length(v, dist * cmd_p4_mv);
    } else if (cmd_col_r <= dist) {
        v = &beh->push;
        if (0.0f < ulVec3Dot(v, &to_pl)) {
            vec_reflect(v, &to_pl, v);
            vec_set_length(v, cmd_p1_mv);
        }
    }
    rate = beh->weight / obj->weight;
    v3_mul(&to_pl, &beh->push, rate);
    v3_add(&obj->vel, &to_pl);
}

/* 0x00cafbf8 */
void uranus_pm_beh_drift(UranusPartWork *w, UranusObj *obj, UranusBeh *beh)
{
    Vec3 acc;
    float rate = beh->weight / obj->weight;

    v3_mul(&acc, &obj->vel0, rate);
    v3_add(&obj->vel, &acc);
}

/* 0x00cafc70 */
void uranus_pm_beh_centre(UranusPartWork *w, UranusObj *obj, UranusBeh *beh)
{
    Vec3 acc;
    float rate;

    v3_sub(&acc, &w->centre, &obj->pos);
    rate = beh->weight / obj->weight;
    v3_scale(&acc, rate);
    v3_add(&obj->vel, &acc);
}

/* 0x00cafd08 */
void uranus_pm_beh_follow(UranusPartWork *w, UranusObj *obj, UranusBeh *beh)
{
    UranusObj *parent = obj->parent;
    Vec3 acc;
    float rate;

    if (parent) {
        rate = beh->weight / obj->weight;
        v3_mul(&acc, &parent->vel, rate);
        v3_add(&obj->vel, &acc);
    }
}

/* 0x00cafd88 */
void uranus_pm_beh_avoid(UranusPartWork *w, UranusObj *obj, UranusBeh *beh)
{
    UranusObj *parent = obj->parent;
    Vec3 acc;

    if (parent) {
        v3_sub(&acc, &obj->pos, &parent->pos);
        vec_set_length(&acc, cmd_p1_mv * beh->weight / obj->weight);
        v3_add(&obj->vel, &acc);
    }
}

/* 0x00cafe48 */
void uranus_pm_beh_wall(UranusPartWork *w, UranusObj *obj, UranusBeh *beh)
{
    Vec3 hit, nrm;
    UranusPtDir ray;
    float sc[4];
    int hit_type;
    float rate;

    if (!(system_flags & SYS_DISPLAY_PASS)) {
        switch (beh->on) {
        case 1:
            obj->flags &= ~2;
            break;
        case 2:
            ulSinCosf(beh->timer * 1.5707964f / (cmd_hit_col_t * FPS), &sc[0], &sc[1]);
            rate = sc[0];
            if (!v3_is_zero(&beh->push)) {
                v3_mul(&nrm, &beh->push, rate);
                v3_add(&obj->vel, &nrm);
            }
            v3_copy(&ray.p, &obj->hit_pos);
            v3_sum(&ray.dir, &obj->hit_dir, &obj->vel);
            obj->flags &= ~2;
            if (--beh->timer <= 0)
                beh->on = 1;
            break;
        }
        return;
    }
    switch (beh->on) {
    case 1:
        v3_copy(&ray.p, &obj->pos);
        v3_copy(&ray.dir, &obj->vel);
        hit_type = uranus_map_col_check(w->map, &ray.p, &hit.x, &nrm.x, obj->r, 60);
        if (hit_type) {
        hit:
            if (hit_type & 0x3c) {
                v3_copy(&obj->base_pos, &hit);
                beh->on = 1;
            } else {
                v3_copy(&obj->base_pos, &hit);
                beh->on = 2;
                v3_set(&obj->vel, 0.0f, 0.0f, 0.0f);
                obj->flags |= 2;
                v3_sub(&beh->push, &nrm, &obj->hit_dir);
                if (!v3_is_zero(&beh->push)) {
                    v3_scale(&beh->push, cmd_p3_mv);
                    v3_add(&obj->base_pos, &beh->push);
                }
                beh->timer = cmd_hit_col_t * FPS;
            }
            return;
        }
        obj->flags &= ~2;
        break;
    case 2:
        ulSinCosf(beh->timer * 1.5707964f / (cmd_hit_col_t * FPS), &sc[2], &sc[3]);
        rate = sc[2];
        if (!v3_is_zero(&beh->push)) {
            v3_mul(&nrm, &beh->push, rate);
            v3_add(&obj->vel, &nrm);
        }
        v3_copy(&ray.p, &obj->hit_pos);
        v3_sum(&ray.dir, &obj->hit_dir, &obj->vel);
        hit_type = uranus_map_col_check(w->map, &ray.p, &hit.x, &nrm.x, obj->r, 60);
        if (hit_type)
            goto hit;
        obj->flags &= ~2;
        if (--beh->timer <= 0)
            beh->on = 1;
        break;
    }
}

/* 0x00cb03a0 */
void uranus_pm_beh_push(UranusPartWork *w, UranusObj *obj, UranusBeh *beh)
{
    if (!v3_is_zero(&beh->push)) {
        v3_scale(&beh->push, cmd_p2_mvr);
        v3_addto(&obj->vel, &beh->push);
    }
}

/* 0x00cb0460 */
UranusObj *uranus_pm_obj_alloc(UranusPartWork *w)
{
    UranusObj *obj = w->objs;
    int n;

    for (n = 200; n; n--, obj++) {
        if (!obj->flags) {
            memset(obj, 0, sizeof(UranusObj));
            obj->index = 200 - n;
            return obj;
        }
    }
    return 0;
}

/* 0x00cb04d0 */
void uranus_pm_obj_init(UranusObj *obj, UranusPartWork *w, int lvl, Vec3 *pos)
{
    UranusBeh *beh = obj->beh;
    float sc[4];

    beh[BEH_HOME].fn = uranus_pm_beh_home;
    beh[BEH_WANDER].fn = uranus_pm_beh_wander;
    beh[BEH_DRIFT].fn = uranus_pm_beh_drift;
    beh[BEH_CENTRE].fn = uranus_pm_beh_centre;
    beh[BEH_FOLLOW].fn = uranus_pm_beh_follow;
    beh[BEH_AVOID].fn = uranus_pm_beh_avoid;
    beh[BEH_PUSH].fn = uranus_pm_beh_push;
    beh[BEH_WALL].fn = uranus_pm_beh_wall;
    obj->owner = w;
    beh[0].on = 1;
    beh[1].on = 1;
    beh[2].on = 1;
    beh[3].on = 1;
    beh[4].on = 1;
    beh[5].on = 1;
    beh[6].on = 1;
    beh[7].on = 1;
    obj->flags = 1;
    obj->lvl = lvl;
    obj->node[1] = 0;
    obj->node[0] = 0;
    switch (lvl) {
    case 0: {
        int hp = cmd_cube_life_x;

        w->core = obj;
        obj->flags = 9;
        obj->r = cmd_cube_r * 4.0f;
        lock_init(3, &obj->lock, w, &obj->pos, 0, hp, 0);
        break;
    }
    case 1:
        obj->r = cmd_cube_r + cmd_cube_r;
        lock_init(4, &obj->lock, w, &obj->pos, 0, w->cube_hp[0], 0);
        break;
    case 2:
        obj->r = cmd_cube_r;
        lock_init(4, &obj->lock, w, &obj->pos, 0, w->cube_hp[1], 0);
        break;
    }
    lockon_entry(&obj->lock);
    obj->vel.x = obj->vel.y = 0.0f;
    obj->vel.z = cmd_p1_mv;
    ROT_X(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), sc, obj->vel.y, obj->vel.z);
    ROT_Y(DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), (sc + 2), obj->vel.x, obj->vel.z);
    v3_sum(&obj->pos, pos, &obj->vel);
    obj->base_pos = obj->pos;
}

/* 0x00cb0828 */
void uranus_pm_push_from_player(Vec3 *pos, Vec3 *vel, float rad, float push)
{
    Vec3 pl, dir;
    float dist;

    player_get_pos(&pl);
    v3_sub(&dir, pos, &pl);
    dist = vec3_length(&dir);
    if (rad < dist)
        return;
    dir.x = dir.y = push;
    dir.z = 0.0f;
    if (fabsf(dist) < FLT_EPSILON) {
        v3_add(vel, &dir);
        return;
    }
    dist = 1.0f - dist / rad;
    v3_scale(&dir, dist);
    v3_add(vel, &dir);
}

/* 0x00cb0968 */
void uranus_pm_flip_ang(float *ang)
{
    switch ((int)(RAND_FLOAT_B() * 4.0f)) {
    case 0:
        if (fabsf(ang[0]) < FLT_EPSILON)
            ang[0] += 180.0f;
        else if (ang[0] < 0.0f)
            ang[0] -= 180.0f;
        else
            ang[0] += 180.0f;
        break;
    case 1:
        if (fabsf(ang[0]) < FLT_EPSILON || ang[0] < 0.0f)
            ang[0] -= 180.0f;
        else
            ang[0] += 180.0f;
        break;
    case 2:
        if (fabsf(ang[1]) < FLT_EPSILON)
            ang[1] += 180.0f;
        else if (ang[1] < 0.0f)
            ang[1] -= 180.0f;
        else
            ang[1] += 180.0f;
        break;
    case 3:
        if (fabsf(ang[1]) < FLT_EPSILON)
            ang[1] -= 180.0f;
        else if (ang[1] < 0.0f)
            ang[1] -= 180.0f;
        else
            ang[1] += 180.0f;
        break;
    }
}

/* 0x00cb0b20 */
void uranus_pm_objs_attach(UranusPartWork *w, int idx, int lvl, UranusNode *node)
{
    UranusObj *obj;

    if (!lvl) {
        obj = uranus_pm_obj_alloc(w);
        uranus_pm_obj_init(obj, w, 0, (Vec3 *)node->data);
        node->obj = (int)obj;
        obj->node[idx] = node;
    }
    node = node->child;
    if (lvl < 2) {
        for (; node; node = node->next) {
            obj = uranus_pm_obj_alloc(w);
            uranus_pm_obj_init(obj, w, 1, (Vec3 *)node->data);
            node->obj = (int)obj;
            obj->node[idx] = node;
        }
    }
}

/* 0x00cb0bf8 */
UranusNode *uranus_pm_node_match(UranusNode *tree, UranusNode *ref)
{
    int cnt = uranus_node_count_children(ref);
    UranusNode *node;

    for (node = tree->child; node; node = node->next) {
        if (cnt == uranus_node_count_children(node) && !(node->flags & 2)) {
            node->flags |= 2;
            return node;
        }
    }
    return 0;
}

/* 0x00cb0c70 */
void uranus_pm_objs_bind(UranusPartWork *w, int k, int on, UranusNode *n)
{
    UranusObj *obj = w->objs;
    UranusObj *sub;
    UranusNode *other, *node, *tip, *slot_node;
    int i, j;

    i = 200;
    if (!on) {
        for (; i; obj++, i--) {
            if (!obj->flags)
                continue;
            if (k == -1) {
                if (obj->node[0])
                    obj->node[0]->obj = 0;
                if (obj->node[1])
                    obj->node[1]->obj = 0;
                obj->node[1] = 0;
                obj->node[0] = 0;
            } else {
                slot_node = obj->node[k];
                if (slot_node)
                    slot_node->obj = 0;
                obj->node[k] = 0;
            }
        }
        return;
    }
    for (; i; obj++, i--) {
        if (!obj->flags)
            continue;
        switch (obj->lvl) {
        case 0:
            obj->node[k] = n;
            n->obj = (int)obj;
            break;
        case 1:
            other = obj->node[k ^ 1];
            if (!other)
                break;
            node = uranus_pm_node_match(n, other);
            obj->node[k] = node;
            if (!node)
                break;
            sub = w->objs;
            node->obj = (int)obj;
            tip = node->child;
            for (j = 200; j && tip; j--, sub++) {
                if (sub->lvl == 2 && sub->flags && (slot_node = sub->node[k ^ 1]) && slot_node->parent->index == other->index
                    && !sub->node[k]) {
                    sub->node[k]->obj = (int)tip;
                    tip->obj = (int)sub;
                    break;
                }
            }
            break;
        }
    }
    uranus_node_clear_flags(n, 2);
}

/* 0x00cb0eb0 */
void uranus_pm_objs_bind_tree(UranusObj *obj, int slot, UranusNode *n)
{
    int cnt[3];
    int i, lvl;
    UranusNode *child, *grand;

    uranus_node_count_tree(n, cnt);
    slot &= 1;
    i = cnt[0] + cnt[1] + cnt[2];
    child = n->child;
    if (child) {
        grand = child->child;
    } else {
        grand = 0;
        lvl = 0;
    }
    for (; i; obj++, i--) {
        if ((grand || child) && obj->flags) {
            lvl = obj->lvl;
            if (lvl <= 0) {
                obj->node[slot] = n;
                n->obj = (int)obj;
            }
            if (lvl == 2 && grand) {
                obj->node[slot] = grand;
                grand->obj = (int)obj;
                grand = grand->next;
                if (!grand && child) {
                    child = child->next;
                    if (child)
                        grand = child->child;
                }
            }
        }
    }
}

/* 0x00cb0fa8 */
void uranus_pm_obj_split(UranusPartWork *w, UranusObj *obj)
{
    Vec3 vel, pos;
    UranusNode *na, *nb;
    UranusObj *sub;
    int n = 0;
    int cnt, left;

    uranus_map_get_vel_z(&vel);
    na = obj->node[0];
    if (na) {
        cnt = uranus_node_count_children(na);
        na = na->child;
        if (n < cnt)
            n = cnt;
    }
    nb = obj->node[1];
    if (nb) {
        cnt = uranus_node_count_children(nb);
        nb = nb->child;
        if (n < cnt)
            n = cnt;
    }
    if (obj->lvl < 2) {
        if (n > 0) {
            left = n;
            do {
                sub = uranus_pm_obj_alloc(w);
                if (sub) {
                    v3_sub(&pos, &obj->pos, &vel);
                    uranus_pm_obj_init(sub, w, obj->lvl + 1, &pos);
                    sub->flags |= 0x10;
                    uranus_pm_obj_kick(w, sub);
                    uranus_pm_obj_exec(w, sub);
                    if (na) {
                        sub->node[0] = na;
                        na->obj = (int)sub;
                        do {
                            na = na->next;
                        } while (0);
                    }
                    if (nb) {
                        sub->node[1] = nb;
                        nb->obj = (int)sub;
                        nb = nb->next;
                    }
                }
            } while (--left);
        }
        uranus_pm_obj_free(w, obj);
    } else {
        if (obj->node[0])
            obj->node[0]->obj = 0;
        if (obj->node[1])
            obj->node[1]->obj = 0;
    }
}

/* 0x00cb1148 */
void uranus_pm_obj_split_at(UranusPartWork *w, UranusObj *obj)
{
    UranusNode *na, *nb;
    UranusObj *sub;
    int n = 0;
    int cnt, left;

    na = obj->node[0];
    if (na) {
        cnt = uranus_node_count_children(na);
        na = na->child;
        if (n < cnt)
            n = cnt;
    }
    nb = obj->node[1];
    if (nb) {
        cnt = uranus_node_count_children(nb);
        nb = nb->child;
        if (n < cnt)
            n = cnt;
    }
    if (obj->lvl < 2) {
        if (n > 0) {
            left = n;
            do {
                sub = uranus_pm_obj_alloc(w);
                if (sub) {
                    uranus_pm_obj_init(sub, w, obj->lvl + 1, &obj->pos);
                    uranus_pm_obj_kick(w, sub);
                    if (na) {
                        sub->node[0] = na;
                        na->obj = (int)sub;
                        do {
                            na = na->next;
                        } while (0);
                    }
                    if (nb) {
                        sub->node[1] = nb;
                        nb->obj = (int)sub;
                        nb = nb->next;
                    }
                }
            } while (--left);
        }
        uranus_pm_obj_free(w, obj);
    } else {
        if (obj->node[0])
            obj->node[0]->obj = 0;
        if (obj->node[1])
            obj->node[1]->obj = 0;
    }
}

/* 0x00cb12a0 */
void uranus_pm_obj_move(UranusPartWork *w, UranusObj *obj)
{
    UranusNode *na = obj->node[0];
    UranusNode *nb;
    Vec3 v;
    UranusBeh *beh;
    int i;

    if (na) {
        UranusNode *nb = obj->node[1];

        if (nb) {
            v3_lerp(w->home + obj->index, (Vec3 *)nb->data, (Vec3 *)na->data, w->blend);
        } else
            v3_copy(w->home + obj->index, (Vec3 *)na->data);
    } else if ((nb = obj->node[1]))
        v3_copy(w->home + obj->index, (Vec3 *)nb->data);
    else {
        uranus_map_get_vel_z(&v);
        v3_addto(w->home + obj->index, &v);
    }
    for (i = 0, beh = obj->beh; i < 8; i++, beh++)
        if (beh->on && beh->weight && beh->fn)
            beh->fn(w, obj, beh);
    if (!(obj->flags & 0x20))
        uranus_pm_push_from_player(&obj->pos, &obj->vel, cmd_player_r, cmd_player_dv);
    else if (player_sphere_test(&obj->pos, obj->r) == 1)
        uranus_player_damage();
    v3_sum(&obj->pos, &obj->base_pos, &obj->vel);
}

/* 0x00cb1518 */
void uranus_pm_obj_hit(UranusPartWork *w, UranusObj *obj)
{
    Vec3 *pos;
    int hits;

    if (obj->flags & 8) {
        if (w->state == PM_DEAD)
            goto kill;
        switch (w->core_state) {
        case 1:
            w->core_frm += uranus_pm_core_frm_step;
            if (w->core_frm_max < w->core_frm) {
                w->core_frm = w->core_frm_max;
                w->core_state = 2;
                w->core_wait = 0;
            }
            break;
        case 2:
            if (uranus_pm_fire_sec * FPS <= (float)++w->core_wait)
                w->core_state = 3;
            break;
        case 3:
            w->core_frm -= uranus_pm_core_frm_step;
            if (w->core_frm <= 0.0f) {
                w->core_frm = 0.0f;
                w->core_state = 0;
            }
            break;
        }
    }
    if (!(obj->lock.flags & LOCK_HIT))
        goto off;
    hits = obj->lock.life;
    w->hits += obj->hp - hits;
    if (obj->lvl < 2)
        uranus_pm_hit_circle(w, &obj->pos, obj->r);
    if (0 < hits)
        goto hit;
    pos = &obj->pos;
    uranus_pm_obj_push_apart(w, obj, pos);
    v3_addto(&w->kill_sum, pos);
    w->kill_cnt++;
    if (!(obj->flags & 8)) {
        if ((unsigned int)(w->state - PM_FORMS) < 2) {
            boss_se_play(w->form * 2 + 4, (int)&obj->lock, 0);
            if (obj->lvl >= 2) {
                lock_score(4, &obj->lock);
                cm_wire_break_simple(w, 38, w->parts[w->form + 26], pos, &obj->vel, (int *)&obj->ang, cm_uranus_col3);
            } else
                uranus_pm_obj_split(w, obj);
        } else
            obj->lock.life = 1;
        goto done;
    }
    if (w->state != PM_DEAD)
        goto done;
    goto dead;
kill:
    pos = &obj->pos;
dead:
    uranus_end_tex_anime_start(w);
    uranus_cm_create(w, w, pos, obj->ang, w->parts);
    boss_se_play(1, 0, (int)pos);
    boss_vol_play(5, 0);
    TASK_OF(w)->exec = 0;
    TASK_OF(w)->disp = 0;
    w->end = 1;
    uranus_map_start_end(w->map);
    uranus_pm_objs_scatter(w);
    uranus_start_finish(w->boss);
    obj->hp = 0;
    return;
hit:
    if (obj->flags & 8) {
        boss_se_play(0, (int)&obj->lock, 0);
        if (!w->hit_fade)
            w->hit_fade = cm_cfchange_create(w, (BossCfKey *)w->hit_col, uranus_pm_hit_keys, 4, 1.0f, 0.5f);
        obj->flags |= 0x100;
        obj->spin_vel.x += RAND_FLOAT_A() * (cmd_do_core_v + cmd_do_core_v) - cmd_do_core_v;
        obj->spin_vel.y += RAND_FLOAT_A() * (cmd_do_core_v + cmd_do_core_v) - cmd_do_core_v;
        obj->spin_vel.z += RAND_FLOAT_A() * (cmd_do_core_v + cmd_do_core_v) - cmd_do_core_v;
    } else {
        boss_se_play(w->form * 2 + 3, (int)&obj->lock, 0);
        uranus_pm_flip_ang(&obj->spin_to.x);
    }
    goto done;
off:
    if (obj->flags & 8)
        obj->flags &= ~0x100;
done:
    obj->hp = obj->lock.life;
}

/* 0x00cb1970 */
void uranus_pm_obj_exec(UranusPartWork *w, UranusObj *obj)
{
    Vec3 dir, dir_a, dir_b;
    UranusBeh *beh;
    float *wgt;
    int i, n, flags;

    wgt = w->wgt.wgts;
    beh = obj->beh;
    for (i = 0; i < 8; i++)
        beh[i].weight = *wgt++;
    for (n = enemy_status_get_ticks(); n; n--) {
        v3_add(&obj->pos, &w->drift);
        v3_copy(&obj->hit_pos, &obj->pos);
        v3_add(&obj->pos, &w->dir);
        v3_copy(&obj->hit_dir, &w->dir);
        v3_copy(&obj->base_pos, &obj->pos);
        {
            Vec3 *v = &obj->vel;

            v3_copy(&obj->vel0, v);
            v3_set(v, 0.0f, 0.0f, 0.0f);
        }
        obj->weight = 0.0f;
        for (i = 0, beh = obj->beh; i < 8; i++, beh++)
            if (beh->on)
                obj->weight += beh->weight;
        if (!(fabsf(obj->weight) < FLT_EPSILON))
            uranus_pm_obj_move(w, obj);
        if (obj->wait)
            obj->wait--;
        spring_to_v(&obj->spin_vel, &obj->spin, &obj->spin_to, cmd_do_mf, cmd_do_kf);
    }
    uranus_pm_obj_hit(w, obj);
    if (obj->hp) {
        flags = obj->flags;
        if (flags & 0x40) {
            flags = obj->flags = flags | 0x20;
            obj->glow = obj->glow * uranus_pm_glow_rate + obj->r * uranus_pm_glow_scl * (1.0f - uranus_pm_glow_rate);
        }
        if (flags & 0x20)
            if (!(flags & 0x40)) {
                obj->glow *= uranus_pm_glow_damp;
                if (obj->glow < uranus_pm_glow_min) {
                    obj->glow = 0.0f;
                    flags = obj->flags = flags & ~0x20;
                }
            }
    } else
        flags = obj->flags = obj->flags & ~0x60;
    v3_sub(&dir, &obj->pos, &obj->hit_pos);
    if (flags & 8) {
        obj->ang[1] += (int)DEG_TO_ANG(uranus_pm_core_spin);
        obj->ang[2] = 0;
        obj->ang[0] = 0;
    } else {
        if (obj->node[0] && obj->node[1]) {
            if (!v3_is_zero(&dir))
                vec3_normalize(&dir, &dir);
            if (obj->node[0]->flags & 1)
                do {
                    dir_a.x = dir_a.y = 0.0f;
                    dir_a.z = -1.0f;
                } while (0);
            else
                v3_copy(&dir_a, &dir);
            if (obj->node[1]->flags & 1) {
                dir_b.x = dir_b.y = 0.0f;
                dir_b.z = -1.0f;
            } else
                v3_copy(&dir_b, &dir);
            v3_lerp(&dir, &dir_b, &dir_a, w->blend);
        }
        obj->ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        obj->ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    }
    if (w->form != 2) {
        obj->ang[0] += (int)DEG_TO_ANG(obj->spin.x);
        obj->ang[1] += ANG_180 + (int)DEG_TO_ANG(obj->spin.y);
    } else {
        obj->ang[1] += ANG_180;
        obj->ang[2] += (int)DEG_TO_ANG(obj->spin.y);
    }
}

/* steer(cast) */
/* 0x00cb1f90 */
void uranus_pm_obj_disp(UranusPartWork *w, UranusObj *obj)
{
    ModelPart *part;
    float sc[2];
    float rate;
    int old_blend;

    ulMatrixPush();
    {
        Vec3 *v = &obj->pos;

        do {
            vu0_matrix_translate(ulMatrixCur, v->x, v->y, v->z);
        } while (0);
    }
    uranus_part_rotate_yxz(obj->ang);
    vu0_matrix_scale(ulMatrixCur, uranus_pm_obj_scl, uranus_pm_obj_scl, uranus_pm_obj_scl);
    if (obj->flags & 8) {
        if (fabsf(w->core_frm) < FLT_EPSILON) {
            old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            draw_push_override_color();
            draw_push_strip_flags();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(-1, 2048);
            draw_set_override_color(w->hit_col);
            dc_object_draw((struct MapPart *)w->parts[24]);
            draw_pop_override_color();
            draw_pop_strip_flags();
            draw_set_override_flags(old_blend);
            dc_object_draw((struct MapPart *)w->parts[25]);
        } else {
            old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            draw_push_override_color();
            draw_push_strip_flags();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(-1, 2048);
            draw_set_override_color(w->hit_col);
            model_object_draw((struct DcObjectTreeView *)w->parts[24], w->core_frm);
            draw_pop_override_color();
            draw_pop_strip_flags();
            draw_set_override_flags(old_blend);
            part = w->parts[25];
            model_node_walk_list_anim(part->root, part->clip, w->core_frm, part->wire_list);
            if (obj->hp) {
                int old_list = RenderSetCurrentDisplayList(6);
                void *fg;

                ulGxSetTextureList(w->textures);
                rate = w->core_frm / w->core_frm_max;
                ulSinCosf(rate * PI, &sc[0], &sc[1]);
                rate = sc[1] * -0.5f + 0.5f;
                fg = fog_set_enable(0);
                draw_set_blend(4, 1);
                flare_draw_here(2, FLARE_COL, obj->r * rate);
                fog_set_enable(fg);
                RenderSetCurrentDisplayList(old_list);
            }
        }
    } else {
        if (obj->hp) {
            if (cmd_test_fast)
                part_draw_culled(w->parts[w->form * 6 + obj->lvl * 2]);
            else
                dc_object_draw((struct MapPart *)w->parts[w->form * 6 + obj->lvl * 2]);
        }
        draw_set_blend(4, 5);
        dc_object_draw((struct MapPart *)w->parts[w->form * 6 + obj->lvl * 2 + 1]);
    }
    if (obj->flags & 0x20) {
        int old_list = RenderSetCurrentDisplayList(6);
        void *fg;

        ulGxSetTextureList(w->textures);
        fg = fog_set_enable(0);
        draw_set_blend(4, 1);
        flare_draw_here(2, FLARE_COL, obj->glow);
        fog_set_enable(fg);
        RenderSetCurrentDisplayList(old_list);
    }
    ulMatrixPop();
}

/* 0x00cb22e0 */
void uranus_pm_obj_free(UranusPartWork *w, UranusObj *obj)
{
    if (obj->flags & 1)
        lock_remove_entry(4, &obj->lock);
    if (obj->task) {
        do {
            FINISH(obj->task);
        } while (0);
        obj->task = 0;
    }
    if (obj->node[0])
        obj->node[0]->obj = 0;
    if (obj->node[1])
        obj->node[1]->obj = 0;
    obj->flags = 0;
}

/* 0x00cb2350 */
void uranus_pm_set_lockable(UranusPartWork *w, int on)
{
    UranusObj *obj = w->objs;
    int i;

    for (i = 200; i; i--, obj++) {
        if (obj->flags & 1) {
            if (!on)
                obj->lock.flags &= ~LOCK_LOCKABLE;
            else
                obj->lock.flags |= LOCK_LOCKABLE;
        }
    }
}

/* 0x00cb2398 */
void uranus_pm_set_layout(UranusPartWork *w, UranusPmLayout *wgts, float sec)
{
    int n = sec * FPS;

    w->wgt_total = w->wgt_cnt = n;
    w->wgt_to = *wgts;
    w->wgt_from = w->wgt;
    if (!w->wgt_cnt)
        w->wgt = *wgts;
}

/* 0x00cb2548 */
int uranus_pm_get_core(UranusPartWork *w, Vec3 **pos, int **ang)
{
    UranusObj *core = w->core;

    if (pos)
        *pos = &core->pos;
    if (ang)
        *ang = core->ang;
    if (!core)
        return -1;
    return (core->flags & 0x100) | w->sw_delay;
}

/* 0x00cb2588 */
struct UranusMapWork *uranus_pm_get_map(UranusPartWork *w)
{
    return w->map;
}

/* 0x00cb2590 */
Vec3 *uranus_pm_get_obj0_pos(UranusPartWork *w)
{
    return &w->objs->pos;
}

/* 0x00cb25a0 */
void uranus_pm_set_mode(UranusPartWork *w, int mode)
{
    w->mode = mode;
}

/* 0x00cb25a8 */
void uranus_pb_exec(UranusPbWork *w)
{
    Vec3 drift, nrm, hit;
    Vec3 *vel = &w->vel;

    uranus_map_get_vel_z(&drift);
    v3_addto(&w->pos, &drift);
    v3_scale(vel, uranus_pb_drag);
    w->vel.y += uranus_pb_grav;
    v3_addto(&w->pos, vel);
    i3_sum(w->ang, w->ang, w->spin);
    if (uranus_map_col_check(w->map, &w->pos, &hit.x, &nrm.x, w->r, 49)) {
        v3_copy(&w->pos, &hit);
        vec_reflect(vel, &nrm, vel);
    }
    if (--w->life <= 0)
        FINISH_IF(w);
}

/* 0x00cb2740 */
void uranus_pb_disp(UranusPbWork *w)
{
    Vec3 *pos = &w->pos;
    int old_blend;

    ulGxSetTextureList(w->textures);
    ulMatrixPush();
    old_blend = draw_set_override_flags(0);
    draw_set_blend(4, 1);
    if (view_sphere_visible(pos, w->r)) {
        MTX_TRANS(pos);
        uranus_part_rotate_yxz(w->ang);
        draw_set_color((cm_uranus_col[2] & 0xffffff) |
                      ((color_get_a(cm_uranus_col[2]) * w->life / w->life_max + 1) >> 1 << 24));
        dc_object_draw((struct MapPart *)w->model);
    }
    draw_set_override_flags(old_blend);
    ulMatrixPop();
}

/* 0x00cb2830 */
UranusPbWork *uranus_pb_create(UranusPartWork *parent, struct UranusMapWork *map, ModelPart *model, Vec3 *pos, int *ang, Vec3 *vel, float rad)
{
    UranusPbWork *w = task_create_inherit(parent, &uranus_pb_desc, 76);
    Vec3 kick;
    float sc[4];
    float z;
    int ang_rnd, life;

    w->r = rad;
    v3_copy(&w->pos, pos);
    w->map = map;
    w->model = model;
    v3_copy(&w->vel, vel);
    i3_copy(w->ang, ang);
    do {
        w->spin[0] = DEG_TO_ANG(RAND_FLOAT_A() * 16.0f + -8.0f);
    } while (0);
    w->spin[1] = DEG_TO_ANG(RAND_FLOAT_A() * 16.0f + -8.0f);
    do {
        w->spin[2] = DEG_TO_ANG(RAND_FLOAT_A() * 16.0f + -8.0f);
    } while (0);
    kick.z = uranus_pb_kick + RAND_FLOAT_B() * uranus_pb_kick_rnd;
    z = 0.0f;
    kick.y = z;
    kick.x = z;
    ang_rnd = DEG_TO_ANG(RAND_FLOAT_B() * 360.0f - 180.0f);
    ROT_X(ang_rnd, sc, kick.y, kick.z);
    ang_rnd = DEG_TO_ANG(RAND_FLOAT_B() * 360.0f - 180.0f);
    ROT_Y(ang_rnd, (sc + 2), kick.x, kick.z);
    v3_add(&w->vel, &kick);
    life = (uranus_pb_sec + RAND_FLOAT_B() * uranus_pb_sec_rnd) * FPS;
    w->life = w->life_max = life;
    return w;
}

/* 0x00cb2b88 */
void uranus_pm_objs_scatter(UranusPartWork *w)
{
    UranusObj *obj = w->objs;
    int i;

    for (i = 200; i; i--, obj++) {
        if ((obj->flags & 1) && obj->lvl) {
            uranus_pb_create(w, w->map, w->parts[w->form * 6 + obj->lvl * 2 + 1], &obj->pos, obj->ang,
                             &obj->vel, obj->r);
            uranus_pm_obj_free(w, obj);
        }
    }
}

/* 0x00cb2c40 */
void uranus_pm_obj_set_glow(UranusObj *obj, int on)
{
    if (on)
        obj->flags |= 0x40;
    else
        obj->flags &= ~0x40;
}

/* 0x00cb2c68 */
int uranus_pm_obj_get_hp(UranusObj *obj)
{
    if (obj->flags & 1)
        return obj->hp;
    return 0;
}

/* 0x00cb2c88 */
void uranus_pm_hit_circle(void *parent, Vec3 *pos, float rad)
{
    EffCircleArg arg;
    Vec3 scr;

    scroll_relative(pos, &scr);
    arg.pos = &scr;
    arg.radius = rad * uranus_pm_hit_circle_scl;
    arg.color = console_get_pcol();
    arg.width = 0.5f;
    arg.frames = 32;
    arg.fade = 32;
    arg.n = 16;
    eff_circle_create(parent, &arg);
}

/* 0x00cb2d18 */
void uranus_part_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
