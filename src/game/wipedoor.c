/*
 * Door wipe: the captured screen pulls back from a centre point to the edges, revealing the new
 * scene. A wipe_start transition type.
 */

#include "tvset.h"
#include "nj_draw.h"
#include "ul_gx.h"
#include "wipedoor.h"
#include "screen.h"
#include "task.h"
#include "color.h"

extern WipeDoor *wipedoor_work;
extern float wipedoor_center[2];
extern float wipedoor_points[5][2];
extern int wipedoor_desc;

/* 0x00173ca8 */
static void wipedoor_init(WipeDoor *w)
{
    w->state = 0;
    wipedoor_work = w;
}

/* 0x00173cb8 */
static void wipedoor_release(WipeDoor *w)
{
    wipedoor_work = 0;
}

/*
 * State 1 waits for two-pass rendering (capture texture), then closed (2); state 3 counts down,
 * ends at 0. The minimum closed time counts down every frame.
 */
/* 0x00173cc8 */
static void wipedoor_exec(WipeDoor *w)
{
    switch (w->state) {
    case 1:
        if (screen_get_pass_cnt() >= 2)
            w->state = 2;
        break;
    case 3:
        if (--w->count == 0)
            TV_TASK(w)->flags |= 1;
        break;
    }
    if (w->timer > 0)
        w->timer--;
}

/*
 * Opening: captured screen as a strip between wipedoor_points and those points pulled toward
 * wipedoor_center, inner edge moving out; UVs map the 1024x512 capture texture.
 */
/* 0x00173d50 */
static void wipedoor_disp(WipeDoor *w)
{
    TvVtxUV vtx[10];
    TvVtxUV *out;
    float *pt;
    float cx, cy, rate, x, y, inner_x, inner_y;
    int i;

    if (w->state != 3 || w->count <= 0)
        return;
    cx = wipedoor_center[0];
    cy = wipedoor_center[1];
    rate = (float)(w->frames - w->count) / (float)w->frames;
    out = &vtx[1];
    pt = wipedoor_points[0];
    for (i = 5; i != 0; i--) {
        x = pt[0];
        y = pt[1];
        out[-1].x = x;
        out[-1].y = y;
        out[-1].u = x * (1.0f / 1024);
        out[-1].v = y * (1.0f / 512);
        out[-1].z = 1.0f;
        out[-1].col = COL_WHITE;
        inner_x = cx + (x - cx) * rate;
        inner_y = cy + (y - cy) * rate;
        out[0].x = inner_x;
        out[0].y = inner_y;
        out[0].u = inner_x * (1.0f / 1024);
        out[0].v = inner_y * (1.0f / 512);
        out[0].z = 1.0f;
        out[0].col = COL_WHITE;
        out += 2;
        pt += 2;
    }
    ulGxSetTextureList(screen_get_tex_list());
    ulGxSelectTexture(ulGxGetTextureList(), 0);
    nj_tex_strip_draw(vtx, 10, 0);
}

/* Wipe type 4. */
/* 0x00173e88 */
static WipeDoor *wipedoor_create(void *parent)
{
    return task_create(parent, &wipedoor_desc, 20);
}

/* State 1, minimum closed time; renders through the capture texture (old setting kept). */
/* 0x00173eb0 */
static void wipedoor_close(WipeDoor *w, int timer)
{
    w->timer = timer;
    w->handle = screen_swap_render(1);
    w->state = 1;
}

/* State 2 and the minimum closed time over. */
/* 0x00173ee8 */
static int wipedoor_is_closed(WipeDoor *w)
{
    int closed = 0;

    if (w->state == 2)
        closed = w->timer == 0;
    return closed;
}

/* State 3; restores the kept render setting. */
/* 0x00173f08 */
static void wipedoor_open(WipeDoor *w, int n)
{
    w->state = 3;
    w->frames = n;
    w->count = n;
    screen_swap_render(w->handle);
}

/* 0x00173f28 */
static int wipedoor_is_open(WipeDoor *w)
{
    return w->count < 1;
}

/* 0x00173f38 */
void wipedoor_set_center(float x, float y)
{
    wipedoor_center[0] = x;
    wipedoor_center[1] = y;
}
