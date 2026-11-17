/*
 * Earth boss arm: segmented tentacles grown from a boss model part, each a chain of up to 32 nodes
 * from a slot pool. Segments swing, thrust, sweep or wave on command; nodes are lock-on targets
 * that shake when hit and drop when destroyed, and can carry bubbles and cubes.
 */

#include "b_earth.h"
#include "mathdef.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "nj_math.h"
#include "player.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "earth_arm.h"
#include "earth.h"
#include "earth_bubble.h"
#include "earth_demo.h"
#include "../common/boss_common.h"

extern BossSlot arm_slots[210];
extern float arm_tip_k, arm_hit_shake, arm_halo_size, arm_halo_timer, arm_halo_fade_start;
extern unsigned int arm_colors[];
extern char earth_arm_vardef[], earth_arm_vardefs[];
extern const Vec3 arm_seg_dir_base;
extern Vec3 arm_seg_start_base;
extern float arm_yaw_limit, arm_seg_len, arm_wave_cfg0, arm_wave_cfg2, arm_wave_cfg1, arm_wave_cfg3;
extern float arm_spread[10];

extern BossArmLevel arm_levels[];
extern BossArmWork *arm_work;
extern struct GwVarNode *earth_arm_vars_group;
extern float arm_head_len;
extern float arm_shake_k, arm_shake_damp, arm_drop_time;
extern float arm_hit_push;
extern int arm_sweep_margin;
extern int arm_max_nodes;
extern signed char arm_wobble_buf[];
extern float arm_wobble_k;
extern float tt_t, tt_t2, tt_l2;
extern float arm_pitch_step;
extern float tt_l, tt_x, tt_y, arm_spin_step, arm_spin_max;
extern float tt_a2, tt_a3;
extern int tt_a1;
extern float arm_node_scale, arm_bubble_scale, arm_wave_time;

/* 0x00cadd60 */
void arm_collect_models(WireObj *obj, NjCnkModel **out, int *n)
{
    if (obj->model) {
        out[*n] = obj->model;
        (*n)++;
    }
    if (obj->child)
        arm_collect_models(obj->child, out, n);
}

/* 0x00cadd98 */
void arm_slots_clear(void)
{
    memset(arm_slots, 0, sizeof(arm_slots));
}

/* 0x00caddb0 */
int *arm_slot_alloc(int size)
{
    BossSlot *slot = arm_slots;
    int i;

    for (i = 0; i < 210; i++, slot++) {
        if (!slot->used) {
            slot->used = 1;
            return slot->data;
        }
    }
    return (int *)arm_slots;
}

/* 0x00caddf0 */
void arm_slot_free(int *data)
{
    unsigned int slot_addr = (unsigned int)data - 4;

    memset(&arm_slots[(slot_addr - (unsigned int)arm_slots) / 220], 0, 220);
}

/* 0x00cade28 */
BossArmNode *arm_seg_append(BossArmSeg *seg)
{
    BossArmNode *n = seg->head;
    BossArmNode *head;
    int i = 0;
    float len;

    if (n) {
        BossArmNode *u;

        for (; n->next; n = n->next)
            i++;
        if (i >= 32)
            return 0;
        u = (BossArmNode *)arm_slot_alloc(216);
        if (!u)
            return 0;
        *u = *n;
        u->flags = 0;
        v3_sum_to(&n->dir, &u->pos, &u->pos);
        n->shake = n->shake_v = 0.0f;
        v3_set(&n->hit_vec, 0.0f, 0.0f, 0.0f);
        n->next = u;
        u->prev = n;
        seg->count++;
        u->next = 0;
        seg->tail = u;
        u->unk_38 = 0;
        return u;
    }
    n = (BossArmNode *)arm_slot_alloc(216);
    i3_set(seg->pitch_add, 0, 0, 0);
    seg->head = head = n;
    seg->swing_yaw = 0;
    seg->pitch = 0;
    if (head) {
        v3_copy(&head->pos, &seg->dir);
        iv3_set(&head->unk_3c, 0, 0, 0);
        head->spread.z = len = sqrtf(seg->dir.x * seg->dir.x + seg->dir.y * seg->dir.y + seg->dir.z * seg->dir.z);
        head->flags = 0x400;
        seg->tail = head;
        seg->count++;
        head->ent = 0;
        head->prev = 0;
        head->next = 0;
        head->dir.x = seg->dir.x / len;
        head->unk_38 = 0;
        head->dir.y = seg->dir.y / len;
        head->dir.z = seg->dir.z / len;
        head->unk_2c.x = head->unk_2c.y = head->unk_2c.z = 0.0f;
        head->spread.x = seg->spread.x;
        head->shake = head->shake_v = 0.0f;
        v3_set(&head->hit_vec, 0.0f, 0.0f, 0.0f);
    }
    return head;
}

/* 0x00cae158 */
void arm_seg_remove(BossArmWork *arm, BossArmSeg *seg, int mode)
{
    BossArmNode *n = seg->head;
    BossArmNode *freed;

    seg->flags = 16;
    if (!n)
        return;
    if (!n->next) {
        arm_slot_free((int *)n);
        seg->head = 0;
        return;
    }
    switch (mode) {
    case 0:
        do {
            seg->head = n->next;
            seg->count--;
            arm_slot_free((int *)n);
        } while (0);
        return;
    case 1:
        while (n->next)
            n = n->next;
        freed = n;
        n = n->prev;
        n->next = 0;
        arm_slot_free((int *)freed);
        seg->count--;
        return;
    case -1:
        do {
            freed = n;
            n = n->next;
            arm_slot_free((int *)freed);
        } while (n);
        break;
    case -2:
        do {
            freed = n;
            n = n->next;
            cm_circle_create(arm, &freed->pos, arm_colors[4], arm_halo_size,
                                   arm_halo_fade_start, arm_halo_timer);
            arm_slot_free((int *)freed);
        } while (n);
        break;
    default:
        return;
    }
    seg->bubble_cell = -1;
    seg->count = 0;
    seg->flags = 0;
    seg->head = 0;
}

