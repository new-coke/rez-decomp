/*
 * Effect toolkit for stage and boss effects: sized blocks, colour packing, screen clipping, 2D and
 * billboard sprites, matrix and angle helpers, node positions, camera ray test, beat colour
 * pulses, model mesh save and restore.
 */

#include "tvset.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "nj_cnk_vu1.h"
#include "nj_system.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "anim.h"
#include "approach.h"
#include "color.h"
#include "heap.h"
#include "math_util.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "player_camera.h"
#include "sound_beat.h"
#include "camera.h"
#include "system.h"

#define YUKI_MAX(a, b) ((a) >= (b) ? (a) : (b))
#define YUKI_MIN(a, b) ((a) < (b) ? (a) : (b))
#define YUKI_VEC_GET(p, vx, vy, vz)     do {         vx = (p)->x;         vy = (p)->y;         vz = (p)->z;     } while (0)

#define YUKI_VTX(p, px, py, pz, pu, pv, pc) \
    do { (p)->x = (px); (p)->y = (py); (p)->z = (pz); (p)->u = (pu); (p)->v = (pv); (p)->col = (pc); } while (0)

#define ROT_Y(ang, x, z)                                                                     \
    do {                                                                                     \
        float rot_s, rot_c, rot_u = x, rot_v = z;                                            \
        ulSinCos(ang, &rot_s, &rot_c);                                                   \
        x = rot_u * rot_c + rot_v * rot_s;                                                   \
        z = -rot_u * rot_s + rot_v * rot_c;                                                  \
    } while (0)

#define ROT_X(ang, y, z)                                                                     \
    do {                                                                                     \
        float rot_s, rot_c, rot_u = y, rot_v = z;                                            \
        ulSinCos(ang, &rot_s, &rot_c);                                                   \
        y = rot_u * rot_c - rot_v * rot_s;                                                   \
        z = rot_u * rot_s + rot_v * rot_c;                                                   \
    } while (0)

#define ROT_Z(ang, x, y)                                                                     \
    do {                                                                                     \
        float rot_s, rot_c, rot_u = x, rot_v = y;                                            \
        ulSinCos(ang, &rot_s, &rot_c);                                                   \
        x = rot_u * rot_c - rot_v * rot_s;                                                   \
        y = rot_u * rot_s + rot_v * rot_c;                                                   \
    } while (0)

extern int yuki_sprite2_buf;

extern void ulMatrixPush(void);

static void yuki_matrix_set(void **rot);

/* Filled with 0xff, behind a size word; free with yuki_free. */
/* 0x00174668 */
void *yuki_alloc(unsigned int size)
{
    unsigned int *hdr;
    unsigned char *buf;
    char *dst;
    unsigned int i;

    hdr = heap_block_tag(game_malloc(size + 4), "yuki_tool.c", 75);
    buf = (unsigned char *)(hdr + 1);
    *hdr = size;
    for (i = 0, dst = (char *)buf; i < size; i++)
        *dst++ = -1;
    return buf;
}

/* Clears first; "FreeErr!" for size 0, freed anyway. */
/* 0x001746e0 */
void yuki_free(void *buf)
{
    int *base = (int *)buf - 1;

    if (*base == 0)
        ERROR_PRINTF("FreeErr!\n");
    heap_bzero(base, *base + 4);
    heap_free(base, 0);
}

/* 0..1, clamped; halved bytes (0x80 = 1.0), GS order a, b, g, r from the top. */
/* 0x00174738 */
static unsigned int yuki_color_argb(float a, float r, float g, float b)
{
    int ia, ir, ig, ib;

    ib = b * 255.0f;
    ia = a * 255.0f;
    ir = r * 255.0f;
    ig = g * 255.0f;
    if (ib < 0) ib = 0;
    if (ia < 0) ia = 0;
    if (ir < 0) ir = 0;
    if (ig < 0) ig = 0;
    if (ib > 255) ib = 255;
    if (ia > 255) ia = 255;
    if (ig > 255) ig = 255;
    if (ir > 255) ir = 255;
    return ((ia + 1) >> 1) << 24 | (((ib + 1) >> 1) & 0xff) << 16 | (((ig + 1) >> 1) & 0xff) << 8
        | (((ir + 1) >> 1) & 0xff);
}

