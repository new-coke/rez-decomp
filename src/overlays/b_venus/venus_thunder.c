/*
 * Venus boss thunder: twisting, springy bolts between two points, run in bundles by a manager that
 * fades them and jolts them when the core is hit; the lockable spiked thunder ball drifting toward
 * the player; and the vertical bolts the plate manager fires with each shock ring.
 */

#include "types_gw.h"
#include "b_venus.h"
#include "color.h"
#include "mathdef.h"
#include "debug.h"
#include "enemy_status.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "venus_thunder.h"
#include "venus_plate.h"
#include "../common/boss_common.h"
#include "../common/boss_joint.h"

extern char venus_thunder_vardef[];
extern GwVarDef venus_thunder_vardefs[];
extern struct GwVarNode *venus_thunder_vars_group;
extern float cm_thl_th_w0;
extern float cm_thl_th_w1;
extern float cm_thl_th_w2;
extern float th_ball_scl;
extern int th_ball_thl_sw;
extern int cm_thl_num;
extern float cm_thl_len_rate;
extern float cm_thl_stiff;
extern float cm_thl_damp;
extern float cm_thl_twist;
extern float cm_thl_amp_min;
extern float cm_thl_amp_max;
extern float cm_thl_spd_min;
extern float cm_thl_spd_max;
extern struct TaskDesc th_ball2_desc;
extern float th_ball2_y0;
extern float th_ball2_y1;
extern float th_ball2_sec;
extern int th_ball2_num;
extern char str_venus_thunder_c[];
extern struct TaskDesc cm_thl_man_desc;
extern float th_ball_y_max;

/* thunder ball spike, 52 bytes, eight per ball */
typedef struct VenusArm {
    char pad_00[4];
    Vec3 base;                  /* 0x04: the near end */
    Vec3 tip;                   /* 0x10: the far end */
    int ang[3];                 /* 0x1c */
    int dang[3];                /* 0x28 */
} VenusArm;

/* thunder ball (th_ball_desc, 576 bytes): a spiked ball drifting towards the player */
typedef struct ThBallWork {
    void *models;
    void *textures;             /* 0x04 */
    Vec3 pos;                   /* 0x08 */
    Vec3 vel;                   /* 0x14 */
    Vec3 dir;                   /* 0x20: the start direction, then the drag base */
    int ang[3];                 /* 0x2c */
    ModelPart **parts;          /* 0x38 */
    VenusArm arms[8];           /* 0x3c */
    float arm_len;              /* 0x1dc */
    float arm_len2;             /* 0x1e0 */
    float sway_phase;           /* 0x1e4 */
    char pad_1e8[4];
    LockObj lock;               /* 0x1ec */
    int mode;                   /* 0x22c: 1 keep out of th_ball_inner_r, 2 spin and rise */
    float spin;                 /* 0x230: degrees */
    float spin_vel;             /* 0x234 */
    float rise;                 /* 0x238 */
    float rise2;                /* 0x23c */
} ThBallWork;

extern int th_ball_num;
extern float th_ball_ring_r;
extern float cm_thl_man_spring_k;
extern float cm_thl_man_spring_damp;
extern float cm_thl_man_jolt_r;
extern struct TaskDesc cm_thl_desc;
extern float cm_thl_w0;
extern float cm_thl_w1;
extern float cm_thl_w2;
extern struct TaskDesc th_ball_desc;
extern float th_ball_start_ofs;
extern const float th_ball_arm_len;
extern const float th_ball_arm_len2;
extern float th_ball_arm_spin;
extern int th_ball_thl_num;
extern float th_ball_thl_amp;
extern float th_ball_drag;
extern float th_ball_accel;
extern float th_ball_sway;
extern float th_ball_vel_rate;
extern float th_ball_spin_accel;
extern const float th_ball_rise;
extern float th_ball_inner_r;
extern float th_ball_outer_r;
extern float th_ball_tgt_r;
extern float th_ball_push;
extern float cm_thl_seg_rate;

/* words of th_ball2_create's work (36 bytes) before its two line ends */
#ifdef VERSION_US
#define VENUS_CB2B_PAD 2
#else
#define VENUS_CB2B_PAD 0
#endif

/* 0x00cb2e18 */
void venus_thunder_vars_add(void)
{
    venus_thunder_vars_group = gw_varedit_add(&gw_varedit_root, 0, venus_thunder_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_thunder_vars_group, venus_thunder_vardefs);
}