/* 0x00cae2f8 */
BossArmNode *arm_seg_node(BossArmSeg *seg, int i)
{
    BossArmNode *n = seg->head;

    if (i >= 0) {
        for (; i; i--) {
            if (!n)
                return 0;
            n = n->next;
        }
    } else {
        while (n->next)
            n = n->next;
    }
    return n;
}

/* always 0: the low bit of a pointer, shifted out */
#define AZ(p) ((((int)(p)) & 1) >> 1)

/* 0x00cae360 */
void arm_init(BossArmWork *arm, BossArmArg *arg)
{
    BossArmSeg *seg = arm->seg;
    int i, k;
    float sl;
    NjCnkModel **models;
    float seg_step = 0.7853982f;

    arm_slots_clear();
    {
        int *spin;
        ModelPart *part;
        void (*disp_cb)(void *w);
        float yaw;
        Vec3 *ofs, *pos, *dir;
        int *ang, *roty, *cmd;
        void *cb_arg;
        void (*update_cb)(void *w);

        part = (ModelPart *)*(int *)&arg->part;
        disp_cb = (void (*)(void *))*(int *)&arg->disp_cb;
        spin = arm->spin;
        models = arm->models;
        yaw = arm_yaw_limit;
        /* never taken (AZ is 0) */
        if (AZ(arg))
            arm->launch_state = 0;
        pos = arg->pos;
        ofs = arg->ofs;
        dir = arg->dir;
        ang = arg->ang;
        roty = arg->roty;
        cmd = arg->cmd;
        cb_arg = (void *)*(void (**)(void *))&arg->cb_arg;
        update_cb = arg->update_cb;
        arm->launch_state = 0;
        spin[0] = 0;
        *(void **)&spin[2] = 0;
        *(WireObj **)&spin[1] = 0;
        arm->pos = pos;
        arm->ofs = ofs;
        arm->dir = dir;
        arm->ang = ang;
        arm->roty = roty;
        arm->cmd = cmd;
        arm->cb_arg = cb_arg;
        arm->update_cb = update_cb;
        arm->disp_cb = disp_cb;
        arm->yaw_limit = yaw;
        arm->part = part;
        i = 0;
        arm_collect_models((WireObj *)part->root, models, &i);
    }
    arm->nmodels = i;
    models += i;
    *models = 0;
    k = boss_get_level();
    for (i = 0; i < 8; i++, seg++) {
        seg->dir.x = arm_seg_dir_base.x * njSin(seg_step * (float)i * ANG_PER_RAD);
        seg->dir.z = arm_seg_dir_base.z * njCos(seg_step * (float)i * ANG_PER_RAD);
        seg->dir.y = arm_seg_dir_base.y;
        seg->start.x = arm_seg_start_base.x * njSin(seg_step * (float)i * ANG_PER_RAD);
        seg->start.z = arm_seg_start_base.z * njCos(seg_step * (float)i * ANG_PER_RAD);
        seg->yaw = seg_step * (float)i * ANG_PER_RAD;
        seg->swing_yaw = 0;
        seg->pitch = 0;
        sl = arm_seg_len;
        seg->start.y = arm_seg_start_base.y;
        seg->unk_d4 = 0;
        seg->spread.x = arm_spread[0];
        seg->spread.y = arm_spread[1];
        seg->flags = 0;
        seg->spread.z = sl;
        seg->counter = arm_levels[k].hp;
        seg->wave_cfg[0] = arm_wave_cfg0 * FPS;
        seg->wave_cfg[1] = arm_wave_cfg1;
        seg->wave_cfg[2] = arm_wave_cfg2 * FPS;
        seg->target = 0;
        seg->count = 0;
        seg->active = 1;
        seg->flags = 16;
        seg->wave_cfg[3] = arm_wave_cfg3;
        seg->waves_left = 0;
        i3_set(seg->pitch_add, 0, 0, 0);
        seg->part = *arm->part;
    }
    arm_work = arm;
    earth_arm_vars_group = gw_varedit_add(&gw_varedit_root, 0, earth_arm_vardef);
    gw_varedit_add_table(&gw_varedit_root, earth_arm_vars_group, earth_arm_vardefs);
    unit_circle_build();
}

/* 0x00cae780 */
void arm_release(BossArmWork *arm)
{
    BossArmSeg *seg = arm->seg;
    BossArmNode *n;
    int i;

    for (i = 0; i < 8; i++, seg++) {
        for (n = seg->head; n; n = n->next) {
            if (n->flags & 0x800)
                lock_remove_entry(9, &n->lock);
        }
        arm_seg_remove(arm, seg, -1);
    }
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)earth_arm_vardef);
}

/* 0x00cae848 */
void arm_reinit(BossArmWork *arm, BossArmArg *arg)
{
    arm_release(arm);
    arm_init(arm, arg);
}

/* 0x00cae880 */
void arm_seg_lock_set(BossArmWork *arm, BossArmSeg *seg, int on)
{
    BossArmNode *n;
    int i;

    if (seg) {
        if (!seg->active || seg->flags & 0x10)
            return;
        for (n = seg->head; n; n = n->next) {
            if (n->flags & 0x800) {
                if (on)
                    n->lock.flags |= LOCK_LOCKABLE;
                else
                    n->lock.flags &= ~LOCK_LOCKABLE;
            }
        }
        return;
    }
    seg = arm->seg;
    for (i = 0; i < 8; i++, seg++) {
        if (!seg->active || seg->flags & 0x10)
            continue;
        for (n = seg->head; n; n = n->next) {
            if (n->flags & 0x800) {
                if (on)
                    n->lock.flags |= LOCK_LOCKABLE;
                else
                    n->lock.flags &= ~LOCK_LOCKABLE;
            }
        }
    }
}

