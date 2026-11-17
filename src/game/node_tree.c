/*
 * Model node trees: world position, angles, scale or matrices of flagged nodes in the animated
 * pose, for attaching effects and hit points. Also angle/vector helpers and sine easing.
 */

#include "types_game.h"
#include "mathdef.h"
#include "types_common.h"
#include "libc.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "node_tree.h"
#include "anim.h"
#include "math_util.h"

extern int *ulMatrixCur;

extern void ulMatrixPush(void);
extern void matrix_get_or_set(int, char *);

static void node_tree_rotate_zyx(int *rot);
static void node_tree_rotate_yxz(int *rot);

/* Per-axis scale of the current matrix. */
/* 0x0013f558 */
static void model_matrix_scale(float *out)
{
    float mtx[9] = { 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f };
    float len_x;
    float len_y;
    float len_z;

    matrix_apply_vectors(mtx, mtx, 3);
    len_x = vec3_length(mtx);
    len_y = vec3_length(mtx + 3);
    len_z = vec3_length(mtx + 6);
    out[0] = len_x;
    out[1] = len_y;
    out[2] = len_z;
}

/*
 * Current matrix -> pitch, yaw, roll (65536 per turn). Straight up/down: pitch a quarter turn,
 * yaw 0. Returns roll.
 */
/* 0x0013f618 */
int model_camera_angles(int *out)
{
    float mtx[16];
    float sc[4];
    float len_sq;
    float a;
    float b;
    int pitch;
    int yaw;
    int roll;
    float ang_scale;
    float two_pi;
    float r;

    vu0_matrix_copy(mtx, ulMatrixCur);
    ang_scale = ANG_TURN;
    two_pi = TWO_PI;
    yaw = (int)(atan2f(mtx[8], mtx[10]) * ang_scale / two_pi);
    len_sq = mtx[8] * mtx[8] + mtx[10] * mtx[10];
    if (fabsf(len_sq) < FLT_EPSILON) {
        yaw = 0;
        if (0.0f < mtx[9]) {
            pitch = -ANG_90;
            a = mtx[4];
            a = -a;
            b = mtx[6];
            b = -b;
        } else {
            pitch = ANG_90;
            a = mtx[4];
            a = -a;
            b = mtx[6];
        }
    } else {
        pitch = (int)(atan2f(-mtx[9], sqrtf(len_sq)) * ang_scale / two_pi);
        ulSinCos(pitch, &sc[0], &sc[1]);
        ulSinCos(yaw, &sc[2], &sc[3]);
        {
            float t, sin_yaw, up_y;

            sin_yaw = sc[2];
            t = sin_yaw * mtx[6];
            a = sc[3];
            a *= mtx[4];
            up_y = mtx[5];
            b = sc[1];
            b = up_y / b;
            a = t - a;
        }
    }
    do {
        r = atan2f(a, b) * ang_scale;
    } while (0);
    roll = (int)(r / two_pi);
    out[0] = pitch;
    do { } while (0);
    out[1] = yaw;
    out[2] = roll;
    return roll;
}

/* Each component wrapped to +-half a turn. */
/* 0x0013f7e0 */
static void model_ivec3_diff16(int *va, int *vb, int *out)
{
    int x, y, z, ax, ay, az, bx, by, bz;

    do {
        ax = va[0];
        ay = va[1];
        az = va[2];
    } while (0);
    do {
        bx = vb[0];
        by = vb[1];
        bz = vb[2];
    } while (0);
    x = ax - bx;
    y = ay - by;
    z = az - bz;
    if (x > ANG_180) x -= 0x10000; else if (x < -ANG_180) x += 0x10000;
    if (y > ANG_180) y -= 0x10000; else if (y < -ANG_180) y += 0x10000;
    if (z > ANG_180) z -= 0x10000; else if (z < -ANG_180) z += 0x10000;
    out[0] = x;
    out[1] = y;
    out[2] = z;
}

