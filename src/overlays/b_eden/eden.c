/*
 * Eden boss: runs the fight's forms (arm, claw, wall, snake), each with its lockable pieces and
 * piece-text interludes between them; drops items and picks the ending from the shot-down rates.
 * Also the fog fade task and the debug wire viewer.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "debug.h"
#include "camera_demo.h"
#include "config.h"
#include "console.h"
#include "fade.h"
#include "file.h"
#include "fog.h"
#include "game.h"
#include "gw_varedit.h"
#include "heap.h"
#include "item.h"
#include "libc.h"
#include "light.h"
#include "model.h"
#include "module.h"
#include "pad_vib.h"
#include "player.h"
#include "player_status.h"
#include "pnb.h"
#include "screen.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_pcmstream.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_vu0.h"
#include "eden.h"
#include "eden_arm.h"
#include "eden_belt.h"
#include "eden_break.h"
#include "eden_bullet.h"
#include "eden_claw.h"
#include "eden_core.h"
#include "eden_demo.h"
#include "eden_dust.h"
#include "eden_effect.h"
#include "eden_enemy.h"
#include "eden_font.h"
#include "eden_laser.h"
#include "eden_map.h"
#include "eden_model.h"
#include "eden_piece.h"
#include "eden_snake.h"
#include "eden_tree.h"
#include "eden_wall.h"
#include "../common/boss_common.h"
#include "../common/boss_sound.h"
#include "../common/boss_texture_anime.h"
#include "../common/boss_wire.h"
#include "../common/eff_circle.h"
#include "game_flow_editor.h"

/* 7796 bytes */
typedef struct EdenWork {
    struct PnbHead *pack;
    void *tex_list;
    int arg0;
    int level;
    int state;                  /* 0 loading, 1 loaded, 2 running */
    char pad_14[24];
    int seq;                    /* 0x2c */
    int seq_next;
    int time;
    int len;
    int fade_time;
    int fade_len;
    int bar_wait;               /* bars to wait before seq_next */
    ModelPart pool[80];
    ModelPart *parts[80];
    int *obj;
    EdenText *font;
    void *wire_edit;
    struct EdenPieceWork *piece;
    char pad_1e58[4];
    struct EdenModelManWork *model_man; /* 0x1e5c */
    struct EdenMapWork *map;
    BossStarCtl *star;
    int *core;
    char pad_1e6c[4];
    int stage;                  /* 0x1e70: 0..4, the form being fought */
} EdenWork;

extern unsigned int stage_exit_flags;

extern BossStarCtl *star_ctl;
extern const unsigned int star_color;
extern EdenWork *eden_work;
extern float cm_ed_d_alpha;
extern int eden_lock_off;
extern int eden_break_all;
extern int eden_flags;
extern struct TaskDesc eden_wire_edit_man_desc;
extern struct TaskDesc eden_desc;
extern float cmd_damage;
extern int eden_end_req;
extern int eden_bgm_flag;
extern unsigned int cm_wire_draw_b_col;
extern unsigned int cm_wire_draw_f_col;
extern float cm_wire_draw_w;
extern float cmd_cir_r2;
extern float cmd_cir_r;
extern float cmd_fog_n;
extern float cmd_fog_f;
extern int cmd_fog_t;
extern int cmd_fog_sw;
extern int cm_fog_col;
extern int eden_wire_edit_models[][4];
extern char *eden_wire_edit_mode_names[];
extern char str_eden_wire_edit_select_fmt[];
extern struct TaskDesc eden_fog_desc;
extern char eden_boss_files[][8];
extern BossModelDef eden_parts_def[];
extern float eden_part_radii[];
#define FINISH_CLR(f) do { if (*(void **)&(f)) { FINISH(*(void **)&(f)); (f) = 0; } } while (0)
extern char eden_vardef[];
extern float cmd_item_sv;
extern float cmd_item_remain_time;
extern int eden_vars_group;
extern GwVarDef eden_vardefs[];
extern float eden_item_vel_scl;
extern float eden_item_vy_max;
extern float eden_item_vel_max;
extern float cmd_font_st_wait;
extern int display_fps;
extern float cmd_item_time;
extern int cmd_piece_score;
extern float eden_clear_osc_amp;
extern float eden_clear_osc_rate;
extern char str_eden_level4_fmt[];
extern char str_eden_level3_fmt[];
extern float cm_wire_at_rate;
extern int cmd_at_mode, cmd_at_mode_sw, cmd_bl_rad, cmd_bl_rate, cmd_freez_level, cmd_test_level;
extern float cmd_bl_t, cmd_bl_t2, cmd_eden_st_wait, cmd_badend_time, eden_fade_sec;
extern int eden_bg_models[];

/* debug wire viewer */
/* 36 bytes */
typedef struct EdenWireEditManWork {
    void *bank;
    void *tex_list;
    void *model;
    void *model2;
    unsigned int *motion;
    void *data;
    void *wire;
    int sel;                    /* the entry, 0..5 */
    int mode;                   /* 0..3 */
} EdenWireEditManWork;

