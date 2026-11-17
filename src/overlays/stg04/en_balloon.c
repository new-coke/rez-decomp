/*
 * Balloon, Area 4 enemy, held by three sprung control bars: swells with each hit, attacks on an
 * off-beat (spins, throws debris, fires its core enb_balloon, bursts into wire), then escapes.
 * A negative placement wait escapes without attacking.
 */

#include "stg04.h"
#include "mathdef.h"
#include "anim.h"
#include "approach.h"
#include "billboard.h"
#include "camera.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_direct.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_balloon.h"
#include "en_balloon_balloon.h"
#include "enb_balloon.h"
#include "../common/en_perfectwall.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"
#include "../common/s_cos.h"
#include "../common/s_sin.h"
#include "../common/util.h"

extern SVtx gx_vertex_buffer[];

/* as en_balloon_release sees it */
struct EnBalloonReleaseWork {
    char pad_00[156];
    ModelPart part[3];             /* 0x9c */
    char pad_1b0[1160];
    LockObj lock;                  /* 0x638 */
};

extern ParamRec en_balloon_params;
extern VarDef en_balloon_vardefs[24];
extern int en_balloon_init_arg;

/* 1788 bytes beyond the object */
struct EnBalloonWork {
    Vec3 *pos;
    Vec3 *vel;                  /* 0x04 */
    int *ang;                   /* 0x08 */
    Vec3 *scale;                /* 0x0c */
    int phase;                  /* 0x10: index into en_balloon_states */
    int next;                   /* 0x14 */
    unsigned int step;          /* 0x18 */
    int next_step;              /* 0x1c */
    Vec3 body;                  /* 0x20: spring offset of the body (ka_body, kv_body) */
    Vec3 body_to;               /* 0x2c */
    Vec3 body_vel;              /* 0x38 */
    Vec3 body_acc;              /* 0x44 */
    Vec3 knock;                 /* 0x50 */
    Vec3 blur;                  /* 0x5c: spring offset (ka_blur, kv_blur) */
    Vec3 blur_vel;              /* 0x68 */
    int spin_x;                    /* 0x74: += 60 per frame */
    int spin_y;                    /* 0x78: += 364 per frame */
    int spin_z;
    float size;                 /* 0x80: 1 .. sca_balloon_max by the hits taken */
    int core[3];                /* 0x84: angles turned toward the player (en_balloon_aim_core) */
    float fade;                 /* 0x90 */
    float spd;                  /* 0x94: escape speed */
    float spd_max;              /* 0x98 */
    ModelPart part[3];             /* 0x9c: models 4/5, 6/7, 9/10 */
    Vec3 pts[3];                /* 0x1b0: the control bars (springs toward bar_to) */
    Vec3 bar_to[3];             /* 0x1d4 */
    Vec3 bar_vel[3];            /* 0x1f8 */
    char pad_21c[36];
    int bar_ang;                /* 0x240 */
    int wire_reset;                   /* 0x244: set by init; en_balloon_wire_update then takes the wire from the model */
    Vec3 wvel[42];              /* 0x248: the wire points' velocities */
    Vec3 wire[42];              /* 0x440: the wire break's source points */
    LockObj lock;                  /* 0x638 */
    Vec3 lpos;                  /* 0x678: the lock's point */
    Vec3 off;                   /* 0x684: swing offset */
    float t;                    /* 0x690: swing time */
    int skip;                   /* 0x694: bit 0/1 skip aim/lock point this frame; 4, 8, 0x10 << i, 0x40 phases; 0x80 fading in */
    float cnt;                  /* 0x698 */
    float cnt_end;              /* 0x69c */
    Vec3 dbg[3];                /* 0x6a0: printed by en_balloon_disp (P0..P2) */
    int turn[3];                /* 0x6c4: en_balloon_aim_core's turn before the limit */
    char pad_6d0[12];
    float turn_len;             /* 0x6dc */
    float turn_max;             /* 0x6e0 */
    Vec3 dbg_2d;                   /* 0x6e4: printed by en_balloon_disp (2D) */
    Vec3 dbg_3d;                   /* 0x6f0: printed by en_balloon_disp (3D) */
};

extern ObjStateFn en_balloon_exec_table[];

/* en_balloon_params, 25 words */
typedef struct EnBalloonRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int score;                  /* 0x0c */
    float ka;                   /* 0x10 */
    float kv;                   /* 0x14 */
    float ka_body;              /* 0x18 */
    float kv_body;              /* 0x1c */
    float ka_blur;              /* 0x20 */
    float kv_blur;              /* 0x24 */
    float ka_ctrlbar;           /* 0x28 */
    float kv_ctrlbar;           /* 0x2c */
    float sca_balloon_max;      /* 0x30 */
    float sca_laser_pow;        /* 0x34 */
    float spd_ang_ctrlbar;      /* 0x38 */
    float frm_attack;           /* 0x3c */
    float blur_pow;             /* 0x40 */
    float acc_escape;           /* 0x44 */
    float r_swing;              /* 0x48 */
    float spd_ang_core;         /* 0x4c */
    float dst_escape;           /* 0x50 */
    unsigned int pcol_balloon[4]; /* 0x54 */
    int test;                   /* 0x64 */
} EnBalloonRec;

#define EN_BALLOON_REC ((EnBalloonRec *)&en_balloon_params)
extern void (*en_balloon_states[])(void *o, EnBalloonWork *w);
extern float en_balloon_attack_frames[3];
extern int en_balloon_wire_col_motion;
extern char en_balloon_name[];
extern char en_balloon_lock_name[];
extern unsigned int system_flags;
extern int en_balloon_skin_model;
extern int en_balloon_skin_wire;
extern int en_balloon_wire_on;
extern float en_balloon_fade_color[4];
extern void (*en_balloon_disp_table[])(void *o, EnBalloonWork *w);
extern float en_balloon_color_frames[6];
extern int en_balloon_rel_model_a;
extern int en_balloon_rel_model_b;
extern int en_balloon_bar_col_motion;
extern int en_balloon_rel_wire_a;
extern int en_balloon_rel_wire_b;
extern unsigned int *en_balloon_break_nodes_a[2];
extern unsigned int *en_balloon_break_nodes_b[2];
extern float en_balloon_rect_minx;
extern float en_balloon_rect_miny;
extern float en_balloon_rect_maxx;
extern float en_balloon_rect_maxy;
extern char str_en_balloon_tex[];
extern char str_en_balloon_p0[];
extern char str_en_balloon_p1[];
extern char str_en_balloon_p2[];
extern char str_en_balloon_2d[];
extern char str_en_balloon_3d[];
extern char str_en_balloon_tail[];
extern char str_en_balloon_pos[];
extern char str_en_balloon_vel[];
extern char str_en_balloon_ang[];
extern char str_en_balloon_frm[];
extern char str_en_balloon_cnt[];
extern char str_en_balloon_sca[];
extern char str_en_balloon_pct[];
extern char str_en_balloon_pcb[];
extern char str_en_balloon_col[];
extern char str_en_balloon_bpm[];

