/*
 * Spring joint chains for boss limbs (Eden's arms): built from a model's node tree, each joint
 * follows the one before with its own stiffness and damping, kept upright by a twist step. Stepped
 * from the root, or from both ends when pulled to a tip.
 */

#include "types_common.h"

#include "mathdef.h"
#include "types_overlays.h"
#include "color.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "boss_joint.h"
#include "boss_common.h"
#include "system.h"

static inline void v3_copy(Vec3 *d, Vec3 *s)
{
    d->x = s->x;
    d->y = s->y;
    d->z = s->z;
}

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void v3_add(Vec3 *d, Vec3 *s)
{
    d->x += s->x;
    d->y += s->y;
    d->z += s->z;
}

/* sc receives the sine and cosine */
#define ROT_Z(ang, sc, x, y) \
    do { \
        float rot_u = x, rot_v = y; \
        int a_ = ang; \
        ulSinCos(a_, &sc[0], &sc[1]); \
        x = rot_u * sc[1] - rot_v * sc[0]; \
        y = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

static inline void v3_sub(Vec3 *d, Vec3 *a, Vec3 *b)
{
    d->x = a->x - b->x;
    d->y = a->y - b->y;
    d->z = a->z - b->z;
}

static inline void v3_sum(Vec3 *d, Vec3 *a, Vec3 *b)
{
    d->x = a->x + b->x;
    d->y = a->y + b->y;
    d->z = a->z + b->z;
}

static inline void v3_scale(Vec3 *v, float k)
{
    v->x *= k;
    v->y *= k;
    v->z *= k;
}

static inline void v3_mul(Vec3 *d, Vec3 *a, float k)
{
    d->x = a->x * k;
    d->y = a->y * k;
    d->z = a->z * k;
}

static inline void v3_lerp(Vec3 *d, Vec3 *a, Vec3 *b, float k)
{
    d->x = a->x * k + b->x * (1.0f - k);
    d->y = a->y * k + b->y * (1.0f - k);
    d->z = a->z * k + b->z * (1.0f - k);
}

static inline void i3_copy(int *d, int *s)
{
    d[0] = s[0];
    d[1] = s[1];
    d[2] = s[2];
}

extern int ulMatrixPush(void);

#define MTX_TRANS(p) \
    do { \
        Vec3 *p_ = (p); \
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z); \
    } while (0)

extern char str_boss_joint_c[];
extern Vec3 boss_joint_pos;
extern Vec3 boss_joint_dir;
extern Vec3 boss_joint_up;
extern Vec3 boss_joint_tree_pos;
extern Vec3 boss_joint_tree_dir;
extern Vec3 boss_joint_tree_up;
extern const Vec3 boss_joint_up_base;
extern void vu0_matrix_inverse_rigid(float *d, float *s);

/* 0x00cd2950 b_eden, 0x00cc5d20 b_venus */
void boss_joint_chain_init(BossJointLink *node, Vec3 *pos, Vec3 *dir, Vec3 *up, int cb, int post_cb, int cb_arg, int n, float stiff, float damp, float up_rate)
{
    BossJointLink *prev = 0;
    Vec3 pt;
    float len;

    v3_copy(&pt, pos);
    len = vec3_length(dir);
    for (; n; node++, n--) {
        v3_copy(&node->pos, &pt);
        node->flags = 0;
        node->cb = (void (*)(BossJointLink *, BossJointLink *))cb;
        v3_copy(&node->dir, dir);
        node->arg[0] = post_cb;
        node->arg[1] = cb_arg;
        v3_copy((Vec3 *)node->prev_dir, dir);
        v3_copy((Vec3 *)node->up_dir, up);
        v3_add(&pt, dir);
        v3_set(&node->vel, 0.0f, 0.0f, 0.0f);
        node->len = len;
        node->roll = 0.0f;
        node->stiff = stiff;
        node->prev = prev;
        node->damp = damp;
        prev = node;
        node->up_rate = up_rate;
        node->next = 0;
        if (node->prev)
            node->prev->next = node;
    }
}