/* 0x00cb2e68 */
void venus_thunder_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_thunder_vardef);
}

/* steer(cast) */
/* 0x00cb2e80 */
void cm_thl_exec(VenusPartObj *w)
{
    Vec3 end_a, end_b, span, twist;
    int ang[3];
    float sc[4];
    ThlWave *wave;
    BossJointLink *node;
    Vec3 *v;
    int k;
    float len;

    wave = w->waves + 1;
    for (k = w->num - 2; k; k--, wave++) {
        wave->phase += wave->spd;
        ulSinCosf(wave->phase * 3.0f, &sc[0], &sc[1]);
        wave->ofs.x = wave->amp * sc[0];
        wave->ofs.y = wave->ofs.z = 0.0f;
        ROT_Y((int)(wave->phase * ANG_PER_RAD), (sc + 2), wave->ofs.x, wave->ofs.z);
    }
    v3_sum(&end_a, w->from, &w->ofs_from);
    v3_sum(&end_b, w->to, &w->ofs_to);
    v3_sub(&span, &end_b, &end_a);
    if (v3_is_zero(&span))
        len = 0.01f;
    else
        len = vec3_length(&span);
    len /= (float)(w->num - 1);
    boss_joint_chain_set_len(w->joints, len * cm_thl_seg_rate);
    v3_sub(&w->dir, &end_b, &end_a);
    wobble_step_slow(&twist, w->twist, &w->twist_phase);
    do {
        float x = twist.x, y = twist.y, z = twist.z;

        ang[0] = DEG_TO_ANG(x);
        ang[1] = DEG_TO_ANG(y);
        ang[2] = DEG_TO_ANG(z);
    } while (0);
    vec_rotate_zxy(&w->dir, ang);
    boss_joint_chain_set_spring(w->joints, w->stiff, w->damp, 1.0f);
    boss_joint_root_set(w->joints, &end_a, &w->dir, 0.0f);
    boss_joint_set_pos(w->joints + (w->num - 1), &end_b);
    boss_joint_step_both(w->joints, w->joints2, 1.0f);
    wave = w->waves + 1;
    node = w->joints;
    v = w->pts + 1;
    for (k = w->num - 2; k; k--) {
        boss_joint_get_pose(node, v, 0);
        node = node->next;
        v3_sum(v, &wave->ofs, v);
        wave++;
        v++;
    }
    v3_copy(w->pts, &end_a);
    v3_copy(w->pts + w->num - 1, &end_b);
    if (w->fade_count && --w->fade_count < 2)
        FINISH_IF(w);
}

/* 0x00cb3320 */
void cm_thl_disp(VenusPartObj *w)
{
    float fade;

    ulGxSetTextureList(w->textures);
    draw_set_blend(4, 1);
    if (w->fade_count) {
        fade = (float)w->fade_count / (float)w->fade_total;
        venus_line_draw((int)w->pts, w->num, w->tex, (w->col & 0xffffff) | (((int)(fade * 255.0f) + 1) >> 1 << 24),
                               w->w0 * fade, w->w1 * fade, w->w2 * fade);
    } else {
        venus_line_draw((int)w->pts, w->num, w->tex, w->col, w->w0, w->w1, w->w2);
    }
}

/* 0x00cb33f8 */
void cm_thl_release(VenusPartObj *w)
{
    heap_free(w->joints, 0);
    heap_free(w->pts, 0);
    heap_free(w->waves, 0);
}

