/*
 * Mars boss: the core appears, then the flower, wall and tree attacks, a swaying last phase and
 * the defeat; fog, lights and music blend between phases. Also the sway task that turns the boss
 * in the last phase and the bounds its debris bounces off.
 */

#include "types_gw.h"
#include "b_mars.h"
#include "mathdef.h"
#include "camera_demo.h"
#include "debug.h"
#include "enemy_status.h"
#include "fade.h"
#include "fog.h"
#include "gw_varedit.h"
#include "libc.h"
#include "light.h"
#include "model.h"
#include "nj_math.h"
#include "node_draw_state.h"
#include "pad.h"
#include "pad_vib.h"
#include "player.h"
#include "player_status.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "mars.h"
#include "mars_bullet.h"
#include "mars_color.h"
#include "mars_core.h"
#include "mars_tree.h"
#include "mars_wall.h"
#include "../common/boss_common.h"
#include "../common/boss_demo_mes.h"
#include "../common/boss_sound.h"
#include "../common/boss_texture_anime.h"
#include "../common/boss_wire.h"

extern Pad *pad_main;
extern unsigned int stage_exit_flags;
extern BossStarCtl *star_ctl;
extern unsigned int star_color;
extern struct TaskDesc mars_desc;
extern int CMD_PLAYER_DAMAGE;
extern struct MarsWork *mars_work;
extern int *mars_fog_table[];

extern char mars_vardef[];
extern struct TaskDesc mars_ak_act_desc;
extern float mars_ak_act_start_sec;
extern float mars_ak_act_turn_sec;
extern const float mars_motion_table[][2];
extern char load_files[][8];
extern char str_mars[];
extern GwVarDef mars_vardefs[];
extern int mars_vars_group;

typedef struct MarsKey {        /* key_table_lerp key, 16 bytes */
    char pad_00[4];
    float x, y;                 /* 0x04: the values */
    float key;                  /* 0x0c: decreasing along the table */
} MarsKey;

extern int CMD_START_SEQ;
extern int CMD_NEXT_SEQ;
extern int mars_wire_pairs[];
extern float mars_amp_xy;
extern float mars_amp_z;
extern float mars_wobble_spd;

extern float mars_bounds_r2;
extern float mars_bounds_z2;
extern BossModelDef model_defs[];
extern float mars_part_radii[];
extern float mars_part23_scl;
extern const float mars_part24_scl;
extern float mars_part24_sub_scl;
extern float mars_pair_ofs;
extern float mars_pair_min_frm;
extern float CMD_PS2_FONG_N;
extern float CMD_PS2_FONG_F;
extern float mars_bounds_zlim;
extern float CMD_BOSS_RZ;
extern float mars_wobble_lead;
extern float mars_aim_dist;
extern float mars_face_ease;
extern float mars_ak_act_spd_a;
extern float mars_ak_act_spd_b;
extern float mars_ak_act_amp_lo;
extern float mars_ak_act_amp_hi;
extern float mars_ak_act_toggle_sec;
extern float mars_ak_act_turn_time;
extern float mars_ak_act_amp_ease;
extern float mars_ak_act_turn_y;
extern float mars_ak_act_turn_x;
extern float mars_ak_act_start_scl;
extern float mars_amp_xy_b;
extern float mars_amp_z_b;
extern float mars_amp_xy_c;
extern float mars_amp_z_c;
extern const float APP_TIME;
extern float APP_CORE_TIME;
extern const float APP_LEN;
extern float FLOWER_TIME;
extern float EAT_TIME;
extern float CMD_FADE_TIME;
extern float DEAD_TIME;
extern float BOSS_LEN;
extern float BOSS_FLOWER_LEN;
extern float BOSS_LEN_RATE;
extern float BOSS_YPOS_RATE;
extern float BOSS_YPOS_RATE2;
extern float BOSS_YPOS;
extern float mars_wait_sec_a;
extern const float mars_wait_sec_b;
extern float mars_shake_a;
extern float mars_shake_b;
extern const float CMD_BS_TIME;
extern float CMD_DT_TIME;
extern unsigned int mars_star_col2;
extern unsigned int cmEyeWireCol;
extern unsigned int cmCoreWB;
extern int mars_level_files[][2];

typedef struct MarsSway {      /* sway task (mars_ak_act_exec) */
    int start_cnt, toggle_cnt, active, turn_cnt, turn_frm, flip;
    float phase_a, phase_b, amp;           /* 0x18: two phases and a radius */
    int ang[3];                 /* 0x24 */
    int add[3];                 /* 0x30 */
    int turn_x, turn_y;               /* 0x3c */
} MarsSway;

/* 0x00ca0700 */
void star_color_apply(void)
{
    if (star_ctl)
        boss_star_set_color(star_ctl, star_color);
}

/* 0x00ca0720 */
int boss_main_demo_cb(MarsWork *w)
{
    if (w->demo_state == 0)
        w->demo_state = 1;
    else if (w->demo_state == 2)
        return 1;
    return 0;
}

/* 0x00ca0750 */
void mars_exec_start(MarsWork *w)
{
    if (w->demo_state == 1) {
        w->map = (char *)mars_map_create((char *)w, w->level);
        TASK_OF(w)->exec = (void (*)(void *))mars_exec_battle;
        TASK_OF(w)->disp = (void (*)(void *))mars_disp;
        boss_star_fade(w->star, star_color, 120);
        boss_adx_request();
        w->start_wait = w->demo_state = 2;
    }
}

/* steer(cast) */
/* 0x00ca07d0 */
void mars_init(MarsWork *w, int *arg)
{
    w->arg0 = arg[0];
    w->level = arg[1];
    w->phase = 0;
    w->demo_state = 0;
    boss_files_request((int)load_files[0], (int)load_files[1]);
    boss_start((int *)w, (int)w->parts, w->arg0, w->level);
    boss_bgm_start(w);
    cm_se_laser_create(load_files[2]);
    mars_1st_demo_create(w, (int)boss_main_demo_cb, (int)w);
    boss_demo_mes_create(w, str_mars);
    mars_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, mars_vardef);
    gw_varedit_add_table(&gw_varedit_root, mars_vars_group, mars_vardefs);
    mars_work = w;
    mars_score_vars_add();
    mars_wall_vars_add();
    mars_tree_vars_add();
    mars_bullet_vars_add();
    mars_demo_vars_add();
}

/* 0x00ca08d0 */