/* 0x00afbdf0 */
void en_balloon_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_balloon_params.name;
    def.fmt = en_balloon_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_balloon_vardefs;
    fld = en_balloon_params.v;
    for (i = 0; i < 24; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_balloon_vardefs);
    en_balloon_balloon_vars_register();
}

/* 0x00afbea8 */
void en_balloon_vars_unregister(void)
{
    en_balloon_balloon_vars_unregister();
    enemy_vars_group_release();
}

/* 0x00afbec8 */
int en_balloon_marker_draw(int obj, void *w)
{
    int *ent;

    ulGxSetTextureList(stage_textures);
    ent = pnb_get_entry(stage_models, 16);
    return obj_draw_at(obj, w, (void *)*ent);
}

/* 0x00afbf20 */
int en_balloon_vars_init(int *prm)
{
    en_balloon_vars_register();
    en_balloon_init_arg = *prm;
    return 0;
}

/* 0x00afbf58 */
int en_balloon_vars_release(void)
{
    en_balloon_vars_unregister();
    return 0;
}

/* 0x00afbf78 */
void en_balloon_aim_core(void *obj, EnBalloonWork *w)
{
    Vec3 pos;
    int dang[3];
    Vec3 v;
    int len, lim;
    float scl;

    enemy_task_get_pos(obj, &pos);
    player_get_pos(&v);
    v.x -= pos.x;
    v.y -= pos.y;
    v.z -= pos.z;
    do {
        dang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
        dang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
    } while (0);
    dang[2] = 0;
    i3_subfrom(dang, w->core);
    do {
        dang[0] = (dang[0] & 0x8000) ? dang[0] | ~0xffff : dang[0] & 0xffff;
        dang[1] = (dang[1] & 0x8000) ? dang[1] | ~0xffff : dang[1] & 0xffff;
        dang[2] = (dang[2] & 0x8000) ? dang[2] | ~0xffff : dang[2] & 0xffff;
    } while (0);
    len = model_ivec3_length(dang);
    lim = (int)DEG_TO_ANG(EN_BALLOON_REC->spd_ang_core * FRAME_SEC);
    i3_copy(w->turn, dang);
    w->turn_len = len;
    w->turn_max = lim;
    if (lim < len) {
        scl = (float)lim / len;
        dang[0] = dang[0] * scl;
        dang[1] = dang[1] * scl;
        dang[2] = dang[2] * scl;
    }
    i3_add(w->core, dang);
    w->skip |= 1;
}

/* steer(cast) */
/* 0x00afc210 */
void en_balloon_calc_lock_point(void *obj, EnBalloonWork *w)
{
    int ang[3];

    enemy_task_get_rot(obj, ang);
    enemy_task_set_rot(obj, w->core);
    ((void (*)(void *, unsigned int, int, void *, int, int))enemy_task_spawn)(obj, 0x80000000, 1, &w->lpos, 0, 0);
    enemy_task_set_rot(obj, ang);
    w->skip |= 2;
}

/* 0x00afc288 */
void en_balloon_calc_points(void *obj, EnBalloonWork *w)
{
    if (!(w->skip & 1))
        en_balloon_aim_core(obj, w);
    if (!(w->skip & 2))
        en_balloon_calc_lock_point(obj, w);
    w->skip &= ~3;
}

/* 0x00afc2f8 */
void en_balloon_swing(void *obj, EnBalloonWork *w)
{
    Vec3 pos;
    float t, amp;

    t = w->t += 0.005f;
    amp = EN_BALLOON_REC->r_swing;
    enemy_task_get_pos(obj, &pos);
    v3_sub(&pos, &pos, &w->off);
    v3_set(&w->off, sinf(t * 3.0f) * amp, cosf(t * 5.0f) * amp, sinf(t * 4.0f) * amp);
    v3_add(&pos, &w->off);
    enemy_task_set_pos(obj, &pos);
}

/* 0x00afc430 */
void en_balloon_hit_check(void *obj, EnBalloonWork *w)
{
    LockObj *lock = &w->lock;
    Vec3 vdir, kdir;
    int hits;
    float dot, kpow;
    double grow;

    if (!(lock->flags & LOCK_HIT))
        return;
    hits = lock->life;
    grow = EN_BALLOON_REC->sca_balloon_max - 1.0f;
    w->size = grow * cos(hits * 1.5707964f / EN_BALLOON_REC->hp) + 1.0;
    vec3_scale_to_length(EN_BALLOON_REC->sca_laser_pow, &lock->imp.v, &w->knock);
    if (w->phase == 0) {
        vec3_normalize(&vdir, enemy_task_param_vec_ptr(obj));
        vec3_normalize(&kdir, &w->knock);
        dot = ulVec3Dot(&vdir, &kdir);
        if (dot < 0.0f) {
            kpow = vec3_length(&w->knock) / 0.029999971f;
            w->cnt_end -= kpow * dot / vec3_length(enemy_task_param_vec_ptr(obj));
        }
    }
    if (hits <= 0) {
        enemy_status_score_popup(enemy_task_get_status_slot(obj), enemy_task_pos_ptr(obj), EN_BALLOON_REC->score, lockon_hits_max_shot_cnt(lock));
        lock->flags &= ~LOCK_LOCKABLE;
        enemy_task_set_state(obj, 3);
    }
}