/* 44 bytes */
typedef struct EdenFogWork {
    int time;
    int len;
    char pad_08[4];
    BossFog from;               /* 0x0c */
    BossFog to;
} EdenFogWork;

/* eden_wire_edit_models entry, 16 bytes */
typedef struct EdenWireEnt {
    int model, node, motion;
    char *name;
} EdenWireEnt;

/* 0x00ca0700 */
void *module_entry(void *parent, void *arg)
{
    return eden_create(parent, arg);
}

/* 0x00ca0720 */
void star_color_apply(void)
{
    if (star_ctl)
        boss_star_set_color(star_ctl, star_color);
}

/* 0x00ca0740 */
int boss_main_demo_cb(BossMainWork *w)
{
    if (w->state == 0)
        w->state = 1;
    else if (w->state == 2)
        return 1;
    return 0;
}

/* 0x00ca0770 */
void eden_exec_start(EdenWork *w)
{
    FogArg arg;
    BossStarCtl *star;

    if (w->state == 1) {
        *(int *)&w->map = eden_map_create(w, w->parts); /* steer(type) */
        TASK_OF(w)->exec = (void (*)(void *))eden_exec;
        TASK_OF(w)->disp = (void (*)(void *))eden_disp;
        eden_set_lock_off(0);
        eden_set_break_all(0);
        eden_set_dalpha(1.0f);
        boss_adx_request();
        arg.range = 500.0f;
        arg.k = 0.5f;
        arg.count = 500;
        arg.color = 0;
        star = boss_star_create(w, &arg, 20.0f);
        star_ctl = star;
        w->star = star;
        w->state = 2;
    }
}

/* 0x00ca0838 */
void eden_exec_load(EdenWork *w)
{
    char *p;
    char *node;

    if (boss_files_ready(eden_boss_files[0], eden_boss_files[1])) {
        w->pack = model_pnb_load(eden_boss_files[0]);
        w->tex_list = model_load_timed(eden_boss_files[1], w->arg0);
        do {
            boss_parts_build((void **)w, eden_parts_def, w->parts, w->pool);
        } while (0);
        parts_set_radii(w->parts, eden_part_radii);
        TASK_OF(w)->exec = (void (*)(void *))eden_exec_start;
        do { /* steer(barrier) */
            node = *(char **)(w->parts[0]->root + 4);
        } while (0);
        p = *(char **)(node + 4);
        p += 14;
        *(short *)p = eden_tex_offset() | 0x4000;
        cm_se_laser_create(eden_boss_files[2]);
        eden_score_vars_add();
        w->seq = 4;
    }
}

/* steer(cast) */
/* 0x00ca0918 */
void eden_init(EdenWork *w, int *arg)
{
    eden_end_req = 0;
    w->seq = 0;
    w->arg0 = arg[0];
    w->level = arg[1];
    w->state = 0;
    eden_bgm_flag = 1;
    boss_files_request((int)eden_boss_files[0], (int)eden_boss_files[1]);
    boss_start((int *)w, (int)w->parts, w->arg0, w->level);
    boss_bgm_start(w);
    eden_demo_create(w, (int)boss_main_demo_cb, (int)w);
    tex_still_create();
    eden_work = w;
    eden_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, eden_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_vars_group, eden_vardefs);
    eden_arm_vars_add();
    eden_dust_vars_add();
    eden_font_vars_add();
    eden_model_vars_add();
    eden_piece_vars_add();
    eden_bullet_vars_add();
    eden_claw_vars_add();
    eden_wall_vars_add();
    eden_laser_vars_add();
    eden_snake_vars_add();
    eden_demo_vars_add();
    eden_belt_vars_add();
    eden_map_vars_add();
    eden_tree_vars_add();
    eden_effect_vars_add();
    eden_core_vars_add();
    eden_enemy_vars_add();
    eden_break_vars_add();
}

/* 0x00ca0a80 */
void eden_release(EdenWork *w)
{
    screen_set_margin(0);
    screen_set_fade_alpha(0);
    light_fog_set();
    if (boss_parts_release(w->parts)) {
        model_unload_timed(w->tex_list);
        model_pnb_unload(w->pack);
        w->tex_list = 0;
        *(int *)&w->pack = 0;
    }
    boss_end(w);
    eden_score_vars_remove();
    tex_still_free();
    if (!light_mode_is_fog() && eden_bgm_flag)
        se_stg_common_clear();
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_vardef);
    eden_arm_vars_remove();
    eden_dust_vars_remove();
    eden_font_vars_remove();
    eden_model_vars_remove();
    eden_piece_vars_remove();
    eden_bullet_vars_remove();
    eden_claw_vars_remove();
    eden_wall_vars_remove();
    eden_laser_vars_remove();
    eden_snake_vars_remove();
    eden_demo_vars_remove();
    eden_belt_vars_remove();
    eden_map_vars_remove();
    eden_tree_vars_remove();
    eden_effect_vars_remove();
    eden_core_vars_remove();
    eden_enemy_vars_remove();
    eden_break_vars_remove();
}

