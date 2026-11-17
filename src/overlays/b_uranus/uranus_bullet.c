/*
 * Uranus boss shots and laser: a ring of missiles that curve towards the player, shootable for
 * score, shattering on the course walls; the laser charges at the core, shifts the fog and fires
 * a beam that hurts on contact. Also the opening demo and the score lock table.
 */

#include "types_gw.h"
#include "b_uranus.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_status.h"
#include "fog.h"
#include "gw_varedit.h"
#include "lens.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "sound_bank.h"
#include "sound_beat.h"
#include "stage.h"
#include "task.h"
#include "texforce.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "uranus_bullet.h"
#include "uranus.h"
#include "uranus_map.h"
#include "uranus_part.h"
#include "uranus_snake.h"
#include "../common/boss_common.h"

extern char uranus_bullet_vardef[];
extern GwVarDef uranus_bullet_vardefs[];
extern char uranus_laser_vardef[];
extern GwVarDef uranus_laser_vardefs[];
extern struct GwVarNode *uranus_bullet_vars_group;
extern struct GwVarNode *uranus_laser_vars_group;
extern char score_vardef[];
extern float cmd_laser_ft;
extern struct TaskDesc uranus_1st_demo_desc;
extern BossLockDef score_lock_defs[];
extern GwVarDef score_vardefs[];
extern int score_vars_group;
extern float venus_cl_spark_sec;
extern const float venus_cl_spark_fade;
extern const float venus_cl_spark_psize;
extern const float venus_cl_spark_k1;
extern const float venus_cl_spark_k2;
extern const float venus_cl_spark_size;
extern const float venus_cl_spark_spread;
extern const float venus_cl_spark_life_min;
extern const float venus_cl_spark_life_max;
extern float venus_cl_spark_dscale;
extern const float venus_cl_spark_spd;
extern const int venus_cl_spark_max;
extern struct TaskDesc uranus_l_desc;
extern struct TaskDesc uranus_mb_man_desc;
extern int cmd_mb_num;

/* spawn per level, 24 bytes: count, angle (degrees), lifetimes (seconds) by phase */
typedef struct UranusSpawnRow {
    int count;
    float ang;                  /* 0x04 */
    float life[4];              /* 0x08 */
} UranusSpawnRow;

extern UranusSpawnRow cm_type_param[];
extern int cm_laser_color[3];
extern float uranus_l_line_k;
extern float uranus_l_draw_k;

extern float uranus_mb_fade[4];
extern struct TaskDesc venus_cl_desc;
extern float cmd_laser_st;
extern unsigned int cm_laser_fog_color;
extern float uranus_laser_fog_t;
extern float cmd_fog_t2;
extern unsigned int cm_mb_col;
extern struct TaskDesc uranus_mb_desc;
extern float cmd_mb_dt;
extern float uranus_mb_tail_sec;
extern float uranus_mb_tail_rate;
extern int uranus_mb_tail_num;
extern float uranus_mb_tail_w0;
extern float uranus_mb_tail_w1;
extern float cmd_laser_wr;
extern float uranus_l_w_min;
extern float uranus_l_w_max;
extern char uranus_1st_demo_files[][8];
extern int uranus_1st_demo_light_cols[];
extern float cmd_mb_sang_s;
extern float cmd_mb_sang_l;
extern float cmd_mb_sv;
extern float cmd_mb_sdv;
extern float cmd_mb_pr;
extern float cmd_mb_sr;
extern float cmd_mb_rzm;
extern float uranus_mb_fade_sec;

/* homing missile, 0xbc bytes */
typedef struct UranusMbWork {
    char pad_00[4];
    void *textures;             /* 0x04 */
    void *parent;
    struct UranusMapWork *map;
    CmBtailWork *tail;
    ModelPart **parts;
    Vec3 start;
    Vec3 pos;
    Vec3 target;
    Vec3 bulge;
    Vec3 vel;
    int ang[3];
    int roll_spd;
    int roll_acc;
    float wobble;
    char pad_6c[8];
    int life;                   /* 0x74 */
    int life_max;
    LockObj lock;
} UranusMbWork;