/* White; alpha clamped 0..255. */
/* 0x00174800 */
static unsigned int yuki_color_alpha(float a)
{
    int ia;

    ia = (int)YUKI_MIN(YUKI_MAX(a, 0), 255);
    return ia << 24 | 0xffffff;
}

/* Outside -1000..1000 on either axis. */
/* 0x00174860 */
int yuki_is_offscreen(float *pos)
{
    float x = pos[0];
    float y = pos[1];

    do { } while (0);
    if (x < -1000.0f || x > 1000.0f || y < -1000.0f || y > 1000.0f)
        return 1;
    return 0;
}

static inline float yuki_fmax(float val, float lo)
{
    return val >= lo ? val : lo;
}

/*
 * End clipped to the -1000..1000 square, z interpolated (at least 1.0); returns the kept
 * fraction. Nearly axis-aligned segments are nudged by one unit first.
 */
/* 0x001748b8 */
float yuki_clip_line(Vec3 *start, Vec3 *end)
{
    float x0, y0, z0;
    float x1, y1, z1;
    float dy, dx, slope, icept, t;

    YUKI_VEC_GET(start, x0, y0, z0);
    YUKI_VEC_GET(end, x1, y1, z1);
    if (fabsf(x0 - x1) < FLT_EPSILON) {
        x1 = x0 + 1.0f;
        end->x = x1;
    }
    if (fabsf(y0 - y1) < FLT_EPSILON) {
        y1 = y0 + 1.0f;
        end->y = y1;
    }
    dy = y1 - y0;
    dx = x1 - x0;
    slope = dy / dx;
    icept = y0 - slope * x0;
    if (x0 < x1) {
        if (y0 < y1) {
            if (slope * 1000.0f + icept > 1000.0f) {
                if (y1 < 1000.0f) {
                    t = 1.0f;
                } else {
                    t = (1000.0f - y0) / dy;
                    y1 = 1000.0f;
                    z1 = z0 + (z1 - z0) * t;
                    x1 = x0 + dx * t;
                    z1 = yuki_fmax(z1, 1.0f);
                    end->x = x1;
                    end->y = y1;
                    end->z = z1;
                }
            } else {
                if (x1 < 1000.0f) {
                    t = 1.0f;
                } else {
                    t = (1000.0f - x0) / dx;
                    x1 = 1000.0f;
                    z1 = z0 + (z1 - z0) * t;
                    y1 = y0 + dy * t;
                    z1 = yuki_fmax(z1, 1.0f);
                    end->x = x1;
                    end->y = y1;
                    end->z = z1;
                }
            }
        } else {
            if (slope * 1000.0f + icept < -1000.0f) {
                if (y1 > -1000.0f) {
                    t = 1.0f;
                } else {
                    t = (y0 - -1000.0f) / (y0 - y1);
                    y1 = -1000.0f;
                    z1 = z0 + (z1 - z0) * t;
                    x1 = x0 + dx * t;
                    z1 = yuki_fmax(z1, 1.0f);
                    end->x = x1;
                    end->y = y1;
                    end->z = z1;
                }
            } else {
                if (x1 < 1000.0f) {
                    t = 1.0f;
                } else {
                    t = (1000.0f - x0) / dx;
                    x1 = 1000.0f;
                    z1 = z0 + (z1 - z0) * t;
                    y1 = y0 + dy * t;
                    z1 = yuki_fmax(z1, 1.0f);
                    end->x = x1;
                    end->y = y1;
                    end->z = z1;
                }
            }
        }
    } else {
        if (y0 < y1) {
            if (slope * -1000.0f + icept > 1000.0f) {
                if (y1 < 1000.0f) {
                    t = 1.0f;
                } else {
                    t = (1000.0f - y0) / dy;
                    y1 = 1000.0f;
                    z1 = z0 + (z1 - z0) * t;
                    x1 = x0 + dx * t;
                    z1 = yuki_fmax(z1, 1.0f);
                    end->x = x1;
                    end->y = y1;
                    end->z = z1;
                }
            } else {
                if (x1 > -1000.0f) {
                    t = 1.0f;
                } else {
                    t = (x0 - -1000.0f) / (x0 - x1);
                    x1 = -1000.0f;
                    z1 = z0 + (z1 - z0) * t;
                    y1 = y0 + dy * t;
                    z1 = yuki_fmax(z1, 1.0f);
                    end->x = x1;
                    end->y = y1;
                    end->z = z1;
                }
            }
        } else {
            if (slope * -1000.0f + icept < -1000.0f) {
                if (y1 > -1000.0f) {
                    t = 1.0f;
                } else {
                    t = (y0 - -1000.0f) / (y0 - y1);
                    y1 = -1000.0f;
                    z1 = z0 + (z1 - z0) * t;
                    x1 = x0 + dx * t;
                    z1 = yuki_fmax(z1, 1.0f);
                    end->x = x1;
                    end->y = y1;
                    end->z = z1;
                }
            } else {
                if (x1 > -1000.0f) {
                    t = 1.0f;
                } else {
                    t = (x0 - -1000.0f) / (x0 - x1);
                    x1 = -1000.0f;
                    z1 = z0 + (z1 - z0) * t;
                    y1 = y0 + dy * t;
                    z1 = yuki_fmax(z1, 1.0f);
                    end->x = x1;
                    end->y = y1;
                    end->z = z1;
                }
            }
        }
    }
    return t;
}