static inline void iv3_set(IVec3 *d, int x, int y, int z)
{
    d->x = x;
    d->y = y;
    d->z = z;
}
void mars_start(char *w, int restart)
{
    MarsCoreArg arg;
    float amp_xy, amp_z, wob_spd;

    {
        char *rot = w + 36;

        *(int *)rot = 0;
        *(int *)(rot + 4) = 0;
        *(int *)(rot + 8) = 0;
    }
    amp_xy = mars_amp_xy;
    i3_set((int *)(w + 48), 0, 0, 0);
    amp_z = mars_amp_z;
    iv3_set((IVec3 *)(w + 60), 0, 0, 0);
    *(float *)(w + 6312) = 0.0f;
    iv3_set((IVec3 *)(w + 72), 0, 0, 0);
    wob_spd = mars_wobble_spd;
    *(int *)(w + 6348) = 0;
    *(int *)(w + 6332) = 0;
    *(float *)(w + 108) = *(float *)(w + 112) = amp_xy;
    *(float *)(w + 116) = amp_z;
    *(float *)(w + 6316) = wob_spd;
    *(int *)(w + 7220) = 0xffff;
    *(float *)(w + 128) = -1.0f;
    ((union { float f; int i; } *)(w + 6364))->i = 0; /* steer(alias) */
    *(float *)(w + 6408) = 0.0f;
    *(float *)(w + 136) = 0.0f;
    *(float *)(w + 6320) = *(float *)(w + 120) = *(float *)(w + 124) = *(float *)(w + 6312);
    *(int *)(w + 144) = 0;
    *(int *)(w + 6428) = 0;
    if (restart) {
        arg.home = (Vec3 *)(w + 24);
        arg.boss_ang = (int *)(w + 36);
        arg.parts = w + 6056;
        arg.body = *(ModelPart **)(w + 6112);
        arg.shell = *(ModelPart **)(w + 6116);
        arg.dust_part = *(ModelPart **)(w + 6056);
        arg.boss = w;
        *(void **)(w + 6452) = mars_c_create(w, &arg);
    }
    mars_motion_start(w, 0, -1);
    *(int *)(w + 132) = 6;
}

/* 0x00ca09f8 */
void mars_exec(MarsWork *w)
{
    FogArg arg;
    BossStarCtl *star;
    BossStarCtl **star_slot;

    if (boss_files_ready(load_files[0], load_files[1])) {
        w->pack = model_pnb_load(load_files[0]);
        w->textures = model_load_timed(load_files[1], w->arg0);
        do {
        } while (0);
        boss_parts_build((void **)w, model_defs, w->parts, w->pool);
        star_slot = &star_ctl;
        parts_set_radii(w->parts, mars_part_radii);
        part_radius_default(w->parts[6]);
        part_radius_default(w->parts[26]);
        mars_b_br_verts_init();
        w->parts[24]->speed *= mars_part23_scl;
        w->parts[26]->speed = mars_part24_scl;
        ((WireObj *)w->parts[26]->root)->scl[2] = mars_part24_sub_scl;
        mars_collect_points(((WireObj *)w->parts[25]->root)->child, w->points, 4);
        w->parts[3]->frames -= 1.0f;
        TASK_OF(w)->exec = (void (*)(void *))mars_exec_start;
        w->unk_9c = 0;
        w->anim_flags = 0;
        w->unk_18dc = 0;
        w->parts[4]->frames -= 1.0f;
        mars_start(w, 1);
        w->vcc = mars_vcc_man_create(w, (void **)w->parts[23], (int)&w->pos, (int)w->ang);
        unit_circle_build();
        arg.range = 400.0f;
        arg.k = 0.083333336f;
        arg.count = 200;
        arg.color = star_color & 0xffffff;
        star = boss_star_create(w, &arg, 0.0f);
        w->star = star;
        *star_slot = star;
    }
}

/* 0x00ca0bc0 */
void mars_restart(MarsWork *w)
{
    w->unk_9c = 0;
    w->anim_flags = 0;
    w->flip = 0;
    if (w->flower) { FINISH_IF(w->flower); w->flower = 0; }
    if (w->attack) { FINISH_IF(w->attack); w->attack = 0; }
    if (w->sway) { FINISH_IF(w->sway); w->sway = 0; }
    if (w->sbm) { FINISH_IF(w->sbm); w->sbm = 0; }
    mars_c_set_state(w->core, 7);
    mars_start(w, 0);
    w->tree_flag = 0;
    boss_parts_reset_time(w->parts);
}

/* 0x00ca0c88 */
void mars_release(MarsWork *w)
{
    light_reset();
    light_fog_set();
    cm_bgm_sound_man_end(0);
    if (boss_parts_release(w->parts)) {
        model_unload_timed(w->textures);
        model_pnb_unload(w->pack);
        do {
            w->textures = 0;
            w->pack = 0;
        } while (0);
    }
    boss_end(w);
    mars_score_vars_remove();
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)mars_vardef);
    mars_wall_vars_remove();
    mars_tree_vars_remove();
    mars_bullet_vars_remove();
    mars_demo_vars_remove();
}

/* 0x00ca0d28 */
void mars_vec_ease(Vec3 *vec, float x, float y, float z)
{
    float t = mars_amp_ease;
    float inv_t = 1.0f - t;

    vec->x = vec->x * t + inv_t * x;
    vec->y = vec->y * t + inv_t * y;
    vec->z = vec->z * t + inv_t * z;
}

/* 0x00ca0d80 */
void mars_bgm_next_cb(MarsWork *w)
{
    int v;

    boss_bgm_seq_get(w->bgm_idx, 0, &v);
    boss_vol_play(v, 0);
    w->bgm_idx += 1;
    if (w->bgm_idx >= 7)
        w->bgm_idx = 7;
    w->timer = 0;
    peri_act_play_boss_break();
}

/* 0x00ca0de0 */
void mars_bgm_next(MarsWork *w)
{
    int v;

    w->timer = 1;
    boss_bgm_seq_get(w->bgm_idx, &v, 0);
    boss_bgm_switch((int)mars_bgm_next_cb, (int)w, v);
}

