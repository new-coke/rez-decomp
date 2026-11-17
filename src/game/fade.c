/*
 * Screen fade: full-screen colour blend (white flash, black, held tint) over n frames. Drawn as
 * four triangles around the centre so corners and centre can fade at different times.
 */

#include "types_game.h"
#include "task.h"
#include "fade.h"
#include "color.h"
#include "node_draw_state.h"
#include "screen_lens.h"
#include "system.h"

extern FadeWork *fade_work;

extern struct {
    float x;
    float y;
} screen_layout;

extern int system_flags;
extern int (*fade_kind_funcs[])(FadeWork *f);
extern void (*fade_mode_funcs[])(FadeWork *f);
extern void ulGxBeginStrip(int a);
extern void ulGxAddStrip(FadePoint *p, int n);
extern void ulGxEndStrip(FadePoint *p);
extern struct TaskDesc fade_desc;

static void fade_set_mode(int mode);
static int fade_kind_linear(FadeWork *w);

/* Starts idle (state 3), speed 0.4; display list 13. */
/* 0x00115588 */
static void fade_init(FadeWork *w)
{
    int i;
    FadePoint *pt2, *pt4, *pt7, *pt8, *pt10, *pt11;

    fade_work = w;
    w->state = 3;
    w->dir = 1;
    w->speed = 0.4f;
    w->len = 0;
    w->cur = 0;
    w->prev_color = 0;
    w->color = 0;
    w->mode = 0;
    w->col_mode = 0;
    w->speed_kind = 0;
    w->flag = 0;
    {
        int j;

        for (j = 11; j >= 0; j--)
            w->kind[j] = 0;
    }
    w = fade_work;
    for (i = 0; i < 4; i++) {
        FadePoint *p = &w->v[i * 3];
        p->x = screen_layout.x * 0.5f;
        p->y = screen_layout.y * 0.5f;
        pt8 = &w->v[8];
        pt10 = &w->v[10];
        pt11 = &w->v[11];
        pt4 = &w->v[4];
        pt7 = &w->v[7];
        pt2 = &w->v[2];
    }
    w->v[1].x = w->v[8].x = w->v[10].x = w->v[11].x = 0.0f;
    w->v[2].x = w->v[4].x = w->v[5].x = w->v[7].x = screen_layout.x;
    w->v[1].y = pt2->y = pt4->y = pt11->y = 0.0f;
    w->v[5].y = pt7->y = pt8->y = pt10->y = screen_layout.y;
    {
        FadePoint *p = w->v;
        for (i = 0; i < 12; i++)
            p[i].w = 1.0f / fade_work->speed;
    }
    task_set_dlist(fade_work, 13);
    scr_saver_setup();
}

/* 0x00115710 */
static void fade_release(void)
{
}

/* Frozen while system_flags bit 0 or 1 is set, unless w->flag. */
/* 0x00115718 */
static void fade_exec(FadeWork *w)
{
    int i;

    if (w->flag == 0) {
        if (system_flags & (SYS_PAUSE_KBD | SYS_PAUSED))
            return;
    }
    if (w->len == 0)
        return;
    w->cur++;
    if (w->len < w->cur) {
        w->cur = w->len;
        if (w->state < 3) {
            w->state++;
        } else if (w->dir == 0) {
            w->color = 0;
            fade_reset();
            return;
        }
    }
    for (i = 0; i < 12; i++)
        w->v[i].val = fade_kind_funcs[w->kind[i]](w);
    fade_mode_funcs[w->mode](w);
}

/* 0x00115848 */
static void fade_disp(FadeWork *w)
{
    FadePoint *p;

    if (w->len != 0) {
        draw_set_blend(w->blend_a, w->blend_b);
        ulGxBeginStrip(1);
        p = w->v;
        do {
            ulGxAddStrip(p, 3);
            p += 3;
        } while ((int)p < (int)(w->v + 12));
        ulGxEndStrip(p);
    }
}

/* 0x001158c8 */
int fade_create(void *parent)
{
    return task_create_sibling(parent, &fade_desc, 296);
}

/*
 * n < 0 fades back out. mode: 0 light grey, 1 black, 2 black closing from the corners, 3 keep
 * colour, ignore freeze. speed: 0 = 0.4/list 13, 1 = 0.9/list 13, 2 = 1.1/list 12.
 */
/* 0x001158f0 */
void fade_start(int speed, int mode, int n)
{
    FadeWork *w = fade_work;
    int i;

    if (n < 0) {
        fade_start_out(-n);
        return;
    }
    if (n <= 0)
        n = 1;
    if (!fade_is_idle())
        fade_work->color = fade_kind_linear(fade_work);
    fade_work->len = n;
    fade_work->cur = 0;
    fade_work->dir = 1;
    fade_work->state = 0;
    fade_work->col_mode = mode;
    fade_work->prev_color = fade_work->color;
    switch (mode) {
    case 0:
        fade_set_mode(0);
        fade_work->color = RGBA(0xc0, 0xc0, 0xc0, 0x80);
        TASK_OF(fade_work)->flags &= ~TASK_F_RUN_PAUSED;
        fade_work->blend_a = 4;
        fade_work->blend_b = 5;
        fade_work->flag = 0;
        break;
    case 1:
        fade_set_mode(0);
        fade_work->color = COL_BLACK;
        TASK_OF(fade_work)->flags &= ~TASK_F_RUN_PAUSED;
        fade_work->blend_a = 4;
        fade_work->blend_b = 5;
        fade_work->flag = 0;
        break;
    case 3:
        fade_set_mode(0);
        fade_work->blend_a = 4;
        fade_work->blend_b = 5;
        fade_work->flag = 1;
        TASK_OF(fade_work)->flags |= TASK_F_RUN_PAUSED;
        break;
    case 2:
        fade_work->color = COL_BLACK;
        fade_set_mode(1);
        TASK_OF(fade_work)->flags &= ~TASK_F_RUN_PAUSED;
        fade_work->blend_a = 4;
        fade_work->blend_b = 5;
        fade_work->flag = 0;
        break;
    }
    switch (speed) {
    case 0:
        fade_work->speed = 0.4f;
        task_set_dlist(fade_work, 13);
        break;
    case 1:
        fade_work->speed = 0.9f;
        task_set_dlist(fade_work, 13);
        break;
    case 2:
        fade_work->speed = 1.1f;
        task_set_dlist(fade_work, 12);
        break;
    }
    for (i = 0; i < 12; i++)
        w->v[i].w = 1.0f / fade_work->speed;
    fade_exec(fade_work);
}

