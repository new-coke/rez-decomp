/*
 * Stage enemy helpers: node show/hide and marked-node positions, random vectors, piecewise-linear
 * curves, conversions into and out of the player's frame, and position tests against the player
 * and view.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
#include "camera.h"
#include "libc.h"
#include "math_util.h"
#include "player.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "util.h"
#include "system.h"
#include "mathdef.h"

static inline void v3_set(Vec3 *v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void v3_scale(Vec3 *v, float k)
{
    v->x *= k;
    v->y *= k;
    v->z *= k;
}

static inline int v3_is_zero(Vec3 *v)
{
    int z = 0;

    if (fabsf(v->x) < 0.01f && fabsf(v->y) < 0.01f && fabsf(v->z) < 0.01f)
        z = 1;
    return z;
}

extern int ulMatrixPush(void);

/* 0x00b11ce8 stg02, 0x00b10440 stg04, 0x00b1c4b0 stg22, 0x00af88a8 stg23 */
void util_node_list_toggle(void *p)
{
    int *node = p;

    do {
        if (node[1])
            node[0] ^= 8;
        if (!(node[0] & 0x10))
            util_node_list_toggle((void *)node[11]);
        node = (int *)node[12];
    } while (node);
}

/* 0x00b11d48 stg02, 0x00b104a0 stg04, 0x00b1c510 stg22, 0x00af8908 stg23 */
void util_node_list_toggle_unmasked(unsigned int *node, unsigned int mask)
{
    do {
        if (!(node[0] & mask)) {
            if (node[1])
                node[0] ^= 8;
            if (!(node[0] & 0x10))
                util_node_list_toggle_unmasked((unsigned int *)node[11], mask);
        }
        node = (unsigned int *)node[12];
    } while (node);
}

/* 0x00b11dc0 stg02, 0x00b10518 stg04, 0x00b1c588 stg22, 0x00af8980 stg23 */
int util_node_list_collect(void *root, void *bits, void *max, int *dst)
{
    unsigned int *node = root;
    unsigned int mask = (unsigned int)bits;
    int count = (int)max;
    unsigned int ***out = (unsigned int ***)dst;
    unsigned int fl;

    do {
        fl = node[0];
        if (fl & mask) {
            *(*out)++ = node;
            if (--count == 0)
                return 0;
        }
        if (!(fl & 0x10)) {
            count = util_node_list_collect((void *)node[11], (void *)mask, (void *)count, (int *)out);
            if (!count)
                return 0;
        }
        node = (unsigned int *)node[12];
    } while (node);
    return count;
}

/* 0x00b11e68 stg02, 0x00b105c0 stg04, 0x00b1c630 stg22, 0x00af8a28 stg23 */
int util_node_list_collect_pos(void *top, int flags, int num, int *out)
{
    unsigned int *node = top;
    unsigned int mask = flags;
    int count = num;
    unsigned int sel;
    int down;

    do {
        sel = node[0] & mask;
        down = !(node[0] & 0x10);
        if (sel || down) {
            ulMatrixPush();
            anim_apply_node(node);
            if (sel) {
                count--;
                ulMatrixGetTranslation((float *)ulMatrixCur, (float *)(*(Vec3 **)out)++);
                if (!count) {
                    ulMatrixPop();
                    return 0;
                }
            }
            if (down) {
                count = util_node_list_collect_pos((void *)node[11], mask, count, out);
                if (!count) {
                    ulMatrixPop();
                    return 0;
                }
            }
            ulMatrixPop();
        } else
            anim_record_advance();
        node = (unsigned int *)node[12];
    } while (node);
    return count;
}

/* 0x00b11f88 stg02, 0x00b106e0 stg04, 0x00b1c750 stg22, 0x00af8b48 stg23 */
void util_rand_vec3(Vec3 *v)
{
    float range = 32769.0f;
    float half = 0.5f;
    float x, y;

    x = rand_lcg_next_a() / range - half;
    y = rand_lcg_next_a() / range - half;
    v3_set(v, x, y, rand_lcg_next_a() / range - half);
}