/* 0x00ca0bc0 */
void eden_children_finish(EdenWork *w)
{
    /* steer(type): the clears store ints */
    FINISH_CLR(*(int *)&w->font);
    FINISH_CLR(*(int *)&w->obj);
    FINISH_CLR(*(int *)&w->piece);
    FINISH_CLR(*(int *)&w->model_man);
    FINISH_CLR(*(int *)&w->core);
    tex_anime_stop();
    w->seq = 4;
    w->time = 3;
    stage_exit_flags &= 0xffffffdf;
    eden_end_req = 0;
}

/* 0x00ca0cb0 */
void eden_phase_change(EdenWork *w, int kind)
{
    if (w->font)
        eden_font_end(w->font);
    switch (kind) {
    case 0:
        w->obj = eden_arm_man_create(w, (int *)w->parts);
        w->piece = (struct EdenPieceWork *)eden_piece_create(w, (int)w->font, (int *)&w->obj, (int *)w->parts, 0);
        break;
    case 1:
        w->obj = eden_claw_create(w, (int *)w->parts);
        w->piece = (struct EdenPieceWork *)eden_piece_create(w, (int)w->font, (int *)&w->obj, (int *)w->parts, 1);
        break;
    case 2:
        w->obj = eden_wall_create(w, (int)w->parts);
        w->piece = (struct EdenPieceWork *)eden_piece_create(w, (int)w->font, (int *)&w->obj, (int *)w->parts, 2);
        break;
    default:
        w->obj = eden_snake_create(w, (int *)w->parts);
        w->piece = (struct EdenPieceWork *)eden_piece_create(w, (int)w->font, (int *)&w->obj, (int *)w->parts, 3);
        break;
    }
    FINISH_IF(w->font);
    w->font = 0;
}

/* 0x00ca0de8 */
int eden_is_piece_clear(EdenWork *w)
{
    return !eden_piece_get_remain(w->piece);
}

/* 0x00ca0e08 */
void eden_bgm_flag_clear(void)
{
    if (eden_bgm_flag)
        eden_bgm_flag = 0;
}

/* steer(cast) */
/* 0x00ca0e20 */
int eden_ending_type(void)
{
    float rec, k, sum;

    if (player_status_get_level() != 6)
        return 1;
    if (game_type_get() == 5 && !ConfigGetExtraType()) {
        float shot_rate = ConfigGameShotdownRate();
        if (shot_rate < 95.0f)
            return 2;
        if (fabsf(shot_rate - 100.0f) < FLT_EPSILON || 100.0f <= shot_rate) {
            DEBUG_PRINTF(str_eden_level4_fmt, (double)shot_rate);
            return 4;
        }
        DEBUG_PRINTF(str_eden_level3_fmt, (double)shot_rate);
        return 3;
    }
    k = 95.0f;
    sum = 0.0f;
    rec = ((float *)ConfigGetStageRecord(0))[1];
    if (rec < k)
        return 2;
    sum += rec;
    rec = ((float *)ConfigGetStageRecord(1))[1];
    if (rec < k)
        return 2;
    sum += rec;
    rec = ((float *)ConfigGetStageRecord(2))[1];
    if (rec < k)
        return 2;
    sum += rec;
    rec = ((float *)ConfigGetStageRecord(3))[1];
    if (rec < k)
        return 2;
    sum += rec;
    rec = ConfigGameShotdownRate();
    if (rec < k)
        return 2;
    sum += rec;
    if (fabsf(sum - 500.0f) < FLT_EPSILON || 500.0f <= sum) {
        DEBUG_PRINTF(str_eden_level4_fmt, (double)sum);
        return 4;
    }
    DEBUG_PRINTF(str_eden_level3_fmt, (double)sum);
    return 3;
}

/* 0x00ca1000 */
void eden_stage_start_cb(EdenWork *w)
{
    int n = w->stage;

    if (n < 4)
        peri_act_set_selection(n * 2 + 1);
    eden_sound_tempo_set(1);
    boss_vol_play(w->stage * 2 + 1, 0);
    w->seq = w->seq_next;
}

/* 0x00ca1060 */
void eden_stage_start_request(EdenWork *w, int req)
{
    boss_bgm_switch((int)eden_stage_start_cb, (int)w, w->stage * 2);
    w->seq_next = req;
    w->seq = 101;
}