/* Half extents, rotated by angle, texture tex of the current list; skipped past -1000..1000. */
/* 0x00174ce8 */
void yuki_sprite_draw(int angle, unsigned int col, int tex, float x, float y, float z, float half_w,
                   float half_h)
{
    TvVtxUV strip[4];
    float s, c;
    float ax, bx, ay, by;
    float mx, my;
    TvVtxUV *vtx;

    ulGxSelectTexture(ulGxGetTextureList(), tex);
    ulSinCos(angle, &s, &c);
    ax = half_w * c + half_h * s;
    bx = half_w * c - half_h * s;
    by = half_w * s + half_h * c;
    ay = half_w * s - half_h * c;
    mx = YUKI_MAX(fabsf(bx), fabsf(ax));
    my = YUKI_MAX(fabsf(by), fabsf(ay));
    if (x - mx < -1000.0f || x + mx > 1000.0f || y - my < -1000.0f || y + my > 1000.0f)
        return;
    ulGxBeginTexStrip(1);
    do {
        strip[0].x = x - bx;
        strip[0].z = z;
        strip[0].u = 0;
        strip[0].y = y - by;
        strip[0].v = 0;
        strip[0].col = col;
    } while (0);
    do {
        strip[1].x = x - ax;
        strip[1].z = z;
        strip[1].u = 0;
        strip[1].y = y - ay;
        strip[1].v = 1.0f;
        strip[1].col = col;
    } while (0);
    do {
        vtx = &strip[3];
        strip[2].x = x + ax;
        strip[2].z = z;
        strip[2].u = 1.0f;
        strip[2].y = y + ay;
        strip[2].v = 0;
        strip[2].col = col;
    } while (0);
    do {
        vtx->x = x + bx;
        vtx->z = z;
        vtx->u = 1.0f;
        vtx->y = y + by;
        vtx->v = 1.0f;
        vtx->col = col;
    } while (0);
    ulGxAddTexStrip(strip, 4);
    ulGxEndTexStrip();
}

