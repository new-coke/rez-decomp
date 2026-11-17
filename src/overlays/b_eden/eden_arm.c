/*
 * Eden boss arm form: a ring of jointed arms (boss_joint chains) that follows the player, banks
 * towards them and swings single arms out to strike. The lockable pieces sit on the arm links.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "gw_varedit.h"
#include "heap.h"
#include "math_util.h"
#include "player.h"
#include "task.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_arm.h"
#include "eden_core.h"
#include "eden_snake.h"
#include "../common/boss_common.h"
#include "../common/boss_joint.h"

extern char eden_arm_vardef[];
extern GwVarDef eden_arm_vardefs[];
extern struct GwVarNode *eden_arm_vars_group;
extern float cmd_arm_rat_t;
extern float cmd_at_arm_dx;
extern float cmd_at_arm_dy;
extern struct TaskDesc eden_arm_joint_desc;
extern int cmd_joint_num;
extern float cmd_j_len;

typedef struct ChainSet {       /* an arm's BossJointLink chain */
    BossJointLink *links;           /* 0x00 */
    void *buf;                  /* 0x04 */
    BossJointLink *last;            /* 0x08 */
    int n;                      /* 0x0c */
    char pad_10[8];
} ChainSet;

extern float cmd_test_len;
extern float cmd_test_y;
extern float eden_arm_do_mf;
extern float eden_arm_do_kf;
extern float cmd_base_tr, cmd_base_mr, cmd_base_tr2, cmd_base_mr2;
extern float cmd_base_day, cmd_base_day2;
extern float cmd_base_rr;
extern struct TaskDesc eden_arm_man_desc;
extern char str_eden_arm_c[];
extern int eden_arm_num;
extern float cmd_base_day;
extern float cmd_base_day3;
extern float cmd_bank_st;
extern float cmd_bank_wt;
extern float cmd_bank_r;
extern float cmd_bank_deg;
extern float cmd_at_arm_len;
extern float cmd_at_arm_r;
extern float cmd_at_st_t;
extern int cmd_at_arm_pn;
extern float cmd_js_len, eden_arm_swing_r;
extern int cmd_arm_at_sw, cmd_arm_at_n;
extern const float cmd_at_shot_wt;
extern const float cmd_at_lmove_wt;
extern const float cmd_at_lmove_t;

/* an arm's strike: the tip leaves the chain and steps round a ring, 84 bytes */
typedef struct EdenArmAttack {
    int state;                  /* 0 idle, 1 reach, 2 step, 3 hold, 4 cool down */
    int time;
    int len;
    Vec3 arm;
    Vec3 cur;
    Vec3 ring;
    Vec3 *base;
    Vec3 anchor;
    float blend;
    int pt_idx;
    int pt_num;
    int sign;
    int ang_base;
} EdenArmAttack;

#define EDEN_RING_ANG(e, n) ((e)->ang_base + (e)->sign * (int)DEG_TO_ANG((float)(n) * 360.0f / (float)(e)->pt_num))

/* one arm's joint chain, 128 bytes */
typedef struct EdenArmJointWork {
    struct PnbHead *pack;
    void *tex_list;
    ChainSet chain;
    char pad_20[28];
    Vec3 dir;                   /* 0x3c */
    Vec3 up;
    Vec3 target;
    int anchor;                 /* Vec3 *: the attack's anchor */
    char pad_64[4];
    float anchor_t;             /* 0x68 */
    int node;
    int node2;
    int anchored;
    float stiff;
    float damp;
} EdenArmJointWork;

/* the arms form, 156 bytes */
typedef struct EdenArmManWork {
    struct PnbHead *pack;
    void *tex_list;
    EdenArmJointWork **joints;
    EdenArmAttack *attacks;
    int *parts;
    int n;
    Vec3 pos;
    float phase;
    int yaw;
    int ang[3];
    Vec3 vel;
    Vec3 spring;
    Vec3 target;
    int slot;                   /* 0..2, the side of the player it holds */
    int fast;
    int fast_time;
    float stiff;
    float damp;
    float yaw_spd;              /* degrees per frame */
    int bank_time;
    int bank;                   /* 0 off, 1 start, 2 in, 3 hold, 4 out */
    Vec3 face;
    float tilt;
    float quat[4];
} EdenArmManWork;

/* 0x00ca2e38 */
void eden_arm_vars_add(void)
{
    eden_arm_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_arm_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_arm_vars_group, eden_arm_vardefs);
}

/* 0x00ca2e88 */
void eden_arm_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_arm_vardef);
}