/* 0x00cae980 */
void arm_seg_node_tips(BossArmSeg *seg)
{
    BossArmNode *n;
    Vec3 v;

    for (n = seg->head; n; n = n->next) {
        v3_mul(&v, &n->dir, arm_tip_k);
        v3_sum(&n->tip, &n->pos, &v);
    }
}

/* 0x00caea00 */
void arm_seg_place_head(Vec3 *pos, int *ang, BossArmSeg *seg)
{
    BossArmNode *n = seg->head;
    Vec3 v;

    if (!n)
        return;
    ulMatrixPush();
    matrix_unit_set(0);
    if (seg->flags & 0x10) {
        v.x = v.y = 0.0f;
        v.z = 1.0f;
        vu0_matrix_rotate_y(ulMatrixCur, seg->yaw);
        vu0_matrix_rotate_x(ulMatrixCur, seg->pitch + seg->pitch_add[0]);
        ulMatrixApplyVector((float *)ulMatrixCur, &v.x, &seg->dir.x);
        matrix_unit_set(0);
        MTX_TRANS(pos);
        arm_rotate_yxz(ang);
        ulMatrixApplyVector((float *)ulMatrixCur, &seg->dir.x, &n->dir.x);
        vec3_normalize(&n->dir, &n->dir);
        ulMatrixApplyPoint((float *)ulMatrixCur, &seg->start.x, &n->pos.x);
    } else {
        v.x = v.y = 0.0f;
        v.z = arm_head_len;
        vu0_matrix_rotate_y(ulMatrixCur, seg->yaw);
        vu0_matrix_rotate_x(ulMatrixCur, seg->pitch + seg->pitch_add[0]);
        ulMatrixApplyVector((float *)ulMatrixCur, &v.x, &seg->dir.x);
        v3_copy(&n->pos, &seg->dir);
        matrix_unit_set(0);
        MTX_TRANS(pos);
        arm_rotate_yxz(ang);
        ulMatrixApplyVector((float *)ulMatrixCur, &seg->dir.x, &n->dir.x);
        vec3_normalize(&n->dir, &n->dir);
        ulMatrixApplyPoint((float *)ulMatrixCur, &n->pos.x, &n->pos.x);
    }
    ulMatrixPop();
    n->spread.x = seg->spread.x;
    n->spread.y = seg->spread.y;
}

/* 0x00caec10 */
void arm_seg_scroll(BossArmSeg *seg)
{
    BossArmNode *n = seg->head;
    Vec3 pl_vec;

    if (!n)
        return;
    if (!(n = n->next))
        return;
    player_get_vec(&pl_vec);
    do {
        n->spread.x = seg->spread.x;
        n->spread.y = seg->spread.y;
        n->spread.z = seg->spread.z;
        v3_sum(&n->pos, &pl_vec, &n->pos);
    } while ((n = n->next));
}

/* 0x00caecb0 */
void arm_seg_swing(BossArmSeg *seg)
{
    float sc[2];

    if (!(seg->flags & 2))
        return;
    if (seg->swing_phase == 0.0f) {
        seg->spread.x = arm_spread[2];
        seg->spread.y = arm_spread[3];
    }
    seg->swing_phase += seg->swing_step;
    if (seg->swing_turns * PI <= seg->swing_phase) {
        seg->flags &= ~2;
        seg->spread.x = arm_spread[0];
        seg->spread.y = arm_spread[1];
    }
    ulSinCosf(seg->swing_phase, &sc[0], &sc[1]);
    seg->swing_yaw += (int)DEG_TO_ANG(seg->swing_amp * (0.5f - sc[1] * 0.5f));
}

/* 0x00caedb0 */
void arm_seg_thrust(BossArmSeg *seg)
{
    if (!(seg->flags & 4))
        return;
    seg->thrust_phase += seg->thrust_step;
    if (PI <= seg->thrust_phase) {
        seg->flags &= ~4;
        if (seg->recoil == 0.0f) {
            seg->flags |= 0x80000000;
            seg->bubble_cell = 0;
            if (seg->target)
                lock_remove_entry(9, &seg->target->lock);
        } else if (seg->target) {
            seg->target->lock.flags |= LOCK_LOCKABLE;
        }
        seg->target = 0;
    }
    seg->thrust_len += seg->thrust_speed;
    seg->spread.z = seg->thrust_len
                    * (1.0f - (1.0f - seg->recoil) * njSin(seg->thrust_phase * ANG_PER_RAD));
}

/* 0x00caeec8 */
void arm_node_hit(BossArmSeg *seg, BossArmNode *n, Vec3 *v)
{
    seg->flags |= 0x2020;
    if (!(seg->flags & 1))
        seg->blend = 1.0f;
    v3_copy(&n->hit_vec, v);
    n->flags |= 0x2000;
    n->shake += arm_hit_shake;
}

/* 0x00caef28 */
void arm_seg_shake(BossArmSeg *seg)
{
    BossArmNode *n = seg->head;
    int busy = 0;

    if (!(seg->flags & 0x2000))
        return;
    for (; n; n = n->next) {
        if (n->flags & 0x2000) {
            spring_step(&n->shake, &n->shake_v, arm_shake_damp, arm_shake_k);
            busy = 1;
            if (fabsf(n->shake) < FLT_EPSILON) {
                n->shake = n->shake_v = 0.0f;
                n->flags &= ~0x2000;
            }
        }
    }
    if (busy)
        return;
    seg->flags &= ~0x2000;
    if (!(seg->flags & 1))
        seg->flags &= ~0x20;
}