/* Sized by distance; nothing outside the view or in front of the near plane. */
/* 0x00174f00 */
static void yuki_sprite_draw3d(int angle, unsigned int col, int tex, Vec3 *pos, float half_w, float half_h)
{
    Vec3 proj;
    Vec3 sc;
    Vec3 *scl;

    if (!view_sphere_visible(pos, YUKI_MAX(half_w, half_h)))
        return;
    ulMatrixApplyPoint(ulMatrixCur, pos, &proj);
    if (!(proj.z < -cam_near))
        return;
    scl = &sc;
    sc.x = 1.0f;
    sc.y = 0;
    sc.z = proj.z;
    project_point(&proj, &proj);
    project_point_abs(scl, scl);
    yuki_sprite_draw(angle, col, tex, proj.x, proj.y, 1.0f / proj.z, half_w * scl->x, half_h * scl->x);
}

/* Projected size at least (wmin, hmin). */
/* 0x00175018 */
void yuki_sprite_draw3d_min(int angle, unsigned int col, int tex, Vec3 *pos, float half_w, float half_h,
                   float wmin, float hmin)
{
    Vec3 proj;
    Vec3 sc;
    Vec3 *scl;

    if (!view_sphere_visible(pos, YUKI_MAX(half_w, half_h)))
        return;
    ulMatrixApplyPoint(ulMatrixCur, pos, &proj);
    if (!(proj.z < -cam_near))
        return;
    scl = &sc;
    sc.x = 1.0f;
    sc.y = 0;
    sc.z = proj.z;
    project_point(&proj, &proj);
    project_point_abs(scl, scl);
    half_w *= scl->x;
    half_h *= scl->x;
    yuki_sprite_draw(angle, col, tex, proj.x, proj.y, 1.0f / proj.z, YUKI_MAX(wmin, half_w), YUKI_MAX(hmin, half_h));
}

/*
 * Mirrored about the centre: texture corner (1, 1) at the centre, so a quarter texture makes a
 * symmetric sprite. Off screen it returns without restoring its draw test mode.
 */
/* 0x00175170 */
void yuki_sprite2_draw(int angle, unsigned int col, int tex, float x, float y, float z, float half_w,
                   float half_h)
{
    TvVtxUV strip[6];
    float s, c;
    float bx, by, ax, ay;
    float half_bx, half_by, half_ax, half_ay;
    float mx, my;
    TvVtxUV *vtx;

    ulGxSelectTexture(ulGxGetTextureList(), tex);
    yuki_sprite2_buf = draw_set_test_mode(0x18000);
    ulSinCos(angle, &s, &c);
    bx = half_w * c - half_h * s;
    ax = half_w * c + half_h * s;
    by = half_w * s + half_h * c;
    ay = half_w * s - half_h * c;
    half_bx = bx * 0.5f;
    half_by = by * 0.5f;
    half_ax = ax * 0.5f;
    half_ay = ay * 0.5f;
    mx = YUKI_MAX(fabsf(bx), fabsf(ax));
    my = YUKI_MAX(fabsf(by), fabsf(ay));
    if (x - mx < -1000.0f || x + mx > 1000.0f || y - my < -1000.0f || y + my > 1000.0f)
        return;
    ulGxBeginTexStrip(1);
    YUKI_VTX(&strip[0], x - bx, y - by, z, 0, 0, col);
    YUKI_VTX(&strip[1], x - (half_bx + half_ax), y - (half_by + half_ay), z, 0, 1.0f, col);
    YUKI_VTX(&strip[2], x - (half_bx - half_ax), y - (half_by - half_ay), z, 1.0f, 0, col);
    YUKI_VTX(&strip[3], x, y, z, 1.0f, 1.0f, col);
    do {
        vtx = &strip[5];
        strip[4].x = x + ax;
        strip[4].y = y + ay;
        strip[4].z = z;
        strip[4].u = 0;
        strip[4].v = 0;
        strip[4].col = col;
    } while (0);
    YUKI_VTX(vtx, x + (half_bx + half_ax), y + (half_by + half_ay), z, 0, 1.0f, col);
    vtx = strip;
    ulGxAddTexStrip(vtx, 6);
    YUKI_VTX(&strip[0], x - (half_bx + half_ax), y - (half_by + half_ay), z, 0, 1.0f, col);
    YUKI_VTX(&strip[1], x - ax, y - ay, z, 0, 0, col);
    YUKI_VTX(&strip[2], x, y, z, 1.0f, 1.0f, col);
    YUKI_VTX(&strip[3], x + (half_bx - half_ax), y + (half_by - half_ay), z, 1.0f, 0, col);
    vtx = &strip[5];
    YUKI_VTX(&strip[4], x + (half_bx + half_ax), y + (half_by + half_ay), z, 0, 1.0f, col);
    YUKI_VTX(vtx, x + bx, y + by, z, 0, 0, col);
    ulGxAddTexStrip(strip, 6);
    ulGxEndTexStrip();
    draw_set_test_mode(yuki_sprite2_buf);
}

