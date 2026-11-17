/*
 * Demo camera of the area select screens: plays a camera motion and its target from the model
 * list a frame at a time; reports when done and the frames left.
 */

#include "types_common.h"

#include "mathdef.h"
#include "types_overlays.h"
#include "camera.h"
#include "pnb.h"
#include "task.h"
#include "demo_camera.h"

/* camera motion in the model list (its target follows it) */
#if defined(OVERLAY_AREASEL)
#define DEMO_CAMERA_MOTION 1
#else
#define DEMO_CAMERA_MOTION 0
#endif

extern char demo_camera_hooks[];
extern struct TaskDesc demo_camera_desc;

/* 0x00ca17e8 areacho, 0x00ca1358 areasel */
void demo_camera_node_init(void)
{
}

/* 0x00ca17f0 areacho, 0x00ca1360 areasel */
void demo_camera_node_update(Camera *out, DemoCamera *w)
{
    *out = w->cam;
}

/* 0x00ca1848 areacho, 0x00ca13b8 areasel */
void demo_camera_node_release(void)
{
}

/* 0x00ca1850 areacho, 0x00ca13c0 areasel */
/* steer(cast) */
void demo_camera_init(DemoCamera *w)
{
    Camera *cur = camera_get_current();

    w->motion = (void *)*((int *(*)(void *, unsigned int))pnb_get_entry)(w->models, DEMO_CAMERA_MOTION);
    w->target = (void *)*((int *(*)(void *, unsigned int))pnb_get_entry)(w->models, DEMO_CAMERA_MOTION + 1);
    w->node = camera_node_add(2, demo_camera_hooks, w);
    w->cam = *cur;
    w->t = 0;
    w->dt = 0;
    w->end = (float)*((unsigned int *)w->target + 1);
    w->done = 0;
}

/* 0x00ca1958 areacho, 0x00ca14c8 areasel */
void demo_camera_release(DemoCamera *w)
{
    camera_node_remove(w->node);
}

/* 0x00ca1960 areacho, 0x00ca14d0 areasel */
void demo_camera_exec(DemoCamera *w)
{
    camera_eval_motion(&w->cam, w->motion, w->target, w->t);
    w->t += w->dt;
    if (w->end <= w->t) {
        w->done = 1;
        w->t = w->end - 1.0f;
    }
}

/* 0x00ca19d0 areacho, 0x00ca1540 areasel */
void demo_camera_disp(void)
{
}

/* 0x00ca19d8 areacho, 0x00ca1548 areasel */
void *demo_camera_create(void *parent)
{
    return task_create_inherit(parent, &demo_camera_desc, 76);
}

/* 0x00ca1a00 areacho, 0x00ca1570 areasel */
void demo_camera_play(DemoCamera *w)
{
    w->dt = 1.0f;
}

/* 0x00ca1a10 areacho, 0x00ca1580 areasel */
int demo_camera_is_done(DemoCamera *w)
{
    return w->done;
}

/* 0x00ca1a18 areacho, 0x00ca1588 areasel */
float demo_camera_get_remaining(DemoCamera *w)
{
    return w->end - w->t;
}
