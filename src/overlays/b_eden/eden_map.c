/*
 * Eden boss map: arena models and the player's path. Sets the player camera presets, carries the
 * player along the passes, changes the fog per map and breaks map models up on request.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "color.h"
#include "config.h"
#include "debug.h"
#include "game.h"
#include "gw_varedit.h"
#include "libc.h"
#include "light.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "player_camera.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_map.h"
#include "eden.h"
#include "eden_break.h"
#include "eden_laser.h"
#include "eden_snake.h"
#include "../common/boss_common.h"
#include "../common/boss_texture_anime.h"
#include "../common/boss_wire.h"

/* 264 bytes */
typedef struct EdenMapWork {
    struct PnbHead *pack;
    struct UlGxTexList *tex_list;
    ModelPart **parts;
    Vec3 pos;
    char pad_18[12];
    Vec3 dir;                   /* 0x24 */
    Vec3 unk_30;                /* 0x30: set to (0, 0, -1) at creation, not read here */
    int ang[3];
    int roll;
    int pass_ang;
    unsigned int *mot;
    unsigned int *pass_mot[6];  /* [5]: the start motion */
    float frm;
    float mot_end;
    float frm_end;
    float rot;                  /* radians around the arena */
    float rot_spd;
    float rot_spd_to;
    int rot_ease;
    int target;
    int unk_8c;                 /* 0x8c: cycles 0..2 on cmd_ch_sw */
    int map_num;
    int move;
    int mode;
    int turn;
    int pass;
    int turn_time;
    int turn_len;
    int move_time;
    int move_len;
    int unk_b4;                 /* 0xb4: written once, never read */
    int spin[3];
    int spin_spd[3];
    EdenFogKey *fog;
    BossCameraWork *cam;
    float body_frm;
    float body_frm_end;
    Vec3 gravity;
    Vec3 wind;
    int wind_ang;
    int col_time;
    int col_len;
    int hide;
} EdenMapWork;

extern char eden_map_vardef[];
extern EdenMapWork *eden_map_work;
extern float cmd_sc_sx;
extern float cmd_sc_sy;
extern float cmd_sc_sw;
extern float cmd_sc_sh;
extern struct GwVarNode *eden_map_vars_group;
extern GwVarDef eden_map_vardefs[];
extern float cmd_map_st_t;
extern float cmd_map_st_ft;
extern float eden_map_level_frames[];
extern float cmd_pass_ry;
extern float cmd_pass_def_fade;
extern float cmd_off_z;
extern float cmd_map_mot_v;
#define FOGKEYS ((EdenFogKey (*)[3])eden_map_fog_keys)
extern float cmd_fog_ct;
extern PlayerCameraPreset eden_map_cam_preset;
extern float cmd_circle_off_z, cmd_circle_off_y;
extern float cmd_pass_v_st, cmd_pass_frm, cmd_pass_fade_time;
extern float cmd_pass_v;
extern const float cmd_break_xz_v;
extern const float cmd_break_y_v;
extern float cmd_break_xz_a;
extern int cmd_break_sw;
extern EdenReq cmd_ch_sw;

extern float eden_map_body_fade[4];
extern int cmd_map_deb_disp_sw;
extern char str_eden_map_frm_fmt[];
extern char str_eden_map_pz_fmt[];
extern char str_eden_map_bz_fmt[];
extern struct TaskDesc eden_map_desc;
extern unsigned int eden_map_models[6];
extern float cmd_pass_ryr;
extern float cmd_pass_v2;
extern float cmd_pass_v3;
extern const float cmd_pass_fade_time2;
extern float eden_map_pass_frames[8];
extern float eden_map_pack_fade[4];
extern unsigned int cm_start_p_col[2];
extern float cmd_plmv_sz, cmd_plmv_cam_ang, cmd_plmv_cam_ang_def, cmd_plmv_cam_ang_t, eden_map_bl_t;
extern const float cmd_plmv_z;
extern const float cmd_plmv_t;
extern int eden_map_bl_rad, eden_map_bl_rate;