/* 0x00cd2af0 b_eden, 0x00cc5ec0 b_venus */
int boss_joint_up_from_dir(Vec3 *dir, Vec3 *up, int ang, int no_flip)
{
    int rot[3];
    Vec3 up_new, up_flip;
    float sc[2];

    up_new = boss_joint_up_base;
    ROT_Z(ang, sc, up_new.x, up_new.y);
    rot[2] = 0;
    do {
        rot[1] = (int)RAD_TO_ANG(atan2f(-dir->x, -dir->z));
        rot[0] = (int)RAD_TO_ANG(atan2f(dir->y, sqrtf(dir->x * dir->x + dir->z * dir->z)));
    } while (0);
    v3_copy(&up_flip, &up_new);
    vec_rotate_zxy(&up_new, rot);
    if (ulVec3Dot(&up_new, up) < 0.0f && !no_flip) {
        rot[2] = ANG_180;
        vec_rotate_zxy(&up_flip, rot);
        v3_copy(up, &up_flip);
        return 1;
    }
    v3_copy(up, &up_new);
    return 0;
}

/* 0x00cd2cd0 b_eden, 0x00cc60a0 b_venus */
void boss_joint_chain_twist(BossJointLink *node)
{
    BossJointLink *cur;
    Vec3 side, up_new;

    for (; (cur = node->next); node = node->next) {
        cur->roll = node->roll;
        if (boss_joint_up_from_dir(&cur->dir, (Vec3 *)cur->up_dir, cur->roll * ANG_PER_RAD,
                                   cur->flags & 1))
            cur->flags |= 2;
        ulVec3Cross(&side, &node->dir, (struct Vec3 *)node->up_dir);
        ulVec3Cross(&up_new, &side, &cur->dir);
        if (ulVec3Dot(&up_new, (struct Vec3 *)node->up_dir) < 0.0f && ulVec3Dot(&node->dir, &cur->dir) >= 0.0f) {
            up_new.x = -up_new.x;
            up_new.y = -up_new.y;
            up_new.z = -up_new.z;
        }
        vec3_normalize(&up_new, &up_new);
        v3_lerp((Vec3 *)cur->up_dir, &up_new, (Vec3 *)cur->up_dir, cur->up_rate);
        angles_from_dirs(&cur->dir, (Vec3 *)cur->up_dir, cur->ang);
    }
}

/* 0x00cd2e88 b_eden, 0x00cc6258 b_venus */
void boss_joint_chain_step(BossJointLink *node)
{
    BossJointLink *cur;
    Vec3 ofs, npos;

    for (; (cur = node->next); node = node->next) {
        cur->flags = 0;
        v3_mul(&ofs, &node->dir, node->len);
        v3_sum(&ofs, &node->pos, &ofs);
        v3_sub(&ofs, &ofs, &cur->pos);
        v3_scale(&ofs, cur->stiff);
        v3_sum(&npos, &cur->pos, &ofs);
        v3_scale(&cur->vel, cur->damp);
        if (node->cb)
            node->cb(cur, node);
        v3_sum(&npos, &cur->vel, &npos);
        v3_sub(&cur->dir, &npos, &node->pos);
        vec3_normalize(&cur->dir, &cur->dir);
        v3_scale(&cur->dir, cur->len);
        v3_copy(&npos, &cur->pos);
        v3_sum(&cur->pos, &node->pos, &cur->dir);
        if (node->arg[0])
            ((void (*)(BossJointLink *, BossJointLink *))node->arg[0])(cur, node);
        v3_sub(&cur->vel, &cur->pos, &npos);
        if (ulVec3Dot((struct Vec3 *)cur->prev_dir, &cur->dir) < 0.0f)
            cur->flags |= 1;
        v3_copy((Vec3 *)cur->prev_dir, &cur->dir);
    }
}