/* Mirrored sprite of yuki_sprite2_draw. */
/* 0x00175528 */
void yuki_sprite2_draw3d(int angle, unsigned int col, int tex, Vec3 *pos, float half_w, float half_h)
{
    Vec3 proj;
    Vec3 sc;
    Vec3 *scl;

    if (!view_sphere_visible(pos, YUKI_MAX(half_w, half_h)))
        return;
    ulMatrixApplyPoint(ulMatrixCur, pos, &proj);
    if (!(proj.z < -cam_near))
        return;
    scl = &sc;
    sc.x = 1.0f;
    sc.y = 0;
    sc.z = proj.z;
    project_point(&proj, &proj);
    project_point_abs(scl, scl);
    yuki_sprite2_draw(angle, col, tex, proj.x, proj.y, 1.0f / proj.z, half_w * scl->x, half_h * scl->x);
}

/* Projected size at least (wmin, hmin). */
/* 0x00175640 */
void yuki_sprite2_draw3d_min(int angle, unsigned int col, int tex, Vec3 *pos, float half_w, float half_h,
                   float wmin, float hmin)
{
    Vec3 proj;
    Vec3 sc;
    Vec3 *scl;

    if (!view_sphere_visible(pos, YUKI_MAX(half_w, half_h)))
        return;
    ulMatrixApplyPoint(ulMatrixCur, pos, &proj);
    if (!(proj.z < -cam_near))
        return;
    scl = &sc;
    sc.x = 1.0f;
    sc.y = 0;
    sc.z = proj.z;
    project_point(&proj, &proj);
    project_point_abs(scl, scl);
    half_w *= scl->x;
    half_h *= scl->x;
    yuki_sprite2_draw(angle, col, tex, proj.x, proj.y, 1.0f / proj.z, YUKI_MAX(wmin, half_w), YUKI_MAX(hmin, half_h));
}

/* 0x00175798 */
void yuki_matrix_get_scale(float *scale)
{
    Vec3 axis[3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };

    matrix_apply_vectors(axis, axis, 3);
    scale[0] = vec3_length(&axis[0]);
    scale[1] = vec3_length(&axis[1]);
    scale[2] = vec3_length(&axis[2]);
}

/* Fresh matrix from rot plus pitch ang about X; angles read back into rot (model_camera_angles). */
/* 0x00175840 */
static void yuki_draw_transformed(void *ang, void **rot)
{
    ulMatrixPush();
    matrix_unit_set(0);
    yuki_matrix_set(rot);
    vu0_matrix_rotate_x(ulMatrixCur, ang);
    model_camera_angles(rot);
    ulMatrixPop();
}

/* 0x00175898 */
static void yuki_node_sub(int n, YNode *node)
{
    node->unk_08 -= n;
}

/*
 * In rot's frame: angle about forward (*yaw) and pitch; side parts under a tenth count as 0.
 * Ahead gives 0, 0; behind 0, 0x8000.
 */