/* 0x00cb68c8 */
void eden_map_vars_add(void)
{
    eden_map_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_map_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_map_vars_group, eden_map_vardefs);
    eden_map_edit_fog_load();
}

/* 0x00cb6920 */
void eden_map_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_map_vardef);
}

/* 0x00cb6938 */
void eden_map_cam_preset_set(void)
{
    PlayerCameraPreset *cur_preset = player_camera_get_preset();

    if (game_type_get() != 5 || !ConfigGetSelect(8)) {
        eden_map_cam_preset = *cur_preset;
        eden_map_cam_preset.eye_limit_x_top = 16200;
        player_camera_set_data(&eden_map_cam_preset);
    }
}

/* 0x00cb6a20 */
void eden_map_cam_preset_apply(void)
{
    void *cfg = player_camera_get_preset();

    if (game_type_get() != 5 || !ConfigGetSelect(8))
        player_camera_set_data(cfg);
}

/* 0x00cb6a78 */
void eden_map_cam_preset_side(int side)
{
    PlayerCameraPreset *cur_preset = player_camera_get_preset();
    PlayerCameraPreset preset;

    if (game_type_get() != 5 || !ConfigGetSelect(8)) {
        preset = *cur_preset;
        switch (side) {
        case 0:
            preset.eye_limit_x_top = 16200;
            break;
        case 1:
            preset.eye_limit_x_top = 16200;
            preset.p_ang[0] = 3640;
            preset.cam_ang_rate_x = 0.8f;
            break;
        case -1:
            preset.eye_limit_x_top = 16200;
            preset.p_ang[0] = -3640;
            preset.cam_ang_rate_x = 0.8f;
            break;
        }
        player_camera_set_target(&preset);
    }
}

/* 0x00cb6bc0 */
void eden_map_dir_to_center(EdenMapWork *w)
{
    Vec3 *dir = &w->dir;
    float x;

    w->pos.z = cmd_off_z;
    w->pos.x = 0;
    w->pos.y = 0;
    x = w->pos.x;
    v3_copy(dir, &w->pos);
    w->dir.y = x;
    vec3_normalize(dir, dir);
    dir->x = -dir->x;
    dir->y = -dir->y;
    dir->z = -dir->z;
}

/* 0x00cb6c50 */
void eden_map_circle_move(EdenMapWork *w)
{
    float sc[2];
    float old_x;
    float rad, hgt;

    w->rot += w->rot_spd;
    if (w->rot >= TWO_PI)
        w->rot -= TWO_PI;
    rad = cmd_circle_off_z;
    hgt = cmd_circle_off_y;
    w->pos.z = rad;
    w->pos.x = 0;
    w->pos.y = hgt;
    do {
        old_x = w->pos.x;
    } while (0);
    do {
        float ua = w->pos.x, ub = w->pos.z;
        float k = ANG_PER_RAD;
        ulSinCos((int)(w->rot * k), &sc[0], &sc[1]);
        w->pos.x = ua * sc[1] + ub * sc[0];
        w->pos.z = -ua * sc[0] + ub * sc[1];
    } while (0);
    {
        Vec3 *src = &w->pos;
        Vec3 *dir = &w->dir;

        v3_copy(dir, src);
        w->dir.y = old_x;
        vec3_normalize(dir, dir);
        dir->x = -dir->x;
        dir->y = -dir->y;
        dir->z = -dir->z;
    }
}

/* 0x00cb6d80 */
void eden_map_pass_step(EdenMapWork *w)
{
    float sc[2];

    w->frm += cmd_pass_v;
    if (w->frm_end <= w->frm)
        w->frm = w->frm_end;
    motion_point_pose(w->pass_mot[w->pass], w->frm, &w->pos, w->ang);
    ROT_Y(w->pass_ang, sc, w->pos.x, w->pos.z);
    angles_turn_y(w->ang, w->pass_ang, w->ang);
    w->dir.x = w->dir.y = 0.0f;
    w->dir.z = 1.0f;
    vec_rotate_zxy(&w->dir, w->ang);
    cmd_pass_frm = w->frm;
}