/* 0x00cd3150 b_eden, 0x00cc6520 b_venus */
BossJointLink *boss_joint_step_fwd(BossJointLink *node, BossJointLink *src, BossJointLink *dst, int *count)
{
    BossJointLink *cur;
    Vec3 ofs;
    Vec3 npos;
    int cnt;

    cnt = 1;
    for (cur = node->next; cur; cur = node->next) {
        v3_mul(&ofs, &src->dir, node->len);
        v3_sum(&ofs, &src->pos, &ofs);
        v3_sub(&ofs, &ofs, &cur->pos);
        v3_scale(&ofs, cur->stiff);
        v3_sum(&npos, &cur->pos, &ofs);
        v3_mul(&dst->vel, &cur->vel, cur->damp);
        if (node->cb)
            node->cb(dst, node);
        v3_sum(&npos, &dst->vel, &npos);
        v3_sub(&dst->dir, &npos, &src->pos);
        vec3_normalize(&dst->dir, &dst->dir);
        cnt++;
        v3_scale(&dst->dir, cur->len);
        node = node->next;
        v3_sum(&dst->pos, &src->pos, &dst->dir);
        dst++;
        src++;
    }
    *count = cnt;
    return node;
}

/* 0x00cd3388 b_eden, 0x00cc6758 b_venus */
void boss_joint_step_back(BossJointLink *node, BossJointLink *src, BossJointLink *dst)
{
    BossJointLink *cur;
    Vec3 ofs;
    Vec3 npos;

    for (cur = node->prev; cur; cur = node->prev) {
        v3_mul(&ofs, &src->dir, -node->len);
        v3_sum(&ofs, &src->pos, &ofs);
        v3_sub(&ofs, &ofs, &cur->pos);
        v3_scale(&ofs, cur->stiff);
        v3_sum(&npos, &cur->pos, &ofs);
        v3_mul(&dst->vel, &cur->vel, cur->damp);
        if (node->cb)
            node->cb(dst, node);
        v3_sum(&npos, &dst->vel, &npos);
        v3_sub(&dst->dir, &npos, &src->pos);
        vec3_normalize(&dst->dir, &dst->dir);
        v3_scale(&dst->dir, cur->len);
        node = node->prev;
        v3_sum(&dst->pos, &src->pos, &dst->dir);
        dst->dir.x = -dst->dir.x;
        dst->dir.y = -dst->dir.y;
        dst->dir.z = -dst->dir.z;
        dst--;
        src--;
    }
}

/* 0x00cd35b8 b_eden, 0x00cc6988 b_venus */
void boss_joint_step_both(BossJointLink *node, BossJointLink *pool, float t)
{
    BossJointLink *head = node;
    BossJointLink *buf = pool;
    BossJointLink *fwd = buf + 1;
    BossJointLink *tail, *back;
    int cnt;
    float step, rate;
    Vec3 old_pos;

    v3_copy(&buf->pos, &head->pos);
    v3_copy(&buf->dir, &head->dir);
    v3_copy(&buf->vel, &head->vel);
    node = boss_joint_step_fwd(head, buf, fwd, &cnt);
    buf += cnt;
    tail = &buf[cnt - 1];
    v3_copy(&tail->pos, &node->pos);
    v3_copy(&tail->dir, &node->dir);
    v3_copy(&tail->vel, &node->vel);
    boss_joint_step_back(node, tail, &buf[cnt - 2]);
    step = -t / (float)cnt;
    rate = step + 1.0f;
    for (node = head->next, back = buf + 1; node; node = node->next, fwd++, back++) {
        v3_copy(&old_pos, &node->pos);
        v3_lerp(&node->pos, &fwd->pos, &back->pos, rate);
        v3_lerp(&node->dir, &fwd->dir, &back->dir, rate);
        if (ulVec3Dot((struct Vec3 *)node->prev_dir, &node->dir) < 0.0f)
            node->flags |= 1;
        rate += step;
        v3_copy((Vec3 *)node->prev_dir, &node->dir);
        v3_sub(&node->vel, &node->pos, &old_pos);
    }
}

/* 0x00cd38a8 b_eden, 0x00cc6c78 b_venus */
void boss_joint_root_set_up(BossJointLink *chain, Vec3 *pos, Vec3 *dir, Vec3 *up, float roll_deg)
{
    int rot[3];

    chain->roll += roll_deg * RAD_PER_DEG;
    v3_copy(&chain->pos, pos);
    v3_copy(&chain->dir, dir);
    vec_set_length(&chain->dir, chain->len);
    if (ulVec3Dot(up, (struct Vec3 *)chain->up_dir) < 0.0f) {
        v3_copy((Vec3 *)chain->up_dir, up);
        rot[2] += ANG_180;
        angles_from_dirs(&chain->dir, (Vec3 *)chain->up_dir, chain->ang);
        chain->flags = 1;
    } else {
        v3_copy((Vec3 *)chain->up_dir, up);
        angles_from_dirs(&chain->dir, (Vec3 *)chain->up_dir, chain->ang);
        chain->flags = 0;
    }
}