/* 0x00cb3438 */
void *cm_thl_create(VenusPartSet *mng, ThlRec *rec)
{
    VenusPartObj *thl = task_create_inherit(mng, &cm_thl_desc, 120);
    ThlWave *wave;
    Vec3 up, step;
    int k;
    float z;
    int rnd;
    float width1, width2, width0;

    thl->len_rate = rec->len_rate;
    thl->from = rec->from;
    thl->stiff = rec->stiff;
    thl->tex = rec->tex;
    thl->damp = rec->damp;
    thl->to = rec->to;
    thl->num = rec->num;
    thl->twist = rec->twist;
    rnd = rand_lcg_next_a();
    width1 = cm_thl_w1;
    width2 = cm_thl_w2;
    width0 = cm_thl_w0;
    thl->w1 = width1;
    thl->w2 = width2;
    thl->w0 = width0;
    thl->col = COL_WHITE;
    thl->twist_phase = rnd / 32769.0f * 2.0f * PI;
    v3_copy(&thl->ofs_from, &rec->ofs_from);
    v3_copy(&thl->ofs_to, &rec->ofs_to);
    thl->joints = heap_block_tag(game_calloc(thl->num * 456), str_venus_thunder_c, 318);
    thl->joints2 = (BossJointLink *)((int)thl->joints + thl->num * 152);
    thl->pts = heap_block_tag(game_calloc(thl->num * 12), str_venus_thunder_c, 320);
    thl->waves = heap_block_tag(game_calloc(thl->num << 5), str_venus_thunder_c, 321);
    wave = thl->waves;
    for (k = thl->num; k; wave++) {
        k--;
        wave->amp = RAND_FLOAT_A() * (rec->amp_max - rec->amp_min) + rec->amp_min;
        wave->phase = wave->spd = RAND_FLOAT_A() * (rec->spd_max - rec->spd_min) + rec->spd_min;
    }
    z = 0.0f;
    up.z = z;
    up.y = 1.0f;
    up.x = z;
    v3_sub(&step, thl->to, thl->from);
    v3_scale(&step, thl->len_rate / (float)thl->num);
    boss_joint_chain_init(thl->joints, thl->from, &step, &up, 0, 0, 0, thl->num, thl->stiff, thl->damp, 1.0f);
    return thl;
}

/* 0x00cb3750 */
void cm_thl_man_exec(VenusPartSet *w)
{
    VenusPartEnt *ent = w->e;
    int i;

    for (i = 0; i < w->n; i++, ent++) {
        spring_step(&ent->jolt_vel.x, &ent->jolt.x, cm_thl_man_spring_k, cm_thl_man_spring_damp);
        spring_step(&ent->jolt_vel.z, &ent->jolt.z, cm_thl_man_spring_k, cm_thl_man_spring_damp);
        if (!v3_is_zero(&ent->jolt))
            v3_sum(&ent->end, w->to, &ent->jolt);
        else
            v3_copy(&ent->end, w->to);
    }
}

/* 0x00cb3898 */
void cm_thl_man_release(VenusPartSet *w)
{
    heap_free(w->e, 0);
}

/* 0x00cb38a8 */
VenusPartSet *cm_thl_man_create(void *parent, ThlRec *rec, int n)
{
    VenusPartSet *w = task_create_inherit(parent, &cm_thl_man_desc, 24);
    int i;

    w->n = n;
    w->e = heap_block_tag(game_calloc(n * 48), str_venus_thunder_c, 388);
    w->from = rec->from;
    w->to = rec->to;
    for (i = 0; i < n; i++) {
        rec->to = &w->e[i].end;
        w->e[i].o = cm_thl_create(w, rec);
    }
    return w;
}

/* 0x00cb3988 */
void cm_thl_man_set_fade(VenusPartSet *w, int fade)
{
    int i;

    for (i = 0; i < w->n; i++) {
        w->e[i].o->fade_total = fade;
        w->e[i].o->fade_count = fade;
    }
}

/* 0x00cb39d0 */
void cm_thl_man_jolt(VenusPartSet *s)
{
    VenusPartEnt *ent;
    Vec3 kick;
    float sc[2];
    int i;

    for (i = 0, ent = s->e; i < s->n; i++, ent++) {
        if (v3_is_zero(&ent->jolt))
            kick.z = cm_thl_man_jolt_r * RAND_FLOAT_B();
        else
            kick.z = (cm_thl_man_jolt_r - vec3_length(&ent->jolt)) * RAND_FLOAT_B();
        kick.x = 0.0f;
        kick.y = 0.0f;
        ROT_Y((int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f), sc, kick.x, kick.z);
        v3_add(&ent->jolt, &kick);
    }
}

/* 0x00cb3c00 */
void cm_thl_rec_init(ThlRec *w, Vec3 *from, Vec3 *to)
{
    int num;

    w->len_rate = cm_thl_len_rate;
    w->stiff = cm_thl_stiff;
    w->damp = cm_thl_damp;
    w->twist = cm_thl_twist;
    w->amp_min = cm_thl_amp_min;
    w->amp_max = cm_thl_amp_max;
    w->spd_min = cm_thl_spd_min;
    w->spd_max = cm_thl_spd_max;
    num = cm_thl_num;
    v3_set(&w->ofs_from, 0.0f, 0.0f, 0.0f);
    w->from = from;
    w->to = to;
    w->tex = 7;
    w->num = num;
    v3_set(&w->ofs_to, 0.0f, 0.0f, 0.0f);
}