/* 0x00ca10b0 */
void eden_stage_clear_cb(EdenWork *w)
{
    float k;
    Vec3 *core_pos;
    ModelPart *part;
    int z[3];
    Vec3 scl;
    CmObjMotWork *mot;

    peri_act_set_selection(w->stage << 1);
    peri_act_play_boss_break();
    if (display_fps < 55)
        w->bar_wait = 5;
    else
        w->bar_wait = 4;
    k = FPS;
    eden_sound_tempo_set(0);
    boss_vol_play(9, 0);
    w->time = w->len = (int)(cmd_font_st_wait * k);
    w->seq_next = 6;
    eden_set_lock_off(1);
    w->time = (int)(cmd_item_time * k);
    w->seq = 12;
    core_pos = (Vec3 *)eden_core_shine_get_pos(eden_get_core(w));
    eden_core_shine_set_active(0);
    eden_items_spawn(core_pos, 3);
    score_at(core_pos, cmd_piece_score);
    part = w->parts[47];
    part->speed = 1.0f;
    z[0] = 0;
    z[1] = 0;
    z[2] = 0;
    mot = cm_obj_mot_create(w, core_pos, z, part, 0, 0.0f, 0.1f);
    scl.x = scl.y = scl.z = 0.4f;
    cm_obj_mot_set_scale(mot, &scl);
    cm_obj_mot_set_time(mot, 62.0f);
    eden_piece_shatter(w->piece);
    FINISH_IF(w->obj);
    FINISH_IF(w->piece);
    w->obj = 0;
    w->piece = 0;
    oscilate_create(eden_clear_osc_amp, eden_clear_osc_rate);
}

/* 0x00ca1280 */
void eden_stage_clear_request(EdenWork *w)
{
    boss_bgm_switch((int)eden_stage_clear_cb, (int)w, (w->stage - 1) * 2 + 1);
    w->seq = 101;
}

/* 0x00ca12c8 */
void eden_bar_countdown(EdenWork *w)
{
    if (w->bar_wait && beat_grid_hit(0) && !(beat_clock_get_haku() & 3) && --w->bar_wait <= 0) {
        w->seq = w->seq_next;
        w->time = w->len = 60;
    }
}