/* 0x00ca2ea0 */
void eden_arm_joint_exec(EdenArmJointWork *w)
{
    BossJointLink *link = w->chain.links;

    boss_joint_chain_set_spring(link, w->stiff, w->damp, 1.0f);
    boss_joint_root_set_up(w->chain.links, &w->target, &w->dir, &w->up, 0.0f);
    if (w->anchored) {
        boss_joint_set_pos(w->chain.last, (Vec3 *)w->anchor);
        boss_joint_step_both(w->chain.links, w->chain.buf, w->anchor_t);
    } else
        boss_joint_chain_step(w->chain.links);
    boss_joint_chain_twist(w->chain.links);
    w->anchored = 0;
}

/* 0x00ca2f38 */
void eden_arm_joint_release(EdenArmJointWork *w)
{
    boss_joint_release((void **)&w->chain);
}

/* 0x00ca2f40 */
int *eden_arm_joint_create(void *parent, int node, int node2)
{
    EdenArmJointWork *w = task_create_inherit(parent, &eden_arm_joint_desc, 128);

    w->node = node;
    w->node2 = node2;
    boss_joint_create(&w->chain.links, cmd_joint_num);
    boss_joint_chain_set_len(w->chain.links, cmd_j_len);
    return (int *)w;
}

/* 0x00ca2fb8 */
void eden_arm_joint_set_target(EdenArmJointWork *w, Vec3 *target, Vec3 *pos, Vec3 *dir)
{
    v3_copy(&w->target, target);
    v3_copy(&w->dir, pos);
    v3_copy(&w->up, dir);
}

/* 0x00ca3010 */
void eden_arm_joint_set_spring(EdenArmJointWork *w, float stiff, float damp)
{
    w->stiff = stiff;
    w->damp = damp;
}

/* 0x00ca3020 */
void eden_arm_joint_set_anchor(EdenArmJointWork *w, int anchor, float t)
{
    w->anchor = anchor;
    w->anchor_t = t;
    w->anchored |= 1;
}

/* 0x00ca3038 */
void eden_arm_attack_start(EdenArmAttack *w, Vec3 *base, float x, float y)
{
    float sc[4];
    float zero = 0.0f;
    float deg;

    w->state = 1;
    w->base = base;
    w->arm.z = cmd_at_arm_len;
    w->arm.y = zero;
    w->arm.x = zero;
    do {
        float rot_u = w->arm.y, rot_v = w->arm.z;

        deg = ANG_PER_DEG;
        ulSinCos((int)(x * deg), &sc[0], &sc[1]);
        w->arm.y = rot_u * sc[1] - rot_v * sc[0];
        w->arm.z = rot_u * sc[0] + rot_v * sc[1];
    } while (0);
    ROT_Y((int)(y * deg), (sc + 2), w->arm.x, w->arm.z);
    w->ring.y = cmd_at_arm_r;
    w->ring.z = zero;
    w->time = w->len = (int)(cmd_at_st_t * FPS);
    w->ring.x = zero;
    w->pt_idx = w->pt_num = cmd_at_arm_pn;
    w->sign = -((int)(RAND_FLOAT_A() * 2.0f) * 2) + 1;
    w->ang_base = (int)((float)(int)(RAND_FLOAT_A() * 2.0f) * 120.0f * deg);
}

/* 0x00ca3208 */
void eden_arm_attack_step(EdenArmAttack *atk)
{
    Vec3 pt_a, pt_b;

    switch (atk->state) {
    case 0:
        return;
    case 1: {
        float ease_sin, ease_cos;
        float roll_sin, roll_cos;

        ulSinCosf((1.0f - (float)atk->time / (float)atk->len) * PI, &ease_sin, &ease_cos);
        atk->blend = ease_cos * -0.5f + 0.5f;
        if (--atk->time <= 0) {
            atk->blend = 1.0f;
            atk->state++;
            atk->time = atk->len = (int)(cmd_at_lmove_t * FPS);
        }
        v3_copy(&atk->cur, &atk->ring);
        ROTS_Z(EDEN_RING_ANG(atk, atk->pt_idx), roll_sin, roll_cos, atk->cur.x, atk->cur.y);
        break;
    }
    case 2: {
        float sin_a, cos_a, sin_b, cos_b, ease_sin, ease_cos;
        float t;

        v3_copy(&pt_a, &atk->ring);
        v3_copy(&pt_b, &atk->ring);
        ROTS_Z(EDEN_RING_ANG(atk, atk->pt_idx), sin_a, cos_a, pt_a.x, pt_a.y);
        ROTS_Z(EDEN_RING_ANG(atk, atk->pt_idx - 1), sin_b, cos_b, pt_b.x, pt_b.y);
        ulSinCosf((float)atk->time / (float)atk->len * PI, &ease_sin, &ease_cos);
        t = ease_cos * -0.5f + 0.5f;
        v3_lerp(&atk->cur, &pt_a, &pt_b, t);
        if (--atk->time <= 0) {
            atk->state = 3;
            atk->pt_idx--;
            atk->time = atk->len = (int)(cmd_at_lmove_wt * FPS);
        }
        break;
    }
    case 3: {
        float roll_sin, roll_cos;

        v3_copy(&atk->cur, &atk->ring);
        ROTS_Z(EDEN_RING_ANG(atk, atk->pt_idx), roll_sin, roll_cos, atk->cur.x, atk->cur.y);
        if (--atk->time <= 0) {
            if (atk->pt_idx < 2) {
                atk->state = 4;
                atk->time = atk->len = (int)(cmd_at_shot_wt * FPS);
            } else {
                atk->state = 2;
                atk->time = atk->len = (int)(cmd_at_lmove_t * FPS);
            }
        }
        break;
    }
    case 4:
        if (--atk->time <= 0)
            atk->state = 0;
        break;
    }
    v3_sum(&atk->anchor, atk->base, &atk->arm);
    v3_add(&atk->anchor, &atk->cur);
}