/* n < 0 restarts the fade in with the previous speed and mode. */
/* 0x00115bc0 */
void fade_start_out(int n)
{
    if (n < 0) {
        fade_start(fade_work->speed_kind, fade_work->col_mode, -n);
        return;
    }
    if (n <= 0)
        n = 1;
    if (!fade_is_idle())
        fade_work->color = fade_kind_linear(fade_work);
    fade_work->len = n;
    fade_work->cur = 0;
    fade_work->dir = 0;
    fade_work->prev_color = 0;
    fade_work->state = 0;
    fade_exec(fade_work);
}

/* Held fade (mode 3) to black at opacity 1 - t, t in 0..1. */
/* 0x00115c60 */
void fade_start_alpha(float t, int speed, int frames)
{
    unsigned int alpha = (int)((1.0f - t) * 255.0f) & 0xff;

    fade_start(speed, 3, frames);
    fade_work->color = ((alpha + 1) >> 1) << 24;
    fade_exec(fade_work);
}

/* 0x00115cc8 */
int fade_is_idle(void)
{
    return fade_work->state == 3;
}

/* 0x00115ce0 */
void fade_reset(void)
{
    FadeWork *w = fade_work;

    w->state = 3;
    w->len = 0;
    w->cur = 0;
    w->color = 0;
    w->flag = 0;
    TASK_OF(w)->flags &= ~TASK_F_RUN_PAUSED;
}

/* 0x00115d18 */
static void fade_skip_to_end(void)
{
    FadeWork *w = fade_work;

    w->cur = w->len;
    fade_exec(w);
}

/* Group 0: the four centre points; 1..4: the corners. */
/* 0x00115d30 */
static void fade_set_kind(unsigned int group, int k)
{
    int *p = fade_work->kind;

    switch (group) {
    case 0:
        p[9] = k;
        p[6] = k;
        p[3] = k;
        p[0] = k;
        break;
    case 1:
        p[11] = k;
        p[1] = k;
        break;
    case 2:
        p[4] = k;
        p[2] = k;
        break;
    case 3:
        p[7] = k;
        p[5] = k;
        break;
    case 4:
        p[10] = k;
        p[8] = k;
        break;
    }
}

/* 0: whole screen evenly; 1: corners in the first half, centre in the second. */
/* 0x00115da8 */
static void fade_set_mode(int mode)
{
    switch (mode) {
    case 0:
        fade_set_kind(0, 0);
        fade_set_kind(1, 0);
        fade_set_kind(2, 0);
        fade_set_kind(4, 0);
        fade_set_kind(3, 0);
        fade_work->mode = 0;
        break;
    case 1:
        fade_set_kind(0, 2);
        fade_set_kind(1, 1);
        fade_set_kind(2, 1);
        fade_set_kind(4, 1);
        fade_set_kind(3, 1);
        fade_work->mode = mode;
        break;
    }
}

/* Kind 0: prev_color -> color over the whole fade. */
/* 0x00115e78 */
static int fade_kind_linear(FadeWork *w)
{
    int len = w->len;
    int prev = w->prev_color;
    int col = w->color;
    int t;

    if (w->dir == 1)
        t = w->cur;
    else
        t = len - w->cur;
    return color_blend(prev, col, (float)t / (float)len);
}

/* Kind 1: blends over the first half, then holds. */
/* 0x00115ed8 */
static int fade_kind_first_half(FadeWork *w)
{
    int len = w->len;
    double half = len / 2;
    int prev = w->prev_color;
    int col = w->color;
    double t;

    t = (w->dir == 1) ? w->cur : len - w->cur;
    if (t >= half)
        return col;
    return color_blend(prev, col, (float)((float)t / half));
}

/* Kind 2: clear for the first half, blends over the second. */
/* 0x00115fa8 */
static int fade_kind_second_half(FadeWork *w)
{
    int len = w->len;
    double half = len / 2;
    int prev = w->prev_color;
    int col = w->color;
    double u;

    u = ((w->dir == 1) ? w->cur : len - w->cur) - half;
    if (u <= 0)
        return 0;
    return color_blend(prev, col, (float)((float)u / half));
}

/* 0x00116088 */
static void fade_mode_plain_exec(void)
{
}

/* Mode hook for the centre wipe (entry 1 of fade_mode_funcs); does nothing. */
/* 0x00116090 */
static void fade_mode_wipe_exec(void)
{
}

/* Moves the fade task to display list n, if the fade exists. */
/* 0x00116098 */
static void fade_set_dlist(int n)
{
    if (fade_work != 0)
        task_set_dlist(fade_work, n);
}

/* 0x001160c0 */
FadeWork *fade_get_work(void)
{
    return fade_work;
}
