/*
 * Venus boss core, the last part: spins inside its fire pod ring; once the pods are down it opens
 * on the beat (lockable only then) and closes as they revive. Hits jolt its thunder lines. Also
 * the ending core model, intro demo, star colour setter and fog tween.
 */

#include "types_gw.h"
#include "b_venus.h"
#include "mathdef.h"
#include "color.h"
#include "console.h"
#include "debug.h"
#include "gw_varedit.h"
#include "libc.h"
#include "light.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "rez_wire_compile.h"
#include "sound_bank.h"
#include "sound_beat.h"
#include "stage.h"
#include "task.h"
#include "texforce.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "venus_core.h"
#include "venus.h"
#include "venus_bullet.h"
#include "venus_mpod.h"
#include "venus_shell.h"
#include "venus_thunder.h"
#include "../common/boss_common.h"
#include "../common/eff_star.h"

extern char venus_core_vardef[];
extern GwVarDef venus_core_vardefs[];
extern char venus_fog_vardef[];
extern GwVarDef venus_fog_vardefs[];
extern struct GwVarNode *venus_core_vars_group;
extern struct GwVarNode *venus_fog_vars_group;
extern float cmd_core_t;
extern struct TaskDesc venus_1st_demo_desc;
extern float venus_core_spin_damp;
extern unsigned int cm_wire_color2;
extern struct TaskDesc venus_end_core_desc;
extern float venus_core_lock_y;
extern float venus_core_lock_dy;
extern const float venus_core_lock_dy2;
extern BossFog venus_fog_start;
extern BossFog venus_fog_end;
extern struct TaskDesc venus_core_desc;
extern int cmd_core_hp;
extern unsigned int cm_wire_color0;
extern float venus_core_open_y;
extern float venus_core_lock_open_y;
extern float venus_core_spin_spd;
extern float venus_core_settle_y;

extern const float venus_core_settle_damp;

extern const float venus_core_open_ofs;
extern float venus_core_spring_damp;
extern float venus_core_spring_k;
extern int cmd_core_open_t;
extern float cm_level_d_pod[];
extern float venus_end_core_fade[4];
extern int venus_1st_demo_light_cols[];
extern char venus_1st_demo_files[];

/* 292 bytes */
typedef struct VenusCoreWork {
    void *pack;
    void *textures;
    Vec3 *src;
    Vec3 home;
    Vec3 pos;
    Vec3 lock_pos;
    VenusFbManWork *fb_man;
    VenusPartSet *thl_a;
    VenusPartSet *thl_b;
    int boss;
    Vec3 pt_a;
    Vec3 pt_b;
    Vec3 pt_c;
    Vec3 pt_d;
    int unk_70;                         /* 0x70: its address goes to venus_fb_man_create */
    int spin_ang;
    char pad_78[8];
    int ring_ang;                       /* 0x80 */
    char pad_84[8];
    int ring2_ang;                      /* 0x8c */
    char pad_90[8];
    int spin_spd;                       /* 0x98 */
    char pad_9c[8];
    int ring_spd;                       /* 0xa4 */
    char pad_a8[4];
    char *parts;                        /* 0xac */
    float open_y;
    float open_to;
    char pad_b8[8];
    float open_v;                       /* 0xc0 */
    int aligned;
    char pad_c8[4];
    int state;                          /* 0xcc */
    int gage_on;
    int open_cnt;
    int wait;
    int burst;
    int revive_cnt;
    LockObj lock;
} VenusCoreWork;

/* 80 bytes */
typedef struct VenusEndCoreWork {
    void *pack;
    void *textures;
    Vec3 pos;
    Vec3 vel;
    int ang[3];
    int dang[3];
    char *parts;
    unsigned int col;
    int state;
    int move_frm;
    int fade;
    int fade_len;
} VenusEndCoreWork;

/* 0x00ca5698 */
void venus_core_vars_add(void)
{
    venus_core_vars_group = gw_varedit_add(&gw_varedit_root, 0, venus_core_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_core_vars_group, venus_core_vardefs);
}

/* 0x00ca56e8 */
void venus_core_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_core_vardef);
}