/* fires a ring of missiles after a delay, 0x24 bytes */
typedef struct UranusMbManWork {
    char pad_00[8];
    UranusPartWork *pm;         /* 0x08 */
    struct UranusMapWork *map;
    ModelPart **parts;
    Vec3 *pos;
    int wait;
    int cnt;
    int type;
} UranusMbManWork;

/* opening demo, 0x64 bytes */
typedef struct UranusDemoWork {
    int state;
    int timer;
    int timer0;
    int fog_n;
    int fog_frames;
    char pad_14[4];
    int (*cb)(int arg);         /* 0x18 */
    int cb_arg;
    BossCameraWork *cam;
    int mpb[2];
    BossFog fog_to;
    BossFog fog_from;
} UranusDemoWork;

/* laser beam, 0x60 bytes */
typedef struct UranusLWork {
    char pad_00[4];
    void *textures;             /* 0x04 */
    void *parent;
    struct UranusMapWork *map;
    char pad_10[4];
    int fade;                   /* 0x14: frames left of the fade out */
    int unk_18;                 /* 0x18: cleared by uranus_l_create, never read */
    int hit;
    int grounded;               /* 1 on the floor, 2 after hitting the player */
    Vec3 *src;
    Vec3 line[2];               /* the beam from its source to the floor */
    Vec3 dir;
    Vec3 dir0;
    ModelPart **parts;
    float width;
} UranusLWork;

/* laser charge at the core, 0x3c bytes */
typedef struct VenusClWork {
    char pad_00[8];
    UranusPartWork *pm;         /* 0x08 */
    struct UranusMapWork *map;
    CmPtclWork *spark;
    UranusLWork *laser;
    VenusFogWork *fog;
    ModelPart **parts;
    Vec3 *core;
    Vec3 dir;                   /* the spark direction and the beam's */
    int fired;
    int wait;
    int stop;
} VenusClWork;

/* 0x00ca50c8 */
void uranus_bullet_vars_add(void)
{
    uranus_bullet_vars_group = gw_varedit_add(&gw_varedit_root, 0, uranus_bullet_vardef);
    gw_varedit_add_table(&gw_varedit_root, uranus_bullet_vars_group, uranus_bullet_vardefs);
}

/* 0x00ca5118 */
void uranus_bullet_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)uranus_bullet_vardef);
}

/* 0x00ca5130 */
void uranus_mb_disp(UranusMbWork *w)
{
    Vec3 *pos = &w->pos;
    int old_blend;

    ulGxSetTextureList(w->textures);
    ulMatrixPush();
    old_blend = draw_set_override_flags(0);
    if (view_sphere_visible(pos, 10.0f)) {
        MTX_TRANS(pos);
        uranus_bullet_rotate_yxz(w->ang);
        dc_object_draw((struct MapPart *)w->parts[34]);
    }
    draw_set_override_flags(old_blend);
    ulMatrixPop();
}

/* steer(cast) */
/* 0x00ca51c8 */
void uranus_mb_disp_fade(UranusMbWork *w)
{
    Vec3 *pos = &w->pos;
    int old_blend;

    ulGxSetTextureList(w->textures);
    ulMatrixPush();
    draw_push_strip_flags();
    draw_push_override_color();
    old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    if (view_sphere_visible(pos, 10.0f)) {
        uranus_mb_fade[0] = (float)w->life / (float)w->life_max + -1.0f;
        draw_set_override_color(uranus_mb_fade);
        MTX_TRANS(pos);
        uranus_bullet_rotate_yxz(w->ang);
        dc_object_draw((struct MapPart *)w->parts[34]);
    }
    draw_set_override_flags(old_blend);
    draw_pop_strip_flags();
    draw_pop_override_color();
    ulMatrixPop();
}