/* 0x00cb6e88 */
void eden_map_pass_step_v2(EdenMapWork *w)
{
    float sc[2];
    Vec3 *dir;
    float t;

    if (w->rot_ease)
        w->rot_spd = w->rot_spd * cmd_pass_ryr + (1.0f - cmd_pass_ryr) * w->rot_spd_to;
    else
        w->rot_spd *= cmd_pass_ryr;
    if (w->frm < w->frm_end) {
        w->frm += cmd_pass_v2;
        if (w->frm_end <= w->frm)
            w->frm = w->frm_end;
    }
    t = w->rot + w->rot_spd;
    cmd_pass_frm = w->frm;
    w->rot = t;
    if (w->rot >= TWO_PI)
        w->rot -= TWO_PI;
    motion_point_pose(w->mot, w->frm, &w->pos, w->ang);
    w->pos.z += cmd_off_z;
    ROT_Y((int)(w->rot * ANG_PER_RAD), sc, w->pos.x, w->pos.z);
    dir = &w->dir;
    v3_copy(dir, &w->pos);
    w->dir.y = 0.0f;
    vec3_normalize(dir, dir);
    dir->x = -dir->x;
    dir->y = -dir->y;
    dir->z = -dir->z;
}

/* 0x00cb7058 */
void eden_map_pass_step_v3(EdenMapWork *w)
{
    float sc[2];
    Vec3 *dir;
    float t;

    if (w->rot_ease)
        w->rot_spd = w->rot_spd * cmd_pass_ryr + (1.0f - cmd_pass_ryr) * w->rot_spd_to;
    else
        w->rot_spd *= cmd_pass_ryr;
    if (w->frm < w->frm_end) {
        w->frm += cmd_pass_v3;
        if (w->frm_end <= w->frm)
            w->frm = w->frm_end;
    }
    t = w->rot + w->rot_spd;
    cmd_pass_frm = w->frm;
    w->rot = t;
    if (w->rot >= TWO_PI)
        w->rot -= TWO_PI;
    motion_point_pose(w->mot, w->frm, &w->pos, w->ang);
    w->pos.z += cmd_off_z;
    ROT_Y((int)(w->rot * ANG_PER_RAD), sc, w->pos.x, w->pos.z);
    dir = &w->dir;
    v3_copy(dir, &w->pos);
    w->dir.y = 0.0f;
    vec3_normalize(dir, dir);
    dir->x = -dir->x;
    dir->y = -dir->y;
    dir->z = -dir->z;
}

/* 0x00cb7228 */
void eden_map_pass_step_start(EdenMapWork *w)
{
    float dif, lim;

    w->frm += cmd_pass_v_st;
    if (w->frm_end <= w->frm)
        w->frm = w->frm_end;
    motion_point_pose(w->pass_mot[5], w->frm, &w->pos, w->ang);
    w->dir.x = w->dir.y = 0.0f;
    w->dir.z = 1.0f;
    vec_rotate_zxy(&w->dir, w->ang);
    dif = fabsf(w->frm - w->frm_end) / cmd_pass_v_st;
    lim = cmd_pass_fade_time * FPS;
    cmd_pass_frm = w->frm;
    if (dif <= lim)
        eden_map_turn_start(w, w->pass_mot[5], lim);
}

/* 0x00cb7330 */
void eden_map_cam_reset(void)
{
    player_swap_fullrange(1);
    eden_map_cam_preset_apply();
}

/* 0x00cb7350 */
void eden_map_spin_step(EdenMapWork *w)
{
    i3_add(w->spin, w->spin_spd);
    do {
        w->spin[0] = ANG_WRAP(w->spin[0]);
        w->spin[1] = ANG_WRAP(w->spin[1]);
        w->spin[2] = ANG_WRAP(w->spin[2]);
    } while (0);
    if (w->body_frm < w->body_frm_end) {
        w->body_frm += cmd_map_mot_v;
        if (w->body_frm_end <= w->body_frm)
            w->body_frm = w->body_frm_end;
    }
}