/* 0x001758a8 */
static void yuki_aim_angles(Vec3 *to, Vec3 *from, int *rot, int *yaw, int *pitch)
{
    Vec3 dir;
    float x, y, z;
    int ang;

    dir.x = to->x - from->x;
    dir.y = to->y - from->y;
    dir.z = to->z - from->z;
    do {
        x = dir.x;
        y = dir.y;
        z = dir.z;
    } while (0);
    ROT_Y(-rot[1], x, z);
    ROT_X(-rot[0], y, z);
    ROT_Z(-rot[2], x, y);
    if (fabsf(x / z) < 0.1f)
        x = 0;
    if (fabsf(y / z) < 0.1f)
        y = 0;
    if (x == 0 && y == 0) {
        if (z <= 0) {
            *yaw = 0;
            *pitch = 0;
        } else {
            *yaw = 0;
            *pitch = ANG_180;
        }
    } else {
        ang = ANG_90 - (int)RAD_TO_ANG(atan2f(y, x));
        *yaw = ang;
        ROT_Z(ang, x, y);
        *pitch = RAD_TO_ANG(atan2f(y, -z));
    }
}

/* Direction into rot's frame; its yaw and pitch added to rot, read back into rot. */
/* 0x00175b08 */
void yuki_draw_aimed(Vec3 *to, Vec3 *from, int *rot)
{
    Vec3 dir;
    int ang[2];
    float x, y, z;

    dir.x = to->x - from->x;
    dir.y = to->y - from->y;
    dir.z = to->z - from->z;
    x = dir.x;
    y = dir.y;
    z = dir.z;
    ROT_Y(-rot[1], x, z);
    ROT_X(-rot[0], y, z);
    ROT_Z(-rot[2], x, y);
    dir.x = x;
    dir.y = y;
    dir.z = z;
    do {
        ang[1] = RAD_TO_ANG(atan2f(-x, -z));
        ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    ulMatrixPush();
    matrix_unit_set(0);
    yuki_matrix_set(rot);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    model_camera_angles(rot);
    ulMatrixPop();
}

/* Aims a copy (out), then limits its roll against rot to +-limit via out[2]. */
/* 0x00175d00 */
static void yuki_aim_limited(Vec3 *to, Vec3 *from, int *rot, int *out, int limit)
{
    float x, y, z;
    int roll;

    x = 0.0f;
    y = 1.0f;
    z = 0.0f;
    out[0] = rot[0];
    out[1] = rot[1];
    out[2] = rot[2];
    yuki_draw_aimed(to, from, out);
    ROT_Z(out[2], x, y);
    ROT_X(out[0], y, z);
    ROT_Y(out[1], x, z);
    ROT_Y(-rot[2], x, z);
    ROT_X(-rot[1], y, z);
    ROT_Z(-rot[0], x, y);
    roll = ANG_90 - (int)RAD_TO_ANG(atan2f(y, x));
    if (roll > limit)
        out[2] -= roll - limit;
    else if (roll < -limit)
        out[2] += -roll - limit;
}

/* The shorter way round. */
/* 0x00175f20 */
void yuki_lerp3(int *out, int *from, int *to, float rate)
{
    out[0] = ang_approach(to[0], from[0], rate);
    out[1] = ang_approach(to[1], from[1], rate);
    out[2] = ang_approach(to[2], from[2], rate);
}

/* Not descending below flag 0x10. */
/* 0x00175fa0 */
int yuki_node_count(YNode *node, unsigned int mask, int count)
{
    do {
        if (node->flags & mask)
            count++;
        if (!(node->flags & 0x10))
            count = yuki_node_count(node->child, mask, count);
        node = node->next;
    } while (node);
    return count;
}

/*
 * Animated transforms; flagged positions to *out (advancing), up to count. Returns the count
 * left.
 */
/* 0x00176008 */
static int yuki_node_positions(Vec3 **out, YNode *node, unsigned int mask, int count)
{
    do {
        ulMatrixPush();
        anim_apply_node(node);
        if (node->flags & mask) {
            ulMatrixGetTranslation(ulMatrixCur, *out);
            (*out)++;
            if (--count <= 0) {
                ulMatrixPop();
                return 0;
            }
        }
        if (!(node->flags & 0x10)) {
            count = yuki_node_positions(out, node->child, mask, count);
            if (count <= 0) {
                ulMatrixPop();
                return 0;
            }
        }
        ulMatrixPop();
        node = node->next;
    } while (node);
    return count;
}

/* At its current animation frame. */
/* 0x001760e8 */
void yuki_model_positions(Vec3 *out, YModel *model, unsigned int mask, int count)
{
    anim_start(model->motion, model->time);
    yuki_node_positions(&out, model->root, mask, count);
}

/* Placed at pos with angles rot, turned to face back. */
/* 0x00176148 */
void yuki_model_positions_at(Vec3 *out, Vec3 *pos, void **rot, YModel *model, unsigned int mask, int count)
{
    ulMatrixPush();
    matrix_unit_set(0);
    do { vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z); } while (0);
    yuki_matrix_set(rot);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    anim_start(model->motion, model->time);
    yuki_node_positions(&out, model->root, mask, count);
    ulMatrixPop();
}