/* 0x00ca52c8 */
void uranus_mb_curve(Vec3 *end, Vec3 *start, Vec3 *pos, Vec3 *bulge, float t)
{
    Vec3 step;
    float sc[2];

    v3_lerp(&step, start, end, t);
    v3_sub(&step, &step, pos);
    v3_scale(&step, t);
    v3_add(pos, &step);
    ulSinCosf(t * PI, &sc[0], &sc[1]);
    v3_mul(&step, bulge, sc[0]);
    v3_add(pos, &step);
}

/* 0x00ca53f0 */
void uranus_mb_exec_fade(UranusMbWork *w)
{
    Vec3 v;

    uranus_map_get_drift(&v);
    v3_add(&w->pos, &v);
    uranus_map_get_scroll(&v);
    v3_scale(&v, (float)w->life / (float)w->life_max);
    v3_add(&w->pos, &v);
    uranus_map_get_drift_b(&v);
    v3_add(&w->pos, &v);
    w->ang[2] += w->roll_spd;
    v3_add(&w->vel, &w->pos);
    if ((w->lock.flags & LOCK_HIT) && w->lock.life <= 0) {
        lock_score(5, &w->lock);
        cm_wire_break_simple(w, 38, w->parts[35], &w->pos, &w->vel, w->ang, cm_mb_col);
        FINISH_IF(w);
    } else if (--w->life <= 0)
        FINISH_IF(w);
}

/* 0x00ca55c8 */
void uranus_mb_exec(UranusMbWork *w)
{
    Vec3 v, sway;
    UranusPtDir ray;
    Vec3 hit, nrm, scroll;
    float t;
    int n, spd_max;

    uranus_map_get_vel_z(&v);
    v3_add(&w->target, &v);
    v3_add(&w->start, &v);
    v3_add(&w->pos, &v);
    v3_copy(&w->vel, &w->pos);
    player_get_pos(&v);
    v.x = w->target.x;
    v.y = w->target.y;
    v3_lerp(&w->target, &w->target, &v, cmd_mb_pr);
    for (n = enemy_status_get_ticks(); n; n--) {
        spd_max = DEG_TO_ANG(cmd_mb_rzm);
        w->roll_spd += w->roll_acc;
        t = 1.0f - (float)w->life / (float)w->life_max;
        if (spd_max < w->roll_spd)
            w->roll_spd = spd_max;
        w->ang[2] += w->roll_spd;
        uranus_mb_curve(&w->start, &w->target, &w->pos, &w->bulge, t);
        wobble_step_slow(&sway, cmd_mb_sr * (1.0f - t), &w->wobble);
        v3_add(&w->pos, &sway);
        v3_copy(&ray.p, &w->pos);
        if (uranus_map_col_check(w->map, &ray.p, &hit.x, &nrm.x, 5.0f, 60)) {
            v3_sub(&w->vel, &w->vel, &w->pos);
            player_get_frame_vel(&scroll);
            v3_sub(&scroll, &w->pos, &scroll);
            cm_wire_break_simple(w, 38, w->parts[35], &scroll, &w->vel, w->ang, cm_mb_col);
            FINISH_IF(w);
            return;
        }
        if (--w->life <= 0) {
            TASK_OF(w)->exec = (void (*)(void *))uranus_mb_exec_fade;
            TASK_OF(w)->disp = (void (*)(void *))uranus_mb_disp_fade;
            cm_btail_stop(w->tail);
            w->life = w->life_max = uranus_mb_fade_sec * FPS;
        }
    }
    {
        v3_sub(&w->vel, &w->vel, &w->pos);
        do {
            w->ang[1] = RAD_TO_ANG(atan2f(-w->vel.x, -w->vel.z));
            w->ang[0] = RAD_TO_ANG(atan2f(w->vel.y, sqrtf(w->vel.x * w->vel.x + w->vel.z * w->vel.z)));
        } while (0);
        if (player_sphere_test(&w->pos, 1.0f) == 1)
            uranus_player_damage();
        else if (!uranus_pm_is_end(w->parent)) {
            if (!(w->lock.flags & LOCK_HIT) || 0 < w->lock.life)
                return;
            boss_se_play(15, (int)&w->lock, 0);
            lock_score(5, &w->lock);
        }
        player_get_frame_vel(&hit);
        v3_sub(&hit, &w->pos, &hit);
        cm_wire_break_simple(w, 38, w->parts[35], &hit, &w->vel, w->ang, cm_mb_col);
    }
    FINISH_IF(w);
}