/* 0x00cb7418 */
void eden_map_circle_step(EdenMapWork *w, Vec3 *pos, Vec3 *dir)
{
    float sc[2];
    float old_x;

    w->rot += w->rot_spd;
    if (w->rot >= TWO_PI)
        w->rot -= TWO_PI;
    pos->x = 0;
    pos->z = cmd_circle_off_z;
    pos->y = cmd_circle_off_y;
    do {
        old_x = pos->x;
    } while (0);
    do {
        float ua = pos->x, ub = pos->z;
        float k = ANG_PER_RAD;
        ulSinCos((int)(w->rot * k), &sc[0], &sc[1]);
        pos->x = ua * sc[1] + ub * sc[0];
        pos->z = -ua * sc[0] + ub * sc[1];
    } while (0);
    v3_copy(dir, pos);
    dir->y = old_x;
    vec3_normalize(dir, dir);
    dir->x = -dir->x;
    dir->y = -dir->y;
    dir->z = -dir->z;
}

/* 0x00cb7540 */
void eden_map_circle_opposite(EdenMapWork *w, Vec3 *pos, Vec3 *dir)
{
    float sc[2];
    float old_x;

    pos->x = 0;
    pos->z = cmd_circle_off_z;
    pos->y = cmd_circle_off_y;
    do {
        old_x = pos->x;
    } while (0);
    do {
        float ua = pos->x, ub = pos->z;
        float k = ANG_PER_RAD;
        ulSinCos((int)(w->rot * k) + ANG_180, &sc[0], &sc[1]);
        pos->x = ua * sc[1] + ub * sc[0];
        pos->z = -ua * sc[0] + ub * sc[1];
    } while (0);
    v3_copy(dir, pos);
    dir->y = old_x;
    vec3_normalize(dir, dir);
    dir->x = -dir->x;
    dir->y = -dir->y;
    dir->z = -dir->z;
}

/* 0x00cb7640 */
void eden_map_turn_start(EdenMapWork *w, void *mot, int n)
{
    Vec3 pt;
    int ang[3];
    float k;

    if (w->turn)
        return;
    w->turn_len = n;
    w->turn = 1;
    w->turn_time = n;
    motion_point_pose(mot, w->frm_end, &pt, ang);
    ang[1] = RAD_TO_ANG(atan2f(-pt.x, -pt.z));
    k = 1.0f / (cmd_pass_ry * FPS) * TWO_PI;
    w->rot_ease = 1;
    w->rot_spd = w->rot_spd_to = k;
    w->rot = ang[1] * RAD_PER_ANG - k * n;
}

/* 0x00cb7748 */
void eden_map_turn_start_rel(EdenMapWork *w, void *mot, int n)
{
    Vec3 pt;
    int ang[3];
    int cur_ang[3];
    float full, tau;

    if (w->turn)
        return;
    w->turn_len = n;
    w->turn = 2;
    w->turn_time = n;
    full = ANG_TURN;
    motion_point_pose(mot, 0.0f, &pt, ang);
    tau = TWO_PI;
    ang[1] = atan2f(-pt.x, -pt.z) * full / tau;
    cur_ang[1] = (int)(atan2f(-w->pos.x, -w->pos.z) * full / tau)
                 + (int)(w->rot_spd * n * ANG_PER_RAD);
    w->mode = EDEN_MAP_PASS;
    w->pass_ang = cur_ang[1] - ang[1];
}

/* 0x00cb7868 */
void eden_map_set_default(EdenMapWork *w)
{
    float spd;

    w->turn = 3;
    w->turn_time = w->turn_len = cmd_pass_def_fade * FPS;
    spd = 1.0f / (cmd_pass_ry * FPS) * TWO_PI;
    w->rot_spd = w->rot_spd_to = spd;
}