/* Ray from the player along the camera forward turned by the player's pitch and yaw; ray_hits. */
/* 0x00176208 */
int yuki_camera_relative(int pos, int clip_ang, float t)
{
    int cam[4];
    Vec3 dir;
    int ang[4];

    player_get_pos(cam);
    player_get_rot(ang);
    player_camera_get_fwd(&dir);
    do {
        float s, c, vy = dir.y, vz = dir.z;
        ulSinCos(ang[0], &s, &c);
        dir.y = vy * c - vz * s;
        dir.z = vy * s + vz * c;
    } while (0);
    do {
        float s, c, vx = dir.x, vz = dir.z;
        ulSinCos(ang[1], &s, &c);
        dir.x = vx * c + vz * s;
        dir.z = -vx * s + vz * c;
    } while (0);
    return ray_hits(cam, &dir, clip_ang, pos, t);
}

/* 0..1 in a div-beat cycle; 0 while the clock has no tempo. */
/* 0x00176318 */
float yuki_beat_phase(int div)
{
    float phase;
    float inv;
    float elapsed;

    phase = beat_clock_get_interval();
    if (fabsf(phase) < FLT_EPSILON) {
        phase = 0;
    } else {
        elapsed = phase - beat_clock_get_frames_to_beat();
        inv = 1.0f / div;
        phase = inv * (elapsed / phase);
        phase += inv * (beat_clock_get_haku() % div);
    }
    return phase;
}

/*
 * col_from -> col_to by a div-beat phase (cosine unless linear). Alpha x scale x depth factor:
 * anear before znear, afar beyond zfar, linear between.
 */
/* steer(cast) */
/* 0x001763e0 */
unsigned int yuki_beat_color(int col_from, int col_to, int div, int linear, float scale, float znear,
                           float zfar, float anear, float afar)
{
    Vec3 origin;
    float s, c;
    float rate, z;
    int col;
    int al, r, g, bl;

    if (linear) {
        rate = yuki_beat_phase(div);
    } else {
        ulSinCos(yuki_beat_phase(div) * ANG_TURN, &s, &c);
        rate = (c + 1.0f) * 0.5f;
    }
    col = color_blend(col_from, col_to, rate);
    al = color_get_a(col) * scale;
    r = ((int (*)(int))color_get_r)(col);
    g = color_get_g(col);
    bl = color_get_b(col);
    ulMatrixGetTranslation(ulMatrixCur, &origin);
    z = -origin.z;
    if (zfar < z) {
        al = al * afar;
    } else if (z < znear) {
        al = al * anear;
    } else {
        rate = (z - znear) / (zfar - znear);
        al = al * (rate * (afar - anear) + anear);
    }
    if (al < 0)
        al = 0;
    if (al > 255)
        al = 255;
    return ((al + 1) >> 1) << 24 | (bl & 0xff) << 16 | (g & 0xff) << 8 | (r & 0xff);
}