/* 0x00ca0e30 */
void mars_phase_step(MarsWork *w)
{
    Vec3 v;
    void *pa;
    void *pb;
    int *ent;
    float z;
    float t;
    int state;

    switch (w->phase) {
    case 8:
        w->dist *= BOSS_LEN_RATE;
        if (--w->timer <= 0)
            w->phase = w->phase_next;
        break;
    case 6:
        z = 0.0f;
        w->flip = 0;
        mars_fog_change(w, 0, 1, z);
        ent = mars_attack_start(w, 0);
        w->vcc_alpha = z;
        w->ypos = z;
        w->cycle = 0;
        w->debris_bounded = 0;
        w->bgm_idx = 0;
        w->unk_1f5c = 0;
        w->flower = ent;
        w->dist = APP_LEN;
        w->phase = 7;
        w->timer_len = w->timer = (int)(APP_TIME * FPS);
        w->fog_id = 21;
        w->ypos_scl = 1.0f;
        mars_vec_ease(&w->amp, mars_amp_xy_c, mars_amp_xy_c, mars_amp_z_c);
        break;
    case 7:
        t = (float)--w->timer / (float)w->timer_len;
        w->dist = APP_LEN * t * t;
        if (w->timer <= 0) {
            mars_c_set_state(w->core, 8);
            w->phase = 9;
            w->dist = 0.0f;
            w->timer_len = w->timer = (int)(APP_CORE_TIME * FPS);
        }
        break;
    case 9:
        if (--w->timer <= 0) {
            mars_fl_command(w->flower, 1);
            mars_fl_command(w->flower, 5);
            w->vcc_alpha = 0.0f;
            w += ((int)w & 1) >> 1; /* steer(zero) */
            w->phase = 10;
            w->timer = w->timer_len = (int)(FLOWER_TIME * FPS);
        }
        break;
    case 10:
        w->dist = (float)(w->flip * -2 + 1) * BOSS_FLOWER_LEN * (1.0f - BOSS_LEN_RATE) + w->dist * BOSS_LEN_RATE;
        w->ypos = w->ypos * BOSS_YPOS_RATE2 + BOSS_YPOS * w->ypos_scl * (1.0f - BOSS_YPOS_RATE2);
        if (mars_c_get_state(w->core) == 1)
            mars_vec_ease(&w->amp, mars_amp_xy_c, mars_amp_xy_c, mars_amp_z_c);
        if (--w->timer <= 0 || !mars_fl_any_busy(w->flower) || w->skip) {
            w->skip = 0;
            w->phase = 11;
            mars_bgm_next(w);
        }
        break;
    case 11:
        if (w->cycle)
            w->ypos *= BOSS_YPOS_RATE;
        mars_vec_ease(&w->amp, mars_amp_xy, mars_amp_xy, mars_amp_z);
        if (w->timer <= 0) {
            mars_fl_command(w->flower, 0);
            w->phase = 12;
            w->timer = w->timer_len = (int)(EAT_TIME * FPS);
        }
        break;
    case 12:
        if (w->cycle)
            w->ypos *= BOSS_YPOS_RATE;
        mars_vec_ease(&w->amp, mars_amp_xy, mars_amp_xy, mars_amp_z);
        w->dist *= BOSS_LEN_RATE;
        w->debris_bounded = 1;
        if (--w->timer > 0)
            break;
        if (w->cycle == 0) {
            w->fog_id = 20;
        } else {
            w->fog_id = 21;
            if (w->cycle == 3)
                w->unk_1f5c = 1;
        }
        w->anim_flags = 0x4000000;
        mars_fog_change(w, 0, w->fog_id, CMD_FADE_TIME);
        w->phase = 13;
        w->timer = w->timer_len = (int)(CMD_FADE_TIME * FPS);
    case 13:
        mars_vec_ease(&w->amp, mars_amp_xy, mars_amp_xy, mars_amp_z);
        w->dist *= BOSS_LEN_RATE;
        if (--w->timer <= 0) {
            mars_fl_command(w->flower, -1);
            w->flower = 0;
            w->phase = 14;
        }
        break;
    case 14:
        w->dist *= BOSS_LEN_RATE;
        mars_vcc_start(w->vcc, mars_c_get_pos(w->core), 0);
        if (w->cycle == 1) {
            w->phase = 18;
            w->cycle = 2;
        } else if (w->cycle == 3) {
            w->phase = 18;
            w->cycle = 4;
        } else {
            w->phase = 15;
            mars_map_set_visible(w->map, 0);
            boss_star_fade(w->star, mars_star_col2, 120);
        }
        break;
    case 15:
        if (w->timer <= 0) {
            mars_vec_ease(&w->amp, mars_amp_xy, mars_amp_xy, mars_amp_z);
            w->phase = 16;
            w->tree_flag = 1;
            if (w->cycle == 0) {
                w->cycle = 1;
                w->attack = mars_attack_start(w, 3);
                w->ypos_scl = -1.0f;
            } else {
                w->cycle = 3;
                w->attack = mars_attack_start(w, 6);
                w->rounds += 1;
            }
        }
        break;
    case 16:
        mars_vec_ease(&w->amp, mars_amp_xy, mars_amp_xy, mars_amp_z);
        if (w->skip) {
            w->skip = 0;
        } else if (w->cycle == 1) {
            if (mars_wall_man_is_active(w->attack))
                break;
        } else if (w->attack && !(TASK_OF(w->attack)->flags & TASK_F_END)) {
            break;
        }
        w->phase = 17;
        mars_bgm_next(w);
        break;
    case 17:
        if (w->timer <= 0) {
            w->attack = 0;
            w->flower = mars_attack_start(w, 1);
            mars_fog_change(w, 0, 1, CMD_FADE_TIME);
            w->timer = w->timer_len = (int)(CMD_FADE_TIME * FPS);
            w->phase = 21;
            mars_map_set_visible(w->map, 1);
            boss_star_fade(w->star, star_color, 120);
        }
        break;
    case 18:
        w->ypos *= BOSS_YPOS_RATE;
        player_get_pos(&v);
        v3_sub(&v, &v, (Vec3 *)mars_c_get_pos(w->core));
        vec_set_length(&v, 1.0f);
        cm_wire_break_set_cb(cm_wire_break_simple(w, 13, w->parts[19], (Vec3 *)mars_c_get_pos(w->core), &v, w->ang, cmEyeWireCol), (void (*)(void *, Vec3 *, Vec3 *))mars_bounds_reflect);
        boss_se_play(4, 0, (int)mars_c_get_pos(w->core));
        w->pair_hidden += 1;
        w->tree_flag = 1;
        if (w->rounds) {
            w->anim_flags |= 0x1000000;
            mars_fog_change(w, 0, 24, CMD_FADE_TIME);
            mars_c_set_state(w->core, 6);
        } else {
            mars_c_set_state(w->core, 3);
        }
        w->phase = 19;
        break;
    case 19:
        w->ypos = w->ypos * BOSS_YPOS_RATE * BOSS_YPOS_RATE;
        mars_vec_ease(&w->amp, mars_amp_xy, mars_amp_xy, mars_amp_z);
        state = mars_c_get_state(w->core);
        if (state == 1 || state == 2)
            w->phase = 20;
        break;
    case 20:
        if (w->rounds > 0) {
            w->phase = 22;
            mars_bgm_next(w);
        } else {
            w->phase = 14;
            w->flip ^= 1;
            mars_bgm_next(w);
        }
        break;
    case 21:
        if (--w->timer <= 0) {
            mars_fl_command(w->flower, 4);
            mars_fl_command(w->flower, 3);
            w->phase = 10;
            w->debris_bounded = 0;
            w->vcc_alpha = 0.0f;
            w->timer = w->timer_len = (int)(FLOWER_TIME * FPS);
        }
        break;
    case 22:
        if (w->timer > 0)
            break;
        mars_map_set_visible(w->map, 0);
        boss_star_fade(w->star, mars_star_col2, 120);
        oscilate_create(mars_shake_a, mars_shake_b);
        w->ypos = BOSS_YPOS_RATE;
        mars_c_set_state(w->core, 2);
        mars_c_trail_start(w->core, 1);
        w->sway = mars_ak_act_create(w, w->flip);
        w->phase = 23;
        w->timer = 1;
    case 23: {
        int *ang;

        w->ypos = BOSS_YPOS_RATE;
        ang = mars_get_ang(w->sway);
        w->ang[0] = ang[0];
        w->ang[1] = ang[1];
        state = mars_c_life_level(w->core);
        if ((w->timer && --w->timer <= 0 && !w->sbm) || (!w->sbm && state == 2)) {
            w->sbm = mars_sbm_create(w, mars_c_get_pos(w->core), (int)w->parts[32], (int)w->parts[33], (int)w->parts[34], (int)&w->sbm_stop);
            w->phase = 24;
            w->timer = (int)(CMD_BS_TIME * FPS);
        }
        if (state < 0)
            w->phase = 24;
        break;
    }
    case 24: {
        int *ang = mars_get_ang(w->sway);
        w->ang[0] = ang[0];
        w->ang[1] = ang[1];
        if (--w->timer <= 0) {
            w->phase = 25;
            w->timer = (int)(CMD_DT_TIME * FPS);
        }
        if (mars_c_life_level(w->core) < 0) {
            w->phase = 26;
            boss_vol_play(5, 0);
            boss_pcm_load(0);
        }
        break;
    }
    case 25: {
        int *ang = mars_get_ang(w->sway);
        w->ang[0] = ang[0];
        w->ang[1] = ang[1];
        if (mars_c_life_level(w->core) < 0) {
            w->phase = 26;
            boss_vol_play(5, 0);
            boss_pcm_load(0);
            if (boss_get_scoring())
                boss_demo_mes2_start(w, str_mars, 0);
            else
                boss_demo_mes2_start(w, str_mars, 2);
        } else if (--w->timer <= 0) {
            boss_scoring_on(0);
            mars_c_trail_start(w->core, -1);
        }
        break;
    }
    case 26:
        if (!beat_grid_hit(0) || (beat_clock_get_haku() & 3))
            break;
        mars_c_get_pose(w->core, &pa, &pb);
        cm_obj_mot_create(w, (Vec3 *)pa, (int *)pb, w->parts[36], 1, 0.2f, 0.2f);
        mars_map_set_visible(w->map, 1);
        boss_star_fade(w->star, star_color, 120);
        boss_pcm_start();
        w->phase = 27;
        mars_fog_change(w, 0, 3, CMD_FADE_TIME);
        w->timer = (int)(CMD_FADE_TIME * FPS + DEAD_TIME * FPS);
        FINISH_IF(w->sway);
        w->sway = 0;
        FINISH_IF(w->sbm);
        w->sbm = 0;
        ent = mars_level_files[(unsigned int)player_status_get_level() % 6];
        if (ent[1])
            tex_anime_start(w, ent, boss_get_arg0(), 120);
        else
            tex_anime_start(0, 0, 0, 0);
    case 27:
        if (--w->timer <= 0) {
            w->timer = (int)(mars_wait_sec_a * FPS);
            w->phase = 28;
            w->anim_flags |= 0x18;
            tex_anime_fade_in_out();
        }
        break;
    case 28:
        if (--w->timer <= 0 && beat_grid_hit(0) && !(beat_clock_get_haku() & 3)) {
            peri_act_play_boss_finish();
            lock_score_level(0, (Vec3 *)mars_c_get_pos(w->core));
            if (boss_get_scoring())
                boss_demo_mes2_start(w, str_mars, 1);
            else
                boss_demo_mes2_start(w, str_mars, 3);
            oscilate_create(mars_shake_a, mars_shake_b);
            mars_c_set_state(w->core, 0);
            cm_wire_break_simple(w, 5, w->parts[16], (Vec3 *)mars_c_get_pos(w->core), 0, w->ang, cmCoreWB);
            cm_wire_mdl_break_part(w, 5, w->parts[4], &w->pos, 0, w->ang, cmCoreWB);
            mars_fog_change(w, 0, 0, 0.0f);
            w->phase = 29;
            w->timer = (int)(mars_wait_sec_b * FPS);
            fade_start(2, 0, 30);
        }
        break;
    case 29:
        if (--w->timer <= 0) {
            fade_start_out(60);
            ((void (*)(char *))boss_fx_release)(w);
            stage_exit_flags |= 0xa;
            TASK_OF(w)->exec = 0;
        }
        break;
    case 30:
        break;
    case 31:
        w->anim_flags = 0x4000000;
        mars_fog_change(w, 0, 20, 0.0f);
        w->phase = 32;
        mars_c_set_state(w->core, 1);
        if (pad_main->menu.on & PAD_X)
            w->sway = mars_ak_act_create(w, w->flip);
    case 32:
        w->dist = BOSS_LEN;
        w->parts[26]->speed = mars_part24_scl;
        w->ang[2] = 0;
        w->dang[0] = 0;
        if (w->cycle == 1) {
            float t_ = mars_amp_ease;
            float s_ = 1.0f - t_;

            (&w->amp)->x = (&w->amp)->x * t_ + s_ * mars_amp_xy_b;
            (&w->amp)->y = (&w->amp)->y * t_ + s_ * mars_amp_xy_b;
            (&w->amp)->z = (&w->amp)->z * t_ + s_ * mars_amp_z_b;
        } else {
            float t_ = mars_amp_ease;
            float s_ = 1.0f - t_;

            (&w->amp)->x = (&w->amp)->x * t_ + s_ * mars_amp_xy;
            (&w->amp)->y = (&w->amp)->y * t_ + s_ * mars_amp_xy;
            (&w->amp)->z = (&w->amp)->z * t_ + s_ * mars_amp_z;
        }
        if (w->sway)
            i3_copy(w->ang, mars_get_ang(w->sway));
        break;
    }
}