/* 0x00cb78d0 */
void eden_map_cam_step(EdenMapWork *w)
{
    switch (w->turn) {
    case 1: {
        Vec3 pos, dir;
        float k;

        if (--w->turn_time <= 0) {
            w->turn = 0;
            w->mode = EDEN_MAP_CIRCLE;
            eden_set_flags(4);
            boss_vol_play(0, 0);
            peri_act_set_selection(9);
        }
        eden_map_circle_step(w, &pos, &dir);
        k = (float)w->turn_time / (float)w->turn_len;
        v3_lerp(&w->pos, &w->pos, &pos, k);
        v3_lerp(&w->dir, &w->dir, &dir, k);
        break;
    }
    case 2: {
        Vec3 pos, dir;
        float k;

        if (--w->turn_time <= 0)
            w->turn = 0;
        eden_map_circle_step(w, &pos, &dir);
        k = 1.0f - (float)w->turn_time / (float)w->turn_len;
        v3_lerp(&w->pos, &w->pos, &pos, k);
        v3_lerp(&w->dir, &w->dir, &dir, k);
        break;
    }
    case 3: {
        Vec3 pos, dir;
        float k;

        if (--w->turn_time <= 0) {
            w->turn = 0;
            w->mode = EDEN_MAP_CIRCLE;
            eden_map_circle_opposite(w, &pos, &dir);
            w->rot += PI;
        } else
            eden_map_circle_opposite(w, &pos, &dir);
        k = (float)w->turn_time / (float)w->turn_len;
        v3_lerp(&w->pos, &w->pos, &pos, k);
        v3_lerp(&w->dir, &w->dir, &dir, k);
        break;
    }
    }
}

/* steer(cast) */
/* 0x00cb7c20 */
void eden_map_player_move(EdenMapWork *map)
{
    float ny = 0.0f;
    float t, k;
    int n;
    Vec3 ofs;
    Vec3 v;

    switch (map->mode) {
    case EDEN_MAP_CENTER:
        eden_map_dir_to_center(map);
        break;
    case EDEN_MAP_CIRCLE:
        eden_map_circle_move(map);
        break;
    case EDEN_MAP_PASS:
        eden_map_pass_step(map);
        break;
    case EDEN_MAP_PATH:
        eden_map_pass_step_v2(map);
        break;
    case EDEN_MAP_PATH_END:
        eden_map_pass_step_v3(map);
        break;
    case EDEN_MAP_START:
        eden_map_pass_step_start(map);
        break;
    case EDEN_MAP_HOLD:
        break;
    }
    eden_map_cam_step(map);
    switch (map->move) {
    case 1:
        FINISH_IF(map->cam);
        n = (int)(beat_clock_get_interval() * 4.0f);
        map->cam = 0;
        map->cam = boss_camera_create(map, 1.0f, 0, 0, 0, 24, 1, 1, 1);
        boss_camera_follow(map->cam, n);
        boss_cam_ang_create(map->cam, 0, (int)DEG_TO_ANG(cmd_plmv_cam_ang), n, (int)(cmd_plmv_cam_ang_t * FPS));
        boss_cam_ang_create(map->cam, (int)DEG_TO_ANG(cmd_plmv_cam_ang), (int)DEG_TO_ANG(cmd_plmv_cam_ang_def), (int)(cmd_plmv_cam_ang_t * FPS + n), (int)((cmd_plmv_t + cmd_plmv_t - cmd_plmv_cam_ang_t) * FPS));
        map->move_len = n;
        map->move_time = n;
        map->move++;
    case 2:
        if (--map->move_time > 0)
            break;
        map->move++;
    case 3:
        map->unk_b4 = (int)(cmd_plmv_cam_ang_t * FPS);
        map->move++;
        map->move_time = map->move_len = (int)(cmd_plmv_t * FPS);
        boss_pl_di_create(map, 0, 0);
        cm_sc_blur_create(map, (float)eden_map_bl_rad, (float)eden_map_bl_rate, 0.0f, eden_map_bl_t);
    case 4:
        t = (float)map->move_time / (float)map->move_len;
        map->move_time--;
        ofs.x = ofs.y = 0.0f;
        ofs.z = -cmd_plmv_z * (1.0f - t);
        v3_add(&map->pos, &ofs);
        ny = -map->pos.z;
        if (map->move_time <= 0) {
            map->move++;
            map->move_time = map->move_len;
            eden_map_fog_change(map, map->map_num + 1);
        }
        break;
    case 5: {
        float u = (float)map->move_time / (float)map->move_len;
        float depth = cmd_plmv_z;

        map->move_time--;
        ofs.x = ofs.y = 0.0f;
        ofs.z = depth * (u * u);
        v3_add(&map->pos, &ofs);
        if (map->move_time <= 0) {
            void *child = map->cam;

            map->move = 0;
            FINISH_IF(child);
            map->cam = 0;
        }
        break;
    }
    case 6: {
        float u = (float)map->move_time / (float)map->move_len;
        float depth = cmd_plmv_sz;

        map->move_time--;
        ofs.x = ofs.y = 0.0f;
        ofs.z = depth * (u * u);
        v3_add(&map->pos, &ofs);
        if (map->move_time <= 0) {
            void *child = map->cam;

            map->move = 0;
            FINISH_IF(child);
            map->cam = 0;
        }
        break;
    }    }
    player_set_cam_pos(&map->pos);
    player_set_cam_dir(&map->dir);
    player_set_cam_roll(map->roll);
    {
        float z, dir_z;

        do { z = 0.0f; dir_z = -1.0f; } while (0);
        do {
            v.y = z;
            v.x = z;
            v.z = dir_z;
        } while (0);
        player_set_view_dir(&v);
        v.y = z;
        v.x = z;
        v.z = ny;
    }
    player_set_view_pos(&v);
}