/* 0x00b12040 stg02, 0x00b10798 stg04, 0x00b1c808 stg22, 0x00af8c00 stg23 */
void util_rand_unit_vec3(Vec3 *v)
{
    do
        util_rand_vec3(v);
    while (v3_is_zero(v));
    vec3_normalize(v, v);
}

/* 0x00b120d0 stg02, 0x00b10828 stg04, 0x00b1c898 stg22, 0x00af8c90 stg23 */
void util_rand_vec3_len(Vec3 *v, float len)
{
    float vlen;

    do {
        util_rand_vec3(v);
        vlen = vec3_length(v);
    } while (fabsf(vlen) < FLT_EPSILON);
    v3_scale(v, len / vlen);
}

/* 0x00b12168 stg02, 0x00b108c0 stg04, 0x00b1c930 stg22, 0x00af8d28 stg23 */
float util_curve_eval(float *p, float t)
{
    float x0, x1 = p[0];

    for (;; p += 2) {
        x0 = x1;
        x1 = p[2];
        if (x0 <= t && t <= x1)
        {
            float y0 = p[1];

            return y0 + (p[3] - y0) * (t - x0) / (x1 - x0);
        }
    }
}

/* 0x00b121c8 stg02, 0x00b10920 stg04, 0x00b1c990 stg22, 0x00af8d88 stg23 */
void util_rotate_vec3(float *in, int *ang, float *out)
{
    ulMatrixPush();
    matrix_unit_set(0);
    util_rotate_yxz(ang);
    ulMatrixApplyVector((float *)ulMatrixCur, in, out);
    ulMatrixPop();
}

/* 0x00b12228 stg02, 0x00b10980 stg04, 0x00b1c9f0 stg22, 0x00af8de8 stg23 */
void util_rotate_xyz(int *ang, float x, float y, float z, Vec3 *out)
{
    Vec3 v;

    v.x = x;
    v.y = y;
    v.z = z;
    ulMatrixPush();
    matrix_unit_set(0);
    util_rotate_yxz(ang);
    ulMatrixApplyVector((float *)ulMatrixCur, &v.x, &out->x);
    ulMatrixPop();
}

/* 0x00b12290 stg02, 0x00b109e8 stg04, 0x00b1ca58 stg22, 0x00af8e50 stg23 */
int util_node_collect(void *root, void *bits, void *max, int dst)
{
    int t = dst;

    return util_node_list_collect(root, bits, max, &t);
}

/* 0x00b122b0 stg02, 0x00b10a08 stg04, 0x00b1ca78 stg22, 0x00af8e70 stg23 */
void util_node_collect_pos_at(void *frm, void *top, float mot, int flags, int num, int out)
{
    int outp = out;

    anim_start(top, mot);
    util_node_list_collect_pos(frm, flags, num, &outp);
}

/* 0x00b12308 stg02, 0x00b10a60 stg04, 0x00b1cad0 stg22, 0x00af8ec8 stg23 */
void util_node_toggle(int *node)
{
    if (node[1])
        node[0] ^= 8;
    if (!(node[0] & 0x10))
        util_node_list_toggle((void *)node[11]);
}

/* 0x00b12340 stg02, 0x00b10a98 stg04, 0x00b1cb08 stg22, 0x00af8f00 stg23 */
void util_node_toggle_unmasked(int *node, unsigned int mask)
{
    if (node[1])
        node[0] ^= 8;
    if (!(node[0] & 0x10))
        util_node_list_toggle_unmasked((void *)node[11], mask);
}

/* 0x00b12378 stg02, 0x00b10ad0 stg04, 0x00b1cb40 stg22, 0x00af8f38 stg23 */
void util_to_player_frame(Vec3 *pos, Vec3 *dir, int *ang_out)
{
    Vec3 pl;
    int ang[3];

    player_get_frame_pos(&pl);
    player_get_frame_ang(ang);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &pl;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    util_rotate_yxz(ang);
    if (pos)
        ulMatrixApplyPoint((float *)ulMatrixCur, &pos->x, &pos->x);
    if (dir)
        ulMatrixApplyVector((float *)ulMatrixCur, &dir->x, &dir->x);
    ulMatrixPop();
    if (ang_out) {
        ang_out[0] += ang[0];
        ang_out[1] += ang[1];
        ang_out[2] += ang[2];
    }
}

