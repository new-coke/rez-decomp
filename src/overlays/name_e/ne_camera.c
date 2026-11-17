/* Name entry screen: camera; entry view, then closing view once stored; holds the last frame. */

#include "name_e.h"
#include "camera.h"
#include "pnb.h"
#include "task.h"
#include "ne_camera.h"

extern struct TaskDesc demo_camera_desc;

/* 72 bytes */
typedef struct NeCam {
    void *models;
    char pad_04[4];
    Camera cam;                 /* 0x08 */
    void *node;                 /* 0x30 */
    void *motion;               /* 0x34 */
    void *target;               /* 0x38 */
    float t;                    /* 0x3c */
    float end;                  /* 0x40 */
    int screen;                 /* 0x44 */
} NeCam;

extern char demo_camera_hooks[];

/* 0x00af6a18 */
void demo_camera_node_init(void)
{
}

/* 0x00af6a20 */
void demo_camera_node_update(Camera *out, Cam *w)
{
    *out = w->cam;
}

/* 0x00af6a78 */
void demo_camera_node_release(void)
{
}

/* 0x00af6a80 */
void demo_camera_init(NeCam *w, int screen)
{
    Camera *cur;

    w->screen = screen;
    cur = camera_get_current();
    if (!screen) {
        w->motion = pnb_get_entry(w->models, 0)->src;
        w->target = pnb_get_entry(w->models, 1)->src;
    } else {
        w->motion = pnb_get_entry(w->models, 2)->src;
        w->target = pnb_get_entry(w->models, 3)->src;
    }
    w->node = camera_node_add(2, demo_camera_hooks, w);
    w->cam = *cur;
    w->t = 0;
    w->end = (float)*((unsigned int *)w->target + 1);
}

/* 0x00af6ba8 */
void demo_camera_release(NeCam *w)
{
    camera_node_remove(w->node);
}

/* 0x00af6bb0 */
void demo_camera_exec(NeCam *w)
{
    camera_eval_motion(&w->cam, w->motion, w->target, w->t);
    w->t += 1.0f;
    if (w->end <= w->t)
        w->t = w->end - 1.0f;
}

/* 0x00af6c10 */
void demo_camera_disp(void)
{
}

/* 0x00af6c18 */
void *demo_camera_create(void *parent, int screen)
{
    return task_create_inherit_arg(parent, &demo_camera_desc, 72, screen);
}