/* 0x00cb3ca0 */
void cm_thl_disp_th(VenusPartObj *w)
{
    void *old_fog = fog_set_enable(0);

    draw_set_blend(4, 1);
    venus_line_draw_capsules((int)w->pts, w->num, w->tex, COL_WHITE, cm_thl_th_w0, cm_thl_th_w1, cm_thl_th_w2);
    fog_set_enable(old_fog);
}

/* 0x00cb3d18 */
void th_ball_arms_step(ThBallWork *w)
{
    VenusArm *arm = w->arms;
    Vec3 v;
    float dir_x = 0.0f, dir_z = -1.0f;
    int i;

    for (i = 8; i != 0; i--, arm++) {
        do {
            i3_sum(arm->ang, arm->ang, arm->dang);
        } while (0);
        v.y = v.x = dir_x * w->arm_len;
        v.z = dir_z * w->arm_len;
        vec_rotate_zxy(&v, arm->ang);
        v3_sum(&arm->base, &w->pos, &v);
        vec_set_length(&v, w->arm_len2);
        v3_sum(&arm->tip, &w->pos, &v);
    }
}

/* steer(cast) */
/* 0x00cb3e48 */
void th_ball_move(ThBallWork *w)
{
    Vec3 sway, tgt, old_vel, pos_xz, push;
    float sc[2];
    float pl_y, len;

    v3_copy(&old_vel, &w->vel);
    v3_mul(&w->vel, &w->dir, th_ball_drag);
    player_get_pos(&tgt);
    pl_y = tgt.y;
    tgt.y = 0.0f;
    vec_set_length(&tgt, th_ball_tgt_r);
    tgt.y = pl_y;
    v3_sub(&tgt, &tgt, &w->pos);
    vec_set_length(&tgt, th_ball_accel);
    v3_add(&w->vel, &tgt);
    v3_lerp(&w->vel, &old_vel, &w->vel, th_ball_vel_rate);
    pos_xz.x = w->pos.x;
    pos_xz.y = 0.0f;
    pos_xz.z = w->pos.z;
    len = vec3_length(&pos_xz);
    if ((w->mode & 1) && len < th_ball_inner_r) {
        push.x = w->pos.x;
        push.y = 0.0f;
        push.z = w->pos.z;
        vec_set_length(&push, th_ball_push * (th_ball_inner_r - len) / th_ball_inner_r);
        v3_add(&w->pos, &push);
    }
    v3_add(&w->pos, &w->vel);
    wobble_step_slow(&sway, th_ball_sway, &w->sway_phase);
    v3_add(&w->pos, &sway);
    if (w->mode & 2) {
        w->spin_vel += th_ball_spin_accel;
        w->spin += w->spin_vel;
        w->rise = w->rise2 = th_ball_rise;
        ROT_Y((int)DEG_TO_ANG(w->spin), sc, w->pos.x, w->pos.z);
        w->pos.y += w->rise;
        if (th_ball_outer_r < len) {
            push.x = -w->pos.x;
            push.y = 0.0f;
            push.z = -w->pos.z;
            vec_set_length(&push, th_ball_push * (len - th_ball_outer_r) / th_ball_outer_r);
            v3_add(&w->pos, &push);
        }
    }
}

/* 0x00cb4218 */
void th_ball_exec(ThBallWork *w)
{
    int n;

    for (n = enemy_status_get_ticks(); n; n--) {
        th_ball_arms_step(w);
        th_ball_move(w);
    }
    if ((w->lock.flags & LOCK_HIT) && w->lock.life <= 0)
        FINISH_IF(w);
    if (th_ball_y_max < w->pos.y)
        FINISH_IF(w);
    if (debug_flags_test(4)) {
        if (kbd_state[7] & 0x44) {
            switch (kbd_state[5]) {
            case 8:
                w->mode |= 1;
                break;
            case 9:
                w->mode |= 2;
                break;
            }
        }
    }
}