/* 0x00caf030 */
void arm_seg_retire(BossArmSeg *seg)
{
    BossArmNode *n;

    for (n = seg->head; n; n = n->next) {
        if (n->flags & 0x800)
            lock_remove_entry(9, &n->lock);
    }
    seg->bubble_cell = 0;
    seg->flags |= 0x80000000;
    seg->counter = arm_drop_time * FPS;
}

/* 0x00caf0c8 */
void arm_seg_drop(BossArmWork *arm, BossArmSeg *seg)
{
    BossArmNode *n = seg->head;
    Vec3 pl_vec, halo_pos;

    player_get_vec(&pl_vec);
    for (; n; n = n->next)
        v3_sum(&n->pos, &pl_vec, &n->pos);
    if (--seg->counter > 0)
        return;
    seg->counter = arm_drop_time * FPS;
    n = seg->head;
    if (!n)
        return;
    while (n->next)
        n = n->next;
    scroll_relative(&n->pos, &halo_pos);
    {
    unsigned int k; float x, y, z;
    k = arm_colors[4];
    x = arm_halo_size;
    y = arm_halo_fade_start;
    z = arm_halo_timer;
    cm_circle_create(arm, &halo_pos, k, x, y, z);
    }
    if (n->flags & 0x400) {
        if ((unsigned int)(n->ent->state - 4) < 2)
            n->ent->state = 6;
        arm_slot_free((int *)n);
        seg->head = 0;
        seg->count = 0;
        seg->flags = 16;
        seg->bubble_cell = -1;
    } else {
        if ((unsigned int)(n->ent->state - 4) < 2)
            n->ent->state = 6;
        n->prev->next = 0;
        arm_slot_free((int *)n);
    }
}

/* 0x00caf2a8 */
int arm_hit_check(BossArmWork *arm)
{
    BossArmSeg *seg = arm->seg;
    BossArmNode *n;
    Vec3 v;
    int i, any = 0;

    for (i = 0; i < 8; i++, seg++) {
        if (!(seg->flags & 0x80000010) && (n = seg->head)) {
            for (; n; n = n->next) {
                if (!(n->flags & 0x800))
                    continue;
                if (!(n->lock.flags & LOCK_HIT))
                    continue;
                seg->counter = n->lock.life;
                if (n->lock.life <= 0) {
                    boss_se_play((int)arm, 10, (int)&n->lock, 0);
                    boss_se_play((int)arm, 12, (int)&n->lock, 0);
                    boss_se_play((int)arm, 24, (int)&n->lock, 0);
                    arm_seg_retire(seg);
                    earth_arm_hit((BossMainWork *)arm->parent, 1);
                    lock_score(9, &n->lock);
                    break;
                }
                boss_se_play((int)arm, 9, (int)&n->lock, 0);
                if (seg->flags & 0x100)
                    continue;
                if (n->lock.imp.v.x == 0.0f && n->lock.imp.v.y == 0.0f && n->lock.imp.v.z == 0.0f)
                    continue;
                vec3_normalize(&v, &n->lock.imp.v);
                v3_scale(&v, arm_hit_push);
                arm_node_hit(seg, n, &v);
            }
        }
        if (seg->active)
            any = 1;
    }
    return any;
}

/* 0x00caf498 */
void arm_seg_sweep_start(BossArmSeg *seg, Vec3 *center, int n, float radius, float move_secs, float wait_secs, float slot_ofs)
{
    float t, u, k;

    if (seg->flags & 1)
        return;
    seg->flags |= 0x21;
    if ((int)(RAND_FLOAT_A() * 2.0f))
        seg->flags |= 0x40;
    else
        seg->flags &= ~0x40;
    if (n <= 3)
        n = 3;
    t = move_secs * FPS;
    u = wait_secs * FPS;
    k = 360.0f / (float)n;
    seg->sweep_r = radius;
    seg->sweep_phase = 0.0f;
    seg->blend = 0.0f;
    seg->sweep_step = 1.5707964f / t;
    seg->sweep_wait = u + u;
    seg->sweep_end = t * (float)n + u - (float)arm_sweep_margin;
    seg->sweep_n = n;
    v3_copy(&seg->sweep_center, center);
    seg->sweep_i = 0;
    seg->sweep_ang = DEG_TO_ANG(270.0f - k * (float)(n - 1) + k * slot_ofs);
}