/* 0x00ca1340 */
void eden_exec(EdenWork *w)
{
    int n;

    eden_bar_countdown(w);
    switch (w->seq) {
    case 4:
        eden_sound_tempo_set(0);
        eden_set_lock_off(0);
        eden_set_break_all(0);
        eden_set_dalpha(1.0f);
        eden_map_set_target(w->map, 1);
        eden_map_pass_change(w->map, EDEN_MAP_CENTER, 0);
        eden_map_fog_change(w->map, 0);
        eden_set_flags(-1);
        w->fade_len = 0;
        w->fade_time = 0;
        w->time = w->len = 120;
        boss_star_fade(w->star, star_color, 120);
        if (!w->core)
            w->core = eden_core_shine_create(w, (int)w->parts);
        if (cmd_at_mode) {
            w->stage = cmd_at_mode;
            eden_map_fog_change(w->map, w->stage);
            if (cmd_at_mode == 4)
                w->seq = 14;
            else
                w->seq = 5;
        } else {
            w->stage = 0;
            eden_map_state_start(w->map);
            w->seq = 5;
        }
        break;
    case 5:
        if (--w->time > 0)
            break;
        w->time = 120;
        w->seq = 6;
        break;
    case 6:
        if (w->time <= 0) {
            eden_set_dalpha(1.0f);
            eden_set_lock_off(0);
            w->font = eden_font_create(w, (int)w->parts, w->stage);
            w->seq = 7;
        } else if ((float)w->time == 60.0f)
            eden_tex_anime_create(w->stage + 5);
        else if (w->time == 2)
            eden_tex_anime_set_play(1);
        w->time--;
        break;
    case 7:
        if (!w->font)
            break;
        if (!eden_font_is_done(w->font))
            break;
        if (display_fps < 55)
            n = (int)(beat_clock_get_interval() * 3.0f + beat_clock_get_frames_to_beat());
        else
            n = (int)(beat_clock_get_interval() * 4.0f + beat_clock_get_frames_to_beat());
        w->time = w->len = n;
        w->seq = 8;
        break;
    case 8:
        if (--w->time > 0)
            break;
        if (!beat_grid_hit(0))
            break;
        if (beat_clock_get_haku() & 3)
            break;
        if (w->stage >= 4) {
            cm_sc_blur_create(w, (float)cmd_bl_rad, (float)cmd_bl_rate, 0.0f, cmd_bl_t2);
            w->time = (int)(cmd_eden_st_wait * FPS);
            w->seq = 13;
        } else {
            w->time = (int)(cmd_bl_t * 0.5f);
            cm_sc_blur_create(w, (float)cmd_bl_rad, (float)cmd_bl_rate, 0.0f, cmd_bl_t);
            eden_stage_start_request(w, 9);
        }
        break;
    case 9:
        if (--w->time > 0)
            break;
        switch (w->stage) {
        case 0:
            eden_phase_change(w, 0);
            w->seq = 10;
            break;
        case 1:
            eden_phase_change(w, 1);
            w->seq = 10;
            break;
        case 2:
            eden_phase_change(w, 2);
            w->seq = 10;
            break;
        default:
            eden_phase_change(w, 3);
            w->seq = 10;
            break;
        }
        eden_core_shine_set_mode(w->core, w->stage, 1);
        break;
    case 10:
        if (!eden_is_piece_clear(w))
            break;
        w->seq = 11;
        tex_anime_finish();
        w->stage++;
        eden_stage_clear_request(w);
        break;
    case 12:
        if (--w->time > 0)
            break;
        if (!beat_grid_hit(0))
            break;
        if (beat_clock_get_haku() & 3)
            break;
        eden_core_shine_set_mode(w->core, w->stage, 2);
        eden_map_request(w->map);
        w->seq = 101;
        boss_vol_play(w->stage * 2, 0);
        break;
    case 13:
        if (--w->time > 0)
            break;
        if (!beat_grid_hit(0))
            break;
        if (beat_clock_get_haku() & 3)
            break;
        w->seq = 14;
        tex_anime_finish();
    case 14:
        eden_set_lock_off(0);
        eden_set_break_all(0);
        FINISH_IF(w->font);
        w->font = 0;
        eden_core_shine_set_mode(w->core, w->stage, 5);
        w->model_man = eden_model_man_create(w, w->map, (int)w->parts);
        w->seq = 15;
        eden_sound_seq_load();
        eden_sound_beat_create(w);
        eden_sound_tempo_set(2);
        break;
    case 15:
        if (!w->model_man)
            break;
        if (eden_model_man_is_end(w->model_man))
            goto stop;
        break;
    case 16:
        if (!pcm_is_playing())
            goto stop;
        if (file_request_all_done() == 1)
            break;
        w->seq = 17;
        break;
    case 17:
        if (pcm_is_playing() && file_request_all_done() != 1)
            break;
    stop:
        if (game_type_get() == 5) {
            if (ConfigGetExtraType() == 3)
                pcm_stop();
            else
                cm_bgm_sound_man_set_stream(-1);
        } else if (game_type_get() == 4)
            pcm_stop();
        else
            cm_bgm_sound_man_set_stream(-1);
        stage_exit_flags |= 0xa;
        module_set_clear_flag(1, eden_ending_type());
        w->seq = 100;
        break;
    case 102:
        if (--w->time <= 0) {
            if (game_type_get() == 5) {
                if (ConfigGetExtraType() == 3)
                    pcm_stop();
                else
                    cm_bgm_sound_man_set_stream(-1);
            } else if (game_type_get() == 4)
                pcm_stop();
            else
                cm_bgm_sound_man_set_stream(-1);
            ((void (*)(EdenWork *))boss_fx_release)(w);
            stage_exit_flags |= 2;
            module_set_clear_flag(1, 0);
            w->seq = 100;
        } else if ((float)w->time == eden_fade_sec * FPS) {
            if (game_type_get() == 5) {
                if (ConfigGetExtraType() != 3)
                    fade_start(0, 1, (int)(eden_fade_sec * FPS));
            } else if (game_type_get() != 4)
                fade_start(0, 1, (int)(eden_fade_sec * FPS));
        }
        break;
    }
    if (w->fade_time) {
        w->fade_time--;
        eden_set_dalpha(1.0f - (float)w->fade_time / (float)w->fade_len);
    }
    if (eden_end_req == 1 && player_status_test_flags(1)) {
        eden_end_req++;
        eden_sound_bgm_start(0);
        eden_bgm_flag_clear();
        eden_model_man_set_visible(w->model_man, 0);
        if (cmd_test_level)
            eden_break_d_create(w, (int)w->parts, cmd_test_level);
        else
            eden_break_d_create(w, (int)w->parts, eden_model_man_get_part(w->model_man));
        w->time = (int)(cmd_badend_time * FPS);
        w->seq = 102;
        eden_set_flags(0);
        eden_set_lock_off(1);
    }
    if (debug_flags_test(4)) {
    if (cmd_at_mode_sw) {
        cmd_at_mode_sw = 0;
        eden_children_finish(w);
    }
    if (kbd_state[7] & 0x44) {
    switch (kbd_state[5]) {
    case 12:
        eden_items_spawn((Vec3 *)eden_core_shine_get_pos(eden_get_core(w)), 1);
        break;
    case 5:
        eden_break_create(w, (int *)w->parts[64]);
        break;
    case 10:
        eden_bullet_fb_spawn(w, w->parts, 1);
        break;
    case 4:
        eden_set_flags(cmd_freez_level);
        break;
    case 8:
        sound_debug_toggle();
        break;
    case 9:
        cmd_at_mode = 4;
    case 22:
        eden_children_finish(w);
        break;
    case 16:
        if (w->wire_edit) {
            FINISH_IF(w->wire_edit);
            w->wire_edit = 0;
            TASK_OF(w)->disp = (void (*)(void *))eden_disp;
            eden_children_finish(w);
            break;
        }
        w->wire_edit = eden_wire_edit_man_create(w);
        eden_children_finish(w);
        TASK_OF(w)->disp = 0;
        w->seq = 100;
        break;
    case 17:
        wire_export_list((void **)w, eden_bg_models, 3);
        break;
    case 13:
        if (w->obj) {
            FINISH(w->obj);
            FINISH_IF(w->piece);
            w->obj = 0;
            w->piece = 0;
            break;
        }
        if (!w->font)
            w->font = eden_font_create(w, (int)w->parts, cmd_at_mode);
        eden_phase_change(w, cmd_at_mode);
    }
    }
    }
}

