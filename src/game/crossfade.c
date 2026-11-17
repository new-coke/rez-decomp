/*
 * Cross fade: CrossFade task holds the last picture as a full-screen quad and fades it over the new
 * scene. One at a time (cross_fade_work).
 */

#include "se_low.h"
#include "nj_draw.h"
#include "ps_gs.h"
#include "ul_gx.h"
#include "crossfade.h"
#include "node_draw_state.h"
#include "screen.h"
#include "task.h"

typedef struct CfVtx {
    float x, y, z, u, v;
    unsigned int col;
} CfVtx;

extern int *cross_fade_work;
extern CfVtx screen_fade_quad[4];
extern struct TaskDesc cross_fade_desc;

/* Display list 9. */
/* 0x0010f0d8 */
static void cross_fade_task_init(int *p)
{
    cross_fade_work = p;
    p[2] = 0;
    p[5] = 0;
    p[8] = 0;
    task_set_dlist(p, 9);
}

/* Restores screen_swap_render if the fade-out never did. */
/* 0x0010f0f8 */
static void cross_fade_task_release(int *p)
{
    if (p[8] == 0)
        screen_swap_render(p[7]);
    cross_fade_work = 0;
}

/* State 1 capturing -> 2 holding once drawn; 3 fading out ends at 0. Hold timer runs always. */
/* 0x0010f128 */
static void cross_fade_task_exec(int *w)
{
    switch (w[2]) {
    case 1:
        if (w[9] <= 0)
            w[2] = 2;
        break;
    case 3:
        if (--w[5] == 0) {
            if (w != 0)
                SND_TASK(w)->flags |= 1;
        }
        break;
    }
    if (w[4] > 0)
        w[4]--;
}

/* Fading out: alpha falls with the frames left; depth writes off, test mode restored after. */
/* 0x0010f198 */
static void cross_fade_task_disp(int *w)
{
    int save;
    int **tex_list;
    CfVtx *vtx;

    switch ((unsigned int)w[2]) {
    case 1:
        if (w[9] > 0)
            w[9]--;
    case 2:
    case 3:
        break;
    default:
        return;
    }
    if (w[5] <= 0)
        return;
    vtx = screen_fade_quad;
    save = draw_set_test_mode(0);
    vtx[0].col = vtx[1].col = vtx[2].col = vtx[3].col =
        ((((int)((float)(w[5] * 255) / (float)w[6]) + 1) >> 1) << 24) | 0x808080;
    tex_list = screen_get_tex_list();
    ulGxSetTextureList(tex_list);
    ulGxSelectTexture(ulGxGetTextureList(), 0);
    psGsWriteReg(6, *(unsigned long *)(*tex_list)[2] & 0xfffffffbffffffffUL);
    draw_set_blend(4, 5);
    psGsSetDepthTest(1, 1);
    psGsSetZMask(1);
    nj_tex_strip_draw(vtx, 4, 1);
    psGsSetDepthTest(1, 2);
    psGsSetZMask(0);
    draw_set_test_mode(save);
}

/* 0x0010f328 */
static void *cross_fade_task_create(void *parent)
{
    return task_create_inherit(parent, &cross_fade_desc, 40);
}

/* Sets screen_swap_render (old value saved); captures three frames, then holds timer frames. */
/* 0x0010f350 */
static void cross_fade_start(int *p, int timer)
{
    int x;

    p[4] = timer;
    x = screen_swap_render(1);
    p[7] = x;
    p[2] = 1;
    p[9] = 3;
}

/* Captured and hold timer run out. */
/* 0x0010f390 */
static int cross_fade_is_done(int *p)
{
    int done = 0;

    if (p[2] == 2)
        done = p[4] == 0;
    return done;
}

/* Restores screen_swap_render so the new scene renders underneath. */
/* 0x0010f3b0 */
static void cross_fade_out(int *p, int frames, int val)
{
    p[2] = 3;
    p[3] = val;
    p[6] = frames;
    p[5] = frames;
    screen_swap_render(p[7]);
    p[8] = 1;
}

/* 0x0010f3f8 */
static int cross_fade_is_free(void)
{
    return cross_fade_work == 0;
}