/* 0x00caf648 */
void arm_seg_sweep(BossArmSeg *seg, Vec3 *pos, float *mtx)
{
    Vec3 rim, v;
    float sc[2];
    float t, k;
    BossArmNode *n;
    int i;

    if (!(seg->flags & 1))
        return;
    seg->sweep_phase += seg->sweep_step;
    if (seg->sweep_end != 0.0f && (seg->sweep_end -= 1.0f) <= 0.0f)
        seg->bubble_cell = 0;
    if (seg->sweep_i < 0) {
        k = 1.0f;
        if ((seg->sweep_wait -= 1.0f) <= 0.0f) {
            seg->flags &= ~0x21;
            arm_seg_lock_set(0, seg, 0);
        }
    } else {
        if (1.5707964f <= seg->sweep_phase) {
            seg->sweep_phase = t = 0.0f;
            i = ++seg->sweep_i;
            if (i >= seg->sweep_n)
                seg->sweep_i = -1;
            else
                seg->sweep_ang += (int)DEG_TO_ANG(i * 90);
        } else {
            t = njSin(seg->sweep_phase * ANG_PER_RAD);
        }
        k = t;
        if (!seg->sweep_i) {
            k = 1.0f;
            seg->blend = t;
        } else {
            seg->blend = 1.0f;
        }
    }
    v3_mul(&v, &seg->sweep_center, seg->blend);
    ulMatrixApplyPoint(mtx, (float *)&v, (float *)&v);
    v3_sum(&v, pos, &v);
    ulSinCos(seg->sweep_ang, &sc[0], &sc[1]);
    rim.x = sc[0] * seg->sweep_r;
    rim.y = sc[1] * seg->sweep_r;
    rim.z = 0.0f;
    ulMatrixApplyPoint(mtx, (float *)&rim, (float *)&rim);
    if (!seg->sweep_i) {
        v3_sum(&seg->tail->pos, &v, &rim);
    } else {
        v3_add(&v, &rim);
        v3_lerp(&seg->tail->pos, &v, &seg->tail->pos, k);
    }
    n = seg->tail;
    v3_mul(&v, &n->hit_vec, n->shake_v);
    v3_add(&n->pos, &v);
}

/* 0x00caf9c8 */
void arm_command(BossArmWork *arm)
{
    BossArmSeg *seg;
    BossArmNode *n;
    int i;

    switch (*arm->cmd) {
    case 2:
        if (!arm->launch_state)
            arm->launch_state = 1;
        break;
    case 3:
        if (!arm->launch_state)
            arm->launch_state = 4;
        break;
    case 4:
        seg = arm->seg;
        for (i = 0; i < 8; i++, seg++) {
            if (seg->active) {
                if (!(seg->flags & 0x80000000)) {
                    for (n = seg->head; n; n = n->next) {
                        if (n->flags & 0x800)
                            lock_remove_entry(9, &n->lock);
                    }
                }
                arm_seg_remove(arm, seg, -2);
            }
            boss_se_play((int)arm, 12, 0, 0);
        }
        arm->launch_state = 0;
        arm->enabled = 0;
        break;
    case 5:
        seg = arm->seg;
        for (i = 0; i < 8; i++, seg++) {
            arm_seg_set_carried(arm, seg, 6, 0);
            arm_seg_remove(arm, seg, -2);
            earth_arm_hit((BossMainWork *)arm->parent, 0);
            boss_se_play((int)arm, 12, 0, 0);
        }
        arm->launch_state = 0;
        break;
    }
    if (arm->launch_state)
        *arm->cmd = 1;
    else
        *arm->cmd = 0;
}

/* 0x00cafbb0 */
int arm_seg_wave_start(BossArmSeg *seg, int period, int steps)
{
    BossArmNode *n = seg->head;

    if (!steps) {
        if (n) {
            if (n->flags & 0x1000)
                return 0;
            n->wave_phase = 0.0f;
            n->flags |= 0x1000;
            n->wave_step = PI / seg->wave_cfg[2];
        }
    } else {
    if (seg->flags & 0x1000)
        return 0;
    seg->flags |= 0x1000;
    seg->wave_time[1] = period;
    seg->waves_left = steps;
    seg->wave_time[0] = period;
    seg->wave_n[0] = steps;
    seg->wave_n[1] = 0;
    if (seg->flags & 1)
        seg->bubble_cell = 1;
    }
    return 1;
}

/* 0x00cafc48 */
void arm_seg_wave(BossArmWork *arm, BossArmSeg *seg)
{
    BossArmNode *n = seg->head;
    BossArmNode *next;
    float t, old;

    if (seg->flags & 0x1000) {
        if ((seg->flags & 1) && seg->wave_step != 0.0f) {
            seg->wave_phase += seg->wave_step;
            if (1.5707964f <= seg->wave_phase) {
                seg->wave_step = 0.0f;
                seg->bubble_scale = arm_bubble_scale * (seg->wave_n[1] + 1);
            } else {
                seg->bubble_scale = arm_bubble_scale
                                    * (seg->wave_n[1] + njSin(seg->wave_phase * ANG_PER_RAD));
            }
        }
        if (seg->wave_n[0] == 0) {
            seg->flags &= ~0x1000;
            if (seg->flags & 1) {
                boss_se_play((int)arm, 12, 0, 0);
                seg->bubble_cell = 0;
            }
        }
        if (seg->waves_left) {
            if (++seg->wave_time[0] >= seg->wave_time[1]) {
                seg->wave_time[0] = 0;
                seg->waves_left--;
                if (!arm_seg_wave_start(seg, 0, 0))
                    seg->wave_n[0]--;
            }
        }
    }
    for (; n; n = n->next) {
        if (n->flags & 0x1000) {
            old = n->wave_phase;
            n->wave_phase = old + n->wave_step;
            if (n->wave_phase >= PI / 2.0 && old < PI / 2.0) {
                next = n->next;
                if (next) {
                    next->wave_phase = 0.0f;
                    next->flags |= 0x1000;
                    if (n->flags & 0x400)
                        n->wave_step = next->wave_step = PI / seg->wave_cfg[0];
                    else
                        next->wave_step = n->wave_step;
                }
            } else if (PI <= n->wave_phase) {
                n->flags &= ~0x1000;
                n->wave_phase = 0.0f;
                n->wave_step = 0.0f;
                if (!n->next) {
                    seg->wave_n[0]--;
                    seg->wave_n[1]++;
                    seg->wave_step = PI / (arm_wave_time * 2.0f * FPS);
                    if (seg->wave_n[0] == 1)
                        seg->bubble_cell = 0;
                }
            }
            t = njSin(n->wave_phase * ANG_PER_RAD);
            if (n->color_cell)
                *n->color_cell = color_lerp(arm_colors[0], arm_colors[1], t);
            t = arm_node_scale + (n->flags & 0x400 ? seg->wave_cfg[3] : seg->wave_cfg[1]) * t;
            do {
                n->scale.x = t;
            } while (0);
            n->scale.z = t;
            n->scale.y = t;
        } else {
            if (n->color_cell) {
                if (!(seg->flags & 0x10) && (n->flags & 0x800))
                    *n->color_cell = arm_colors[1];
                else
                    *n->color_cell = arm_colors[0];
            }
            n->scale.x = arm_node_scale;
            n->scale.z = arm_node_scale;
            do {
                n->scale.y = arm_node_scale;
            } while (0);
        }
    }
}