/* steer(cast) */
/* 0x00ca1e20 */
void eden_disp(void **w)
{
    ulGxSetTextureList(w[1]);
    light_update();
    boss_texlist_bind((int *)w);
    if (cmd_fog_sw) {
        BackColReq(cm_fog_col);
        do {
            ((void (*)(int, float, float, int))fog_set)(cm_fog_col, cmd_fog_n, cmd_fog_f, cmd_fog_t);
            fog_set_enable((void *)1);
        } while (0);
    }
}

/* 0x00ca1e98 */
void *eden_create(void *parent, void *arg)
{
    return task_create_inherit_arg(parent, &eden_desc, 7796, arg);
}

/* 0x00ca1ec0 */
void task_finish_if(void *w)
{
    FINISH_IF(w);
}

/* 0x00ca1ee0 */
void eden_circle_spawn(void *w, Vec3 *pos, int flag)
{
    EffCircleArg arg;

    arg.radius = !flag ? cmd_cir_r2 : cmd_cir_r;
    arg.pos = pos;
    arg.color = console_get_pcol();
    arg.width = 0.5f;
    arg.frames = 70;
    arg.fade = 70;
    arg.n = 16;
    eff_circle_create(w, &arg);
}

/* 0x00ca1f58 */
void eden_player_damage(void)
{
    player_status_add_damage(cmd_damage);
}

/* 0x00ca1f68 */
int eden_get_arg0(void)
{
    return eden_work->arg0;
}

/* 0x00ca1f78 */
int eden_tex_offset(void)
{
    return 0;
}

/* steer(cast) */
/* 0x00ca1f80 */
int eden_fog_exec(EdenFogWork *w)
{
    BossFog fog;

    if (--w->time <= 0)
        FINISH_IF(w);
    fog_blend(&w->from, &w->to, &fog, (float)w->time / (float)w->len);
    do {
        BackColReq(fog.back);
    } while (0);
    fog_set(fog.col, cmd_fog_t, fog.near, fog.far);
    return (int)fog_set_enable((void *)1);
}

/* 0x00ca2008 */
int *eden_fog_create(void *parent, BossFog *to, float secs)
{
    EdenFogWork *w = task_create(parent, &eden_fog_desc, 44);

    w->time = w->len = secs * FPS;
    w->from.col = fog_get_color();
    w->from.back = fog_get_back_color();
    w->from.near = fog_get_near();
    w->from.far = fog_get_far();
    w->to = *to;
    return (int *)w;
}

/* 0x00ca20b0 */
void eden_set_dalpha(float x)
{
    cm_ed_d_alpha = x;
}

/* 0x00ca20c0 */
float eden_get_dalpha(void)
{
    return cm_ed_d_alpha;
}

/* 0x00ca20d0 */
int eden_get_lock_off(void)
{
    return eden_lock_off;
}

/* 0x00ca20e0 */
int eden_get_break_all(void)
{
    return eden_break_all;
}

/* 0x00ca20f0 */
void eden_set_lock_off(int on)
{
    eden_lock_off = on;
}

/* 0x00ca2100 */
void eden_set_break_all(int on)
{
    eden_break_all = on;
}

/* 0x00ca2110 */
int eden_test_flags(int mask)
{
    return (eden_flags & mask) != 0;
}

/* 0x00ca2128 */
void eden_set_flags(int flags)
{
    eden_flags = flags;
}

/* 0x00ca2138 */
int eden_get_flags(void)
{
    return eden_flags;
}

/* 0x00ca2148 */
void *eden_get_piece(EdenWork *w)
{
    return w->piece;
}

/* 0x00ca2150 */
int eden_get_core(EdenWork *w)
{
    return (int)w->core;
}

/* 0x00ca2158 */
int eden_get_parts(EdenWork *w)
{
    return (int)w->map;
}

/* 0x00ca2160 */
void eden_wire_colors_set(void)
{
    wire_colors_set(cm_wire_draw_f_col, cm_wire_draw_b_col, cm_wire_draw_w);
}

/* 0x00ca2180 */
CmWireBreakWork *eden_shatter_spawn(void *parent, unsigned int flags, ModelPart *src, Vec3 *pos, Vec3 *vel, int *ang, unsigned int color)
{
    return cm_wire_break_create(parent, flags, src, pos, vel, ang, 0, color, 1.25f, 64.0f, RAND_FLOAT_B() * 0.5f + 0.2f, 0);
}