/* 0x00ca5700 */
void venus_core_spin(VenusCoreWork *w)
{
    w->spin_spd *= venus_core_spin_damp;
    w->ring_spd *= venus_core_spin_damp;
    w->spin_ang += w->spin_spd;
    w->ring_ang += w->ring_spd;
    w->ring2_ang += w->ring_spd * 2;
    dc_object_advance_time(*(ModelPart **)(w->parts + 56));
}

/* 0x00ca5770 */
void venus_core_set_lock_points(VenusCoreWork *w)
{
    float half, top, dy, bottom;

    half = w->open_y * 0.5f;
    v3_copy(&w->lock_pos, &w->home);
    top = venus_core_lock_y + half;
    bottom = venus_core_lock_dy2 - half;
    w->lock_pos.y += top;
    v3_copy(&w->pt_a, &w->lock_pos);
    v3_copy(&w->pt_b, &w->lock_pos);
    dy = venus_core_lock_dy;
    w->pt_a.y += dy;
    w->pt_b.y += bottom;
    v3_copy(&w->pt_c, &w->lock_pos);
    v3_copy(&w->pt_d, &w->lock_pos);
    w->pt_c.y -= dy;
    w->pt_d.y -= bottom;
}

/* 0x00ca5870 */
void venus_core_exec(VenusCoreWork *w)
{
    float sc[2];
    int ang_sum, dang, ang_wrap, dang_wrap;
    int hits;

    if (w->wait)
        w->wait--;
    spring_to(&w->open_v, &w->open_y, w->open_to, venus_core_spring_damp, venus_core_spring_k);
    if (fabsf(w->open_y) < venus_core_settle_y && w->state != 1) {
        w->open_y *= venus_core_settle_damp;
        if (fabsf(w->open_y) < venus_core_open_ofs)
            ((union { int i; float f; } *)&w->open_y)->i = 0; /* steer(alias) */
    }
    w->spin_spd = DEG_TO_ANG(venus_core_spin_spd);
    ang_sum = w->spin_ang + w->spin_spd;
    w->spin_ang = ang_sum;
    if (ang_sum < 0)
        ang_wrap = ang_sum | 0xffff0000;
    else
        ang_wrap = ang_sum & 0xffff;
    w->spin_ang = ang_wrap;
    do { /* steer(barrier) */
    } while (0);
    if (fabsf(w->open_y) < FLT_EPSILON) {
        if (w->aligned == 0) {
            dang = w->spin_ang - w->ring_ang;
            if (dang < 0)
                dang_wrap = dang | 0xffff0000;
            else
                dang_wrap = dang & 0xffff;
            dang = dang_wrap & 0x1fff;
            if (dang < w->spin_spd) {
#define B_VENUS_ALWAYS_ZERO(p) ((((int)(p)) & 1) >> 1) /* steer(zero) */
                if (B_VENUS_ALWAYS_ZERO(dang)) {
                    int *words = (int *)w;

                    words[60] = dang;
                    words[61] = dang;
                    words[62] = dang;
                    words[63] = dang;
                    words[64] = dang;
                    words[65] = dang;
                    words[66] = dang;
                    words[67] = dang;
                    words[68] = dang;
                }
#undef B_VENUS_ALWAYS_ZERO
                venus_fb_man_set_on_now(w->fb_man, 1);
                w->aligned = 1;
                w->ring_spd = w->spin_spd;
                w->ring_ang += dang - w->spin_spd;
            } else {
                w->ring_spd = w->ring_spd * venus_core_spin_damp;
            }
        } else {
            w->ring_spd = w->spin_spd;
        }
    } else {
        w->aligned = 0;
        w->ring_spd = w->ring_spd * venus_core_spin_damp;
    }
    v3_copy(&w->pos, &w->home);
    w->ring_ang += w->ring_spd;
    w->ring2_ang += w->ring_spd * 2;
    w->pos.y += w->open_y;
    do {
        float px = w->pos.x, pz = w->pos.z;
        int rot = w->spin_ang;

        ulSinCos(rot, &sc[0], &sc[1]);
        w->pos.x = px * sc[1] + pz * sc[0];
        w->pos.z = -px * sc[0] + pz * sc[1];
    } while (0);
    venus_core_set_lock_points(w);
    switch (w->state) {
    case 0:
        if (w->burst) {
            w->revive_cnt++;
            if ((float)w->revive_cnt >= cm_level_d_pod[boss_get_level()] * FPS) {
                w->revive_cnt = 0;
                venus_fb_man_revive(w->fb_man);
            }
        }
        if (!venus_fb_man_get_count(w->fb_man)) {
            w->open_cnt = cmd_core_open_t;
            venus_fb_man_set_on_now(w->fb_man, 0);
            w->revive_cnt = 0;
            w->state++;
        }
        if (!w->gage_on) {
            boss_gage_create(0);
            w->gage_on = 1;
        }
        break;
    case 1:
        if (!beat_grid_hit(0) || (beat_clock_get_haku() & 3))
            break;
        w->state++;
        w->open_to = venus_core_open_y;
        boss_vol_play(4, 0);
        peri_act_play_number(1);
    case 2:
        if (!beat_grid_hit(0) || (beat_clock_get_haku() & 3))
            break;
        if (--w->open_cnt > 0)
            break;
        w->open_to = 0.0f;
        w->state++;
    case 3:
        if (w->aligned && beat_grid_hit(0) && !(beat_clock_get_haku() & 3)) {
            w->state = 0;
            boss_vol_play(3, 0);
            peri_act_play_number(1);
        }
        break;
    }
    if (w->open_y > venus_core_lock_open_y)
        w->lock.flags &= ~LOCK_LOCKABLE;
    else
        w->lock.flags |= LOCK_LOCKABLE;
    if (!w->wait) {
        if (w->burst == 1 && w->aligned) {
            venus_fb_man_start_burst(w->fb_man);
            w->burst = 2;
        } else if (w->burst == 2 && !venus_fb_man_count_shots(w->fb_man)) {
            boss_scoring_on(0);
            w->lock.life = 0;
            goto dead;
        }
    }
    hits = w->lock.life;
    if (w->lock.flags & LOCK_HIT) {
        if (hits <= 0) {
            boss_se_play(1, (int)&w->lock, 0);
        dead:
            venus_seq_step(w->boss);
        } else {
            cm_thl_man_jolt(w->thl_a);
            cm_thl_man_jolt(w->thl_b);
            boss_se_play(0, (int)&w->lock, 0);
        }
    }
    if (w->gage_on)
        console2_set_boss_rate((float)hits / (float)cmd_core_hp);
    if (debug_flags_test(4) && (kbd_state[7] & 0x44)) {
        switch (kbd_state[5]) {
        case 18:
            cm_thl_man_jolt(w->thl_a);
            cm_thl_man_jolt(w->thl_b);
            break;
        case 11:
            venus_seq_step(w->boss);
            venus_fb_man_stop(w->fb_man);
            TASK_OF(w)->exec = (void (*)(void *))venus_core_spin;
            console2_swap_boss_on(0);
            w->lock.life = 0;
            break;
        }
    }
}