/* 0x00ca5b60 */
void uranus_mb_release(UranusMbWork *w)
{
    lock_remove_entry(5, &w->lock);
}

/* 0x00ca5b70 */
UranusMbWork *uranus_mb_create(UranusPartWork *parent, struct UranusMapWork *map, Vec3 *pos, Vec3 *vel, ModelPart **parts, int type)
{
    UranusMbWork *w = task_create_inherit(parent, &uranus_mb_desc, 188);
    BossRibbonDef tail;
    float life, rnd;
    int n;

    v3_copy(&w->pos, pos);
    w->map = map;
    w->parts = parts;
    w->parent = parent;
    v3_copy(&w->start, &w->pos);
    v3_copy(&w->bulge, vel);
    player_get_pos(&w->target);
    w->wobble = RAND_FLOAT_A() * 2.0f * PI;
    n = boss_get_level();
    life = cm_type_param[type].life[n];
    rnd = RAND_FLOAT_A();
    n = (life + rnd * cmd_mb_dt) * FPS;
    w->roll_acc = DEG_TO_ANG(cm_type_param[type].ang);
    w->life_max = n;
    w->life = n;
    lock_init(5, &w->lock, w, &w->pos, 0, 1, 0);
    lockon_entry(&w->lock);
    tail.color = COL_WHITE;
    tail.rate = uranus_mb_tail_rate;
    tail.secs = uranus_mb_tail_sec;
    tail.tex = 3;
    tail.n = uranus_mb_tail_num;
    tail.width_a = uranus_mb_tail_w0;
    tail.width_b = uranus_mb_tail_w1;
    w->tail = cm_btail_create(parent, w, &w->pos, &tail);
    return w;
}

/* 0x00ca5dc8 */
void uranus_mb_man_exec(UranusMbManWork *w)
{
    Vec3 pl, org, dir;
    int ang[3];
    Vec3 v;
    float sc[8];
    float z;
    int i;

    if (uranus_pm_is_end(w->pm))
        goto done;
    if (--w->wait > 0)
        return;
    player_get_frame_vel(&dir);
    v3_sum(&org, &dir, w->pos);
    player_get_pos(&pl);
    v3_sub(&dir, &pl, &org);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    ang[2] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
    for (i = 0; i < w->cnt; i++) {
        z = 0.0f;
        v.y = z;
        v.z = -1.0f;
        v.x = z;
        ROT_X(DEG_TO_ANG(RAND_FLOAT_A() * (cmd_mb_sang_l - cmd_mb_sang_s) + cmd_mb_sang_s), sc, v.y, v.z);
        ROT_Z((int)DEG_TO_ANG((float)(i * 360 / w->cnt)) + ang[2], (sc + 2), v.x, v.y);
        vec3_normalize(&v, &v);
        v3_scale(&v, RAND_FLOAT_A() * (cmd_mb_sv + cmd_mb_sdv - cmd_mb_sv) + cmd_mb_sv);
        ROT_X(ang[0], (sc + 4), v.y, v.z);
        ROT_Y(ang[1], (sc + 6), v.x, v.z);
        uranus_mb_create(w->pm, w->map, &org, &v, w->parts, w->type);
    }
    boss_se_play(14, 0, (int)&org);
    do {
done:
        if (w)
            FINISH(w);
    } while (0);
}