/* 0x00b12468 stg02, 0x00b10bc0 stg04, 0x00b1cc30 stg22, 0x00af9028 stg23 */
void util_pos_to_player_frame(Vec3 *in, Vec3 *out)
{
    Vec3 pl;
    int ang[3];

    player_get_frame_pos(&pl);
    player_get_frame_ang(ang);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &pl;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    util_rotate_yxz(ang);
    ulMatrixApplyPoint((float *)ulMatrixCur, &in->x, &out->x);
    ulMatrixPop();
}

/* 0x00b12500 stg02, 0x00b10c58 stg04, 0x00b1ccc8 stg22, 0x00af90c0 stg23 */
void util_dir_to_player_frame(float *in, float *out)
{
    int ang[3];

    player_get_frame_ang(ang);
    ulMatrixPush();
    matrix_unit_set(0);
    util_rotate_yxz(ang);
    ulMatrixApplyVector((float *)ulMatrixCur, in, out);
    ulMatrixPop();
}

/* 0x00b12568 stg02, 0x00b10cc0 stg04, 0x00b1cd30 stg22, 0x00af9128 stg23 */
void util_ang_to_player_frame(int *in, int *out)
{
    int ang[3];

    player_get_frame_ang(ang);
    out[0] = in[0] + ang[0];
    out[1] = in[1] + ang[1];
    out[2] = in[2] + ang[2];
}