/* Truncated. */
/* 0x0013f880 */
int model_ivec3_length(IVec3 *vec)
{
    int x, y, z;

    do {
        x = vec->x;
        y = vec->y;
        z = vec->z;
    } while (0);
    return (int)sqrtf((float)(x * x + y * y + z * z));
}

/* Yaw 0 looks down -Z (0 when vertical); no roll. */
/* 0x0013f8e8 */
static void model_unit_vec_to_angles(float *dir, int *out)
{
    float x, y, z;
    int ang;

    do {
        x = dir[0];
    } while (0);
    z = dir[2];
    do {
        y = dir[1];
    } while (0);
    if (fabsf(x) < FLT_EPSILON && fabsf(z) < FLT_EPSILON)
        do {
            ang = 0;
        } while (0);
    else
        ang = (int)RAD_TO_ANG(atan2f(-x, -z));
    out[0] = ulAsin(y);
    out[1] = ang;
    out[2] = 0;
}

/* All zero for a near-zero vector. */
/* 0x0013f9a0 */
static void model_vec_to_angles(float *vec, int *out)
{
    float dir[3];
    int zero = 0;

    if (fabsf(vec[0]) < 0.01f && fabsf(vec[1]) < 0.01f && fabsf(vec[2]) < 0.01f)
        zero = 1;
    if (zero) {
        out[2] = out[1] = out[0] = 0;
    } else {
        vec3_normalize(dir, vec);
        model_unit_vec_to_angles(dir, out);
    }
}

/* Returns 0 without writing for a zero-length vec. */
/* 0x0013fa40 */
int vec3_scale_to_length(float len, float *vec, float *out)
{
    float mag = vec3_length(vec);
    float scale;

    if (fabsf(mag) < FLT_EPSILON)
        return 0;
    scale = len / mag;
    out[0] = vec[0] * scale;
    out[1] = vec[1] * scale;
    out[2] = vec[2] * scale;
    return 1;
}

/*
 * Unanimated tree. Flags 1/2/4 skip translate/rotate/scale (all three: no matrix push); rotation
 * Y-X-Z with flag 0x20, else Z-Y-X.
 */
/* 0x0013fad0 */
static int node_tree_collect_xforms_local(NodeTreeNode *node, unsigned int mask, int max, char **pos_out, char **ang_out, char **scale_out)
{
    int count = 0;
    unsigned int flags;
    unsigned int hit;
    unsigned int kind;

    do {
        flags = node->flags;
        hit = flags & mask;
        kind = flags & 7;
        if (hit || !(flags & 0x10)) {
            if (kind != 7) {
                ((void * (*)(void))ulMatrixPush)(); /* steer(cast) */
                if (!(flags & 1)) {
                    float *src = node->pos;

                    do {
                        vu0_matrix_translate(ulMatrixCur, src[0], src[1], src[2]);
                    } while (0);
                }
                if (!(flags & 2)) {
                    if (flags & 0x20)
                        node_tree_rotate_yxz(node->rot);
                    else
                        node_tree_rotate_zyx(node->rot);
                }
                if (!(flags & 4))
                    vu0_matrix_scale(ulMatrixCur, node->scl[0], node->scl[1], node->scl[2]);
            }
            if (hit) {
                if (pos_out) {
                    ulMatrixGetTranslation(ulMatrixCur, *pos_out);
                    *pos_out += 12;
                }
                if (ang_out) {
                    ((void (*)(char *))model_camera_angles)(*ang_out); /* steer(cast) */
                    *ang_out += 12;
                }
                if (scale_out) {
                    model_matrix_scale((float *)*scale_out);
                    *scale_out += 12;
                }
                count++;
            }
            if (count != max && !(flags & 0x10))
                count += node_tree_collect_xforms_local(node->child, mask, max, pos_out, ang_out, scale_out);
            if (kind != 7)
                ulMatrixPop();
        }
    } while (count != max && (node = node->next) != 0);
    return count;
}