/* 0x00ca5e60 */
void venus_core_disp(void *work)
{
    VenusCoreWork *w = work;
    int old_blend;
    float diff0, ambi0, diff1, ambi1;
    float dummy;
    char *part;
    int *nodes;

    ulGxSetTextureList(w->textures);
    old_blend = draw_set_override_flags(0);
    draw_set_color(color_lerp(cm_wire_color0, (&cm_wire_color0)[1], beat_curve(1)));
    ulMatrixPush();
    MTX_TRANS(&w->home);
    light_get_intensity(&diff0, &ambi0);
    light_set_intensity(diff0, 0.0f);
    dc_object_draw(*(ModelPart **)(w->parts + 52));
    light_set_intensity(diff0, ambi0);
    ulMatrixPush();
    vu0_matrix_rotate_y(ulMatrixCur, w->ring2_ang);
    light_get_intensity(&diff1, &ambi1);
    light_set_intensity(diff1, 0.0f);
    dc_object_draw(*(ModelPart **)(w->parts + 68));
    light_set_intensity(diff1, ambi1);
    ulMatrixPop();
    vu0_matrix_rotate_y(ulMatrixCur, w->ring_ang);
    dc_object_draw(*(ModelPart **)(w->parts + 72));
    dc_object_draw(*(ModelPart **)(w->parts + 76));
    ulMatrixPop();
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    vu0_matrix_rotate_y(ulMatrixCur, w->spin_ang);
    part = *(char **)(w->parts + 80);
    model_object_draw((struct DcObjectTreeView *)part, *(float *)(part + 60));
    part = *(char **)(w->parts + 80);
    nodes = *(int **)(w->parts + 84);
    model_node_walk_list_anim(nodes[1], nodes[2], *(float *)(part + 60), nodes[5]);
    ulMatrixPop();
    ulMatrixPush();
    MTX_TRANS(&w->lock_pos);
    dc_object_draw(*(ModelPart **)(w->parts + 60));
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00ca6048 */
void venus_core_disp_broken(void *work)
{
    VenusCoreWork *w = work;
    int old_blend;
    float diff0, ambi0, diff1, ambi1;
    float dummy;
    char *part;
    int *nodes;

    ulGxSetTextureList(w->textures);
    old_blend = draw_set_override_flags(0);
    draw_set_color(cm_wire_color0);
    ulMatrixPush();
    MTX_TRANS(&w->home);
    light_get_intensity(&diff0, &ambi0);
    light_set_intensity(diff0, 0.0f);
    dc_object_draw(*(ModelPart **)(w->parts + 56));
    light_set_intensity(diff0, ambi0);
    ulMatrixPush();
    vu0_matrix_rotate_y(ulMatrixCur, w->ring2_ang);
    light_get_intensity(&diff1, &ambi1);
    light_set_intensity(diff1, 0.0f);
    dc_object_draw(*(ModelPart **)(w->parts + 68));
    light_set_intensity(diff1, ambi1);
    ulMatrixPop();
    ulMatrixPop();
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    vu0_matrix_rotate_y(ulMatrixCur, w->spin_ang);
    part = *(char **)(w->parts + 80);
    model_object_draw(part, *(float *)(part + 60));
    part = *(char **)(w->parts + 80);
    nodes = *(int **)(w->parts + 84);
    model_node_walk_list_anim(nodes[1], nodes[2], *(float *)(part + 60), nodes[5]);
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00ca61b8 */
void venus_core_release(VenusCoreWork *w)
{
    console2_swap_boss_on(0);
    lock_remove_entry(0, &w->lock);
}

/* 0x00ca61e8 */
int *venus_core_create(void *parent, int parts, Vec3 *pos)
{
    VenusCoreWork *w = task_create_inherit(parent, &venus_core_desc, 292);
    ThlRec rec;

    w->wait = cmd_core_t * FPS;
    w->parts = (char *)parts;
    w->src = (void *)pos;
    w->boss = (int)parent;
    v3_copy(&w->home, pos);
    v3_copy(&w->pos, &w->home);
    w->fb_man = (void *)venus_fb_man_create(w, parts, (int)&w->pos, &w->unk_70);
    cm_thl_rec_init(&rec, &w->pt_c, &w->pt_d);
    w->thl_a = cm_thl_man_create(w, &rec, 4);
    cm_thl_rec_init(&rec, &w->pt_a, &w->pt_b);
    w->thl_b = cm_thl_man_create(w, &rec, 4);
    lock_init(0, &w->lock, w, &w->lock_pos, 0, cmd_core_hp, 0);
    lockon_entry(&w->lock);
    w->lock.flags &= ~LOCK_LOCKABLE;
    w->aligned = 1;
    return (int *)w;
}

/* 0x00ca6348 */
void venus_core_start(VenusCoreWork *w, int on)
{
    venus_fb_man_set_on_delay(w->fb_man, 1);
    w->wait = cmd_core_t * FPS;
    w->burst = 1;
}

/* 0x00ca6398 */
void venus_core_break(VenusCoreWork *w)
{
    int ang[3];

    venus_fb_man_stop(w->fb_man);
    cm_thl_man_set_fade(w->thl_a, 60);
    cm_thl_man_set_fade(w->thl_b, 60);
    TASK_OF(w)->exec = (void (*)(void *))venus_core_spin;
    TASK_OF(w)->disp = venus_core_disp_broken;
    console2_swap_boss_on(0);
    ang[0] = 0;
    ang[1] = 0;
    ang[2] = 0;
    cm_wire_break_simple(w, 1, *(ModelPart **)(w->parts + 64), &w->lock_pos, 0, ang, cm_wire_color2);
}

/* 0x00ca6430 */
void venus_end_core_exec(VenusEndCoreWork *w)
{
    switch (w->state) {
    case 0:
        i3_sum(w->ang, w->ang, w->dang);
        v3_add(&w->pos, &w->vel);
        w->dang[1] = w->dang[1] * 0.99f + -3.0f;
        if (--w->move_frm > 0)
            break;
        w->state++;
    case 1:
        if (--w->fade <= 0)
            FINISH_IF(w);
        break;
    }
}

/* steer(cast) */
/* 0x00ca6538 */
void venus_end_core_disp(VenusEndCoreWork *w)
{
    int old_blend;
    int alpha;

    ulGxSetTextureList(w->textures);
    ulMatrixPush();
    if (w->state == 0) {
        old_blend = draw_set_override_flags(0);
        dc_object_draw(*(ModelPart **)(w->parts + 32));
        MTX_TRANS(&w->pos);
        venus_core_rotate_yxz(w->ang);
        dc_object_draw(*(ModelPart **)(w->parts + 80));
        draw_set_color(w->col);
        dc_object_draw(*(ModelPart **)(w->parts + 84));
    } else {
        old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
        draw_push_override_color();
        draw_push_strip_flags();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
        venus_end_core_fade[0] = (float)w->fade / (float)w->fade_len + -1.0f;
        draw_set_override_color(venus_end_core_fade);
        dc_object_draw(*(ModelPart **)(w->parts + 32));
        MTX_TRANS(&w->pos);
        venus_core_rotate_yxz(w->ang);
        dc_object_draw(*(ModelPart **)(w->parts + 80));
        alpha = color_get_a(w->col);
        draw_set_color((w->col & 0xffffff) | (((int)(venus_end_core_fade[0] * alpha + alpha) + 1) >> 1 << 24));
        dc_object_draw(*(ModelPart **)(w->parts + 84));
        draw_pop_override_color();
        draw_pop_strip_flags();
    }
    ulMatrixPop();
    draw_set_override_flags(old_blend);
}

/* 0x00ca66f8 */
int *venus_end_core_create(void *parent, int parts, Vec3 *pos, int col, int move_frm, int fade_frm)
{
    VenusEndCoreWork *w = task_create_inherit(parent, &venus_end_core_desc, 80);

    w->parts = (char *)parts;
    if (pos)
        v3_copy(&w->pos, pos);
    w->vel.y = -2.2f;
    w->col = col;
    w->move_frm = move_frm;
    w->fade_len = w->fade = fade_frm;
    return (int *)w;
}

/* 0x00ca67a8 */
void venus_core_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}

/* 0x00ca67f8 */
void venus_1st_demo_calc_bars(int *frm, int *bars)
{
    float bar_frm = beat_clock_get_interval() * 4.0f;
    float bar_cnt = 1200.0f / bar_frm;
    float whole = floorf(bar_cnt);

    bar_cnt -= whole;
    *frm = (int)((1.0f - bar_cnt) * bar_frm) - 2;
    *bars = whole;
}

/* venus_1st_demo_desc, 64 bytes: the intro, from loading the music to the first cue */
typedef struct Venus1stDemoWork {
    int step;
    int timer;                  /* 0x04 */
    int timer_max;              /* 0x08 */
    int fogged;                 /* 0x0c */
    int cued;                   /* 0x10 */
    int cue_frm;                /* 0x14: frames to the cue's bar */
    int (*cb)(int);             /* 0x18: holds the intro until it returns nonzero */
    int cb_arg;                 /* 0x1c */
    BossCameraWork *cam;        /* 0x20 */
    char *parent;               /* 0x24 */
    int load_a;                 /* 0x28 */
    int load_b;                 /* 0x2c */
    BossFog fog;                /* 0x30: the fog at the start */
} Venus1stDemoWork;

/* steer(cast) */
/* 0x00ca6890 */
void venus_1st_demo_exec(Venus1stDemoWork *w)
{
    BossFog fog;
    int bars;
    CmLightChangeWork *light;

    texforce_disable();
    switch (w->step) {
    case 0:
        fog_get(&w->fog);
        w->cam = boss_camera_create(w, 1.0f, 0, 0, 0, 88, 1, 1, 1);
        boss_camera_follow(w->cam, 240);
        boss_cam_ang_create(w->cam, 0, 13653, 0, 240);
        w->load_a = sound_mpb_load_create_bpms(venus_1st_demo_files, 1);
        w->load_b = sound_mpb_load_create_bpms(venus_1st_demo_files + 8, 2);
        w->step++;
    case 1:
        if (w->load_a && (TASK_OF(w->load_a)->flags & TASK_F_END))
            w->load_a = 0;
        if (w->load_b && (TASK_OF(w->load_b)->flags & TASK_F_END))
            w->load_b = 0;
        if (!w->load_a && !w->load_b)
            w->step++;
        break;
    case 2:
        w->timer_max = w->timer = 30;
        light = cm_light_change_create(0.0f, 2.0f);
        if (light)
            cm_light_change_set_target(light, venus_1st_demo_light_cols);
        w->step++;
    case 3:
        if (--w->timer <= 0)
            w->step++;
        fog.back = 0;
        fog.col = 0;
        fog.near = 21.0f;
        fog.far = 50.0f;
        fog_blend(&w->fog, &fog, &fog, (float)w->timer / (float)w->timer_max);
        fog_apply(&fog, 4);
        break;
    case 4:
        if (!w->cued) {
            if (beat_grid_hit(0) && !(beat_clock_get_haku() & 3)) {
                w->cued = 1;
                venus_1st_demo_calc_bars(&w->cue_frm, &bars);
                venus_cue(0, 1, bars + 1);
            }
        } else if (--w->cue_frm <= 0) {
            if (!w->cb || w->cb(w->cb_arg)) {
                if (!light_mode_is_fog())
                    peri_act_beat_tick();
                w->timer = 30;
                w->step++;
            }
        }
        break;
    case 5:
        stage_end_child();
        player_cell_move_clear();
        player_swap_fullrange(0);
        w->step++;
        venus_ch_create(w, *(void **)(w->parent + 6244));
        break;
    case 6:
        if (w->timer)
            w->timer--;
        if (!w->fogged) {
            if (beat_grid_hit(0) && !(beat_clock_get_haku() & 3)) {
                venus_fog_create(w->parent, 0, 4.0f);
                boss_vol_play(0, 0);
                if (light_mode_is_fog())
                    peri_act_beat_tick();
                w->fogged = 1;
            }
        } else if (w->timer <= 0) {
            FINISH(w);
        }
        break;
    }
}

/* 0x00ca6c18 */
void venus_1st_demo_release(Venus1stDemoWork *w)
{
    BossCameraWork *cam = w->cam;

    if (cam)
        TASK_OF(cam)->flags |= TASK_F_END;
}

/* 0x00ca6c38 */
int *venus_1st_demo_create(void *parent, int cb, int cb_arg)
{
    Venus1stDemoWork *w = task_create(parent, &venus_1st_demo_desc, 64);

    w->cb = (int (*)(int))cb;
    w->cb_arg = cb_arg;
    w->parent = parent;
    return (int *)w;
}

/* 0x00ca6c90 */
void star_color_apply(void)
{
    if (star_ctl)
        eff_star_set_color(star_ctl, star_color);
}

/* 0x00ca6cb0 */
void venus_fog_vars_add(void)
{
    venus_fog_vars_group = gw_varedit_add(&gw_varedit_root, 0, venus_fog_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_fog_vars_group, venus_fog_vardefs);
}

/* 0x00ca6d00 */
void venus_fog_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_fog_vardef);
}

/* 0x00ca6d18 */
void venus_fog_exec(VenusFogWork *w)
{
    BossFog base, start;
    int mode;

    if (--w->frm <= 0)
        FINISH(w);
    mode = light_mode_index();
    if (!w->mode) {
        base.col = base.back = (&cm_fog_col)[mode];
        base.near = cmd_fog_n;
        base.far = cmd_fog_f;
        fog_get(&start);
        start.near = venus_fog_start.near;
        start.far = venus_fog_start.far;
        fog_blend(&start, &base, &venus_fog_cur, (float)w->frm / (float)w->frm_max);
    } else {
        base.col = base.back = (&cm_fog_col)[mode];
        base.near = cmd_fog_n;
        base.far = cmd_fog_f * 0.5f;
        fog_blend(&base, &venus_fog_end, &venus_fog_cur, (float)w->frm / (float)w->frm_max);
    }
}