/* 0x00afc640 */
void en_balloon_state_move_in(void *obj, EnBalloonWork *w)
{
    float dur, cnt, end, n;
    Vec3 *vel, *pos;

    switch (w->step) {
    case 0:
        dur = enemy_task_get_param(obj, 0);
        if (0.0f < dur) {
            w->lock.flags &= ~LOCK_LOCKABLE;
            w->skip |= 0x80;
        }
        w->cnt_end = dur;
        w->cnt = 1.0f;
        w->step++;
        w->next_step++;
    case 1:
        cnt = w->cnt;
        end = w->cnt_end;
        vel = enemy_task_param_vec_ptr(obj);
        pos = enemy_task_pos_ptr(obj);
        v3_add(pos, vel);
        if (w->skip & 0x80) {
            n = 30.0f < end ? 30.0f : (int)end;
            if (cnt < n)
                w->fade = w->cnt / n - 1.0f;
            else {
                w->skip &= ~0x80;
                w->lock.flags |= LOCK_LOCKABLE;
            }
        }
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt) {
            do {
                w->next = 1;
                w->next_step = 0;
            } while (0);
        }
        if (w->phase == w->next && w->step == w->next_step)
            break;
        w->step++;
    case 2:
        v3_copy(&w->knock, enemy_task_param_vec_ptr(obj));
        break;
    }
}