/* 0x00cd39d8 b_eden, 0x00cc6da8 b_venus */
void boss_joint_root_set(BossJointLink *chain, Vec3 *pos, Vec3 *dir, float roll_deg)
{
    Vec3 up_new;
    Vec3 up_flip;
    int rot[3];
    float roll_sc[2];

    up_new = boss_joint_up_base;
    chain->roll += roll_deg * RAD_PER_DEG;
    v3_copy(&chain->pos, pos);
    v3_copy(&chain->dir, dir);
    vec_set_length(&chain->dir, chain->len);
    ROT_Z((int)(*(float *)&chain->roll * ANG_PER_RAD), roll_sc, up_new.x, up_new.y);
    rot[2] = 0;
    do {
        rot[1] = (int)RAD_TO_ANG(atan2f(-chain->dir.x, -chain->dir.z));
        rot[0] = (int)RAD_TO_ANG(atan2f(chain->dir.y, sqrtf(chain->dir.x * chain->dir.x + chain->dir.z * chain->dir.z)));
    } while (0);
    v3_copy(&up_flip, &up_new);
    vec_rotate_zxy(&up_new, rot);
    if (ulVec3Dot(&up_new, (struct Vec3 *)chain->up_dir) < 0.0f) {
        rot[2] += ANG_180;
        vec_rotate_zxy(&up_flip, rot);
        v3_copy((Vec3 *)chain->up_dir, &up_flip);
        angles_from_dirs(&chain->dir, (Vec3 *)chain->up_dir, chain->ang);
        chain->flags = 1;
    } else {
        v3_copy((Vec3 *)chain->up_dir, &up_new);
        angles_from_dirs(&chain->dir, (Vec3 *)chain->up_dir, chain->ang);
        chain->flags = 0;
    }
}

/* 0x00cd3c58 b_eden, 0x00cc7028 b_venus */
void boss_joint_set_pos(BossJointLink *node, Vec3 *pos)
{
    v3_copy(&node->pos, pos);
}

/* 0x00cd3c78 b_eden, 0x00cc7048 b_venus */
void boss_joint_get_pose(BossJointLink *node, Vec3 *pos, int *ang)
{
    if (!node)
        return;
    if (pos)
        v3_copy(pos, &node->pos);
    if (ang)
        i3_copy(ang, node->ang);
}

/* 0x00cd3cd0 b_eden, 0x00cc70a0 b_venus */
void boss_joint_chain_set_spring(BossJointLink *node, float stiff, float damp, float up_rate)
{
    while (node) {
        node->stiff = stiff;
        node->damp = damp;
        node->up_rate = up_rate;
        node = node->next;
    }
}

/* 0x00cd3d00 b_eden, 0x00cc70d0 b_venus */
void boss_joint_chain_set_all(BossJointLink *node, void *cb, int post_cb, float len, float stiff, float damp, float up_rate)
{
    while (node) {
        node->cb = cb;
        node->arg[0] = post_cb;
        node->len = len;
        node->stiff = stiff;
        node->damp = damp;
        node->up_rate = up_rate;
        node = node->next;
    }
}

/* 0x00cd3d38 b_eden, 0x00cc7108 b_venus */
void boss_joint_chain_reset(BossJointLink *node)
{
    BossJointLink *prev = node;
    Vec3 ofs;

    ulMatrixPush();
    matrix_unit_set(0);
    boss_joint_rotate_yxz(node->ang);
    for (node = node->next; node; node = node->next) {
        ulMatrixApplyVector((float *)ulMatrixCur, &node->dir.x, &node->dir.x);
        ulMatrixApplyVector((float *)ulMatrixCur, (float *)node->up_dir, (float *)node->up_dir);
        vec3_normalize(&ofs, &node->dir);
        v3_scale(&ofs, node->len);
        v3_sum(&node->pos, &prev->pos, &ofs);
        prev = node;
        v3_set(&node->vel, 0.0f, 0.0f, 0.0f);
        angles_from_dirs(&node->dir, (Vec3 *)node->up_dir, node->ang);
    }
    ulMatrixPop();
}

