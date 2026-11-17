/*
 * Scripted camera nodes: "Pass Camera" along a timeline camera path, and spring camera shakes
 * (OscilateAngle swings the look-at point, Oscilate moves the eye).
 */

#include "types_game.h"
#include "mathdef.h"
#include "libc.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "camera_demo.h"
#include "approach.h"
#include "camera.h"
#include "game.h"
#include "heap.h"
#include "pass.h"

#define ROT_X(ang, y, z) \
    do { \
        float rot_s, rot_c, rot_u = y, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        y = rot_u * rot_c - rot_v * rot_s; \
        z = rot_u * rot_s + rot_v * rot_c; \
    } while (0)

#define ROT_Y(ang, x, z) \
    do { \
        float rot_s, rot_c, rot_u = x, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        x = rot_u * rot_c + rot_v * rot_s; \
        z = -rot_u * rot_s + rot_v * rot_c; \
    } while (0)

#define ROT_Z(ang, x, y) \
    do { \
        float rot_s, rot_c, rot_u = x, rot_v = y; \
        ulSinCos(ang, &rot_s, &rot_c); \
        x = rot_u * rot_c - rot_v * rot_s; \
        y = rot_u * rot_s + rot_v * rot_c; \
    } while (0)

typedef struct {
    char *name;
    void (*f0)(void);
    void (*f1)(void);
    void (*f2)(void);
} PassDesc;

extern int pass_camera_symbol;
extern int pass_camera_node;
extern char str_pass_camera[];
extern char oscilate_angle_hooks[];
extern char oscilate_hooks[];

/* 0x00106e90 */
static void pass_camera_start(void)
{
}

/* 0x00106e98 */
static void pass_camera_end(void)
{
}