/* 0x00ca6268 */
UranusMbManWork *uranus_mb_man_create(UranusPartWork *parent, struct UranusMapWork *map, Vec3 *pos, ModelPart **parts, int delay, int type)
{
    UranusMbManWork *w = task_create_inherit(parent, &uranus_mb_man_desc, 36);

    w->pm = parent;
    w->map = map;
    w->pos = pos;
    w->parts = parts;
    w->wait = delay;
    w->type = type;
    if (type < 3) {
        w->cnt = cm_type_param[type].count;
    } else {
        w->cnt = type - 2;
        if (cmd_mb_num < w->cnt)
            w->cnt = cmd_mb_num;
        w->type = 2;
    }
    return w;
}

/* 0x00ca6340 */
void uranus_bullet_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}

/* 0x00ca6390 */
void uranus_1st_demo_exec(UranusDemoWork *w)
{
    BossFog fog;
    CmLightChangeWork *light;

    texforce_disable();
    switch (w->state) {
    case 0:
        w->cam = boss_camera_create(w, 1.0f, 0, 0, 0, 88, 1, 1, 1);
        boss_camera_follow(w->cam, 60);
        boss_cam_ang_create(w->cam, 0, 13653, 0, 60);
        w->timer = 60;
        w->timer0 = 60;
        w->mpb[0] = sound_mpb_load_create_bpms(uranus_1st_demo_files[0], 1);
        w->mpb[1] = sound_mpb_load_create_bpms(uranus_1st_demo_files[1], 2);
        w->state++;
    case 1:
        w->timer--;
        if (w->mpb[0] && (TASK_OF(w->mpb[0])->flags & TASK_F_END))
            w->mpb[0] = 0;
        if (w->mpb[1] && (TASK_OF(w->mpb[1])->flags & TASK_F_END))
            w->mpb[1] = 0;
        if (w->mpb[0] || w->mpb[1])
            break;
        w->state++;
        break;
    case 2:
        if (--w->timer > 0)
            break;
        uranus_get_fog((int *)&w->fog_to);
        fog_get(&w->fog_from);
        w->timer = 60;
        w->timer0 = 60;
        w->state++;
        light = cm_light_change_create(0.0f, 2.0f);
        if (light)
            cm_light_change_set_target(light, uranus_1st_demo_light_cols);
        w->fog_frames = 72;
        w->fog_n = 72;
    case 3:
        if (--w->timer <= 0) {
            w->state++;
            cm_sc_cross_create(w, 255, 0.0f, 1.5f);
            cm_sc_blur_create(w, 1.0f, 200.0f, 0.0f, 1.2f);
        } else if (w->timer == 2)
            stage_end_child();
        break;
    case 4:
        if (beat_grid_hit(0) && (!w->cb || w->cb(w->cb_arg))) {
            w->timer = 90;
            boss_vol_play(0, 0);
            w->state++;
        }
        break;
    case 5:
        if (w->timer)
            w->timer--;
        if (w->fog_n)
            break;
        if (w->timer)
            break;
        if (beat_grid_hit(0)) {
            if (light_mode_is_fog())
                do {
                    peri_act_beat_tick();
                } while (0);
            FINISH(w);
        }
        break;
    }
    if (w->fog_n) {
        w->fog_n--;
        fog_blend(&w->fog_from, &w->fog_to, &fog, (float)w->fog_n / (float)w->fog_frames);
        fog_apply(&fog, 4);
    }
}

/* 0x00ca66d0 */
UranusDemoWork *uranus_1st_demo_create(void *parent, int (*cb)(int arg), int cb_arg)
{
    UranusDemoWork *w = task_create(parent, &uranus_1st_demo_desc, 100);

    w->cb = cb;
    w->cb_arg = cb_arg;
    return w;
}