/* 0x00ca3798 */
void eden_arm_orbit_offset(int ang, Vec3 *out)
{
    float sc[2];

    out->x = 0;
    out->y = cmd_test_y;
    out->z = -cmd_test_len;
    ROT_Y(ang * 21845, sc, out->x, out->z);
}

/* 0x00ca3830 */
void eden_arm_man_follow(EdenArmManWork *w)
{
    Vec3 v;

    eden_arm_orbit_offset(w->slot, &v);
    player_get_pos(&w->target);
    v3_add(&w->target, &v);
    spring_to_v(&w->vel, &w->spring, &w->target, eden_arm_do_mf, eden_arm_do_kf);
    v3_copy(&w->pos, &w->spring);
}

/* 0x00ca38e8 */
void eden_arm_man_face_player(EdenArmManWork *w)
{
    Vec3 pl_pos;

    player_get_pos(&pl_pos);
    v3_sub(&w->face, &pl_pos, &w->pos);
    vec3_normalize(&w->face, &w->face);
    quat_from_axis_angle(DEG_TO_ANG(w->tilt), w->quat, w->face.x, w->face.y, w->face.z);
    ulMatrixPush();
    matrix_unit_set(0);
    quat_to_matrix(w->quat);
    matrix_current_pose(0, w->ang);
    ulMatrixPop();
}

/* 0x00ca39c0 */
void eden_arm_man_bank(EdenArmManWork *w)
{
    float k, j, bank, pitch;

    switch (w->bank) {
    case 1:
        w->bank_time = (int)(cmd_bank_st * FPS);
        w->bank = 2;
        w->tilt = 0;
    case 2:
        k = cmd_base_rr;
        j = cmd_bank_r;
        bank = cmd_base_day3;
        pitch = cmd_bank_deg;
        w->yaw_spd = w->yaw_spd * k + (1.0f - k) * bank;
        w->tilt = w->tilt * j + pitch * (1.0f - j);
        if (--w->bank_time <= 0) {
            w->bank_time = (int)(cmd_bank_wt * FPS);
            w->bank++;
        }
        break;
    case 3:
        if (--w->bank_time <= 0) {
            w->bank_time = (int)(cmd_bank_st * FPS);
            w->bank = 4;
        }
        break;
    case 4:
        {
            float fade = cmd_bank_r;

            w->yaw_spd = w->yaw_spd * cmd_base_rr + (1.0f - cmd_base_rr) * cmd_base_day;
            w->tilt *= fade;
        }
        if (--w->bank_time <= 0)
            w->bank = 0;
        break;
    default:
        w->tilt *= cmd_bank_r;
        break;
    }
    eden_arm_man_face_player(w);
}

/* 0x00ca3b68 */
void eden_arm_man_base_update(EdenArmManWork *w)
{
    if (w->fast_time && --w->fast_time)
        w->fast = 1;
    else
        w->fast = 0;
    if (w->fast) {
        float k = cmd_base_rr;
        float x = cmd_base_tr2;
        float y = cmd_base_mr2;
        float z = cmd_base_day2;
        w->stiff = w->stiff * k + (1.0f - k) * x;
        w->damp = w->damp * k + (1.0f - k) * y;
        w->yaw_spd = w->yaw_spd * k + (1.0f - k) * z;
    } else {
        float k = cmd_base_rr;
        float x = cmd_base_tr;
        float y = cmd_base_mr;
        float z = cmd_base_day;
        w->stiff = w->stiff * k + (1.0f - k) * x;
        w->damp = w->damp * k + (1.0f - k) * y;
        w->yaw_spd = w->yaw_spd * k + (1.0f - k) * z;
    }
}