/* 0x00ca1e20 */
void mars_fog_change(MarsWork *w, int fog_from, int fog_to, float t)
{
    int v;

    if (!fog_from)
        w->fog_from = w->fog_to;
    else
        w->fog_from = fog_from;
    w->fog_from_part = w->parts[w->fog_from];
    w->fog_to = fog_to;
    v = (int)(t * FPS);
    w->fog_to_part = w->parts[fog_to];
    w->fog_t = w->fog_len = v;
    if (!v) {
        w->fog_from_part = 0;
        w->fog_from = 0;
    }
}

/* 0x00ca1e98 */
void mars_move(MarsWork *w, int dir)
{
    Vec3 wob;
    Vec3 ofs;
    int ang[2];
    float phase;

    player_get_pos(&w->pl_pos);
    wobble_step(&wob, &w->amp, &w->wob_ph, mars_wobble_spd);
    v3_sum(&w->pos, &w->pl_pos, &wob);
    i3_sum(w->ang, w->ang, w->dang);
    ofs.x = 0.0f;
    ofs.y = w->ypos;
    ofs.z = -w->dist;
    v3_add(&w->pos, &ofs);
    do {
        w->ang[0] = w->ang[0] < 0 ? w->ang[0] | 0xffff0000 : w->ang[0] & 0xffff;
        w->ang[1] = w->ang[1] < 0 ? w->ang[1] | 0xffff0000 : w->ang[1] & 0xffff;
        w->ang[2] = w->ang[2] < 0 ? w->ang[2] | 0xffff0000 : w->ang[2] & 0xffff;
    } while (0);
    w->roll += CMD_BOSS_RZ;
    w->dang[2] = (int)DEG_TO_ANG(w->roll);
    if (dir < 0)
        return;
    phase = w->wob_ph + mars_wobble_spd * (mars_wobble_lead * FPS - 1.0f);
    wobble_step(&wob, &w->amp, &phase, w->wob_spd);
    wob.z = -(mars_aim_dist - w->dist);
    wob.x = -(ofs.x + wob.x);
    wob.y = -(ofs.y + wob.y);
    vec3_normalize(&wob, &wob);
    v3_lerp(&w->face, &w->face, &wob, mars_face_ease);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-(&w->face)->x, -(&w->face)->z));
        ang[0] = RAD_TO_ANG(atan2f((&w->face)->y, sqrtf((&w->face)->x * (&w->face)->x + (&w->face)->z * (&w->face)->z)));
    } while (0);
    if (dir) {
        w->ang[0] = ang[0];
        w->ang[1] = ang[1];
    }
}