/* 0x00cafff8 */
void arm_seg_turn_nodes(BossArmSeg *seg)
{
    BossArmNode *n;

    for (n = seg->head; n; n = n->next) {
        n->ang[1] = RAD_TO_ANG(atan2f(-n->dir.x, -n->dir.z));
        n->ang[0] = RAD_TO_ANG(atan2f(n->dir.y, sqrtf(n->dir.x * n->dir.x + n->dir.z * n->dir.z)));
        do {
            n->ang[2] = 0;
        } while (0);
    }
}

/* 0x00cb00d8 */
void arm_wobble_shift(BossArmWork *arm, signed char *buf)
{
    int i;

    if (--arm->wobble_timer <= 0) {
        arm->wobble_timer = beat_clock_get_interval() / (float)arm_max_nodes;
        for (i = arm_max_nodes; i > 0; i--)
            buf[i] = buf[i - 1];
    }
    if (beat_clock_get_frames_to_beat() < 2)
        buf[0] = RAND_FLOAT_A() * 20.0f + 40.0f;
    else
        buf[0] = 0;
}

/* 0x00cb01c8 */
void arm_wobble_update(BossArmWork *arm)
{
    int i;

    arm_wobble_shift(arm, arm_wobble_buf);
    for (i = 0; i < arm_max_nodes; i++)
        arm->wave[i] = arm_wobble_k * arm_wobble_buf[i] * 0.0078125f;
}

/* 0x00cb0258 */
void arm_seg_apply_wobble(BossArmWork *arm, BossArmSeg *seg)
{
    BossArmNode *n;
    int i = 0, lit;

    for (n = seg->head; n; n = n->next, i++) {
        float wave = arm->wave[i];
        int flags = n->flags & 0x1800;

        n->drawflags = flags;
        n->scale.x += wave;
        n->scale.y += wave;
        n->scale.z += wave;
        lit = (unsigned int)(arm->launch_state - 1) < 3 ? seg->flags & 1 : (unsigned int)(arm->launch_state - 4) < 3;
        if (lit)
            n->drawflags = flags | 2;
    }
}

/* 0x00cb02e8 */
void arm_segs_update(BossArmWork *arm, int k)
{
    BossArmSeg *seg;
    BossArmNode *n;
    float t, dyaw;
    int i, ry, ok;

    arm_wobble_update(arm);
    dyaw = (*arm->roty - arm->prev_roty) * DEG_PER_ANG;
    if (arm->yaw_limit < dyaw)
        dyaw = arm->yaw_limit;
    t = dyaw / arm->yaw_limit;
    arm->prev_roty = *arm->roty;
    ry = DEG_TO_ANG((1.0f - t) * 90.0f);
    seg = arm->seg;
    for (i = 0; i < 8; i++, seg++) {
        if (!seg->active || seg->flags < 0)
            continue;
        if (seg->flags & 0x200) {
            ok = 1;
            for (n = seg->head; n; n = n->next) {
                if (n->ent->state != 4) {
                    ok = 0;
                    break;
                }
            }
            if (ok) {
                seg->flags &= ~0x10;
                seg->flags &= ~0x200;
                arm_seg_set_carried(arm, seg, 5, 1);
            }
        }
        seg->spread.x = arm_spread[0] * (1.0f - t) + arm_spread[4] * t;
        seg->spread.y = arm_spread[1] * (1.0f - t) + arm_spread[5] * t;
        if (arm->beat_toggle) {
            seg->spread.x = arm_spread[8];
            seg->spread.y = arm_spread[9];
        }
        if (seg->flags & 0x10) {
            seg->pitch = ry;
        } else {
            seg->pitch = ry;
            if (seg->flags & 0x100) {
                seg->pitch_add[0] = seg->pitch_add[0] * 0.95f;
                seg->spread.x = arm_spread[4];
                seg->pitch = 0;
                seg->spread.y = arm_spread[5];
            } else {
                if ((unsigned int)(arm->launch_state - 2) < 2) {
                    seg->spread.x = arm_spread[6];
                    seg->spread.y = arm_spread[7];
                    if (!arm->beat_toggle)
                        seg->pitch_add[0] += arm->spin[0];
                } else if (!arm->beat_toggle) {
                    seg->pitch_add[0] += (int)DEG_TO_ANG(arm_pitch_step);
                }
                seg->pitch_add[0] &= 0xffff;
            }
        }
        arm_seg_place_head(arm->pos, arm->ang, seg);
        if (k == 1)
            arm_seg_scroll(seg);
        arm_seg_swing(seg);
        arm_seg_thrust(seg);
        arm_seg_sweep(seg, arm->pos, arm->mtx);
        arm_seg_shake(seg);
        if (seg->flags & 0x20) {
            if (seg->head)
                chain_step_both((BossChain *)seg->head, (BossChain *)arm->chain_buf, seg->blend);
        } else if (seg->head) {
            chain_step((BossChain *)seg->head);
        }
        arm_seg_wave(arm, seg);
        arm_seg_node_tips(seg);
        arm_seg_turn_nodes(seg);
        arm_seg_apply_wobble(arm, seg);
    }
}