/* 0x00cb80c8 */
void eden_map_exec(EdenMapWork *w)
{
    float sc[2];

    if (w->hide)
        w->hide--;
    eden_map_player_move((char *)w);
    ((void (*)(EdenMapWork *, ModelPart **, EdenFogKey *))eden_map_spin_step)(w, w->parts, w->fog); /* steer(cast) */
    w->wind_ang += (int)DEG_TO_ANG(cmd_break_xz_a);
    w->wind.x = 0;
    w->wind.z = cmd_break_xz_v;
    w->wind.y = cmd_break_y_v;
    ROT_Y(w->wind_ang, sc, w->wind.x, w->wind.z);
    if (w->col_time)
        w->col_time--;
    if (cmd_ch_sw) {
        cmd_ch_sw = 0;
        if (++w->unk_8c >= 3)
            w->unk_8c = 0;
    }
    if (cmd_break_sw) {
        cmd_break_sw = 0;
        eden_break_create(w, (int *)w->parts[64]);
    }
}

/* steer(cast) */
/* 0x00cb8208 */
void eden_map_pack_draw(EdenMapWork *w)
{
    float t;
    int old;
    float amb, dif;
    EdenFogKey *key;
    unsigned int col[3];
    float sv;

    t = eden_get_dalpha();
    light_get_intensity(&amb, &dif);
    light_set_intensity(amb, 0.0f);
    if (fabsf(t - 1.0f) < FLT_EPSILON) {
        old = draw_set_override_flags(0);
        dc_object_draw(w->parts[26]);
    } else {
        old = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        draw_push_override_color();
        draw_push_strip_flags();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        eden_map_pack_fade[0] = t + -1.0f;
        draw_set_override_color(eden_map_pack_fade);
        dc_object_draw(w->parts[26]);
        draw_pop_override_color();
        draw_pop_strip_flags();
    }
    light_set_intensity(amb, dif);
    key = w->fog;
    if (w->col_time) {
        float k = 1.0f - (float)w->col_time / (float)w->col_len;

        col[0] = color_lerp(key->m_col, cm_start_p_col[0], k);
        col[1] = color_lerp(key->m_bcol, cm_start_p_col[1], k);
        col[2] = boss_color_blend(col[0], col[1], beat_curve(1));
    } else {
        col[0] = (key->m_col & 0xffffff) | ((int)(color_get_a(key->m_col) * t) + 1) >> 1 << 24;
        col[1] = (key->m_bcol & 0xffffff) | ((int)(color_get_a(key->m_bcol) * t) + 1) >> 1 << 24;
        col[2] = color_lerp(col[0], col[1], beat_curve(1));
    }
    sv = line_width_set(2.0f);
    draw_set_color(col[2]);
    dc_object_draw(w->parts[27]);
    line_width_set(sv);
    draw_set_override_flags(old);
}