static inline void pc_vcopy2(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void pc_vsub(float *dst, float *src)
{
    float x = dst[0], y = dst[1], z = dst[2];
    float sx = src[0], sy = src[1], sz = src[2];

    dst[0] = x - sx;
    dst[1] = y - sy;
    dst[2] = z - sz;
}

static inline void pc_vset(float *dst, float x, float y, float z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

/*
 * While game time is inside pass_camera_symbol: up vector turned by the path's roll. Removes its
 * node once outside the path or the path is gone.
 */
/* 0x00106ea0 */
static void pass_camera_render(float *p)
{
    int handle;
    float cur;
    float eye[4];
    float look_at[4];
    int ang[3];

    handle = pass_find_by_symbol(pass_camera_symbol);
    if (handle < 0 || (cur = (float)(int)game_time_get(), cur < pass_table_start(handle)) || pass_table_end(handle) < cur) {
        camera_node_remove(pass_camera_node);
        pass_camera_node = 0;
    } else {
        pass_eval_part(handle, game_time_get(), &ang[2], eye, look_at);
        pc_vcopy2(p, eye);
        pc_vcopy2(p + 3, look_at);
        pc_vsub(look_at, eye);
        do {
            ang[1] = (int)RAD_TO_ANG(atan2f(-look_at[0], -look_at[2]));
            ang[0] = (int)RAD_TO_ANG(atan2f(look_at[1], sqrtf(look_at[0] * look_at[0] + look_at[2] * look_at[2])));
        } while (0);
        do {
            float one = 1.0f;

            pc_vset(p + 6, 0.0f, one, 0.0f);
        } while (0);
        ROT_X(ang[0], p[7], p[8]);
        ROT_Y(ang[1], p[6], p[8]);
        ROT_Z(ang[2], p[6], p[7]);
    }
}

/* Replaces any running pass camera. */
/* 0x00107140 */
static void pass_camera_register(int symbol)
{
    PassDesc desc = { str_pass_camera, pass_camera_start, pass_camera_render, pass_camera_end };

    if (pass_camera_node) {
        camera_node_remove(pass_camera_node);
        pass_camera_node = 0;
    }
    pass_camera_symbol = symbol;
    pass_camera_node = camera_node_add(2, &desc, 0);
}

/* damp is both stiffness and damping. */
/* 0x001071c8 */
static void oscilate_angle_ramp_init(int handle, Ramp *ramp, float offset, float damp)
{
    ramp->pos = offset;
    ramp->vel = 0.0f;
    ramp->k = damp;
    ramp->damp = damp;
    ramp->handle = handle;
}

/* 0x001071e0 */
static void oscilate_angle_release(int owner, int ramp)
{
    heap_free(ramp, 0);
}

/*
 * Per-frame hook: centre moved along up by the spring, up re-orthogonalized. Node removed once
 * position and velocity are within 0.001.
 */
/* 0x001071f0 */
static void oscilate_angle_init(int owner, Ramp *ramp)
{
    Camera *cam;
    float dir[4];
    float side[4];
    float view[4];
    float acc[4];
    float zero;

    cam = camera_get_current();
    zero = 0.0f;
    acc[0] = zero;
    spring_step_pos(ramp, &ramp->vel, acc, zero, ramp->k, ramp->damp);
    if (fabsf(ramp->pos) <= 0.001f && fabsf(ramp->vel) <= 0.001f) {
        camera_node_remove(ramp->handle);
        return;
    }
    vec3_normalize(dir, &cam->up.x);
    dir[0] *= ramp->pos;
    dir[1] *= ramp->pos;
    dir[2] *= ramp->pos;
    {
        Vec3 *look_at = &cam->center;
        look_at->x += dir[0];
        look_at->y += dir[1];
        look_at->z += dir[2];
        view[0] = look_at->x - cam->eye.x;
        view[1] = look_at->y - cam->eye.y;
        view[2] = look_at->z - cam->eye.z;
    }
    vec3_normalize(view, view);
    ulVec3Cross(side, &cam->up.x, view);
    ulVec3Cross(&cam->up.x, view, side);
}

/* Node id 4 (after all cameras). x: initial offset, y: spring constant. */
/* 0x00107368 */
static int oscilate_angle_create(float x, float y)
{
    void *ramp;
    int handle;

    ramp = heap_block_tag(game_malloc(20), "camera_demo.c", 237);
    handle = camera_node_add(4, oscilate_angle_hooks, ramp);
    oscilate_angle_ramp_init(handle, ramp, x, y);
    return handle;
}

/* Same as oscilate_angle_ramp_init. */
/* 0x001073f8 */
static void oscilate_ramp_init(int handle, Ramp *ramp, float offset, float damp)
{
    ramp->pos = offset;
    ramp->vel = 0.0f;
    ramp->k = damp;
    ramp->damp = damp;
    ramp->handle = handle;
}

/* 0x00107410 */
static void oscilate_release(int owner, int ramp)
{
    heap_free(ramp, 0);
}

/* As oscilate_angle_init, moving the eye instead of the centre. */
/* 0x00107420 */
static void oscilate_init(int owner, Ramp *ramp)
{
    Camera *cam;
    float dir[4];
    float side[4];
    float view[4];
    float acc[4];
    float zero;

    cam = camera_get_current();
    zero = 0.0f;
    acc[0] = zero;
    spring_step_pos(ramp, &ramp->vel, acc, zero, ramp->k, ramp->damp);
    if (fabsf(ramp->pos) <= 0.001f && fabsf(ramp->vel) <= 0.001f) {
        camera_node_remove(ramp->handle);
        return;
    }
    vec3_normalize(dir, &cam->up.x);
    dir[0] *= ramp->pos;
    dir[1] *= ramp->pos;
    dir[2] *= ramp->pos;
    {
        Vec3 *look_at = &cam->center;
        cam->eye.x += dir[0];
        cam->eye.y += dir[1];
        cam->eye.z += dir[2];
        view[0] = look_at->x - cam->eye.x;
        view[1] = look_at->y - cam->eye.y;
        view[2] = look_at->z - cam->eye.z;
    }
    vec3_normalize(view, view);
    ulVec3Cross(side, &cam->up.x, view);
    ulVec3Cross(&cam->up.x, view, side);
}

/* See oscilate_angle_create. */
/* 0x00107598 */
int oscilate_create(float x, float y)
{
    void *ramp;
    int handle;

    ramp = heap_block_tag(game_malloc(20), "camera_demo.c", 355);
    handle = camera_node_add(4, oscilate_hooks, ramp);
    oscilate_ramp_init(handle, ramp, x, y);
    return handle;
}

static inline void pc_vcopy(float *dst, float *src)
{
    float x = src[0], y = src[1], z = src[2];

    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}