/* 0x00ca21e0 */
void mars_exec_battle(MarsWork *w)
{
    Vec3 pl_vec;
    int n;

    if (w->start_wait)
        w->start_wait -= 1;
    player_get_vec(&pl_vec);
    v3_add(&w->pl_pos, &pl_vec);
    v3_add(&w->pos, &pl_vec);
    n = enemy_status_get_ticks();
    if (n) {
        if (w->anim_flags & 8)
            dc_object_advance_time(w->parts[3]);
        if (w->anim_flags & 0x10)
            dc_object_advance_time(w->parts[4]);
        do {
            if (w->phase < 23) {
                if (w->phase < 8)
                    mars_move(w, -1);
                else
                    mars_move(w, 1);
            } else {
                mars_move(w, 0);
            }
            if (w->anim_flags & 0x4000000)
                dc_object_advance_time(w->parts[26]);
            if (w->anim_flags & 0x1000000)
                dc_object_advance_time(w->parts[24]);
            mars_phase_step(w);
            if (w->fog_t) {
                if (--w->fog_t <= 0) {
                    w->fog_t = 0;
                    w->fog_from_part = 0;
                    w->fog_from = 0;
                }
            }
            mars_motion_step(w);
        } while (--n);
    } else if (w->phase >= 27) {
        mars_phase_step(w);
    }
    if (debug_flags_test(4))
        mars_debug_keys(w);
}

/* 0x00ca23c8 */
void mars_motion_start(MarsWork *w, int kind, int mode)
{
    ModelPart *part;

    kind %= 2;
    if (mode == -1) {
        part = w->parts[17];
        w->core_open = 0;
        part->frame = 0.0f;
        w->open_mot = 0;
        w->open_to = w->open_frm = part->frame;
        return;
    }
    if (w->open_mot == 0 || w->open_mode != 0) {
        w->open_loops = (int)mars_motion_table[kind][1];
        w->open_mot = 1;
        w->open_mode = mode;
        w->open_t = w->open_len = mars_motion_table[kind][0] * FPS;
        w->open_to = w->parts[17]->frames - 1.0f;
    }
}

/* 0x00ca2480 */
void mars_motion_step(MarsWork *w)
{
    float k, t, rate;

    if (w->open_mot == 1) {
        w->open_t -= 1.0f;
        if (w->open_mode)
            rate = 0.5f;
        else
            rate = 1.0f;
        t = njSin((int)(rate * PI * (1.0f - w->open_t / w->open_len) * ANG_PER_RAD));
        if (w->open_mode == 2)
            k = w->open_frm * (1.0f - t);
        else
            k = w->open_frm * (1.0f - t) + w->open_to * t;
        dc_object_set_time(w->parts[17], k);
        if (w->open_t <= 0.0f) {
            if (--w->open_loops == 0) {
                w->open_frm = k;
                w->open_mot = 0;
            } else {
                w->open_t = w->open_len;
            }
        } else {
            w->core_open = 1;
        }
    } else if (w->open_frm < 0.5f) {
        w->core_open = 0;
    } else {
        w->core_open = 1;
    }
}

/* 0x00ca25d0 */
void mars_draw_pair(MarsWork *w, ModelPart *part_a, ModelPart *part_b)
{
    float t, front_frame, back_frame;
    Vec3 v;

    ulMatrixPush();
    if (w->fog_to != 1) {
        t = part_a->frame;
        if (((float *)mars_c_get_pos(w->core))[2] < w->pos.z) {
            front_frame = t;
            back_frame = 0.0f;
        } else {
            front_frame = 0.0f;
            back_frame = t;
        }
        {
            float z = -mars_pair_ofs;

            part_a->frame = front_frame;
            vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, z);
        }
        ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&v);
        if (sphere_on_screen((float *)&v, 20.0f)) {
            if (w->pair_hidden == 0)
                dc_object_draw(part_a);
            dc_object_draw(part_b);
        }
        if (front_frame != 0.0f && front_frame < mars_pair_min_frm)
            front_frame = 0.0f;
        {
            float z = mars_pair_ofs + mars_pair_ofs;

            part_a->frame = back_frame;
            vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, z);
        }
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        ulMatrixGetTranslation((float *)ulMatrixCur, (float *)&v);
        if (sphere_on_screen((float *)&v, 20.0f)) {
            if (w->pair_hidden < 2)
                dc_object_draw(part_a);
            dc_object_draw(part_b);
        }
        if (back_frame != 0.0f && back_frame < mars_pair_min_frm)
            back_frame = 0.0f;
        w->parts[17]->frame = t;
    }
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00ca2798 */
void mars_disp_body(MarsWork *w, int kind)
{
    float inten_a, inten_b;
    int list;
    void *fog;

    if (kind == 21) {
        light_get_intensity(&inten_a, &inten_b);
        light_set_intensity(inten_a, 0.0f);
        dc_object_draw(w->parts[21]);
        light_set_intensity(inten_a, inten_b);
        mars_draw_pair(w, w->parts[17], w->parts[18]);
        draw_set_override_flags(DRAW_STRIP_OVERRIDE);
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        if (w->unk_1f5c) {
            ulMatrixPop();
            ulMatrixPush();
            MTX_TRANS(&w->pos);
            dc_object_draw(w->parts[23]);
            mars_rotate_yxz(w->ang);
        } else {
            dc_object_draw(w->parts[23]);
        }
        list = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(w->textures);
        draw_set_override_flags(DRAW_STRIP_OVERRIDE);
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 6144);
        do {
            fog = fog_set_enable(0);
        } while (0);
        {
            ModelPart *part = w->parts[26];

            *(short *)(*(char **)(*(char **)(*(char **)(*(char **)((char *)part + 4) + 44) + 4) + 4) + 14) = 16386;
            do {
                dc_object_draw(part);
            } while (0);
        }
        fog_set_enable(fog);
        RenderSetCurrentDisplayList(list);
    } else if (kind == 20) {
        dc_object_draw(w->parts[20]);
        mars_draw_pair(w, w->parts[17], w->parts[18]);
        draw_set_override_flags(DRAW_STRIP_OVERRIDE);
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        if (w->unk_1f5c) {
            ulMatrixPop();
            ulMatrixPush();
            MTX_TRANS(&w->pos);
            dc_object_draw(w->parts[23]);
            mars_rotate_yxz(w->ang);
        } else {
            dc_object_draw(w->parts[23]);
        }
        list = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(w->textures);
        draw_set_override_flags(DRAW_STRIP_OVERRIDE);
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 6144);
        do {
            fog = fog_set_enable(0);
        } while (0);
        {
            ModelPart *part = w->parts[26];

            *(short *)(*(char **)(*(char **)(*(char **)(*(char **)((char *)part + 4) + 44) + 4) + 4) + 14) = 16385;
            do {
                dc_object_draw(part);
            } while (0);
        }
        fog_set_enable(fog);
        RenderSetCurrentDisplayList(list);
    }
}