/* steer(cast) */
/* 0x00ca3c38 */
void eden_arm_man_exec(EdenArmManWork *arm)
{
    Vec3 wob;
    Vec3 ofs;
    Vec3 target;
    Vec3 wob_r, ofs_r;
    float sc[8];
    float z;
    int i;

    eden_arm_man_base_update(arm);
    eden_arm_man_follow(arm);
    wobble_step_slow(&wob, eden_arm_swing_r, &arm->phase);
    v3_add(&arm->pos, &wob);
    eden_arm_man_bank(arm);
    do {
        float k = ANG_PER_DEG;
        arm->yaw += (int)(arm->yaw_spd * k);
    } while (0);
    z = 0.0f;
    wob.y = z;
    wob.z = -1.0f;
    do {
        wob.x = z;
    } while (0);
    ofs.z = z;
    ofs.x = z;
    ofs.y = 1.0f;
    ROT_Y(arm->yaw, sc, wob.x, wob.z);
    ROT_Y(arm->yaw, (sc + 2), ofs.x, ofs.z);
    for (i = 0; i < arm->n; i++) {
        EdenArmAttack *atk = &arm->attacks[i];

        eden_arm_joint_set_spring(arm->joints[i], arm->stiff, arm->damp);
        v3_copy(&wob_r, &wob);
        v3_copy(&ofs_r, &ofs);
        vec_rotate_zxy(&wob_r, arm->ang);
        vec_rotate_zxy(&ofs_r, arm->ang);
        v3_mul(&target, &wob_r, cmd_js_len);
        v3_sum(&target, &arm->pos, &target);
        eden_arm_joint_set_target(arm->joints[i], &target, &wob_r, &ofs_r);
        ROT_Y((int)DEG_TO_ANG(360.0f / arm->n), (sc + 4), wob.x, wob.z);
        ROT_Y((int)DEG_TO_ANG(360.0f / arm->n), (sc + 6), ofs.x, ofs.z);
        if (atk->state) {
            eden_arm_attack_step(atk);
            eden_arm_joint_set_anchor(arm->joints[i], (int)&atk->anchor, atk->blend);
        }
        if (cmd_arm_at_sw) {
            cmd_arm_at_sw = 0;
            eden_arm_attack_start(&arm->attacks[cmd_arm_at_n], &arm->pos, cmd_at_arm_dx, cmd_at_arm_dy);
        }
    }
}

/* 0x00ca4060 */
void eden_arm_man_release(EdenArmManWork *w)
{
    heap_free(w->joints, 0);
    heap_free(w->attacks, 0);
    eden_core_shine_reset(0);
}

/* 0x00ca40a0 */
int *eden_arm_man_create(void *parent, int *arg)
{
    EdenArmManWork *w = task_create_inherit(parent, &eden_arm_man_desc, 156);
    int i;

    w->parts = arg;
    w->n = eden_arm_num;
    w->joints = heap_block_tag(game_malloc(w->n * 4), str_eden_arm_c, 431);
    w->attacks = heap_block_tag(game_calloc(w->n * 84), str_eden_arm_c, 432);
    for (i = 0; i < w->n; i++)
        w->joints[i] = (EdenArmJointWork *)eden_arm_joint_create(w, w->parts[0], w->parts[1]);
    eden_core_shine_set_req(0, (int)&w->pos);
    return (int *)w;
}

/* 0x00ca41a0 */
void eden_arm_man_get_counts(EdenArmManWork *w, int *nchain, int *nlink)
{
    int n = w->n;
    int *joints = (int *)w->joints; /* steer(alias): read as ints, after the store */

    *nchain = n;
    *nlink = ((EdenArmJointWork *)joints[0])->chain.n;
}

/* 0x00ca41c0 */
void eden_arm_man_get_link(EdenArmManWork *w, int i, int j, Vec3 **pos, int **ang)
{
    EdenArmJointWork *joint = w->joints[i % w->n];
    ChainSet *set = &joint->chain;

    if (pos)
        *pos = &set->links[j % set->n].pos;
    if (ang)
        *ang = set->links[j % set->n].ang;
}

/* 0x00ca4250 */
int eden_arm_man_get_chain(EdenArmManWork *w, int i)
{
    return w->attacks[i].state;
}

/* 0x00ca4268 */
void eden_arm_man_attack(EdenArmManWork *w, int i, int flags)
{
    float x, y;

    if (flags & 1)
        x = cmd_at_arm_dx;
    else
        x = -cmd_at_arm_dx;
    if (flags & 2)
        y = cmd_at_arm_dy;
    else
        y = -cmd_at_arm_dy;
    eden_arm_attack_start(&w->attacks[i], &w->pos, x, y);
}

/* 0x00ca42c0 */
void eden_arm_man_rotate_start(EdenArmManWork *w)
{
    w->fast_time = cmd_arm_rat_t * FPS;
    boss_se_play(10, 0, (int)&w->pos);
}

/* 0x00ca42f0 */
void eden_arm_man_next_mode(EdenArmManWork *w)
{
    if (!w->bank) {
        w->bank = 1;
        w->slot = (w->slot + 1) % 3;
    }
}