/*
 * Appends pos/angles/scale of nodes flagged in mask to the given cursors, up to max. An unflagged
 * node with 0x10 (no descend) only advances the animation.
 */
/* 0x0013fcb8 */
static int node_tree_collect_xforms(NodeTreeNode *node, unsigned int mask, int max, char **pos_out, char **ang_out, char **scale_out)
{
    int count = 0;
    unsigned int flags;
    unsigned int hit;

    do {
        flags = node->flags;
        hit = flags & mask;
        if (hit || !(flags & 0x10)) {
            ulMatrixPush();
            anim_apply_node(node);
            if (hit) {
                if (pos_out) {
                    ulMatrixGetTranslation(ulMatrixCur, *pos_out);
                    *pos_out += 12;
                }
                if (ang_out) {
                    ((void (*)(char *))model_camera_angles)(*ang_out); /* steer(cast) */
                    *ang_out += 12;
                }
                if (scale_out) {
                    model_matrix_scale((float *)*scale_out);
                    *scale_out += 12;
                }
                count++;
            }
            if (count != max && !(flags & 0x10))
                count += node_tree_collect_xforms(node->child, mask, max, pos_out, ang_out, scale_out);
            ulMatrixPop();
        } else {
            anim_record_advance();
        }
    } while (count != max && (node = node->next) != 0);
    return count;
}

/*
 * Model at pos/rot/scl (each optional), turned half a turn about Y; posed by tex at frame scale,
 * else rest pose.
 */
/* 0x0013fe20 */
void node_tree_collect_xforms_at(NodeTreeNode *node, int tex, float scale, unsigned int mask, int max, float *pos,
                   int *rot, float *scl, char *pos_out, char *ang_out, char *scale_out)
{
    ulMatrixPush();
    matrix_unit_set(0);
    if (pos)
        do { vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]); } while (0);
    if (rot)
        node_tree_rotate_yxz(rot);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    if (scl)
        vu0_matrix_scale(ulMatrixCur, scl[0], scl[1], scl[2]);
    if (tex) {
        anim_start(tex, scale);
        node_tree_collect_xforms(node, mask, max, pos_out ? &pos_out : 0, ang_out ? &ang_out : 0, scale_out ? &scale_out : 0);
    } else {
        node_tree_collect_xforms_local(node, mask, max, pos_out ? &pos_out : 0, ang_out ? &ang_out : 0, scale_out ? &scale_out : 0);
    }
    ulMatrixPop();
}

/* Angles only, no half turn. */
/* 0x0013ff88 */
void node_tree_collect_xforms_at_buf(NodeTreeNode *node, int tex, float scale, unsigned int mask, int max, float *pos,
                   int *rot, float *scl, char *ang_out)
{
    ulMatrixPush();
    matrix_unit_set(0);
    if (pos) {
        do {
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
        } while (0);
    }
    if (rot)
        node_tree_rotate_yxz(rot);
    if (scl)
        vu0_matrix_scale(ulMatrixCur, scl[0], scl[1], scl[2]);
    if (tex) {
        anim_start(tex, scale);
        node_tree_collect_xforms(node, mask, max, 0, &ang_out, 0);
    } else {
        node_tree_collect_xforms_local(node, mask, max, 0, &ang_out, 0);
    }
    ulMatrixPop();
}

/* 0x001400b0 */
static void node_tree_collect(NodeTreeNode *node, unsigned int mask, NodeTreeNode ***out)
{
    unsigned int flags;

    do {
        flags = node->flags;
        if (flags & mask) {
            **out = node;
            (*out)++;
        }
        if (!(flags & 0x10))
            node_tree_collect(node->child, mask, out);
        node = node->next;
    } while (node);
}

/* 0x00140130 */
void node_tree_collect_to(NodeTreeNode *node, unsigned int mask, NodeTreeNode **out)
{
    node_tree_collect(node, mask, &out);
}