/* steer(cast) */
/* 0x00cb8468 */
void eden_map_body_draw(EdenMapWork *w)
{
    float t;
    int old;
    float amb, dif;
    EdenFogKey *key;
    int col[2];

    t = eden_get_dalpha();
    old = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    light_get_intensity(&amb, &dif);
    light_set_intensity(amb, 0.0f);
    draw_push_override_color();
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    eden_map_body_fade[0] = t + -1.0f;
    draw_set_override_color(eden_map_body_fade);
    model_object_draw(w->parts[24], w->body_frm);
    draw_pop_override_color();
    draw_pop_strip_flags();
    light_set_intensity(amb, dif);
    key = w->fog;
    col[0] = (key->m_col & 0xffffff) | ((int)(color_get_a(key->m_col) * t) + 1) >> 1 << 24;
    col[1] = (key->m_bcol & 0xffffff) | ((int)(color_get_a(key->m_bcol) * t) + 1) >> 1 << 24;
    part_draw_beat_frame(w->parts[25], w->body_frm, col[0], col[1], 2.0f);
    draw_set_override_flags(old);
}

/* 0x00cb85e0 */
void eden_map_disp(EdenMapWork *w)
{
    Vec3 v;

    ulGxSetTextureList(w->tex_list);
    if (w->hide)
        return;
    if (w->fog->unk_00 == 2)
        eden_map_pack_draw(w);
    else
        eden_map_body_draw(w);
    if (!debug_flags_test(4) || !cmd_map_deb_disp_sw)
        return;
    debug_set_origin(10, 10);
    debug_printf(str_eden_map_frm_fmt, (int)w->frm, (int)(w->rot * 57.2957916f));
    if (w->mode == EDEN_MAP_PASS) {
        ulMatrixPush();
        vu0_matrix_rotate_y(ulMatrixCur, w->pass_ang);
        motion_path_draw(w->pass_mot[w->pass]);
        ulMatrixPop();
    } else if (w->mode == EDEN_MAP_PATH || w->mode == EDEN_MAP_PATH_END)
        motion_path_draw(w->mot);
    player_get_vec(&v);
    debug_set_origin(10, 11);
    debug_printf(str_eden_map_pz_fmt, (int)v.z);
    player_get_frame_accel(&v);
    debug_set_origin(10, 12);
    debug_printf(str_eden_map_bz_fmt, (int)v.z);
}

/* 0x00cb8758 */
int eden_map_create(void *parent, void *arg)
{
    EdenMapWork *w = task_create_inherit(parent, &eden_map_desc, 264);
    unsigned int i;
    Vec3 *dir;

    w->parts = arg;
    eden_map_work = w;
    w->hide = 4;
    dir = &w->unk_30;
    player_cell_move_clear();
    player_swap_fullrange(0);
    eden_map_cam_preset_set();
    w->target = 1;
    w->mot = pnb_get_entry(w->pack, 85)->src;
    for (i = 0; i < 6; i++)
        w->pass_mot[i] = pnb_get_entry(w->pack, eden_map_models[i])->src;
    w->mot_end = w->mot[1] - 1;
    do {
        w->unk_30.x = dir->y = 0.0f;
        w->unk_30.z = -1.0f;
    } while (0);
    eden_map_fog_change(w, w->map_num);
    eden_map_task = w;
    cm_wire_break_set_drift(&w->gravity, &w->wind);
    task_set_dlist(w, 3);
    return (int)w;
}

/* 0x00cb88d0 */
void eden_map_set_turn(EdenMapWork *w, float x, float y)
{
    w->rot = x * RAD_PER_DEG;
    w->rot_spd = w->rot_spd_to = y * RAD_PER_DEG;
}

/* 0x00cb88f8 */
void eden_map_request(EdenMapWork *w)
{
    if (!w->move)
        w->move = 1;
}

/* 0x00cb8910 */
void eden_map_state_start(EdenMapWork *w)
{
    w->move = 6;
    w->move_time = w->move_len = cmd_map_st_t * FPS;
    w->col_time = w->col_len = cmd_map_st_ft * FPS;
}

/* 0x00cb8960 */
void eden_map_set_target(EdenMapWork *w, int tgt)
{
    w->target = tgt;
}