/* 0x00afc860 */
void en_balloon_state_wait(void *obj, EnBalloonWork *w)
{
    switch (w->step) {
    case 0:
        w->cnt = 1.0f;
        w->cnt_end = fabsf(enemy_task_get_param(obj, 1));
        w->step++;
        w->next_step++;
    case 1:
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt) {
            if (beat_grid_hit(0) && beat_clock_get_haku() % 2 == 1) {
                int next = !(0.0f < enemy_task_get_param(obj, 1)) ? 4 : 2;

                w->next_step = 0;
                do { w->next = next; } while (0);
            }
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00afc990 */
void en_balloon_spawn_debris_all(void *obj, EnBalloonWork *w, int piece)
{
    Vec3 pos;
    int bang[3];
    Vec3 v;
    Vec3 *pt;
    int i;

    bang[2] = w->bar_ang;
    enemy_task_get_pos(obj, &pos);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_balloon_rotate_yxz(enemy_task_rot_ptr(obj));
    pt = &w->pts[2];
    for (i = 2; i >= 0; i--) {
        v.x = pt->x - pos.x;
        v.y = pt->y - pos.y;
        v.z = pt->z - pos.z;
        do {
            bang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
            bang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
        ulMatrixPush();
        pt--;
        en_balloon_rotate_yxz(bang);
        en_balloon_balloon_spawn_debris(obj, piece);
        ulMatrixPop();
    }
    ulMatrixPop();
}

/* 0x00afcb30 */
void en_balloon_state_attack(void *obj, EnBalloonWork *w)
{
    ModelPart *spin = &w->part[0];
    ModelPart *glow = &w->part[1];
    ModelPart *bar = &w->part[2];
    float frm, len, scl;
    Vec3 v;
    int i;

    switch (w->step) {
    case 0:
        {
            float t = 360.0f / beat_clock_get_bpm() * FPS;

            w->step++;
            w->next_step++;
            bar->speed = glow->speed = spin->speed = spin->frames / t;
        }
    case 1:
        dc_object_advance_time(spin);
        dc_object_advance_time(glow);
        dc_object_advance_time(bar);
        frm = spin->frame;
        i = 2;
        do {
            if (en_balloon_attack_frames[i] <= frm) {
                if (i == 2) {
                    SoundRhythmSe_Enemy(en_balloon_init_arg, 6);
                    w->skip |= 0x40;
                } else {
                    if (w->skip & (16 << i))
                        break;
                    do {
                        v3_set(&v, RAND_FLOAT_A(), RAND_FLOAT_A(),
                               RAND_FLOAT_A());
                        len = vec3_length(&v);
                    } while (fabsf(len) < FLT_EPSILON);
                    scl = EN_BALLOON_REC->blur_pow / len;
                    w->skip |= 16 << i;
                    v3_scale(&v, scl);
                    v3_add(&w->blur_vel, &v);
                    en_balloon_spawn_debris_all(obj, w, i);
                    SoundRhythmSe_Enemy(en_balloon_init_arg, i + 4);
                }
            }
        } while (--i >= 0);
        if (spin->flags2 & 1) {
            do {
                w->next = 3;
                w->next_step = 0;
            } while (0);
        }
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00afce48 */
void en_balloon_state_fire(void *obj, EnBalloonWork *w)
{
    Vec3 pos;
    int bang[3];
    Vec3 v;
    Vec3 *pt;
    int i;

    switch (w->step) {
    case 0:
        w->skip |= 4;
        w->cnt = 1.0f;
        w->cnt_end = (int)(120.0f / beat_clock_get_bpm() * FPS);
        w->step++;
        w->next_step++;
    case 1:
        w->cnt += 1.0f;
        if (w->cnt_end < w->cnt) {
            do {
                enb_balloon_create(obj, enemy_task_pos_ptr(obj), enemy_task_rot_ptr(obj));
                ulMatrixPush();
                enemy_task_get_pos(obj, &pos);
                bang[2] = w->bar_ang;
                matrix_unit_set(0);
                en_balloon_rotate_yxz(enemy_task_rot_ptr(obj));
                for (i = 2; i >= 0; i--) {
                    pt = &w->pts[i];
                    v.x = pt->x - pos.x;
                    v.y = pt->y - pos.y;
                    v.z = pt->z - pos.z;
                    do {
                        bang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
                        bang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
                    } while (0);
                    ulMatrixPush();
                    en_balloon_rotate_yxz(bang);
                    model_camera_angles(bang);
                    ulMatrixPop();
                    balloon_ctrlbar_create(obj, &pos, bang);
                }
                ulMatrixPop();
                ulMatrixPush();
                v3_set((Vec3 *)bang, 0, -0.5f, 0);
                matrix_unit_set(0);
                do {
                    Vec3 *p_ = enemy_task_pos_ptr(obj);

                    vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
                } while (0);
                en_balloon_rotate_yxz(enemy_task_rot_ptr(obj));
                ulMatrixApplyPoint((float *)ulMatrixCur, (float *)bang, (float *)bang);
                en_balloon_balloon_create(obj, (Vec3 *)bang, w->wire,
                                              motion_sample_rgba((void *)en_balloon_wire_col_motion,
                                                                        (float)(unsigned int)(((int *)en_balloon_wire_col_motion)[1] - 1) *
                                                                            beat_clock_get_phase(),
                                                                        100.0f, 400.0f, 1.0f, 0.2f));
                ulMatrixPop();
                w->lock.flags &= ~LOCK_LOCKABLE;
                w->skip |= 8;
                do {
                    w->next_step = w->step + 1;
                } while (0);
            } while (0);
        }
        if (w->phase == w->next && w->step == w->next_step)
            break;
        w->step++;
    case 2:
        enemy_task_set_state(obj, 2);
        break;
    }
}

/* 0x00afd230 */
void en_balloon_state_escape(void *obj, EnBalloonWork *w)
{
    Vec3 v;
    Vec3 pos;
    Vec3 mv;
    Vec3 eye;
    float spd, smax, n, dis;

    switch (w->step) {
    case 0:
        enemy_task_get_param_vec(obj, &v);
        w->spd = 0;
        w->spd_max = vec3_length(&v) + enemy_task_get_param(obj, 2);
        w->step++;
        w->next_step++;
    case 1:
        spd = w->spd;
        smax = w->spd_max;
        w->spd = spd < smax ? MIN(spd + EN_BALLOON_REC->acc_escape * 0.00027777778f, smax)
                       : MAX_GE(spd - EN_BALLOON_REC->acc_escape * 0.00027777778f, smax);
        enemy_task_get_pos(obj, &pos);
        enemy_task_get_param_vec(obj, &mv);
        vec3_scale_to_length(w->spd, &mv, &mv);
        v3_add(&pos, &mv);
        enemy_task_set_pos(obj, &pos);
        dis = EN_BALLOON_REC->dst_escape;
        camera_get_eye(&eye);
        if (vec3_distance_squared(&pos, &eye) > dis * dis)
            enemy_task_set_state(obj, 2);
        if (w->phase != w->next || w->step != w->next_step)
            w->step++;
        break;
    case 2:
        break;
    }
}

/* 0x00afd420 */
void en_balloon_place_ctrlbars(void *obj, EnBalloonWork *w)
{
    Vec3 v;

    v.x = 0;
    v.y = 40.0f;
    v.z = 0;
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_balloon_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_rotate_z(ulMatrixCur, 21845);
    ulMatrixApplyPoint((float *)ulMatrixCur, &v.x, &w->pts[0].x);
    vu0_matrix_rotate_z(ulMatrixCur, 21845);
    ulMatrixApplyPoint((float *)ulMatrixCur, &v.x, &w->pts[1].x);
    vu0_matrix_rotate_z(ulMatrixCur, 21845);
    ulMatrixApplyPoint((float *)ulMatrixCur, &v.x, &w->pts[2].x);
    ulMatrixPop();
}

/* 0x00afd510 */
void en_balloon_exec_normal(void *obj, EnBalloonWork *w)
{
    Vec3 rest;
    Vec3 v;
    Vec3 *pos;
    void (*fn)(void *, EnBalloonWork *);

    do {
        w->phase = w->next;
        w->step = w->next_step;
    } while (0);
    if (enemy_task_get_phase(obj) != 1)
        return;
    en_balloon_swing(obj, w);
    v3_scale(&w->knock, 0.97f);
    v3_add(enemy_task_pos_ptr(obj), &w->knock);
    pos = enemy_task_pos_ptr(obj);
    v3_sub(pos, pos, &w->body);
    spring3_step(EN_BALLOON_REC->ka_body, EN_BALLOON_REC->kv_body, &w->body_to, &w->body, &w->body_vel, &w->body_acc);
    v3_add(enemy_task_pos_ptr(obj), &w->body);
    v3_set(&rest, 0, 0, 0);
    pos = enemy_task_pos_ptr(obj);
    v3_sub(pos, pos, &w->blur);
    spring3_step(EN_BALLOON_REC->ka_blur, EN_BALLOON_REC->kv_blur, &rest, &w->blur, &w->blur_vel, &rest);
    v3_add(enemy_task_pos_ptr(obj), &w->blur);
    v3_set(&v, 0, 40.0f, 0);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = enemy_task_pos_ptr(obj);

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_balloon_rotate_yxz(enemy_task_rot_ptr(obj));
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_rotate_z(ulMatrixCur, 21845);
    ulMatrixApplyPoint((float *)ulMatrixCur, &v.x, &w->bar_to[0].x);
    vu0_matrix_rotate_z(ulMatrixCur, 21845);
    ulMatrixApplyPoint((float *)ulMatrixCur, &v.x, &w->bar_to[1].x);
    vu0_matrix_rotate_z(ulMatrixCur, 21845);
    ulMatrixApplyPoint((float *)ulMatrixCur, &v.x, &w->bar_to[2].x);
    ulMatrixPop();
    v3_set(&v, 0, 0, 0);
    spring3_step(EN_BALLOON_REC->ka_ctrlbar, EN_BALLOON_REC->kv_ctrlbar, &w->bar_to[0], &w->pts[0], &w->bar_vel[0], &v);
    spring3_step(EN_BALLOON_REC->ka_ctrlbar, EN_BALLOON_REC->kv_ctrlbar, &w->bar_to[1], &w->pts[1], &w->bar_vel[1], &v);
    spring3_step(EN_BALLOON_REC->ka_ctrlbar, EN_BALLOON_REC->kv_ctrlbar, &w->bar_to[2], &w->pts[2], &w->bar_vel[2], &v);
    w->bar_ang = (float)w->bar_ang + EN_BALLOON_REC->spd_ang_ctrlbar;
    w->spin_x += 60;
    w->spin_y += 364;
    fn = en_balloon_states[w->phase];
    if (fn)
        fn(obj, w);
}

/* 0x00afd8c0 */
void en_balloon_exec_destroyed(void *obj, EnBalloonWork *w)
{
    SpawnDesc desc;
    SpawnDesc bdesc;
    Vec3 pos;
    int bang[3];
    Vec3 v;
    Vec3 *q;
    int i;
    int sub = enemy_task_get_phase(obj);

    if (sub != 1) {
        if (!sub) {
            nj_fill8(&desc, 0, sizeof desc);
            desc.models = stage_models;
            desc.type = 11;
            desc.kind = 0;
            enemy_task_get_pos(obj, &desc.pos);
            enemy_task_get_rot(obj, desc.ang);
            v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
            v3_set(&desc.v, 0, 0, 0);
            enemy_shatter_create(obj, &desc, 0, 0);
            nj_fill8(&bdesc, 0, sizeof bdesc);
            bdesc.kind = 0xc0000;
            bdesc.models = stage_models;
            bdesc.time = w->part[0].frame;
            ulMatrixPush();
            enemy_task_get_pos(obj, &pos);
            bang[2] = w->bar_ang;
            matrix_unit_set(0);
            do {
                Vec3 *p_ = &pos;

                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            en_balloon_rotate_yxz(enemy_task_rot_ptr(obj));
            for (i = 2; i >= 0; i--) {
                v3_sub(&v, &w->pts[i], &pos);
                do {
                    bang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
                    bang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
                } while (0);
                ulMatrixPush();
                en_balloon_rotate_yxz(bang);
                bdesc.type = 4;
                bdesc.motion = 5;
                enemy_shatter_create(obj, &bdesc, 0, 0);
                bdesc.type = 9;
                bdesc.motion = 10;
                enemy_shatter_create(obj, &bdesc, 0, 0);
                ulMatrixPop();
            }
            ulMatrixPop();
            ulMatrixPush();
            v3_set((Vec3 *)bang, 0, -0.5f, 0);
            matrix_unit_set(0);
            do {
                Vec3 *p_ = enemy_task_pos_ptr(obj);

                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            en_balloon_rotate_yxz(enemy_task_rot_ptr(obj));
            ulMatrixApplyPoint((float *)ulMatrixCur, (float *)bang, (float *)bang);
            en_balloon_balloon_create(obj, (Vec3 *)bang, w->wire, RGBA(0x68, 0x1e, 0, 0x80));
            ulMatrixPop();
        }
    } else {
        enemy_task_next_phase(obj);
    }
}

/* 0x00afdc10 */
void en_balloon_exec(void *obj, void *w)
{
    ObjStateFn *tab = en_balloon_exec_table;
    unsigned int i = enemy_task_get_pstate(obj);

    if (tab[i])
        tab[i](obj, w);
}

/* 0x00afdc70 */
void en_balloon_pre_exec(void *obj, EnBalloonWork *w)
{
    Vec3 shift;
    Vec3 *p;
    int i;

    player_get_frame_vel(&shift);
    if (!v3_is_zero(&shift)) {
        for (i = 0, p = w->wire; i < 42; i++, p++)
            v3_add(p, &shift);
        for (i = 2, p = w->pts; i >= 0; i--, p++)
            v3_add(p, &shift);
    }
    en_balloon_hit_check(obj, w);
}

/* 0x00afdda0 */
void en_balloon_post_exec(void *obj, EnBalloonWork *w)
{
    en_balloon_calc_points(obj, w);
}

/* 0x00afdda8 */
void en_balloon_wire_update(void *obj, EnBalloonWork *w, int first, int n)
{
    Vec3 res[42];
    Vec3 tgt[42];
    Vec3 rest;
    Vec3 ofs;
    SVtx *vtx = &gx_vertex_buffer[first];
    int num = n;
    Vec3 *pos, *wp, *tp, *vp;
    float lim, len, ka, kv;

    if (w->wire_reset) {
        Vec3 *dst = w->wire;
        do {
            dst->x = vtx->x;
            dst->y = vtx->y;
            dst->z = vtx->z;
            vtx++;
            dst++;
        } while (--n);
        ulMatrixPush();
        ulMatrixLoad(camera_get_inv_mtx());
        matrix_apply_points(w->wire, w->wire, num);
        ulMatrixPop();
        w->wire_reset = 0;
        return;
    }
    if (system_flags & (SYS_PAUSED | SYS_PAUSE_KBD)) {
        ulMatrixPush();
        ulMatrixLoad(camera_get_view_mtx());
        matrix_apply_points(w->wire, res, n);
        ulMatrixPop();
        for (n = 0; n < num; n++) {
            vtx->x = res[n].x;
            vtx->y = res[n].y;
            vtx->z = res[n].z;
            vtx++;
        }
        return;
    }
    ulMatrixPush();
    for (n = 0; n < num; n++) {
        tgt[n].x = vtx->x;
        tgt[n].y = vtx->y;
        tgt[n].z = vtx->z;
        vtx++;
    }
    ulMatrixLoad(camera_get_inv_mtx());
    matrix_apply_points(tgt, tgt, num);
    pos = enemy_task_pos_ptr(obj);
    lim = w->size * 10.0f;
    ka = EN_BALLOON_REC->ka;
    kv = EN_BALLOON_REC->kv;
    v3_set(&rest, 0, 0, 0);
    tp = &tgt[n - 1];
    wp = &w->wire[n - 1];
    vp = &w->wvel[n - 1];
    do {
        ((void (*)(Vec3 *, Vec3 *, Vec3 *, Vec3 *, float, float))spring3_step)(tp, wp, vp, &rest, ka, kv); /* steer(cast): pointers before floats */
        { float wx = wp->x, wy = wp->y, wz = wp->z; ofs.x = wx - pos->x; ofs.y = wy - pos->y; ofs.z = wz - pos->z; }
        len = vec3_length(&ofs);
        if (len < 6.0f) {
            v3_scale(&ofs, 6.0f / len);
            v3_sum(wp, pos, &ofs);
        } else if (lim < len) {
            v3_scale(&ofs, lim / len);
            v3_sum(wp, pos, &ofs);
        }
        tp--;
        wp--;
        vp--;
    } while (--n);
    vtx -= num;
    ulMatrixLoad(camera_get_view_mtx());
    matrix_apply_points(w->wire, tgt, num);
    for (n = 0; n < num; n++) {
        vtx->x = tgt[n].x;
        vtx->y = tgt[n].y;
        vtx->z = tgt[n].z;
        vtx++;
    }
    ulMatrixPop();
}

/* 0x00afe1d8 */
void en_balloon_wire_draw_tree(void *obj, EnBalloonWork *w, unsigned int *node, int *chain)
{
    unsigned int fl;
    int *mdl;
    short *pts;

    do {
        fl = node[0];
        mdl = (int *)node[1];
        ulMatrixPush();
        anim_apply_node(node);
        if (!(fl & 8)) {
            pts = (short *)mdl[0];
            ulGxSetMatrix(0, ulMatrixCur);
            nj_cnk_wire_vlist(pts, gx_vertex_buffer);
            en_balloon_wire_update(obj, w, pts[2], pts[3]);
            mdl[0] = 0;
            model_obj_draw(mdl, chain);
            mdl[0] = (int)pts;
        }
        chain = (int *)*chain;
        if (!(fl & 0x10))
            en_balloon_wire_draw_tree(obj, w, (unsigned int *)node[11], chain);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
}

/* 0x00afe2e0 */
void en_balloon_wire_draw(void *obj, EnBalloonWork *w)
{
    float save;
    unsigned int ocol;

    save = line_width_set(1.0f);
    ocol = draw_set_color(motion_sample_rgba((void *)en_balloon_wire_col_motion,
                                           (float)(unsigned int)(((int *)en_balloon_wire_col_motion)[1] - 1) *
                                               beat_clock_get_phase(),
                                           100.0f, 400.0f, 1.0f, 0.2f));
    draw_set_blend(4, 5);
    en_balloon_wire_on = 1;
    anim_start(0, 0.0f);
    en_balloon_wire_draw_tree(obj, w, (unsigned int *)en_balloon_skin_model, (int *)en_balloon_skin_wire);
    draw_set_color(ocol);
    line_width_set(save);
}

/* 0x00afe400 */
void en_balloon_draw_rect(void *obj, EnBalloonWork *w, float z)
{
    Vec3 scr[42];
    float *tbl = en_balloon_color_frames;
    float frm = w->part[0].frame;
    float lo, hi;
    unsigned int col;
    float minx, maxx, miny, maxy, x, y;
    float nz;
    int i;

    if (frm < (lo = tbl[0]))
        col = EN_BALLOON_REC->pcol_balloon[0];
    else if (frm < (hi = tbl[1]))
        col = color_blend(EN_BALLOON_REC->pcol_balloon[0], EN_BALLOON_REC->pcol_balloon[1],
                          sin((frm - lo) * 1.5707964f / (hi - lo)));
    else if (frm < (lo = tbl[2]))
        col = EN_BALLOON_REC->pcol_balloon[1];
    else if (frm < (hi = tbl[3]))
        col = color_blend(EN_BALLOON_REC->pcol_balloon[1], EN_BALLOON_REC->pcol_balloon[2],
                          sin((frm - lo) * 1.5707964f / (hi - lo)));
    else if (frm < (lo = tbl[4]))
        col = EN_BALLOON_REC->pcol_balloon[2];
    else if (frm < (hi = tbl[5]))
        col = color_blend(EN_BALLOON_REC->pcol_balloon[2], EN_BALLOON_REC->pcol_balloon[3],
                          sin((frm - lo) * 1.5707964f / (hi - lo)));
    else
        col = EN_BALLOON_REC->pcol_balloon[3];
    nz = -z;
    matrix_apply_points(w->wire, scr, 42);
    project_point(&scr[0], &scr[0]);
    minx = maxx = scr[0].x;
    miny = maxy = scr[0].y;
    for (i = 41; i != 0; i--) {
        project_point(&scr[i], &scr[i]);
        x = scr[i].x;
        y = scr[i].y;
        minx = MIN(minx, x);
        maxx = MAX_GE(maxx, x);
        miny = MIN(miny, y);
        maxy = MAX_GE(maxy, y);
    }
    en_balloon_rect_minx = minx;
    en_balloon_rect_miny = miny;
    en_balloon_rect_maxx = maxx;
    en_balloon_rect_maxy = maxy;
    z = nz;
    draw_set_blend(4, 1);
    tex_rect_draw(4, col, en_balloon_rect_minx, en_balloon_rect_miny, z, en_balloon_rect_maxx - en_balloon_rect_minx,
                  en_balloon_rect_maxy - en_balloon_rect_miny);
}

/* 0x00afe728 */
void en_balloon_draw_release(void *obj, EnBalloonWork *w)
{
    float t = (w->cnt - 1.0f) / w->cnt_end;
    float save = line_width_set(t * 4.0f);

    draw_set_blend(1, 1);
    draw_set_color((((int)(t * 255.0f) + 1) >> 1 << 24) | 0x2040);
    model_draw_kind_list(en_balloon_rel_model_a, en_balloon_rel_wire_a);
    model_draw_kind_list(en_balloon_rel_model_b, en_balloon_rel_wire_b);
    line_width_set(save);
}

/* 0x00afe7e0 */
void en_balloon_toggle_parts(void *obj, EnBalloonWork *w)
{
    int i;

    for (i = 1; i >= 0; i--) {
        if (w->skip & (16 << i)) {
            util_node_toggle(en_balloon_break_nodes_a[i]);
            util_node_toggle(en_balloon_break_nodes_b[i]);
        }
    }
}

/* steer(cast) */
/* 0x00afe878 */
void en_balloon_disp_fade_in(void *obj, EnBalloonWork *w)
{
    Vec3 pos;
    int ang[3];
    Vec3 v;
    int bang[3];
    int blend;
    int *spin;
    float scl, z;
    Vec3 *pt;
    ModelPart *bar;
    int i;

    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    if (!view_sphere_visible(&pos, ((ModelPart *)enemy_task_gfx_ptr(obj))->radius))
        return;
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    ulGxSetTextureList(stage_textures);
    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    scl = w->size;
    en_balloon_fade_color[0] = w->fade;
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(en_balloon_fade_color);
    do {
        Vec3 *p_ = &pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ulMatrixPush();
    en_balloon_rotate_yxz(w->core);
    bar = &w->part[2];
    spin = &w->spin_x;
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(enemy_task_gfx_ptr(obj));
    ulMatrixPop();
    en_balloon_rotate_yxz(ang);
    for (i = 2, pt = &w->pts[2]; i >= 0; i--) {
        v.x = pt->x - pos.x;
        v.y = pt->y - pos.y;
        v.z = pt->z - pos.z;
        do {
            bang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
            bang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
        bang[2] = 0;
        ulMatrixPush();
        pt--;
        en_balloon_rotate_yxz(bang);
        dc_object_draw(bar);
        vu0_matrix_rotate_z(ulMatrixCur, w->bar_ang);
        dc_object_draw(&w->part[0]);
        ulMatrixPop();
    }
    v3_set(&pos, 0, -0.5f, 0);
    ulMatrixApplyVector((float *)ulMatrixCur, &pos.x, &pos.x);
    do {
        Vec3 *p_ = &pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_balloon_rotate_yxz(spin);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, scl, scl, scl);
    en_balloon_wire_draw(obj, w);
    v3_set(&v, 0, 0, 0);
    ulMatrixApplyPoint((float *)ulMatrixCur, &v.x, &v.x);
    z = project_point(&v, &v);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
    en_balloon_draw_rect(obj, w, z);
}

/* steer(cast) */
/* 0x00afebb8 */
void en_balloon_disp_body(void *obj, EnBalloonWork *w)
{
    Vec3 pos;
    int ang[3];
    Vec3 v;
    Vec3 *vp;
    int bang[3];
    float rgba[4];
    int blend;
    float scl, z;
    int i;

    if (w->skip & 0x80) {
        en_balloon_disp_fade_in(obj, w);
        return;
    }
    enemy_task_get_pos(obj, &pos);
    enemy_task_get_rot(obj, ang);
    if (w->skip & 8)
        return;
    if (!view_sphere_visible(&pos, ((ModelPart *)enemy_task_gfx_ptr(obj))->radius))
        return;
    ulGxSetTextureList(((void **)obj)[1]);
    ulMatrixPush();
    scl = w->size;
    do {
        Vec3 *p_ = &pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ulMatrixPush();
    en_balloon_rotate_yxz(w->core);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(enemy_task_gfx_ptr(obj));
    if (w->skip & 4)
        en_balloon_draw_release(obj, w);
    ulMatrixPop();
    en_balloon_rotate_yxz(ang);
    en_balloon_toggle_parts(obj, w);
    for (i = 2; i >= 0; i--) {
        vp = &v;
        v3_sub(&v, &w->pts[i], &pos);
        do {
            bang[1] = (int)RAD_TO_ANG(atan2f(-v.x, -v.z));
            bang[0] = (int)RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
        bang[2] = 0;
        ulMatrixPush();
        en_balloon_rotate_yxz(bang);
        dc_object_draw(&w->part[2]);
        vu0_matrix_rotate_z(ulMatrixCur, w->bar_ang);
        dc_object_draw(&w->part[0]);
        motion_sample_color((void *)en_balloon_bar_col_motion, (float)(unsigned int)(((int *)en_balloon_bar_col_motion)[1] - 1) * beat_clock_get_phase(),
                             100.0f, 400.0f,
                             1.0f, 0.2f, rgba);
        blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
        draw_push_strip_flags();
        draw_push_override_color();
        ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 3072);
        draw_set_override_color(rgba);
        dc_object_draw(&w->part[1]);
        draw_pop_override_color();
        draw_pop_strip_flags();
        draw_set_override_flags(blend);
        ulMatrixPop();
    }
    en_balloon_toggle_parts(obj, w);
    v3_set(&pos, 0, -0.5f, 0);
    ulMatrixApplyVector((float *)ulMatrixCur, &pos.x, &pos.x);
    do {
        Vec3 *p_ = &pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_balloon_rotate_yxz(&w->spin_x);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    vu0_matrix_scale(ulMatrixCur, scl, scl, scl);
    en_balloon_wire_draw(obj, w);
    v3_set(&v, 0, 0, 0);
    ulMatrixApplyPoint((float *)ulMatrixCur, &vp->x, &vp->x);
    z = project_point(vp, vp);
    ulMatrixPop();
    en_balloon_draw_rect(obj, w, z);
}

/* steer(cast) */
/* 0x00afefe8 */
void en_balloon_disp(void *obj, EnBalloonWork *w)
{
    void (**tbl)(void *obj, EnBalloonWork *w) = en_balloon_disp_table;
    unsigned int i;
    Vec3 pos, vel, rot, body, v3d, tail;
    Vec3 bar, pt;
    int core[3];
    ModelPart *gfx;
    unsigned int col;
    unsigned int bcol;

    i = enemy_task_get_pstate(obj);
    if (tbl[i])
        tbl[i](obj, w);
    if (debug_flags_test(16)) {
        debug_set_origin(10, 10);
        debug_printf(str_en_balloon_tex, (int)en_balloon_rect_minx, (int)en_balloon_rect_miny, (int)en_balloon_rect_maxx,
                     (int)en_balloon_rect_maxy);
        debug_newline();
        v3_copy(&pos, &w->dbg[0]);
        debug_printf(str_en_balloon_p0, (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        v3_copy(&vel, &w->dbg[1]);
        debug_printf(str_en_balloon_p1, (int)vel.x, (int)vel.y, (int)vel.z);
        debug_newline();
        v3_copy(&rot, &w->dbg[2]);
        debug_printf(str_en_balloon_p2, (int)rot.x, (int)rot.y, (int)rot.z);
        debug_newline();
        v3_copy(&body, &w->dbg_2d);
        debug_printf(str_en_balloon_2d, (int)body.x, (int)body.y, (int)body.z);
        debug_newline();
        v3_copy(&v3d, &w->dbg_3d);
        debug_printf(str_en_balloon_3d, (int)v3d.x, (int)v3d.y, (int)v3d.z);
        debug_newline();
        v3_copy(&tail, &w->wire[41]);
        debug_printf(str_en_balloon_tail, (int)tail.x, (int)tail.y, (int)tail.z);
        debug_newline();
    }
    if (debug_flags_test(16) && EN_BALLOON_REC->test) {
        debug_set_origin(10, 10);
        enemy_task_get_pos(obj, &pos);
        debug_printf(str_en_balloon_pos, (int)pos.x, (int)pos.y, (int)pos.z);
        debug_newline();
        enemy_task_get_param_vec(obj, &vel);
        debug_printf(str_en_balloon_vel, vel.x, vel.y, vel.z);
        debug_newline();
        enemy_task_get_rot(obj, (int *)&rot);     /* reuses the first block's vectors */
        debug_printf(str_en_balloon_ang, (int)(((int *)&rot)[0] * DEG_PER_ANG), (int)(((int *)&rot)[1] * DEG_PER_ANG),
                     (int)(((int *)&rot)[2] % 65536 * DEG_PER_ANG));
        debug_newline();
        gfx = enemy_task_gfx_ptr(obj);
        debug_printf(str_en_balloon_frm, (int)gfx->frame, (int)gfx->frames);
        debug_newline();
        debug_printf(str_en_balloon_cnt, (int)w->cnt, (int)w->cnt_end);
        debug_newline();
        gfx = &w->part[0];
        debug_printf(str_en_balloon_frm, (int)gfx->frame, (int)gfx->frames);
        debug_newline();
        debug_printf(str_en_balloon_sca, w->size);
        debug_newline();
        v3_copy(&body, &w->body);
        debug_printf(str_en_balloon_pos, (int)body.x, (int)body.y, (int)body.z);
        debug_newline();
        v3_copy(&bar, &w->bar_to[0]);
        debug_printf(str_en_balloon_pct, (int)bar.x, (int)bar.y, (int)bar.z);
        debug_newline();
        v3_copy(&pt, &w->pts[0]);
        debug_printf(str_en_balloon_pcb, (int)pt.x, (int)pt.y, (int)pt.z);
        debug_newline();
        col = motion_sample_rgba((void *)en_balloon_wire_col_motion,
                                   (float)(unsigned int)(((int *)en_balloon_wire_col_motion)[1] - 1) * beat_clock_get_phase(),
                                   0.0f, 0.0f, 0.0f, 0.0f);
        debug_set_color(col);
        debug_printf(str_en_balloon_col, color_get_a(col), ((unsigned int (*)(int))color_get_r)(col), color_get_g(col),
                     color_get_b(col));
        debug_set_color(RGBA(0x60, 0x60, 0x60, 0x80));
        debug_newline();
        bcol = motion_sample_rgba((void *)en_balloon_bar_col_motion,
                                   (float)(unsigned int)(((int *)en_balloon_bar_col_motion)[1] - 1) * beat_clock_get_phase(),
                                   0.0f, 0.0f, 0.0f, 0.0f);
        debug_set_color(bcol);
        debug_printf(str_en_balloon_col, color_get_a(bcol), ((unsigned int (*)(int))color_get_r)(bcol), color_get_g(bcol),
                     color_get_b(bcol));
        debug_set_color(RGBA(0x60, 0x60, 0x60, 0x80));
        debug_newline();
        if (beat_grid_hit(0)) {
            debug_set_color(RGBA(0x80, 0, 0, 0x80));
            debug_printf(str_en_balloon_bpm, beat_clock_get_phase(), beat_clock_get_bar());
            debug_set_color(RGBA(0x60, 0x60, 0x60, 0x80));
        } else
            debug_printf(str_en_balloon_bpm, beat_clock_get_phase(), beat_clock_get_bar());
        debug_newline();
        i3_copy(core, w->core);
        debug_printf(str_en_balloon_ang, (int)(core[0] * DEG_PER_ANG), (int)(core[1] * DEG_PER_ANG),
                     (int)(core[2] % 65536 * DEG_PER_ANG));
        debug_newline();
    }
}

/* steer(cast) */
/* 0x00aff818 */
void en_balloon_init(void *obj, EnBalloonWork *w)
{
    w->pos = enemy_task_pos_ptr(obj);
    w->vel = enemy_task_param_vec_ptr(obj);
    w->ang = enemy_task_rot_ptr(obj);
    w->scale = enemy_task_scl_ptr(obj);
    enemy_task_clear_flags(obj, 64);
    lockon_obj_init(&w->lock, obj, &w->lpos, 0, en_balloon_lock_name, enemy_task_get_status_slot(obj), EN_BALLOON_REC->hp, EN_BALLOON_REC->hp, 0);
    lockon_entry(&w->lock);
    enemy_task_get_rot(obj, w->core);
    w->size = 1.0f;
    dc_object_init(&w->part[0], *((struct DcNode **)pnb_get_entry(stage_models, 4)), *((struct DcClip **)pnb_get_entry(stage_models, 5)), 0);
    dc_object_init(&w->part[1], *((struct DcNode **)pnb_get_entry(stage_models, 6)), *((struct DcClip **)pnb_get_entry(stage_models, 7)), 0);
    dc_object_init(&w->part[2], *((struct DcNode **)pnb_get_entry(stage_models, 9)), *((struct DcClip **)pnb_get_entry(stage_models, 10)), 0);
    dc_object_rebuild(&w->part[0]);
    dc_object_rebuild(&w->part[1]);
    dc_object_rebuild(&w->part[2]);
    en_balloon_place_ctrlbars(obj, w);
    w->wire_reset = 1;
    enemy_task_set_appear(obj, 0, 0);
    enemy_task_set_vanish(obj, 0, 0);
}

/* 0x00aff9e8 */
void en_balloon_release(void *obj, EnBalloonReleaseWork *w)
{
    dc_object_release(&w->part[2]);
    dc_object_release(&w->part[1]);
    dc_object_release(&w->part[0]);
    lockon_remove(&w->lock);
}

/* 0x00affa28 */
int en_balloon_create(void *parent, void *rec)
{
    ObjDesc desc;
    ObjFuncs funcs;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&funcs, 0, sizeof funcs);
    funcs.fn[0] = en_balloon_init;
    funcs.fn[1] = en_balloon_release;
    funcs.fn[2] = en_balloon_exec;
    funcs.fn[3] = en_balloon_disp;
    funcs.fn[4] = en_balloon_pre_exec;
    funcs.fn[5] = en_balloon_post_exec;
    desc.name = en_balloon_name;
    desc.funcs = &funcs;
    desc.arg = rec;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 16;
    desc.model_no2 = -1;
    desc.hp = EN_BALLOON_REC->hp;
    desc.score = EN_BALLOON_REC->score;
    desc.size = 1788;
    desc.time = 0.0f;
    desc.wob_amp = 0;
    desc.wob_spd = 0;
    return enemy_task_task_create(parent, &desc);
}

/* steer(cast) */
/* 0x00affb58 */
void en_balloon_models_init(void)
{
    int *skin = &en_balloon_skin_wire;
    int *rel_a = &en_balloon_rel_wire_a;
    int *rel_b = &en_balloon_rel_wire_b;

    en_balloon_skin_model = *((int *)pnb_get_entry(stage_models, 12));
    en_balloon_rel_model_a = *((int *)pnb_get_entry(stage_models, 18));
    en_balloon_rel_model_b = *((int *)pnb_get_entry(stage_models, 11));
    *skin = model_pnb_get_wire(stage_models, 12);
    *rel_a = model_pnb_get_wire(stage_models, 18);
    *rel_b = model_pnb_get_wire(stage_models, 11);
    en_balloon_wire_col_motion = *((int *)pnb_get_entry(stage_models, 13));
    en_balloon_bar_col_motion = *((int *)pnb_get_entry(stage_models, 8));
    node_tree_collect_to((void *)*((int *)pnb_get_entry(stage_models, 4)), 0x2000000, (struct NodeTreeNode * *)en_balloon_break_nodes_a);
    node_tree_collect_to((void *)*((int *)pnb_get_entry(stage_models, 6)), 0x2000000, (struct NodeTreeNode * *)en_balloon_break_nodes_b);
    en_balloon_balloon_nodes_collect();
}

/* 0x00affca8 */
void en_balloon_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