/* 0x00cb0680 */
void arm_seg_launch(BossArmWork *arm, BossArmSeg *seg, Vec3 *v, int n)
{
    BossArmNode *p;

    boss_se_play((int)arm, 11, 0, (int)&arm_seg_node(seg, -1)->tip);
    arm_seg_sweep_start(seg, v, arm_levels[earth_get_level(arm->parent)].n, tt_l2, tt_t, tt_t2, n);
    seg->bubble_scale = 0.0f;
    seg->bubble_cell = 9;
    earth_bubble_spawn_carried(arm, (int)&arm_seg_node(seg, -1)->tip, &seg->bubble_cell, (int)&seg->bubble_scale, 1.0f);
    arm_seg_wave_start(seg, 15, 16);
    for (p = seg->head; p; p = p->next) {
        if (p->flags & 0x800)
            p->lock.flags |= LOCK_LOCKABLE;
    }
}

/* 0x00cb07b0 */
void arm_launch_step(BossArmWork *arm)
{
    BossArmSeg *seg;
    int ang[2];
    Vec3 v;
    float sc[4];
    int i, j, k, n, tmp, held, cnt;

    switch (arm->launch_state) {
    case 1:
        k = earth_get_level(arm->parent);
        for (i = 3; i >= 0; i--)
            arm->order[i] = i;
        for (i = 0; i < 4; i++) {
            j = RAND_FLOAT_A() * 4.0f;
            tmp = arm->order[i];
            arm->order[i] = arm->order[j];
            arm->order[j] = tmp;
        }
        arm->timer = 0;
        arm->period = arm_levels[k].period * FPS;
        if (!arm->launch_started) {
            arm->launch_started = arm->left = 1;
        } else {
            arm->left = arm_levels[k].count;
        }
        arm->spin[0] = 0;
        arm->launch_state = 2;
        arm_seg_lock_set(arm, 0, 0);
        break;
    case 2:
        arm->spin[0] += (int)DEG_TO_ANG(arm_spin_step);
        if (arm->spin[0] >= (int)DEG_TO_ANG(arm_spin_max))
            arm->spin[0] = DEG_TO_ANG(arm_spin_max);
        if (--arm->timer > 0)
            break;
        arm->timer = arm->period;
        if (--arm->left < 0) {
            arm->launch_state = 3;
            break;
        }
        cnt = arm_levels[earth_get_level(arm->parent)].n;
        j = RAND_FLOAT_A() * 8.0f;
        v.x = v.y = 0.0f;
        v.z = 1.0f;
        if (arm->order[arm->left] & 1)
            ang[0] = DEG_TO_ANG(tt_x);
        else
            ang[0] = DEG_TO_ANG(-tt_x);
        if (arm->order[arm->left] & 2)
            ang[1] = DEG_TO_ANG(tt_y);
        else
            ang[1] = DEG_TO_ANG(-tt_y);
        v3_scale(&v, tt_l);
        do {
            ROT_X(ang[0], (sc + 0), v.y, v.z);
        } while (0);
        ROT_Y(ang[1], (sc + 2), v.x, v.z);
        i = 0;
        while (i < 8) {
            seg = &arm->seg[j % 8];
            if (seg->head && seg->active && !(seg->flags & 0x80000011)) {
                arm_seg_launch(arm, seg, &v, cnt);
                if (--cnt == 0)
                    break;
            }
            i++;
            j++;
        }
        if (i == 8 && cnt == 4)
            arm->launch_state = 3;
        break;
    case 3:
        arm->spin[0] += (int)DEG_TO_ANG(arm_spin_step);
        if (arm->spin[0] >= (int)DEG_TO_ANG(arm_spin_max))
            arm->spin[0] = DEG_TO_ANG(arm_spin_max);
        held = 0;
        for (i = 0, seg = arm->seg; i < 8; i++, seg++) {
            if (seg->flags & 1) {
                held = 1;
                break;
            }
        }
        if (!held)
            arm->launch_state = 0;
        break;
    case 4:
        arm->left = tt_a1;
        arm->timer = tt_a3 * FPS;
        arm->period = tt_a2 * FPS;
        for (i = 0, seg = arm->seg, j = 0; i < 8; i++, seg++) {
            if (seg->active && !(seg->flags & 0x80000011)) {
                seg->flags |= 0x100;
                j++;
                arm_seg_wave_start(seg, 15, 40);
            }
        }
        if (!j)
            arm->launch_state = 0;
        else
            arm->launch_state = 5;
        arm_seg_lock_set(arm, 0, 1);
        break;
    case 5:
        if (--arm->timer)
            break;
        arm->launch_state = 6;
    case 6:
        seg = arm->seg;
        if (--arm->timer <= 0) {
            arm->timer = arm->period;
            if (--arm->left != -1) {
                n = 0;
                for (i = 0; i < 8; i++, seg++) {
                    if (seg->flags & 0x100) {
                        n++;
                        seg->bubble_cell = 33;
                        seg->bubble_scale = 1.0f;
                        earth_bubble_spawn_carried(arm, (int)&seg->tail->tip, &seg->bubble_cell, (int)&seg->bubble_scale,
                                               (float)arm_sweep_margin / (tt_a2 * FPS));
                    }
                }
                if (!n)
                    arm->launch_state = 0;
            } else {
                for (i = 0; i < 8; i++, seg++) {
                    if (seg->flags & 0x100)
                        seg->flags &= ~0x100;
                }
                arm->launch_state = 0;
                arm_seg_lock_set(arm, 0, 0);
            }
        } else if (arm->timer == arm_sweep_margin || arm->timer == 1) {
            for (i = 0; i < 8; i++, seg++)
                seg->bubble_cell = 0;
        }
        break;
    }
}