/* 0x00ca29f8 */
void mars_fog_blend(MarsWork *w, float t, int fog_from, int fog_to)
{
    char *tab;
    char *x;

    if (fog_from == 20)
        fog_from = 1;
    else if (fog_from == 21)
        fog_from = 2;
    else if (fog_from == 24)
        fog_from = 3;
    else
        fog_from = 0;
    if (fog_to == 20)
        fog_to = 1;
    else if (fog_to == 21)
        fog_to = 2;
    else if (fog_to == 24)
        fog_to = 3;
    else
        fog_to = 0;
    tab = (char *)mars_fog_table[light_mode_index()];
    if (t != 0.0f) {
        char *y;
        unsigned int fog_col, back_col;
        float u, v;

        x = (char *)(fog_from * 16 + (int)tab);
        y = (char *)(fog_to * 16 + (int)tab);
        fog_col = boss_color_blend(*(unsigned int *)(x + 8), *(unsigned int *)(y + 8), t);
        back_col = boss_color_blend(*(unsigned int *)(x + 12), *(unsigned int *)(y + 12), t);
        u = *(float *)x * t + *(float *)y * (1.0f - t);
        v = *(float *)(x + 4) * t + *(float *)(y + 4) * (1.0f - t);
        do {
            fog_set(fog_col, 4, u * CMD_PS2_FONG_N, v * CMD_PS2_FONG_F);
        } while (0);
        fog_set_enable((void *)1);
        BackColReq(back_col);
    } else {
        char *y;

        do {
            y = (char *)(fog_to * 16 + (int)tab);
            fog_set(*(int *)(y + 8), 4, *(float *)y * CMD_PS2_FONG_N, *(float *)(y + 4) * CMD_PS2_FONG_F);
        } while (0);
        fog_set_enable((void *)1);
        BackColReq(*(int *)(y + 12));
    }
}

/* 0x00ca2bc8 */
void mars_fog_update(MarsWork *w)
{
    float t = (float)w->fog_t / (float)w->fog_len;

    if (w->fog_from)
        mars_fog_blend(w, t, w->fog_from, w->fog_to);
    else
        mars_fog_blend(w, 0.0f, 0, w->fog_to);
}

/* 0x00ca2c08 */
void mars_fog_params(int *out)
{
    int *tab = mars_fog_table[light_mode_index()];

    out[0] = tab[2];
    ((float *)out)[2] = ((float *)tab)[0];
    out[1] = tab[3];
    ((float *)out)[3] = ((float *)tab)[1];
}

/* 0x00ca2c68 */
int mars_wait_done(int task)
{
    MarsWork *boss = (MarsWork *)task;

    if (!boss)
        boss = (MarsWork *)boss_get_task();
    return boss->start_wait == 0;
}

/* steer(cast) */
/* 0x00ca2c98 */
void mars_disp(char *w)
{
    MarsWork *boss = (MarsWork *)w;
    float rgba[4];
    int save;
    int *mdl;

    ulGxSetTextureList(boss->textures);
    light_update();
    boss_texlist_bind((int *)w);
    mars_fog_update(boss);
    if (!mars_wait_done((int)w))
        return;
    ulMatrixPush();
    MTX_TRANS(&boss->pos);
    mars_rotate_yxz(boss->ang);
    draw_push_strip_flags();
    draw_push_override_color();
    save = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    if (boss->fog_from_part) {
        rgba[0] = (float)boss->fog_t / (float)boss->fog_len - 1.0f;
        rgba[1] = rgba[2] = rgba[3] = 0.0f;
        draw_set_override_color(rgba);
        if (boss->fog_from == 1) {
            mars_vcc_man_set_alpha(boss->vcc, 1.0f - (float)boss->fog_t / (float)boss->fog_len);
        } else if (boss->fog_from == 20) {
            mars_disp_body(boss, 20);
        } else if (boss->fog_from == 21) {
            mars_disp_body(boss, 21);
        } else if (boss->fog_from == 24) {
            dc_object_draw(boss->fog_from_part);
            ulMatrixPop();
            ulMatrixPush();
            ulMatrixPush();
            MTX_TRANS(&boss->pos);
            dc_object_draw(boss->parts[23]);
            ulMatrixPop();
            MTX_TRANS(&boss->pos);
            mars_rotate_yxz(boss->ang);
        }
        rgba[0] = -1.0f - rgba[0];
        draw_set_override_color(rgba);
        if (boss->fog_to == 1) {
            mars_vcc_man_set_alpha(boss->vcc, -rgba[0]);
        } else if (boss->fog_to == 20) {
            mars_disp_body(boss, 20);
        } else if (boss->fog_to == 21) {
            mars_disp_body(boss, 21);
        } else if (boss->fog_to == 24) {
            int list;

            ((void (*)(char *, float))mars_vcc_set_alpha)(boss->vcc, boss->vcc_alpha + 1.0f);
            list = RenderSetCurrentDisplayList(4);
            ulGxSetTextureList(boss->textures);
            dc_object_draw(boss->fog_to_part);
            RenderSetCurrentDisplayList(list);
        } else if (boss->fog_to == 3) {
            dc_object_draw(boss->parts[3]);
            draw_set_color((int)((rgba[0] + 1.0f) * 255.0f) << 24);
            mdl = (int *)boss->parts[4];
            model_node_walk_list_anim(mdl[1], mdl[2], *(float *)(mdl + 15), mdl[5]);
        }
    } else if (boss->fog_to_part) {
        ((void (*)(char *, float))mars_vcc_set_alpha)(boss->vcc, boss->vcc_alpha + 1.0f);
        rgba[0] = *(float *)(w + 7204);
        rgba[1] = rgba[2] = rgba[3] = 0.0f;
        draw_set_override_color(rgba);
        if (boss->fog_to == 1) {
        } else if (boss->fog_to == 20) {
            mars_disp_body(boss, 20);
        } else if (boss->fog_to == 21) {
            mars_disp_body(boss, 21);
        } else if (boss->fog_to == 24) {
            int list;

            list = RenderSetCurrentDisplayList(4);
            ulGxSetTextureList(boss->textures);
            dc_object_draw(boss->fog_to_part);
            RenderSetCurrentDisplayList(list);
            ulMatrixPop();
            ulMatrixPush();
            ulMatrixPush();
            MTX_TRANS(&boss->pos);
            dc_object_draw(boss->parts[23]);
            ulMatrixPop();
        } else if (boss->fog_to == 3) {
            dc_object_draw(boss->fog_to_part);
            draw_set_color((int)((rgba[0] + 1.0f) * 255.0f) << 24);
            dc_object_draw(boss->parts[4]);
        }
    }
    draw_set_override_flags(0);
    draw_set_override_flags(save);
    draw_pop_strip_flags();
    draw_pop_override_color();
    ulMatrixPop();
    duos_plot_debug();
}

/* 0x00ca3100 */
void *mars_create(void *parent, void *arg)
{
    return task_create_inherit_arg(parent, &mars_desc, 8044, arg);
}

/* 0x00ca3128 */
void task_finish_if(void *w)
{
    FINISH_IF(w);
}

/* 0x00ca3148 */
void mars_pose_cb(MarsWork *w, Vec3 *pos, int *ang)
{
    Vec3 *src_pos = &w->pos;
    int *src_ang = w->ang;

    pos->x = src_pos->x;
    pos->y = src_pos->y;
    pos->z = src_pos->z;
    ang[0] = src_ang[0];
    ang[1] = src_ang[1];
    ang[2] = src_ang[2];
    if (w->flip)
        ang[1] += ANG_180;
}