/* 0x00ca2260 */
CmWireBreakWork *eden_shatter_model(void *parent, unsigned int flags, ModelPart *src, Vec3 *pos, Vec3 *vel, int *ang, unsigned int color)
{
    return cm_wire_break_create(parent, flags, src, pos, vel, ang, 0, color, 1.25f, 64.0f, 0.0f, 0);
}

/* 0x00ca2298 */
void eden_end_request(void)
{
    eden_end_req = 1;
    stage_exit_flags |= 0x20;
}

/* 0x00ca22b8 */
int eden_is_end(void)
{
    return eden_end_req == 2;
}

/* 0x00ca22d0 */
void eden_item_point_random(Vec3 *out)
{
    Vec3 v;
    int ang[3];
    float sc[12];
    float x, y, z, rnd;

    player_get_eye(ang);
    out->x = out->y = 0.0f;
    out->z = -100.0f;
    vec_rotate_zxy(out, ang);
    x = 1.0f;
    y = 0.0f;
    z = 0.0f;
    ROT_Z(ang[2], sc, x, y);
    ROT_X(ang[0], (sc + 2), y, z);
    ROT_Y(ang[1], (sc + 4), x, z);
    rnd = RAND_FLOAT_A() * 140.0f + -70.0f;
    v.x = x * rnd;
    v.y = y * rnd;
    v.z = z * rnd;
    x = 0.0f;
    y = 1.0f;
    z = 0.0f;
    ROT_Z(ang[2], (sc + 6), x, y);
    ROT_X(ang[0], (sc + 8), y, z);
    ROT_Y(ang[1], (sc + 10), x, z);
    rnd = RAND_FLOAT_A() * 140.0f + -70.0f;
    v.x += x * rnd;
    v.y += y * rnd;
    v.z += z * rnd;
    v3_add(out, &v);
}

/* 0x00ca2540 */
int eden_item_kind(void)
{
    switch ((int)(RAND_FLOAT_A() * 3.0f)) {
    case 1:
        return 2;
    case 2:
        return 3;
    }
    return 0;
}

/* 0x00ca25c0 */
void eden_item_vel_adjust(float *pos, Vec3 *vel)
{
    Vec3 n;
    float sc[2];

    if (v3_is_zero(vel))
        return;
    vec3_normalize(&n, vel);
    v3_scale(&n, eden_item_vel_scl);
    v3_sum(&n, (Vec3 *)pos, &n);
    if (n.y < -eden_item_vy_max || eden_item_vy_max < n.y)
        vel->y = -vel->y;
    if (vec3_length(&n) > eden_item_vel_max)
        ROT_Y(ANG_180, sc, vel->x, vel->z);
}

/* 0x00ca2738 */
void eden_items_spawn(Vec3 *pos, int n)
{
    Spawn spawn;
    void *item;
    int i;

    for (i = 0; i < n;) {
        if (!pos) {
            spawn.x = 0;
            spawn.y = 0;
            spawn.z = 0;
        } else {
            spawn.x = pos->x;
            spawn.y = pos->y;
            spawn.z = pos->z;
        }
        i++;
        eden_item_point_random((Vec3 *)&spawn.vel_x);
        ((Vec3 *)&spawn.vel_x)->x -= pos->x;
        ((Vec3 *)&spawn.vel_x)->y -= pos->y;
        ((Vec3 *)&spawn.vel_x)->z -= pos->z;
        vec_set_length((Vec3 *)&spawn.vel_x, cmd_item_sv);
        eden_item_vel_adjust(&spawn.x, (Vec3 *)&spawn.vel_x);
        spawn.scale = 0.9f;
        spawn.kind = eden_item_kind();
        item = item_create(&spawn);
        item_state_if_alive(item, 3);
        item_set_level(item, (int)(cmd_item_remain_time * FPS));
    }
}

/* 0x00ca2888 */
void eden_items_drop(Vec3 *pos, int n)
{
    Spawn spawn;
    int i;

    for (i = 0; i < n; i++) {
        spawn.x = pos->x;
        spawn.y = pos->y;
        spawn.z = pos->z;
        spawn.vel_x = 0;
        spawn.vel_y = 0;
        spawn.vel_z = 0;
        spawn.scale = 0.9f;
        spawn.kind = eden_item_kind();
        item_state_if_alive(item_create(&spawn), 3);
    }
}