/* 0x00b125d0 stg02, 0x00b10d28 stg04, 0x00b1cd98 stg22, 0x00af9190 stg23 */
void util_from_player_frame(Vec3 *pos, Vec3 *dir, int *ang_out)
{
    Vec3 pl;
    int ang[3];
    int *rot;
    int x, y, z;

    player_get_frame_pos(&pl);
    player_get_frame_ang(ang);
    rot = ang;
    pl.x = -pl.x;
    pl.y = -pl.y;
    pl.z = -pl.z;
    do {
        x = rot[0];
        y = rot[1];
        z = rot[2];
    } while (0);
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_z(ulMatrixCur, -z);
    vu0_matrix_rotate_x(ulMatrixCur, -x);
    vu0_matrix_rotate_y(ulMatrixCur, -y);
    do {
        Vec3 *p_ = &pl;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    if (pos)
        ulMatrixApplyPoint((float *)ulMatrixCur, &pos->x, &pos->x);
    if (dir)
        ulMatrixApplyVector((float *)ulMatrixCur, &dir->x, &dir->x);
    ulMatrixPop();
    if (ang_out) {
        ang_out[0] -= ang[0];
        ang_out[1] -= ang[1];
        ang_out[2] -= ang[2];
    }
}

/* 0x00b12718 stg02, 0x00b10e70 stg04, 0x00b1cee0 stg22, 0x00af92d8 stg23 */
void util_pos_from_player_frame(Vec3 *in, Vec3 *out)
{
    Vec3 pl;
    int ang[3];
    int *rot;
    int x, y, z;

    player_get_frame_pos(&pl);
    player_get_frame_ang(ang);
    rot = ang;
    pl.x = -pl.x;
    pl.y = -pl.y;
    pl.z = -pl.z;
    do {
        x = rot[0];
        y = rot[1];
        z = rot[2];
    } while (0);
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_z(ulMatrixCur, -z);
    vu0_matrix_rotate_x(ulMatrixCur, -x);
    vu0_matrix_rotate_y(ulMatrixCur, -y);
    do {
        Vec3 *p_ = &pl;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ulMatrixApplyPoint((float *)ulMatrixCur, &in->x, &out->x);
    ulMatrixPop();
}

/* 0x00b12808 stg02, 0x00b10f60 stg04, 0x00b1cfd0 stg22, 0x00af93c8 stg23 */
void util_dir_from_player_frame(Vec3 *in, Vec3 *out)
{
    int ang[3];
    int x, y, z;

    player_get_frame_ang(ang);
    x = ang[0];
    y = ang[1];
    z = ang[2];
    do {
        ulMatrixPush();
    } while (0);
    matrix_unit_set(0);
    vu0_matrix_rotate_z(ulMatrixCur, -z);
    vu0_matrix_rotate_x(ulMatrixCur, -x);
    vu0_matrix_rotate_y(ulMatrixCur, -y);
    ulMatrixApplyVector((float *)ulMatrixCur, &in->x, &out->x);
    ulMatrixPop();
}

/* 0x00b128b8 stg02, 0x00b11010 stg04, 0x00b1d080 stg22, 0x00af9478 stg23 */
void util_ang_from_player_frame(int *in, int *out)
{
    int ang[3];

    player_get_frame_ang(ang);
    out[0] = in[0] - ang[0];
    out[1] = in[1] - ang[1];
    out[2] = in[2] - ang[2];
}

/* 0x00b12920 stg02, 0x00b11078 stg04, 0x00b1d0e8 stg22, 0x00af94e0 stg23 */
int util_player_near_axis(Vec3 *pos, int *ang, float r, float lim)
{
    Vec3 rel;
    Vec3 fwd;
    float len;

    if (player_sphere_test(pos, r) == 1) {
        player_get_pos(&rel);
        v3_sub(&rel, &rel, pos);
        v3_set(&fwd, 0.0f, 0.0f, 1.0f);
        ulMatrixPush();
        matrix_unit_set(0);
        util_rotate_yxz(ang);
        ulMatrixApplyVector((float *)ulMatrixCur, &fwd.x, &fwd.x);
        ulMatrixPop();
        len = vec3_length(&rel);
        vec3_normalize(&rel, &rel);
        vec3_normalize(&fwd, &fwd);
        return len * fabsf(ulVec3Dot(&rel, &fwd)) < lim;
    }
    return 0;
}

/* 0x00b12a48 stg02, 0x00b111a0 stg04, 0x00b1d210 stg22, 0x00af9608 stg23 */
int util_player_outside(Vec3 *pos, float r)
{
    Vec3 pl;

    player_get_pos(&pl);
    return r * r <= vec3_distance_squared(&pl, pos);
}

/* 0x00b12aa0 stg02, 0x00b111f8 stg04, 0x00b1d268 stg22, 0x00af9660 stg23 */
int util_behind_player(Vec3 *pos, float r)
{
    Vec3 pl;
    Vec3 dir;
    Vec3 fwd;
    float rad_sq = r * r;
    float dist_sq;
    float dot;

    player_get_pos(&pl);
    dist_sq = vec3_distance_squared(&pl, pos);
    if (rad_sq < dist_sq) {
        v3_sub(&dir, pos, &pl);
        vec3_normalize(&dir, &dir);
        v3_set(&fwd, 0.0f, 0.0f, 1.0f);
        util_dir_to_player_frame(&fwd.x, &fwd.x);
        dot = ulVec3Dot(&dir, &fwd);
        if (dot < 0.0f && rad_sq < dist_sq * dot * dot)
            return 1;
    }
    return 0;
}

/* 0x00b12ba8 stg02, 0x00b11300 stg04, 0x00b1d370 stg22, 0x00af9768 stg23 */
int util_ahead_of_player(Vec3 *pos, float r)
{
    Vec3 pl;
    Vec3 dir;
    Vec3 fwd;
    float rad_sq = r * r;
    float dist_sq;
    float dot;

    player_get_pos(&pl);
    dist_sq = vec3_distance_squared(&pl, pos);
    if (rad_sq < dist_sq) {
        v3_sub(&dir, pos, &pl);
        vec3_normalize(&dir, &dir);
        v3_set(&fwd, 0.0f, 0.0f, 1.0f);
        util_dir_to_player_frame(&fwd.x, &fwd.x);
        dot = ulVec3Dot(&dir, &fwd);
        if (dot > 0.0f && rad_sq < dist_sq * dot * dot)
            return 1;
    }
    return 0;
}

/* 0x00b12cb0 stg02, 0x00b11408 stg04, 0x00b1d478 stg22, 0x00af9870 stg23 */
int util_eye_outside(Vec3 *pos)
{
    Vec3 eye;
    float scl;

    camera_get_eye(&eye);
    scl = camera_get_far();
    return scl * scl <= vec3_distance_squared(&eye, pos);
}

/* 0x00b12d10 stg02, 0x00b11468 stg04, 0x00b1d4d8 stg22, 0x00af98d0 stg23 */
void util_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