/* Blocks up to end word 255; length in words -> *count. */
/* 0x001765e8 */
static void *yuki_data_save(int *src, int *count)
{
    int *cur;
    int n;
    short len;

    n = 0;
    cur = src;
    if (cur) {
        do {
            if (*cur == 255)
                break;
            len = ((unsigned short *)cur)[1] + 1;
            n += len;
            cur += len;
        } while (1);
        n++;
        cur = heap_block_tag(game_malloc(n * 4), "yuki_tool.c", 1798);
        nj_copy32(cur, src, n);
    }
    *count = n;
    return cur;
}

/* YSave tree of the same shape. */
/* 0x001766a8 */
YSave *yuki_save_create(YNode *node)
{
    YSave *save;
    YSave *top;

    top = heap_block_tag(game_malloc(16), "yuki_tool.c", 1815);
    save = top;
    while (1) {
        save->data = node->mesh ? yuki_data_save(node->mesh->data, &save->count) : 0;
        save->child = node->child ? yuki_save_create(node->child) : 0;
        if (node->next) {
            save->next = heap_block_tag(game_malloc(16), "yuki_tool.c", 1834);
            node = node->next;
            save = save->next;
        } else {
            save->next = 0;
            break;
        }
    }
    return top;
}

/* 0x00176780 */
void yuki_save_free(YSave *save)
{
    YSave *next;

    while (save) {
        next = save->next;
        if (save->data)
            heap_free(save->data, 0);
        if (save->child)
            yuki_save_free(save->child);
        heap_free(save, 0);
        save = next;
    }
}

/* Saved mesh data instead of the meshes' own, plus a blended pass with each mesh's texture. */
/* 0x001767f0 */
static void yuki_save_draw(YNode *node, YSave *save)
{
    YDrawPrm prm;
    YDrawPrm tex_prm;
    YMesh *mesh;
    int tex;

    do {
        mesh = node->mesh;
        ulMatrixPush();
        anim_apply_node(node);
        if (mesh) {
            tex = mesh->tex;
            if (save->data) {
                prm.data = save->data;
                prm.tex = 0;
                prm.center = mesh->center;
                prm.radius = mesh->radius;
                nj_cnk_vu1_draw(&prm);
            }
            if (tex) {
                draw_set_blend(4, 5);
                tex_prm.data = 0;
                tex_prm.tex = tex;
                tex_prm.center = mesh->center;
                tex_prm.radius = mesh->radius;
                nj_cnk_vu1_draw(&tex_prm);
            }
        }
        if (node->child)
            yuki_save_draw(node->child, save->child);
        ulMatrixPop();
        node = node->next;
        save = save->next;
    } while (node);
}

/* At its current animation frame. */
/* 0x001768f8 */
static void yuki_model_draw_saved(YModel *model, YSave *save)
{
    anim_start(model->motion, model->time);
    yuki_save_draw(model->root, save);
}

/* Into nodes flagged in mask; clears those bits. */
/* 0x00176938 */
void yuki_save_restore(YSave *save, YNode *node, unsigned int mask)
{
    do {
        if (node->flags & mask) {
            nj_copy32(node->mesh->data, save->data, save->count);
            node->flags &= ~mask;
        }
        if (node->child)
            yuki_save_restore(save->child, node->child, mask);
        node = node->next;
        save = save->next;
    } while (node);
}

/* 0x001769d0 */
static void yuki_matrix_set(void **rot)
{
    vu0_matrix_rotate_y(ulMatrixCur, rot[1]);
    vu0_matrix_rotate_x(ulMatrixCur, rot[0]);
    vu0_matrix_rotate_z(ulMatrixCur, rot[2]);
}