/* 0x00cb0ec0 */
void arm_exec(BossArmWork *arm)
{
    int aim_ang[2];
    Vec3 dir;
    BossArmSeg *seg;
    BossArmNode *n;
    int i, k, ang;
    float seg_step;

    player_get_pos(&dir);
    v3_sub(&dir, &dir, arm->pos);
    do {
        aim_ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        aim_ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    matrix_unit_set((int)arm->mtx);
    vu0_matrix_rotate_y(arm->mtx, aim_ang[1]);
    vu0_matrix_rotate_x(arm->mtx, aim_ang[0]);
    arm_command(arm);
    for (k = enemy_status_get_ticks(); k; k--)
        arm_launch_step(arm);
    arm_hit_check(arm);
    seg_step = 0.7853982f;
    seg = arm->seg;
    for (i = 0; i < 8; i++, seg++) {
        v3_mul(&dir, &arm_seg_dir_base, arm_seg_len);
        ang = seg_step * (float)i * ANG_PER_RAD;
        seg->dir.x = dir.x * njSin(ang);
        seg->dir.z = dir.z * njCos(ang);
        seg->dir.y = dir.y;
        seg->start.x = arm_seg_start_base.x * njSin(ang);
        seg->start.z = arm_seg_start_base.z * njCos(ang);
        seg->start.y = arm_seg_start_base.y;
        seg->spread.z = arm_seg_len;
        seg->spread.x = arm_spread[0];
        seg->spread.y = arm_spread[1];
        seg->wave_cfg[0] = arm_wave_cfg0 * FPS;
        seg->wave_cfg[1] = arm_wave_cfg1;
        seg->wave_cfg[2] = arm_wave_cfg2 * FPS;
        seg->wave_cfg[3] = arm_wave_cfg3;
    }
    if ((unsigned int)(arm->launch_state - 4) < 3) {
        arm->beat_toggle = 0;
    } else if (beat_grid_hit(0)) {
        if (!(arm->beat_toggle ^= 1)) {
            seg = arm->seg;
            for (i = 0; i < 8; i++, seg++) {
                for (n = seg->head; n; n = n->next)
                    v3_set(&n->unk_2c, 0.0f, 0.0f, 0.0f);
            }
        }
    }
    if ((k = enemy_status_get_ticks() != 0)) {
        for (; k; k--)
            arm_segs_update(arm, k);
    } else {
        seg = arm->seg;
        for (i = 0; i < 8; i++, seg++) {
            if (seg->active) {
                arm_seg_place_head(arm->pos, arm->ang, seg);
                arm_seg_scroll(seg);
                arm_seg_node_tips(seg);
            }
        }
    }
    seg = arm->seg;
    for (i = 0; i < 8; i++, seg++) {
        if (seg->flags & 0x80000000 && seg->active)
            arm_seg_drop(arm, seg);
    }
    if (arm->enabled)
        arm->update_cb(arm->cb_arg);
}

/* 0x00cb1310 */
void arm_disp(BossArmWork *arm)
{
    if (arm->enabled)
        arm->disp_cb(arm->cb_arg);
}

/* 0x00cb1340 */
BossArmWork *arm_create(void *parent, BossArmArg *arg)
{
    BossArmWork *arm = task_create_inherit_arg(parent, arm_desc, 16896, arg);

    arm->parent = parent;
    arm->enabled = 0;
    return arm;
}

/* 0x00cb1380 */
void arm_enable(BossArmWork *arm)
{
    arm->enabled = 1;
}

/* 0x00cb1390 */
void arm_seg_set_carried(BossArmWork *arm, BossArmSeg *seg, int state, int relock)
{
    BossArmNode *n = seg->head;
    int hp = arm_levels[boss_get_level()].hp;

    for (; n; n = n->next) {
        if (relock) {
            if (n->flags & 0x800) {
                lock_init(9, &n->lock, arm, &n->pos, 0, hp, 128);
                lockon_entry(&n->lock);
                n->lock.flags &= ~LOCK_LOCKABLE;
            }
        } else if (n->flags & 0x800) {
            lock_remove_entry(9, &n->lock);
        }
        if (n->ent && (unsigned int)(n->ent->state - 4) < 2)
            n->ent->state = state;
    }
}

/* 0x00cb14a0 */
int arm_attach_cube(BossArmWork *arm, int *pos_out, int *ang_out, int *scale_out, int *color, BossCube *cube, int *flags_out)
{
    BossArmSeg *seg;
    BossArmNode *n;
    int i, j;
    i = RAND_FLOAT_A() * 8.0f;
    for (j = 0; j < 10; j++) {
        seg = &arm->seg[i];
        if (seg->active && seg->count < arm_max_nodes && (n = arm_seg_append(seg))) {
            if (seg->count >= arm_max_nodes) {
                seg->flags |= 0x200;
                earth_arm_add(arm->parent);
            } else {
                seg->flags |= 0x10;
                seg->spread.x = arm_spread[0];
                seg->spread.y = arm_spread[1];
            }
            if (seg->count == 20)
                n->flags |= 0x800;
            goto found;
        }
        i = (i + 1) % 8;
    }
    return 0;
found:
    *(int **)flags_out = &n->drawflags;
    *(Vec3 **)pos_out = &n->pos;
    *(int **)ang_out = n->ang;
    *(float **)scale_out = &n->scale.x;
    n->color_cell = (unsigned int *)color;
    n->ent = cube;
    return 1;
}

/* 0x00cb1680 */
void arm_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