/* steer(cast) */
/* 0x00ca2920 */
void eden_wire_edit_man_exec(EdenWireEditManWork *w)
{
    EdenWireEnt *ent;
    WireEditWork *ed;

    switch (kbd_key) {
    case 'P':
        if (--w->mode <= 0)
            w->mode = 0;
        break;
    case 'O':
        if (++w->mode >= 3)
            w->mode = 3;
        break;
    case 'Q':
        if ((unsigned int)++w->sel >= 6)
            w->sel = 0;
        break;
    case 'R':
        if (--w->sel < 0)
            w->sel = 5;
        break;
    case '(':
        if (w->mode == 0) {
            ent = &((EdenWireEnt *)eden_wire_edit_models)[w->sel];
            w->model = (void *)*((int *)pnb_get_entry(w->bank, ent->model));
            if (ent->motion)
                w->motion = (unsigned int *)*((int *)pnb_get_entry(w->bank, ent->motion));
            else
                w->motion = 0;
            w->data = model_load_wire_data((void *)*((int *)pnb_get_entry(w->bank, ent->model)));
            ed = wire_edit_create(w, w->model, (int)w->data, w->motion, ent->name);
            wire_edges_build(w->model, *((int *)pnb_get_entry(w->bank, ent->node)));
            wire_edges_rebuild(ed, cm_wire_at_rate);
            wire_edges_free();
            TASK_OF(w)->exec = 0;
            TASK_OF(w)->disp = 0;
        } else if (w->mode == 1) {
            ent = &((EdenWireEnt *)eden_wire_edit_models)[w->sel];
            w->model = (void *)*((int *)pnb_get_entry(w->bank, ent->model));
            if (ent->motion)
                w->motion = (unsigned int *)*((int *)pnb_get_entry(w->bank, ent->motion));
            else
                w->motion = 0;
            wire_edit_create(w, w->model, *((int *)pnb_get_entry(w->bank, ent->node)), w->motion, ent->name);
            TASK_OF(w)->exec = 0;
            TASK_OF(w)->disp = 0;
        } else if (w->mode == 2) {
            int ent0, ent1, ent2;

            if (w->wire) {
                heap_free(w->wire, 0);
                w->wire = 0;
            }
            ent = &((EdenWireEnt *)eden_wire_edit_models)[w->sel];
            w->model = (void *)*((int *)pnb_get_entry(w->bank, ent->model));
            if (ent->motion)
                w->motion = (unsigned int *)*((int *)pnb_get_entry(w->bank, ent->motion));
            else
                w->motion = 0;
            ent0 = *((int *)pnb_get_entry(w->bank, eden_wire_edit_models[4][0]));
            ent1 = *((int *)pnb_get_entry(w->bank, eden_wire_edit_models[4][1]));
            ent2 = *((int *)pnb_get_entry(w->bank, eden_wire_edit_models[4][2]));
            w->data = model_load_wire_data((void *)*((int *)pnb_get_entry(w->bank, ent->model)));
            w->wire = (void *)((int (*)(int, int, void *, void *, int, char *, float))wire_create_none)(ent0, ent1, w->model, w->data, ent2, 0, cm_wire_at_rate);
            model_free_block(w->data);
            w->data = 0;
            w->model2 = w->model;
        } else {
            int ent0, ent1, ent2;

            if (w->data) {
                model_free_block(w->data);
                w->data = 0;
            }
            ent = &((EdenWireEnt *)eden_wire_edit_models)[w->sel];
            w->model = (void *)*((int *)pnb_get_entry(w->bank, ent->model));
            if (ent->motion)
                w->motion = (unsigned int *)*((int *)pnb_get_entry(w->bank, ent->motion));
            else
                w->motion = 0;
            ent0 = *((int *)pnb_get_entry(w->bank, eden_wire_edit_models[4][0]));
            ent1 = *((int *)pnb_get_entry(w->bank, eden_wire_edit_models[4][1]));
            ent2 = *((int *)pnb_get_entry(w->bank, eden_wire_edit_models[4][2]));
            w->data = model_load_wire_data((void *)*((int *)pnb_get_entry(w->bank, ent->model)));
            ((int (*)(int, int, void *, void *, int, char *, float))wire_create_none)(ent0, ent1, w->model, w->data, ent2, ent->name, cm_wire_at_rate);
            if (w->data) {
                model_free_block(w->data);
                w->data = 0;
            }
        }
        break;
    }
}

/* 0x00ca2d18 */
void eden_wire_edit_man_disp(EdenWireEditManWork *w)
{
    int *ent = eden_wire_edit_models[w->sel];

    debug_set_origin(10, 10);
    debug_printf(str_eden_wire_edit_select_fmt, eden_wire_edit_mode_names[w->mode], ent[3]);
    if (w->mode == 2) {
        ulGxSetTextureList(w->tex_list);
        if (w->wire)
            model_draw_kind_list(w->model2, (int)w->wire);
    }
}

/* 0x00ca2dc0 */
void eden_wire_edit_man_release(EdenWireEditManWork *w)
{
    if (w->data)
        model_free_block(w->data);
    if (w->wire)
        heap_free(w->wire, 0);
}

/* 0x00ca2e10 */
void *eden_wire_edit_man_create(void *parent)
{
    return task_create_inherit(parent, &eden_wire_edit_man_desc, 36);
}