/* 0x00140150 */
static int node_tree_count(NodeTreeNode *node, unsigned int mask)
{
    unsigned int flags;
    int count = 0;

    do {
        flags = node->flags;
        if (flags & mask)
            count++;
        if (!(flags & 0x10))
            count += node_tree_count(node->child, mask);
        node = node->next;
    } while (node);
    return count;
}

/* Animated tree: 4x4 matrix of each flagged node at *out, until *count stored. */
/* 0x001401c8 */
static void node_tree_draw_matching(NodeTreeNode *node, unsigned int mask, int *count, char **out)
{
    unsigned int flags;

    do {
        if (*count > 0) {
            flags = node->flags;
            ulMatrixPush();
            anim_apply_node(node);
            if (flags & mask) {
                matrix_get_or_set(0, *out);
                *out += 64;
                (*count)--;
            }
            if (*count > 0 && !(flags & 0x10))
                node_tree_draw_matching(node->child, mask, count, out);
            ulMatrixPop();
        }
        node = node->next;
    } while (node);
}

/* Relative to the model origin. flag 0 clears the animation first. */
/* 0x00140298 */
void node_tree_draw_matching_at(int node, int mask, int count, int out, int flag)
{
    ulMatrixPush();
    matrix_unit_set(0);
    if (flag == 0)
        anim_start(0, 0.0f);
    node_tree_draw_matching(node, mask, &count, &out);
    ulMatrixPop();
}

/* Depth-first numbers of flagged nodes; stops descending at max. */
/* 0x00140318 */
static int node_tree_collect_indices(NodeTreeNode *node, unsigned int mask, int max, int **out, int *counter)
{
    unsigned int flags;
    int count = 0;

    do {
        flags = node->flags;
        if (flags & mask) {
            count++;
            **out = *counter;
            (*out)++;
        }
        (*counter)++;
        if (count != max) {
            if (!(flags & 0x10))
                count += node_tree_collect_indices(node->child, mask, max, out, counter);
        }
        node = node->next;
    } while (node);
    return count;
}

/* Depth-first numbers of up to max flagged nodes. */
/* 0x001403e8 */
int node_tree_collect_counted(NodeTreeNode *node, unsigned int mask, int max, int **out)
{
    int counter = 0;

    return node_tree_collect_indices(node, mask, max, &out, &counter);
}

/* (sin(pi*t - pi/2) + 1) / 2, t in 0..1. */
/* 0x00140410 */
float ease_sine_in_out(float t)
{
    float sc[2];

    ulSinCos((int)(t * ANG_HALF - ANG_QUARTER), &sc[0], &sc[1]);
    return (sc[0] + 1.0f) * 0.5f;
}

/* 0x00140470 */
float ease_sine_in_out_rev(float t)
{
    return 1.0f - ease_sine_in_out(t);
}

/* Twice the rate: 0..1 over t in 0..0.5. */
/* 0x00140498 */
float ease_sine_in_out_2x(float t)
{
    float sc[2];

    ulSinCos((int)((t + t - 0.5f) * ANG_HALF), &sc[0], &sc[1]);
    return sc[0] * 0.5f + 0.5f;
}

/* 0x001404f0 */
float ease_sine_in_out_2x_neg(float t)
{
    return -ease_sine_in_out_2x(t);
}

/* 0x00140510 */
static void node_tree_rotate_zyx(int *rot)
{
    vu0_matrix_rotate_z(ulMatrixCur, rot[2]);
    vu0_matrix_rotate_y(ulMatrixCur, rot[1]);
    vu0_matrix_rotate_x(ulMatrixCur, rot[0]);
}

/* 0x00140560 */
static void node_tree_rotate_yxz(int *rot)
{
    vu0_matrix_rotate_y(ulMatrixCur, rot[1]);
    vu0_matrix_rotate_x(ulMatrixCur, rot[0]);
    vu0_matrix_rotate_z(ulMatrixCur, rot[2]);
}