/* 0x00cb4338 */
void th_ball_disp(ThBallWork *w)
{
    float scl;

    ulGxSetTextureList(w->textures);
    draw_set_override_flags(0);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    venus_thunder_rotate_yxz(w->ang);
    scl = th_ball_scl;
    vu0_matrix_scale(ulMatrixCur, scl, scl, scl);
    if (!th_ball_thl_sw)
        dc_object_draw(w->parts[15]);
    ulMatrixPop();
}

/* 0x00cb43d0 */
void th_ball_release(ThBallWork *w)
{
    lock_remove_entry(0, &w->lock);
}

/* 0x00cb43e0 */
int *th_ball_create(void *parent, void *models, Vec3 *pos, Vec3 *center)
{
    ThlRec rec;
    ThBallWork *ball = task_create_inherit(parent, &th_ball_desc, 576);
    Vec3 *dir;
    VenusArm *arm;
    int i;

    v3_copy(&ball->pos, pos);
    ball->parts = models;
    dir = &ball->dir;
    ball->dir.x = ball->pos.x;
    ball->dir.z = ball->pos.z;
    vec3_normalize(dir, dir);
    v3_scale(dir, th_ball_start_ofs);
    v3_add(&ball->pos, dir);
    ball->sway_phase = RAND_FLOAT_A() * 2.0f * PI;
    ball->arm_len = th_ball_arm_len;
    ball->arm_len2 = th_ball_arm_len2;
    for (i = 0, arm = ball->arms; i < 8; i++, arm++) {
        arm->ang[0] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        arm->ang[1] = DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f);
        arm->dang[0] = DEG_TO_ANG(RAND_FLOAT_A() * (th_ball_arm_spin + th_ball_arm_spin) - th_ball_arm_spin);
        arm->dang[1] = DEG_TO_ANG(RAND_FLOAT_A() * (th_ball_arm_spin + th_ball_arm_spin) - th_ball_arm_spin);
    }
    if (th_ball_thl_sw) {
        cm_thl_rec_init(&rec, center, &ball->pos);
        rec.amp_min = th_ball_thl_amp;
        rec.num = th_ball_thl_num;
        rec.amp_max = th_ball_thl_amp;
        TASK_OF(cm_thl_create((VenusPartSet *)ball, &rec))->disp = (void (*)(void *))cm_thl_disp_th;
    }
    lock_init(0, &ball->lock, ball, &ball->pos, 0, 1, 0);
    lockon_entry(&ball->lock);
    return (int *)ball;
}

/* 0x00cb46e8 */
void th_ball_ring_create(void *parent, void *models, Vec3 *center)
{
    Vec3 v;
    float sc[2];
    int i;

    for (i = 0; i < th_ball_num; i++) {
        v.z = th_ball_ring_r;
        v.x = v.y = 0.0f;
        ROT_Y(i * 65535 / th_ball_num, sc, v.x, v.z);
        v3_sum(&v, center, &v);
        th_ball_create(parent, models, &v, center);
    }
}

/* 0x00cb4828 */
void th_ball2_exec(int *w)
{
    if (--w[VENUS_CB2B_PAD + 6] <= 0)
        FINISH_IF(w);
}

/* 0x00cb4858 */
int *th_ball2_create(void *parent)
{
    ThlRec rec;
#ifdef VERSION_US
    int *w = task_create_inherit(parent, &th_ball2_desc, (VENUS_CB2B_PAD + 7) * 4);
#else
    int *w = task_create(parent, th_ball2_desc, (VENUS_CB2B_PAD + 7) * 4);
#endif
    VenusPartSet *mng;
    int i;

    ((float *)w)[VENUS_CB2B_PAD + 1] = th_ball2_y0;
    ((float *)w)[VENUS_CB2B_PAD + 4] = th_ball2_y1;
    cm_thl_rec_init(&rec, (Vec3 *)(w + VENUS_CB2B_PAD), (Vec3 *)(w + VENUS_CB2B_PAD + 3));
    mng = cm_thl_man_create(w, &rec, th_ball2_num);
    w[VENUS_CB2B_PAD + 6] = th_ball2_sec * FPS;
    for (i = 0; i < mng->n; i++)
        TASK_OF(mng->e[i].o)->disp = (void (*)(void *))cm_thl_disp_th;
    return w;
}

/* 0x00cb4920 */
void venus_thunder_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