/* 0x00ca6718 */
void uranus_laser_vars_add(void)
{
    uranus_laser_vars_group = gw_varedit_add(&gw_varedit_root, 0, uranus_laser_vardef);
    gw_varedit_add_table(&gw_varedit_root, uranus_laser_vars_group, uranus_laser_vardefs);
}

/* 0x00ca6768 */
void uranus_laser_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)uranus_laser_vardef);
}

/* 0x00ca6780 */
void uranus_l_exec(UranusLWork *w)
{
    Vec3 end, unused, hit, v, pl, near; /* steer(dead): unused keeps the stack layout */
    float rate;

    dc_object_advance_time((struct MapPart *)w->parts[36]);
    if (!w->fade) {
        float w_max;

        rate = cmd_laser_wr;
        w_max = uranus_l_w_max;
        w->width = w->width * rate + w_max * (1.0f - rate);
        v3_mul(&w->dir, &w->dir0, w->width / w_max);
    } else {
        w->width *= cmd_laser_wr;
        v3_mul(&w->dir, &w->dir0, w->width / uranus_l_w_max);
        if (w->width < uranus_l_w_min) {
            FINISH_IF(w);
            return;
        }
        if (--w->fade <= 0) {
            FINISH_IF(w);
            return;
        }
    }
    v3_copy(&w->line[0], w->src);
    v3_copy(&v, &w->dir);
    vec_set_length(&v, w->width);
    v3_add(&w->line[0], &v);
    v3_copy(&end, w->src);
    v3_copy(&w->line[1], w->src);
    v3_add(&end, &w->dir);
    if (uranus_map_col_check(w->map, &end, &hit.x, &v.x, 0.0f, 1072)) {
        w->line[1].y = hit.y + w->width;
        if (!w->grounded)
            w->grounded = 1;
    }
    if (w->hit)
        return;
    player_get_pos(&pl);
    if (segment_point_near(&w->line[0], &w->line[1], &pl, &near, w->width)) {
        uranus_player_damage();
        w->hit = 1;
        if (!w->fade)
            w->fade = cmd_laser_ft * FPS;
        w->grounded = 2;
    }
}

