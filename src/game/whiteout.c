/* White-out: fades a full-screen white quad in over n frames, holds, fades out, ends. */

#include "tvset.h"
#include "libc.h"
#include "ul_gx_prim.h"
#include "whiteout.h"
#include "node_draw_state.h"
#include "task.h"

extern WhiteOut *whiteout_work;
/* screen width, height */
extern float screen_layout[2];

extern TvVtx whiteout_vtx[4];

extern int whiteout_desc;

/* 0x00173a58 */
static void whiteout_init(WhiteOut *w)
{
    w->count = 0;
    whiteout_work = w;
    w->state = 0;
}

/* 0x00173a70 */
static void whiteout_release(WhiteOut *w)
{
    whiteout_work = 0;
}

/* State 1 counts up to frames, then holds (2); state 3 counts down, ends at 0. */
/* 0x00173a80 */
static void whiteout_exec(WhiteOut *w)
{
    switch (w->state) {
    case 1:
        if (++w->count >= w->frames)
            w->state = 2;
        break;
    case 3:
        if (--w->count == 0)
            TV_TASK_END(w);
        break;
    default:
        printf("Illigal switch whiteout.c : Exec()\n");
        break;
    }
}

/* Unless idle; opaque while held, alpha count / frames while fading. */
/* 0x00173af0 */
static void whiteout_disp(WhiteOut *w)
{
    int a = 0x80606060;
    float wd, ht;

    if (w->state == 0)
        return;
    wd = screen_layout[0];
    ht = screen_layout[1];
    whiteout_vtx[0].x = 0;
    whiteout_vtx[0].y = 0;
    whiteout_vtx[1].x = 0;
    whiteout_vtx[1].y = ht;
    whiteout_vtx[2].x = wd;
    whiteout_vtx[2].y = 0;
    whiteout_vtx[3].x = wd;
    whiteout_vtx[3].y = ht;
    switch (w->state) {
    case 2:
        a = 255;
        break;
    case 1:
    case 3:
        a = w->count * 255 / w->frames;
        break;
    default:
        printf("Illigal switch whiteout.c : Disp()\n");
        break;
    }
    draw_set_blend(4, 5);
    whiteout_vtx[0].col = whiteout_vtx[1].col = whiteout_vtx[2].col = whiteout_vtx[3].col =
        ((a + 1) >> 1) << 24 | 0xffffff;
    ulGxBeginStrip(1);
    ulGxAddStrip(whiteout_vtx, 4);
    ulGxEndStrip();
}

/* 0x00173c30 */
static WhiteOut *whiteout_create(void *parent)
{
    return task_create_inherit(parent, &whiteout_desc, 20);
}

/* State 1. */
/* 0x00173c58 */
static void whiteout_fade_in(WhiteOut *w, int n)
{
    w->frames = n;
    w->state = 1;
    w->count = 0;
}

/* State 2. */
/* 0x00173c70 */
static int whiteout_is_white(WhiteOut *w)
{
    return w->state == 2;
}

/* State 3; ends at count 0. */
/* 0x00173c80 */
static void whiteout_fade_out(WhiteOut *w, int n)
{
    w->frames = n;
    w->state = 3;
    w->count = n;
}

/* No whiteout task. */
/* 0x00173c98 */
static int whiteout_is_done(void)
{
    return whiteout_work == 0;
}