/* 0x00cb8968 */
void eden_map_fog_change(EdenMapWork *w, int n)
{
    EdenFogKey *k, *key;
    BossFog fog;
    int i;
    int *p;
    float *fade;

    w->map_num = n;
    if (n >= 4)
        w->map_num = 4;
    w->fog = FOGKEYS[w->map_num];
    key = w->fog;
    fade = (float *)w->spin; /* steer(type) */
    p = w->spin_spd;
    i = 2;
    k = key;
    do {
        *p = DEG_TO_ANG(key->deg);
        if (!*p)
            *fade = 0.0f;
        fade++;
        p++;
        key++;
    } while (--i >= 0);
    fog.near = k->near;
    fog.far = k->far;
    fog.back = fog.col = k->col;
    eden_fog_create(w, &fog, cmd_fog_ct);
}

/* 0x00cb8a38 */
int eden_map_get_mode(EdenMapWork *w)
{
    if (!w)
        w = eden_map_work;
    return w->map_num;
}

/* 0x00cb8a50 */
int eden_map_get_pass(EdenMapWork *w)
{
    if (!w)
        w = eden_map_work;
    return w->pass;
}

/* 0x00cb8a68 */
void eden_map_pass_change(EdenMapWork *w, int mode, int pass)
{
    unsigned int *mot;

    w->pass = pass;
    w->mode = mode;
    if (w->cam)
        FINISH(w->cam);
    *(int *)&w->cam = 0; /* steer(type) */
    switch (mode) {
    case EDEN_MAP_CENTER:
        w->rot_ease = 0;
        w->unk_8c = 0;
        w->body_frm = w->body_frm_end = 0.0f;
        break;
    case EDEN_MAP_CIRCLE:
        w->rot_spd = w->rot_spd_to = 1.0f / (cmd_pass_ry * FPS) * TWO_PI;
        w->rot_ease = 1;
        break;
    case EDEN_MAP_START:
        w->frm = 0;
        w->frm_end = (float)w->pass_mot[5][1] - 1.0f;
        break;
    case EDEN_MAP_PASS:
        w->frm = 0;
        mot = w->pass_mot[w->pass];
        w->frm_end = (float)mot[1] - 1.0f;
        eden_map_turn_start_rel(w, w->pass_mot[w->pass], (int)(cmd_pass_fade_time2 * FPS));
        break;
    case EDEN_MAP_PATH:
        {
            float start = eden_map_pass_frames[w->pass];
            float end = eden_map_pass_frames[w->pass + 1];

            w->frm = start;
            w->frm_end = end;
        }
        break;
    case EDEN_MAP_PATH_END:
        w->rot_ease = 0;
        *(int *)&w->rot_spd = 0; /* steer(type) */
        {
            float start = eden_map_pass_frames[5];
            float end = eden_map_pass_frames[6];

            w->frm_end = end;
            w->frm = start;
        }
        break;
    }
}

/* 0x00cb8c40 */
int eden_map_pass_is_end(EdenMapWork *w)
{
    int ret = 0;

    if (w->frm >= w->frm_end)
        ret = 1;
    return ret;
}

/* 0x00cb8c68 */
void eden_map_level_set(EdenMapWork *w, int n)
{
    if (n < 4) {
        int i = w->pass;
        float lv_a = eden_map_level_frames[i];
        float lv_b = eden_map_level_frames[i + 1];

        w->body_frm = lv_a;
        w->body_frm_end = lv_b;
    } else {
        w->body_frm_end = w->body_frm = eden_map_level_frames[5];
        eden_break_create(w, (int *)w->parts[64]);
    }
}

/* 0x00cb8cc8 */
void eden_map_get_fog(int *out)
{
    out[0] = out[1] = FOGKEY[0].col;
    ((float *)out)[2] = FOGKEY[0].near;
    ((float *)out)[3] = FOGKEY[0].far;
}

/* 0x00cb8cf0 */
void eden_map_screen_rect_set(void)
{
    tex_anime_set_rect(cmd_sc_sx, cmd_sc_sy, cmd_sc_sw, cmd_sc_sh);
}