/* 0x00ca6a68 */
void uranus_l_disp(UranusLWork *w)
{
    WideLineStyle line;
    float old_k;
    int old_blend;

    ulGxSetTextureList(w->textures);
    old_k = polyline_set_width_min(uranus_l_draw_k);
    draw_set_blend(4, 1);
    line.col[0] = cm_laser_color[0];
    line.col[1] = cm_laser_color[1];
    line.col[2] = cm_laser_color[2];
    line.width = w->width;
    line.k = uranus_l_line_k;
    draw_polyline_wide_capped((int)&line, w->line, 1);
    polyline_set_width_min(old_k);
    old_blend = draw_set_override_flags(0);
    ulMatrixPush();
    MTX_TRANS(&w->line[1]);
    dc_object_draw((struct MapPart *)w->parts[36]);
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00ca6b50 */
void uranus_l_release(UranusLWork *w)
{
}

/* 0x00ca6b58 */
void uranus_l_stop(UranusLWork *w)
{
    w->fade = cmd_laser_ft * FPS;
}

/* 0x00ca6b78 */
UranusLWork *uranus_l_create(void *parent, struct UranusMapWork *map, Vec3 *src, Vec3 *dir, ModelPart **parts)
{
    UranusLWork *w = task_create_inherit(parent, &uranus_l_desc, 96);

    v3_copy(&w->dir0, dir);
    w->parent = parent;
    parts[36]->frame = 0.0f;
    w->map = map;
    w->src = src;
    w->parts = parts;
    w->unk_18 = 0;
    w->fade = 0;
    return w;
}

/* 0x00ca6c18 */
void venus_cl_spark_def(BossSparkDef *def)
{
    float a = venus_cl_spark_sec;
    float b = venus_cl_spark_spd;
    float dscale = venus_cl_spark_dscale;
    float psize = venus_cl_spark_psize;
    float spark_k = venus_cl_spark_k1;
    float size = venus_cl_spark_size;
    float g = venus_cl_spark_k2;
    int count_max = venus_cl_spark_max;
    float i = venus_cl_spark_spread;
    float j = venus_cl_spark_life_max;
    float k = venus_cl_spark_life_min;
    float fade = venus_cl_spark_fade;

    def->secs = a;
    def->dscale = dscale;
    def->speed = b;
    def->psize = psize;
    def->k1 = spark_k;
    def->size = size;
    def->k2 = g;
    def->max = count_max;
    def->spread = i;
    def->life_max = j;
    def->life_min = k;
    def->fade = fade;
}

/* 0x00ca6cb0 */
void venus_cl_exec(VenusClWork *w)
{
    BossFog fog;

    if (w->fog && (TASK_OF(w->fog)->flags & TASK_F_END))
        w->fog = 0;
    if (w->laser && (TASK_OF(w->laser)->flags & TASK_F_END))
        w->laser = 0;
    if (w->spark && (TASK_OF(w->spark)->flags & TASK_F_END))
        w->spark = 0;
    if (uranus_pm_get_core(w->pm, 0, 0) || uranus_pm_is_end(w->pm) || w->stop) {
        if (w->spark)
            ((void (*)(CmPtclWork *, int))cm_ptcl_hide)(w->spark, 1); /* steer(cast) */
        if (w->laser)
            ((void (*)(UranusLWork *, int))uranus_l_stop)(w->laser, 1); /* steer(cast) */
        if (w->fog) {
            task_end_with_exit(w->fog);
            w->fog = 0;
        }
        w->fired = 1;
    }
    if (!w->laser && !w->spark) {
        uranus_get_fog((int *)&fog);
        w->fog = venus_fog_create(w->pm, &fog, cmd_fog_t2);
        FINISH_IF(w);
        return;
    }
    if (!w->fired && --w->wait <= 0) {
        w->laser = uranus_l_create(w, w->map, w->core, &w->dir, w->parts);
        w->fired++;
    }
}

/* 0x00ca6e30 */
VenusClWork *venus_cl_create(UranusPartWork *parent, ModelPart **parts)
{
    VenusClWork *w = task_create_inherit(parent, &venus_cl_desc, 60);
    float prm[16];
    BossFog fog;
    CmPtclWork *spark;

    w->pm = parent;
    w->map = uranus_pm_get_map(parent);
    w->parts = parts;
    uranus_pm_get_core(w->pm, &w->core, 0);
    w->dir.x = w->dir.z = 0.0f;
    w->dir.y = -500.0f;
    venus_cl_spark_def((BossSparkDef *)prm);
    ((int *)prm)[9] = 1; /* steer(alias): def.tex */
    spark = cm_ptcl_create(w, w->core, &w->dir, (BossSparkDef *)prm);
    w->wait = (int)(cmd_laser_st * FPS);
    w->spark = spark;
    fog.col = ((struct { unsigned int c; } *)&cm_laser_fog_color)->c; /* steer(alias): stays below the stores to w */
    fog.back = cm_laser_fog_color;
    fog.near = fog_get_near();
    fog.far = fog_get_far();
    w->fog = venus_fog_create(w->pm, &fog, uranus_laser_fog_t);
    boss_se_play(18, 0, (int)w->core);
    return w;
}

/* 0x00ca6f48 */
void venus_cl_stop(VenusClWork *w)
{
    w->stop = 1;
}

/* steer(cast) */
/* 0x00ca6f58 */
void score_vars_add(void)
{
    lock_table_set(score_lock_defs, 7);
    score_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, score_vardef);
    gw_varedit_add_table(&gw_varedit_root, score_vars_group, score_vardefs);
}

/* 0x00ca6fb8 */
void score_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)score_vardef);
}
