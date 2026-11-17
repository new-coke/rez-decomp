/* Wire screen: one picture; fades in, stays ten seconds or until decide/START, fades out. */

#include "wire_shared.h"
#include "color.h"
#include "fade.h"
#include "fog.h"
#include "model.h"
#include "pad.h"
#include "pnb.h"
#include "system.h"
#include "ul_gx.h"
#include "task.h"

extern Pad *pad_main;

extern struct TaskDesc wire_desc;

void *wire_create(void *parent);

/* 12 bytes */
typedef struct WireWork {
    void *model;                /* wire_textures */
    int state;                  /* 0x04 */
    int timer;                  /* 0x08 */
} WireWork;

extern unsigned int system_flags;
extern unsigned int pad_decide_mask;
extern char wire_textures[];

/* steer(cast) */
/* 0x00af6700 */
void wire_init(WireWork *w)
{
    system_flags |= SYS_TITLE_LOOP;
    w->model = model_create_timed(wire_textures, 0xf700);
    w->state = 0;
    w->timer = 0;
    fog_set_enable(0);
    fog_set(COL_BLACK, 0, 1000.0f, 2000.0f);
    ((void (*)(unsigned int))BackColReq)(COL_BLACK);
    fade_start(0, 1, 0);
    pad_input_disable(0);
}

/* 0x00af6798 */
void wire_release(WireWork *w)
{
    model_list_remove(w->model);
}

/* 0x00af67a0 */
void wire_exec(WireWork *w)
{
    int pressed = 0;

    if (pad_main->menu.trig & (pad_decide_mask | 8))
        pressed = 1;

    switch (w->state) {
    case 0:
        pad_input_enable(0);
        fade_start_out(60);
        w->state++;
        break;
    case 1:
        if (fade_is_idle() == 1) {
            w->state = 100;
            w->timer = 0;
        }
        break;
    case 100:
        if (pressed)
            w->state = 200;
        if (600.0f <= (float)w->timer++)
            w->state = 200;
        break;
    case 200:
        fade_start(0, 1, 60);
        pad_input_disable(0);
        w->state++;
        break;
    case 201:
        if (fade_is_idle() == 1 && w)
            FINISH(w);
        break;
    }
}

/* 0x00af6900 */
void wire_disp(WireWork *w)
{
    ulGxSetTextureList(w->model);
    ulGxSetTexFilter(1);
    screen_image_draw(0, COL_WHITE, 1.0f);
}

/* 0x00af6938 */
void *wire_create(void *parent)
{
    return task_create(parent, &wire_desc, 12);
}