/* 0x00ca31a0 */
void mars_pose_still_cb(MarsWork *w, Vec3 *pos, int *ang)
{
    if (w->fog_to == 24) {
        v3_copy(pos, &w->pos);
        ang[0] = 0;
        ang[1] = 0;
        ang[2] = 0;
    } else {
        Vec3 *src_pos = &w->pos;
        int *src_ang = w->ang;

        pos->x = src_pos->x;
        pos->y = src_pos->y;
        pos->z = src_pos->z;
        ang[0] = src_ang[0];
        ang[1] = src_ang[1];
        ang[2] = src_ang[2];
    }
}

/* 0x00ca3218 */
void mars_pose_back_cb(MarsWork *w, Vec3 *pos, int *ang)
{
    Vec3 *src_pos = &w->pos;
    int *src_ang = w->ang;

    pos->x = src_pos->x;
    pos->y = src_pos->y;
    pos->z = src_pos->z;
    ang[0] = src_ang[0];
    ang[1] = src_ang[1];
    ang[2] = src_ang[2];
    if (w->sway && w->flip == 0)
        ang[1] += ANG_180;
}

/* 0x00ca3278 */
void mars_collect_points(WireObj *obj, MarsIV *out, int n)
{
    char *mdl = (char *)obj->model;
    Vec3 *vt = (Vec3 *)(*(char **)mdl + 8);
    char *rec = *(char **)(mdl + 4) + 20;
    int i;

    for (i = 0; i < 4; i++, rec += 10, out++) {
        Vec3 *vtx = &vt[*(short *)(rec + 6) << 1];

        out->i = i;
        out->v.x = fabsf(vtx->x);
        out->v.y = fabsf(vtx->y);
        out->v.z = fabsf(vtx->z);
    }
}

/* 0x00ca32f0 */
void key_table_lerp(void *keys, int n, Vec3 *out, float x)
{
    MarsKey *k = keys;
    float ax = fabsf(x);
    int i;

    for (i = 0; i < n; i++) {
        if (k[i].key < ax)
            break;
    }
    if (i == 0) {
        v3_copy(out, (Vec3 *)&k[0].x);
    } else if (i == n) {
        v3_copy(out, (Vec3 *)&k[i - 1].x);
    } else {
        MarsKey *cur = &k[i];
        MarsKey *prev = &k[i - 1];

        ax = (ax - cur->key) / (prev->key - cur->key);
        out->x = ax * prev->x + (1.0f - ax) * cur->x;
        out->y = ax * prev->y + (1.0f - ax) * cur->y;
    }
    out->z = x;
}

/* 0x00ca33f8 */
float mars_curve_y(MarsWork *w, float x)
{
    Vec3 v;

    key_table_lerp(&w->points[0], 4, &v, x);
    return v.y;
}

/* 0x00ca3420 */
int mars_bounds_reflect(MarsWork *w, Vec3 *pos, Vec3 *v)
{
    Vec3 ofs;
    Vec3 next;
    float len_xy, z;

    v3_sub(&ofs, pos, &w->pos);
    v3_sum(&next, &ofs, v);
    z = next.z;
    len_xy = sqrtf(next.x * next.x + next.y * next.y);
    if (len_xy * len_xy / mars_bounds_r2 + z * z / mars_bounds_z2 > 1.0f) {
        ofs.x = -ofs.x;
        ofs.y = -ofs.y;
        ofs.z = 0.0f;
        if (ulVec3Dot(&ofs, v) < 0.0f) {
            vec_reflect(v, &ofs, v);
            return 1;
        }
    }
    return 0;
}

/* 0x00ca3568 */
int mars_is_early_phase(MarsWork *w)
{
    return w->phase < 22;
}

/* 0x00ca3578 */
void mars_player_damage(void)
{
    if (CMD_PLAYER_DAMAGE)
        player_status_add_damage((float)CMD_PLAYER_DAMAGE);
}

/* 0x00ca35a0 */
int mars_get_level(int boss)
{
    return mars_work->level;
}

/* 0x00ca35b0 */
float mars_fog_progress(MarsWork *w, int s)
{
    int cur = w->fog_from;

    if (cur != 0) {
        if (cur == s)
            return (float)w->fog_t / (float)w->fog_len - 1.0f;
        if (w->fog_to == s)
            return -(float)w->fog_t / (float)w->fog_len;
    }
    return w->vcc_alpha;
}

/* 0x00ca3628 */
void mars_set_ref_pose(MarsWork *w, Vec3 *pos, int *ang)
{
    if (pos) {
        v3_copy(&w->ref_pos, pos);
    } else {
        int *dst = (int *)&w->ref_pos.x;

        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
    }
    if (ang) {
        int *dst = w->ref_ang;

        dst[0] = ang[0];
        dst[1] = ang[1];
        dst[2] = ang[2];
    } else {
        int *dst = w->ref_ang;

        dst[0] = 0;
        dst[1] = 0;
        dst[2] = 0;
    }
}

/* 0x00ca3698 */
void mars_get_ref_pose(MarsWork *w, Vec3 *pos, int *ang)
{
    if (pos) {
        Vec3 *src_pos = &w->ref_pos;

        pos->x = src_pos->x;
        pos->y = src_pos->y;
        pos->z = src_pos->z;
    }
    if (ang) {
        int *src_ang = w->ref_ang;

        ang[0] = src_ang[0];
        ang[1] = src_ang[1];
        ang[2] = src_ang[2];
    }
}

/* 0x00ca36e8 */
void mars_request_next(MarsWork *w)
{
    if (w->phase == 10 || w->phase == 16)
        w->skip = 1;
}

/* 0x00ca3710 */
int mars_debris_bounded(void)
{
    return ((MarsWork *)boss_get_task())->debris_bounded;
}

/* 0x00ca3730 */
void mars_debug_keys(MarsWork *w)
{
    if (CMD_NEXT_SEQ) {
        char *child = w->attack;

        CMD_NEXT_SEQ = 0;
        FINISH_IF(child);
        w->skip = 1;
    }
    if (CMD_START_SEQ) {
        CMD_START_SEQ = 0;
        mars_restart(w);
    }
    if (kbd_state[7] & 0x44) {
        switch (kbd_state[5]) {
        case 16:
            wire_export_list((void **)w, mars_wire_pairs, 25);
            break;
        case 17:
            FINISH_IF(w->attack);
            w->skip = 1;
            break;
        case 21:
        case 22:
            mars_restart(w);
            break;
        case 8:
            sound_debug_toggle();
            break;
        }
    }
}

