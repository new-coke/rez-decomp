/*
 * Ending: model scene camera (times right before the cuts at frames 160, 700, 920 hold the
 * previous frame) and the expanding rings as Ramses rises.
 */

#include "ending_shared.h"
#include "camera.h"
#include "color.h"
#include "pnb.h"
#include "task.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "end_circle.h"

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))

extern struct TaskDesc end_circle_desc;

/* camera task (76 bytes): camera motion in model file entries 3 and 4 */

extern char demo_camera_hooks[];
extern struct TaskDesc demo_camera_desc;

/* 0x00af79e0 */
void demo_camera_node_init(void)
{
}

/* 0x00af79e8 */
void demo_camera_node_update(Camera *out, DemoCamera *w)
{
    *out = w->cam;
}

/* 0x00af7a40 */
void demo_camera_node_release(void)
{
}

/* steer(cast) */
/* 0x00af7a48 */
void demo_camera_init(DemoCamera *w)
{
    Camera *cur = camera_get_current();

    w->motion = (void *)*((int *)pnb_get_entry(w->models, 3));
    w->target = (void *)*((int *)pnb_get_entry(w->models, 4));
    w->node = camera_node_add(2, demo_camera_hooks, w);
    w->cam = *cur;
    w->t = 0;
    w->dt = frame_scale;
    w->end = (float)*((unsigned int *)w->target + 1);
    w->done = 0;
}

/* 0x00af7b58 */
void demo_camera_release(DemoCamera *w)
{
    camera_node_remove(w->node);
}

/* 0x00af7b60 */
void demo_camera_exec(DemoCamera *w)
{
    float t = w->t;

    if (159.0f < t && t < 160.0f)
        t = 159.0f;
    if (699.0f < t && t < 700.0f)
        t = 699.0f;
    if (919.0f < t && t < 920.0f)
        t = 919.0f;
    camera_eval_motion(&w->cam, w->motion, w->target, t);
    w->t += w->dt;
    if (w->end - 1.0f < w->t) {
        w->t = w->end - 1.0f;
        w->done = 1;
    }
}

/* 0x00af7c60 */
void demo_camera_disp(void)
{
}

/* 0x00af7c68 */
void *demo_camera_create(void *parent)
{
    return task_create_inherit(parent, &demo_camera_desc, 76);
}

/* 0x00af7c90 */
int demo_camera_is_done(DemoCamera *w)
{
    return w->done;
}

/* 0x00af7c98 */
float demo_camera_get_remaining(DemoCamera *w)
{
    return w->end - w->t;
}

/* 0x00af7ca8 */
void end_circle_init(RingWork *w, RingArg *prm)
{
    task_set_dlist(w, 4);
    i3_copy(w->ang, prm->ang);
    v3_copy(&w->pos, prm->pos);
    w->count = 0;
}

/* 0x00af7d20 */
void end_circle_release(void)
{
}

/* 0x00af7d28 */
void end_circle_exec(RingWork *w)
{
    int life;

    w->count++;
    w->pos.x += frame_scale * 0.33333334f;
    life = time_scale * FPS + time_scale * 0.1f * FPS * 3.0f;
    if (w->count > life) {
        w->count = life;
        FINISH_IF(w);
    }
}

/* 0x00af7dc0 */
void end_circle_disp(RingWork *w)
{
    float sc[2];
    Vec3 *ring_pos;
    unsigned int col;
    float r, a;
    int i, age;

    for (i = 2, age = w->count; i != -1; i--) {
        if (age < time_scale * FPS) {
            r = age / (time_scale * FPS);
            ulSinCos((int)(r * ANG_QUARTER), &sc[0], &sc[1]);
            ulSinCos((int)(sc[0] * ANG_TURN - ANG_QUARTER), &sc[0], &sc[1]);
            a = (sc[0] + 1.0f) * 0.5f * 255.0f;
            col = ((int)MIN(MAX_GE(a, 0.0f), 255.0f) + 1) >> 1 << 24 | 0xff;
            r *= 30.0f;
            ulMatrixPush();
            ring_pos = &w->pos;
            do {
                Vec3 *p_ = ring_pos;
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            end_circle_rotate_yxz(w->ang);
            draw_ring_xz(r, col, 16);
            ulMatrixPop();
        }
        age -= time_scale * 0.1f * FPS;
        if (age < 0)
            break;
    }
}

/* 0x00af7fb8 */
void *end_circle_create(void *owner, RingArg *prm)
{
    return task_create_sibling_inherit_arg(owner, &end_circle_desc, 36, prm);
}

/* 0x00af7fe0 */
void end_circle_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