/* 0x00cd3e68 b_eden, 0x00cc7238 b_venus */
void boss_joint_chain_set_len(BossJointLink *node, float len)
{
    while (node) {
        node->len = len;
        node = node->next;
    }
}

/* 0x00cd3e98 b_eden, 0x00cc7268 b_venus */
void boss_joint_chain_set_cb(BossJointLink *node, void *cb, int post_cb, int cb_arg)
{
    while (node) {
        node->cb = cb;
        node->arg[0] = post_cb;
        node->arg[1] = cb_arg;
        node = node->next;
    }
}

/* 0x00cd3ec8 b_eden, 0x00cc7298 b_venus */
void boss_joint_chain_pull(BossJointLink *node, float rate)
{
    BossJointLink *prev = node;
    int rot[3];
    Vec3 v;

    i3_copy(rot, node->ang);
    for (node = node->next; node; node = node->next) {
        v3_mul(&v, &node->fwd, node->len);
        vec_rotate_zxy(&v, rot);
        v3_sum(&v, &prev->pos, &v);
        prev = node;
        v3_sub(&v, &v, &node->pos);
        v3_lerp(&node->vel, &v, &node->vel, rate);
    }
}

/* 0x00cd4028 b_eden, 0x00cc73f8 b_venus */
void boss_joint_count_r(WireObj *obj, int *n)
{
    do {
        if (obj->model && obj->model->vlist)
            (*n)++;
        if (obj->child)
            boss_joint_count_r(obj->child, n);
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cd40a0 b_eden, 0x00cc7470 b_venus */
int boss_joint_count(WireObj *obj)
{
    int n = 0;

    boss_joint_count_r(obj, &n);
    return n;
}

/* 0x00cd40c8 b_eden, 0x00cc7498 b_venus */
void boss_joint_tree_collect(WireObj *obj, int *cur)
{
    float mtx[16];

    do {
        ulMatrixPush();
        MTX_TRANS((Vec3 *)obj->pos);
        if (obj->flags & 0x20)
            boss_joint_rotate_yxz(obj->ang);
        else
            boss_joint_rotate_zyx(obj->ang);
        vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
        if (obj->model && *(int *)obj->model) {
            ulMatrixGetTranslation((float *)ulMatrixCur, (float *)((char *)*cur + 28));
            ulMatrixPush();
            ulMatrixSetTranslation((float *)ulMatrixCur, 0.0f, 0.0f, 0.0f);
            vu0_matrix_copy(mtx, ulMatrixCur);
            ulMatrixPop();
            matrix_to_angles(mtx, (int *)((char *)*cur + 140));
            *cur = ((int *)*cur)[1];
        }
        if (obj->child)
            boss_joint_tree_collect(obj->child, cur);
        ulMatrixPop();
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cd4200 b_eden, 0x00cc75d0 b_venus */
void boss_joint_chain_build(WireObj *obj, BossJointLink *node, int *base_ang)
{
    int ang[3];
    float mtx[16];
    BossJointLink *cur;
    float pitch_sin, pitch_cos, yaw_sin, yaw_cos;

    ulMatrixPush();
    matrix_unit_set(0);
    boss_joint_rotate_yxz(base_ang);
    cur = node;
    boss_joint_tree_collect(obj, (int *)&cur);
    for (; node; node = node->next) {
        cur = node->next;
        if (!node->prev) {
            v3_sub(&node->dir, &node->pos, &cur->pos);
            node->len = vec3_length(&node->dir);
            vec3_normalize(&node->fwd, &node->dir);
        } else {
            v3_sub(&node->dir, &node->prev->pos, &node->pos);
            node->len = vec3_length(&node->dir);
            vec3_normalize(&node->fwd, &node->dir);
        }
        node->up.x = node->up.z = 0.0f;
        node->up.y = 1.0f;
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-node->dir.x, -node->dir.z));
            ang[0] = (int)RAD_TO_ANG(atan2f(node->dir.y, sqrtf(node->dir.x * node->dir.x + node->dir.z * node->dir.z)));
        } while (0);
        do {
            float ua = node->up.y, ub = node->up.z;

            ulSinCos(ang[0], &pitch_sin, &pitch_cos);
            node->up.y = ua * pitch_cos - ub * pitch_sin;
            node->up.z = ua * pitch_sin + ub * pitch_cos;
        } while (0);
        do {
            float ua = node->up.x, ub = node->up.z;

            ulSinCos(ang[1], &yaw_sin, &yaw_cos);
            node->up.x = ua * yaw_cos + ub * yaw_sin;
            node->up.z = -ua * yaw_sin + ub * yaw_cos;
        } while (0);
        angles_from_dirs(&node->fwd, &node->up, node->ang);
        ulMatrixPush();
        matrix_unit_set(0);
        boss_joint_rotate_yxz(node->ang);
        vu0_matrix_inverse_rigid(0, 0);
        boss_joint_rotate_yxz(node->rel_ang);
        vu0_matrix_copy(mtx, ulMatrixCur);
        matrix_to_angles(mtx, node->rel_ang);
        ulMatrixPop();
    }
    ulMatrixPop();
}

/* 0x00cd4540 b_eden, 0x00cc7910 b_venus */
void boss_joint_axes_draw(float len)
{
    Vec3 seg[2];
    Vec3 *tip = &seg[1];
    float org;

    seg[0].x = 0.0f;
    seg[0].y = 0.0f;
    seg[0].z = 0.0f;
    org = seg[0].x;
    tip->y = org;
    tip->x = org;
    tip->z = -len;
    lines_begin(1.0f, RGBA(0xff, 0, 0, 0xff), 0);
    lines_add_pairs(seg, 2);
    lines_end();
    tip->z = org;
    tip->x = org;
    tip->y = len;
    lines_begin(0.5f, RGBA(0, 0xff, 0, 0xff), 0);
    lines_add_pairs(seg, 2);
    lines_end();
}

/* 0x00cd45f8 b_eden, 0x00cc79c8 b_venus */
void boss_joint_tree_draw(int tree, int draw_cb, int *cur, int *scl)
{
    WireObj *obj = (WireObj *)tree;
    char *node;

    do {
        if (obj->model) {
            ulMatrixPush();
            if (*(int *)obj->model) {
                node = (char *)*cur;
                MTX_TRANS((Vec3 *)(node + 28));
                boss_joint_rotate_yxz((int *)((char *)*cur + 88));
                boss_joint_rotate_yxz((int *)((char *)*cur + 140));
                vu0_matrix_scale(ulMatrixCur, ((float *)scl)[0], ((float *)scl)[1], ((float *)scl)[2]);
                *cur = ((int *)*cur)[1];
            }
            ((void (*)(void *))draw_cb)(obj->model);
            ulMatrixPop();
        }
        if (obj->child)
            boss_joint_tree_draw((int)obj->child, draw_cb, cur, scl);
        obj = obj->sibling;
    } while (obj);
}

/* 0x00cd4710 b_eden, 0x00cc7ae0 b_venus */
void boss_joint_tree_draw_axes(int tree, int draw_cb, int *cur)
{
    WireObj *obj = (WireObj *)tree;

    do {
        if (obj->model) {
            ulMatrixPush();
            if (*(int *)obj->model) {
                MTX_TRANS(&((BossJointLink *)*cur)->pos);
                boss_joint_rotate_yxz(((BossJointLink *)*cur)->ang);
                boss_joint_rotate_yxz(((BossJointLink *)*cur)->rel_ang);
                *cur = (int)((BossJointLink *)*cur)->next;
                boss_joint_axes_draw(2.0f);
            }
            ulMatrixPop();
        }
        if (obj->child)
            boss_joint_tree_draw_axes((int)obj->child, draw_cb, cur);
    } while ((obj = obj->sibling) != 0);
}

/* 0x00cd47f8 b_eden, 0x00cc7bc8 b_venus */
void boss_joint_tree_draw_rest(WireObj *obj, int order)
{
    do {
        ulMatrixPush();
        MTX_TRANS((Vec3 *)obj->pos);
        if (order)
            boss_joint_rotate_yxz(obj->ang);
        else
            boss_joint_rotate_zyx(obj->ang);
        vu0_matrix_scale(ulMatrixCur, obj->scl[0], obj->scl[1], obj->scl[2]);
        if (obj->model && *(int *)obj->model)
            boss_joint_axes_draw(2.0f);
        if (obj->child)
            boss_joint_tree_draw_rest(obj->child, order);
        ulMatrixPop();
    } while ((obj = obj->sibling) != 0);
}

/* 0x00cd48d0 b_eden, 0x00cc7ca0 b_venus */
void boss_joint_draw(int *w)
{
    int cur = w[0];

    if (w[12])
        boss_joint_tree_draw_axes(w[4], w[5], &cur);
    else
        boss_joint_tree_draw(w[4], w[5], &cur, w + 9);
}

/* 0x00cd4928 b_eden, 0x00cc7cf8 b_venus */
void boss_joint_create_tree(WireObj *obj, int draw_cb, BossJointLink **w)
{
    ((WireObj **)w)[4] = obj;
    ((int *)w)[12] = 0;
    ((int *)w)[5] = draw_cb;
    ((int *)w)[3] = boss_joint_count(obj);
    w[0] = heap_block_tag(game_calloc(((int *)w)[3] * 152), str_boss_joint_c, 635);
    w[1] = heap_block_tag(game_calloc(((int *)w)[3] * 304), str_boss_joint_c, 636);
    w[2] = &w[0][((int *)w)[3] - 1];
    boss_joint_chain_init(w[0], &boss_joint_tree_pos, &boss_joint_tree_dir, &boss_joint_tree_up, 0, 0, 0, ((int *)w)[3], 0.015f, 0.48f, 1.0f);
    boss_joint_chain_build(obj, w[0], (int *)(w + 6));
}

/* 0x00cd4a38 b_eden, 0x00cc7e08 b_venus */
void boss_joint_create(BossJointLink **w, int n)
{
    ((int *)w)[12] = 0;
    ((int *)w)[5] = 0;
    ((int *)w)[4] = 0;
    ((int *)w)[3] = n;
    w[0] = heap_block_tag(game_calloc(n * 152), str_boss_joint_c, 653);
    w[1] = heap_block_tag(game_calloc(((int *)w)[3] * 304), str_boss_joint_c, 654);
    w[2] = &w[0][((int *)w)[3] - 1];
    boss_joint_chain_init(w[0], &boss_joint_pos, &boss_joint_dir, &boss_joint_up, 0, 0, 0, ((int *)w)[3], 0.015f, 0.48f, 1.0f);
}

/* 0x00cd4b28 b_eden, 0x00cc7ef8 b_venus */
void boss_joint_set_pose(char *w, int *ang, Vec3 *pos)
{
    if (ang)
        i3_copy((int *)(w + 24), ang);
    if (pos)
        v3_copy((Vec3 *)(w + 36), pos);
}

/* 0x00cd4b78 b_eden, 0x00cc7f48 b_venus */
void boss_joint_update(BossJointLink **w, Vec3 *root, Vec3 *tip, float t)
{
    Vec3 dir;

    if (tip) {
        v3_sub(&dir, tip, root);
        boss_joint_root_set(w[0], root, &dir, 0.0f);
        boss_joint_set_pos(w[2], tip);
        boss_joint_step_both(w[0], w[1], t);
    } else {
        boss_joint_root_set(w[0], root, &w[0]->dir, 0.0f);
        boss_joint_chain_step(w[0]);
    }
    boss_joint_chain_twist(w[0]);
}

/* 0x00cd4c40 b_eden, 0x00cc8010 b_venus */
void boss_joint_release(void **w)
{
    heap_free(w[0], 0);
    heap_free(w[1], 0);
}

/* 0x00cd4c78 b_eden, 0x00cc8048 b_venus */
void boss_joint_rotate_zyx(int *ang)
{
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
}

/* 0x00cd4cc8 b_eden, 0x00cc8098 b_venus */
void boss_joint_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
