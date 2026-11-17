/* Option menu camera: holds the first frame, plays through once per option_camera_play. */

#include "title_shared.h"
#include "camera.h"
#include "pnb.h"
#include "task.h"
#include "title_option_camera.h"

extern char option_camera_hooks[];
extern struct TaskDesc option_camera_desc;

/* 0x00af9560 */
void option_camera_node_init(void)
{
}

/* 0x00af9568 */
void option_camera_node_update(Camera *out, CameraWork *w)
{
    *out = w->cam;
}

/* 0x00af95c0 */
void option_camera_node_release(void)
{
}

/* 0x00af95c8 */
void option_camera_init(CameraWork *w)
{
    Camera *cur = camera_get_current();

    w->motion = pnb_get_entry(w->models, 0)->src;
    w->target = pnb_get_entry(w->models, 1)->src;
    w->node = camera_node_add(2, option_camera_hooks, w);
    w->cam = *cur;
    w->dt = 0;
}

/* 0x00af9690 */
void option_camera_release(CameraWork *w)
{
    camera_node_remove(w->node);
}

/* 0x00af9698 */
void option_camera_exec(CameraWork *w)
{
    float end;

    if (w->dt) {
        end = (float)*((unsigned int *)w->target + 1) - 1.0f;
        w->t += 1.0f;
        if (end <= w->t) {
            w->t = 0;
            w->dt = 0;
        }
    }
    camera_eval_motion(&w->cam, w->motion, w->target, w->t);
}

/* 0x00af9728 */
void option_camera_disp(void)
{
}

/* 0x00af9730 */
void *option_camera_create(void *parent)
{
    return task_create_inherit(parent, &option_camera_desc, 68);
}

/* 0x00af9758 */
void option_camera_play(CameraWork *w)
{
    w->t = 0;
    w->dt = 1.0f;
}