/* 0x00ca3850 */
void mars_ak_act_exec(int *work)
{
    MarsSway *sway = (MarsSway *)work;
    Vec3 v;
    int ang[2];
    float amp_sin, amp_cos, x_sin, x_cos, y_sin, y_cos, ease_sin, ease_cos, pitch_sin, pitch_cos;
    float rad, ease, phase;
    int n;

    for (n = enemy_status_get_ticks(); n != 0; n--) {
        if (--sway->toggle_cnt <= 0) {
            sway->active ^= 1;
            sway->toggle_cnt = (int)(mars_ak_act_toggle_sec * FPS);
        }
        sway->phase_a += mars_ak_act_spd_a;
        sway->phase_b += mars_ak_act_spd_b;
        while (sway->phase_a >= TWO_PI)
            sway->phase_a -= TWO_PI;
        while (sway->phase_b >= TWO_PI)
            sway->phase_b -= TWO_PI;
        phase = sway->phase_b;
        if (sway->active)
            sway->amp = sway->amp * mars_ak_act_amp_ease + mars_ak_act_amp_hi * (1.0f - mars_ak_act_amp_ease);
        else
            sway->amp = sway->amp * mars_ak_act_amp_ease + mars_ak_act_amp_lo * (1.0f - mars_ak_act_amp_ease);
        ulSinCosf(phase, &amp_sin, &amp_cos);
        {
            float k = amp_sin;

            rad = sway->amp * k;
        }
        if (sway->flip)
            v.z = -1.0f;
        else
            v.z = 1.0f;
        if (sway->start_cnt) {
            sway->start_cnt--;
            v.z *= mars_ak_act_start_scl * (float)sway->start_cnt / (mars_ak_act_start_sec * FPS) + 1.0f;
        }
        ulSinCosf(sway->phase_a, &x_sin, &x_cos);
        v.x = rad * x_cos;
        ulSinCosf(sway->phase_a, &y_sin, &y_cos);
        v.y = rad * y_sin;
        do {
            ang[1] = RAD_TO_ANG(atan2f(-v.x, -v.z));
            ang[0] = RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
        sway->ang[0] = ang[0];
        sway->ang[1] = ang[1];
        if (--sway->turn_cnt <= 0) {
            sway->turn_cnt = (int)(mars_ak_act_turn_sec * FPS);
            sway->turn_frm = (int)(mars_ak_act_turn_time * FPS);
            sway->turn_y = (int)DEG_TO_ANG((RAND_FLOAT_A() * 2.0f - 1.0f) * (mars_ak_act_turn_y + 180.0f));
            sway->turn_x = (int)DEG_TO_ANG((RAND_FLOAT_A() * 2.0f - 1.0f) * mars_ak_act_turn_x);
        }
        if (sway->turn_frm) {
            if (--sway->turn_frm <= 0) {
                sway->add[1] += sway->turn_y;
            } else {
                ease = 1.0f - (float)sway->turn_frm / (mars_ak_act_turn_time * FPS);
                ulSinCosf(ease * PI, &ease_sin, &ease_cos);
                ease = ease_cos * -0.5f + 0.5f;
                sway->ang[1] = (float)sway->ang[1] + (float)sway->turn_y * ease;
                ulSinCosf(ease * PI, &pitch_sin, &pitch_cos);
                sway->ang[0] = (float)sway->ang[0] + (float)sway->turn_x * pitch_sin;
            }
        }
        i3_sum(sway->ang, sway->ang, sway->add);
    }
}

/* 0x00ca3db0 */
int *mars_ak_act_create(void *parent, int flip)
{
    int *sway = task_create(parent, &mars_ak_act_desc, 72);

    sway[5] = flip;
    sway[3] = (int)(mars_ak_act_turn_sec * FPS);
    sway[0] = (int)(mars_ak_act_start_sec * FPS);
    mars_ak_act_exec(sway);
    return sway;
}

/* 0x00ca3e30 */
void *mars_get_ang(MarsWork *w)
{
    return &w->ang[0];
}

/* 0x00ca3e38 */
void *mars_attack_start(MarsWork *w, unsigned int kind)
{
    int arg[22];
    MarsWallManArg wall_arg;
    int spare[20];              /* unused */
    int k;

    switch (kind) {
    case 7:
        arg[12] = -1;
        goto blob;
    case 5:
        arg[12] = 0;
        goto blob;
    case 6:
        arg[12] = 1;
    blob:
        arg[20] = arg[19] = (int)w;
        arg[1] = arg[0] = (int)w->parts[37];
        arg[2] = (int)w->parts[38];
        arg[3] = (int)w->parts[28];
        arg[4] = (int)w->parts[29];
        arg[5] = (int)&w->pos;
        arg[7] = (int)w->ang;
        arg[11] = (int)&w->tree_flag;
        arg[14] = 4;
        arg[15] = 3;
        return mars_tr_man_create(w, arg);
    case 2:
        k = 0;
        goto worm;
    case 3:
    case 4:
        k = 1;
    worm:
        wall_arg.params[0] = (int)w->parts[39];
        wall_arg.params[1] = (int)w->parts[43];
        wall_arg.params[2] = (int)w->parts[40];
        wall_arg.params[3] = (int)w->parts[44];
        wall_arg.params[4] = (int)w->parts[41];
        wall_arg.params[5] = (int)w->parts[45];
        wall_arg.params[6] = (int)w->parts[42];
        wall_arg.params[7] = (int)w->parts[46];
        wall_arg.params[8] = (int)w->parts[47];
        wall_arg.params[9] = (int)w->parts[48];
        wall_arg.params[10] = (int)w->parts[49];
        wall_arg.params[11] = (int)w->parts[50];
        return mars_wall_man_create(w, &w->pos, w->ang, &wall_arg, k);
    case 0:
        return mars_fl_create(w, (int *)w->parts, w->flip, &w->fl_mask, 0);
    case 1:
        if (w->cycle == 1)
            k = 1;
        else if (w->cycle == 3)
            k = 2;
        else
            k = 0;
        return mars_fl_create(w, (int *)w->parts, 1, &w->fl_mask, k);
    case 8:
        break;
    }
    return 0;
}

/* 0x00ca3fe0 */
int mars_bounds_bounce(MarsWork *w, Vec3 *pos, Vec3 *v, float k, float rad)
{
    Vec3 ofs;
    float len_xy, z;

    v3_sub(&ofs, pos, &w->pos);
    z = ofs.z;
    len_xy = sqrtf(ofs.x * ofs.x + ofs.y * ofs.y);
    if (len_xy * len_xy / mars_bounds_r2 + z * z / mars_bounds_z2 > 1.0f) {
        vec_set_length(&ofs, sqrtf((1.0f - z * z / mars_bounds_z2) * mars_bounds_r2 + z * z) - rad);
        v3_sum(pos, &w->pos, &ofs);
        if (ofs.z < -mars_bounds_zlim) {
            ofs.z = 1.0f;
            ofs.x = ofs.y = 0.0f;
        } else if (mars_bounds_zlim < ofs.z) {
            ofs.x = ofs.y = 0.0f;
            ofs.z = -1.0f;
        } else {
            ofs.z = 0.0f;
            ofs.x = -ofs.x;
            ofs.y = -ofs.y;
            ofs.z = -ofs.z;
        }
        if (ulVec3Dot(&ofs, v) < 0.0f) {
            vec_reflect(v, &ofs, v);
            v3_scale(v, k);
            return 1;
        }
    } else {
        if (ofs.z < -mars_bounds_zlim) {
            ofs.z = 1.0f;
            ofs.x = ofs.y = 0.0f;
            if (ulVec3Dot(&ofs, v) < 0.0f) {
                vec_reflect(v, &ofs, v);
                return 1;
            }
        } else if (mars_bounds_zlim < ofs.z) {
            ofs.x = ofs.y = 0.0f;
            ofs.z = -1.0f;
            if (ulVec3Dot(&ofs, v) < 0.0f) {
                vec_reflect(v, &ofs, v);
                return 1;
            }
        } else {
            return 0;
        }
    }
    return 0;
}

/* 0x00ca42b0 */
void mars_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
