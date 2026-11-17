/*
 * In-game HUD: score window, beat-pulsed progress bar graph, overdrive cells, boss life gauge.
 * Windows slide with the console level; the stage sets the two colours.
 */

#include "types_game.h"
#include "types_gw.h"
#include "libc.h"
#include "gw_varedit.h"
#include "nj_strip.h"
#include "ul_gx.h"
#include "console.h"
#include "billboard.h"
#include "color.h"
#include "console_com.h"
#include "float_bits.h"
#include "line_batch.h"
#include "node_draw_state.h"
#include "player_status.h"
#include "sound_beat.h"
#include "system.h"
#include "task.h"
#include "screen.h"

typedef struct { char pad_00[8]; float x; float y; } ConLay;
typedef struct { float level; int state; float x; float y; float in_x; float out_x; float in_y; float out_y; } Console2CellsDispConLay;
typedef struct { char pad_00[40]; float x1; float y1; } ConLayView;
typedef struct { unsigned int level; int state; float x; float y; float in_x; float out_x; float in_y; float out_y; } ConLayLevelView;

extern ScrInfo screen_layout;
extern float console2_score_fade;
extern int pcolp;
extern int pcol;
extern char console2_score_uv[];
extern char str_console2_score[];
extern void font_set(int);
extern char console2_bar_bg_uv[];
extern char console2_bar_uv[];
extern float console2_cells_fade;
extern char console2_cells_uv[];
extern char console2_cell_uv[];
extern float console2_gauge_fade;
extern float boss_width;
extern float boss_rate;
extern float console2_gauge_rate;
extern int console_flag;
extern int boss_on;
extern int console_vars_group;
extern int scr_root;
extern char gw_varedit_root[];
extern GwVarDef console_vardef[];
extern GwVarDef console_vardefs[];
extern GwVarDef console_flag_vardef[];
extern GwVarDef console_flag_vardefs[];
extern unsigned int game_flags;
extern int console_disp_level;
extern struct TaskDesc console_desc;
extern unsigned int system_flags;
extern int system_ptm;
extern struct TaskDesc console2_desc;
extern ConLayLevelView console_windows[];
extern ConLayLevelView console_bar_window;
extern unsigned int console_level;

static int console2_task_create(void *parent);
static void console_layout_reset(void);
static void console_windows_update(void);
static float console_fade_step(float v, int up);

/* fill: pixels, at most width. Outline tinted by col; alpha fades the whole gauge. */
/* 0x0010b7b0 */
static void console2_boss_gauge_draw(float x, float y, float z, float width, float height, float fill, unsigned int col, float alpha)
{
    LineVtx vtx[4];
    float buf[4];
    float xr;
    float bottom;
    float line_bottom;
    float line_left;
    int n;
    int vc;
    int tex_col;
    int pkt;
    unsigned int rgb;

    n = color_get_a(col);
    tex_col = ((((int)(alpha * 255.0f) + 1) >> 1) << 24) | 0x808080;
    rgb = 0xffffff;
    rgb = col & rgb;
    col = rgb | ((((int)((float)n * alpha) + 1) >> 1) << 24);
    if (width < fill)
        fill = width;
    /* Never taken. */
    if ((col & 1) >> 1)
        nj_color_strip_vertices(vtx, rgb);
    /* tex_col's low bits are always 0, so height is 11.0f both times. */
    height = (float)(11 | (tex_col & 1));
    xr = x + width;
    vtx[0].x = x;
    bottom = y + height;
    vc = (((int)(alpha * 180.0f) + 1) >> 1) << 24;
    width = y + 1.0f;
    vtx[1].x = x;
    vtx[2].x = xr;
    vtx[0].col = vtx[1].col = vtx[2].col = vtx[3].col = vc;
    vtx[3].x = xr;
    vtx[0].y = width;
    vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = z;
    vtx[2].y = width;
    vtx[1].y = bottom + 1.0f;
    vtx[3].y = bottom + 1.0f;
    nj_color_strip_begin(1);
    nj_color_strip_vertices(vtx, 4);
    nj_color_strip_end();
    height = (float)(11 + (tex_col & 2));
    z = float_add_bits16(1, z);
    if (0.0f < fill) {
        buf[0] = 0.0f;
        buf[1] = 0.671875f;
        buf[2] = (fill + 0.0f) * 0.00390625f;
        buf[3] = 0.71484375f;
        tex_rect_draw_uv(0, tex_col, buf, x, width, z, fill, height);
    }
    /* Never taken. */
    if ((col & 2) >> 2)
        vtx[0].col = 0;
    z = float_add_bits16(1, z);
    line_left = x + 1.0f;
    line_bottom = bottom + 2.0f;
    pkt = line_set_color(col);
    pkt = line_add_seg(pkt, line_left - 1.0f, width, z, xr, width, z);
    pkt = line_add_seg(pkt, line_left, width, z, line_left, line_bottom, z);
    pkt = line_add_seg(pkt, xr, line_bottom, z, xr, width, z);
    pkt = line_add_seg(pkt, xr, line_bottom, z, line_left, line_bottom, z);
    line_flush(pkt);
}

/* Ring from the rectangle out to border bx, by: grey inside (col alpha * alpha), clear outside. */
/* 0x0010ba70 */
static void console2_draw_frame(float x, float y, float z, float width, float height, float bx, float by, float alpha, unsigned int col)
{
    LineVtx vtx[10];
    LineVtx *p;
    float out_left;
    float out_top;
    float right;
    float bottom;
    float out_right;
    float out_bottom;
    int out_col;
    int in_col;
    int i;

    p = vtx;
    for (i = 10; i != 0; i--, p++)
        p->z = z;
    out_left = x - bx;
    out_top = y - by;
    right = x + width;
    bottom = y + height;
    in_col = ((((int)((float)color_get_a(col) * alpha) + 1) >> 1) << 24) | 0x808080;
    out_col = col & 0xffffff;
    out_right = right + bx;
    out_bottom = bottom + by;
    p = &vtx[0];
    p->x = out_left;
    p->y = out_top;
    p->col = out_col;
    p = &vtx[1];
    p->x = x;
    p->y = y;
    p->col = in_col;
    p = &vtx[2];
    p->x = out_right;
    p->y = out_top;
    p->col = out_col;
    p = &vtx[3];
    p->x = right;
    p->y = y;
    p->col = in_col;
    p = &vtx[4];
    p->x = out_right;
    p->y = out_bottom;
    p->col = out_col;
    p = &vtx[5];
    p->x = right;
    p->y = bottom;
    p->col = in_col;
    p = &vtx[6];
    p->x = out_left;
    p->y = out_bottom;
    p->col = out_col;
    p = &vtx[7];
    p->x = x;
    p->y = bottom;
    p->col = in_col;
    p = &vtx[8];
    p->x = out_left;
    p->y = out_top;
    p->col = out_col;
    p = &vtx[9];
    p->x = x;
    p->y = y;
    p->col = in_col;
    draw_set_blend(4, 1);
    nj_color_strip_begin(1);
    nj_color_strip_vertices(vtx, 10);
    nj_color_strip_end();
}

/* Top right; backing while console2_score_fade is up. */
/* steer(cast) */
/* 0x0010bc98 */
static void console2_score_disp(float z)
{
    char buf[16];
    float x;
    float y;
    float text_x;
    float text_y;
    int col;
    int n;

    x = screen_layout.right - (*(ConLay *)&console_windows).x; /* steer(cast) */
    y = screen_layout.top + (*(ConLay *)&console_windows).y; /* steer(cast) */
    if (0.0f < console2_score_fade) {
        int frame_col = color_blend(pcolp, COL_WHITE, console2_score_fade);
        console2_draw_frame(x, y, z, 165.0f, 22.0f, 10.0f, 10.0f, console2_score_fade, frame_col);
        col = color_blend(pcol, COL_WHITE, console2_score_fade);
    } else
        col = pcol;
    draw_set_blend(4, 5);
    tex_rect_draw_uv(0, col, console2_score_uv, x, y, z, 165.0f, 22.0f);
    z = float_add_bits16(1, z);
    text_x = x + 64.0f;
    text_y = y + 1.0f;
    n = player_status_get_score();
    sprintf(buf, str_console2_score, n);
    font_set(2);
    font_print(text_x, text_y, z, 0x80237d23, buf);
}

/* Shifts hist[1..7] up; hist[1] decays 0.05/frame to 0, kicked to 0.5 each beat. */
/* 0x0010be30 */
static void console2_history_update(float *hist)
{
    int i;
    float *src = hist + 7;
    float *dst = hist + 8;
    float v;

    for (i = 7; i != 0; i--)
        *dst-- = *src--;
    v = hist[1] - 0.05f;
    if (v >= 0.0f)
        hist[1] = v;
    else
        hist[1] = 0.0f;
    if (beat_grid_hit(0))
        hist[1] = 0.5f;
}

/* Bottom left: eight cells 20 px apart, one bar per progress step, stretched by arr to bounce. */
/* 0x0010bec0 */
static void console2_bar_graph_disp(float *arr, float z)
{
    int i = 8;
    float x = screen_layout.left + console_bar_window.x;
    float y = screen_layout.bottom - console_bar_window.y;
    float xx;
    float yy;
    float height;
    int col;
    char *uv;

    draw_set_blend(4, 5);
    col = pcol;
    uv = console2_bar_bg_uv;
    xx = x;
    do {
        tex_rect_draw_uv(0, col, uv, xx, y, z, 17.0f, 13.0f);
        xx += 20.0f;
    } while (--i != 0);
    z = float_add_bits16(1, z);
    i = player_status_get_progress();
    if (i > 0) {
        float *p;
        float unit;

        p = arr + 1;
        uv = console2_bar_uv;
        unit = 11.0f;
        yy = y + 1.0f;
        xx = x + 1.0f;
        for (; i != 0; p++, i--) {
            height = unit * *p;
            tex_rect_draw_uv(0, COL_WHITE, uv, xx, yy - height, z, 15.0f, unit + (height + height));
            xx += 20.0f;
        }
    }
}

/* Bottom right: backing while console2_cells_fade; one cell per stored overdrive, 40 px apart. */
/* 0x0010c058 */
static void console2_cells_disp(float z)
{
    float x;
    float y;
    int col;
    float cell_x;
    float cell_y;
    int n;

    x = screen_layout.right - console_windows[1].x;
    y = screen_layout.bottom - console_windows[1].y;
    if (0.0f < console2_cells_fade) {
        int frame_col = color_blend(pcolp, COL_WHITE, console2_cells_fade);
        console2_draw_frame(x, y, z, 157.0f, 27.0f, 10.0f, 10.0f, console2_cells_fade, frame_col);
        col = color_blend(pcol, COL_WHITE, console2_cells_fade);
    } else
        col = pcol;
    draw_set_blend(4, 5);
    tex_rect_draw_uv(0, col, console2_cells_uv, x, y, z, 157.0f, 27.0f);
    z = float_add_bits16(1, z);
    n = player_status_get_overdrive();
    cell_x = x + 1.0f;
    cell_y = y + 15.0f;
    if (n > 0) {
        do {
            tex_rect_draw_uv(0, COL_WHITE, console2_cell_uv, cell_x, cell_y, z, 35.0f, 11.0f);
            cell_x += 40.0f;
        } while (--n != 0);
    }
}

/*
 * Top centre. console2_gauge_rate drains to boss_rate by 0.01/frame, never below it; boss_width in
 * pixels. Hidden while console2_gauge_fade is 0.
 */
/* 0x0010c208 */
static void console2_boss_gauge_disp(float z)
{
    float width;
    float gauge_w;
    float fill;
    float half;
    int col;
    float x;
    float y;
    float fade;

    draw_set_blend(4, 5);
    if (boss_rate < console2_gauge_rate)
        console2_gauge_rate = console2_gauge_rate - 0.01f;
    if (console2_gauge_rate < boss_rate)
        console2_gauge_rate = boss_rate;
    col = pcol;
    width = floorf(boss_width);
    gauge_w = floorf(width * console2_gauge_rate);
    fill = floorf(width * boss_rate);
    half = floorf(width * 0.5f);
    x = screen_cx - half;
    fade = console2_gauge_fade;
    y = screen_top + 10.0f;
    if (0.0f < fade)
        console2_boss_gauge_draw(x, y, z, width, gauge_w, fill, col, fade);
}

/* Creates the Console2 drawing task; registers the console variables. */
/* 0x0010c368 */
static void console_init(int *w)
{
    console_flag = 0;
    {
        int task = console2_task_create(scr_root);
        (*(float *)&console_level) = 0; /* steer(cast) */
        *w = task;
    }
    console_layout_reset();
    console2_cells_fade = 0;
    console2_score_fade = 0;
    boss_on = 0;
    console2_gauge_fade = 0;
    boss_width = 0;
    boss_rate = 0;
    console2_gauge_rate = 0;
    task_set_dlist(w, 13);
    console_vars_group = gw_varedit_add(gw_varedit_root, 0, console_vardef);
    gw_varedit_add_table(gw_varedit_root, console_vars_group, console_vardefs);
    gw_varedit_add_table(gw_varedit_root, gw_varedit_add(gw_varedit_root, console_vars_group, console_flag_vardef), console_flag_vardefs);
}

/* 0x0010c458 */
static void console_release(char **w)
{
    char *child;

    console_clear_home(0);
    child = *w;
    do {
        if (child)
            TASK_OF(child)->flags |= TASK_F_END;
    } while (0);
    gw_varedit_remove(gw_varedit_root, 0, console_vars_group);
}

/* 0x0010c4b0 */
static void console_exec(void)
{
}

/*
 * Level 0 while game_flags 0x10000 hides the console. Fades the boss gauge and the window backings
 * by their enable/status flags; slides the windows.
 */
/* 0x0010c4b8 */
static void console_disp(void)
{
    if (game_flags & 0x10000)
        console_level = 0;
    else
        console_level = console_disp_level;
    if (console_level != 0 && boss_on != 0) {
        console2_gauge_fade += 0.1f;
        if (console2_gauge_fade > 1.0f)
            console2_gauge_fade = 1.0f;
    } else {
        console2_gauge_fade -= 0.1f;
        if (console2_gauge_fade < 0.0f)
            console2_gauge_fade = 0.0f;
    }
    console_windows_update();
    console2_cells_fade = console_fade_step(console2_cells_fade, player_status_test_flags(4096));
    console2_score_fade = console_fade_step(console2_score_fade, player_status_test_flags(8192));
}

/* 0x0010c5b8 */
int console_task_create(void *parent)
{
    return task_create(parent, &console_desc, 4);
}

/* Task flag 8, display list 13. */
/* 0x0010c5e0 */
static void console2_init(void *w)
{
    char *base = (char *)w;
    TASK_OF(base)->flags |= TASK_F_RUN_PAUSED;
    task_set_dlist(w, 13);
}

/* 0x0010c5f8 */
static void console2_release(void)
{
}

/* Not while system_flags & 3. */
/* 0x0010c600 */
static void console2_exec(float *w)
{
    if ((system_flags & (SYS_PAUSE_KBD | SYS_PAUSED)) == 0)
        console2_history_update(w);
}

/*
 * Hidden by player status flag 0x80000000. Score only with game_flags 0x40. console_flag bits
 * 1, 2, 4, 8 hide score, bars, cells, gauge.
 */
/* steer(cast) */
/* 0x0010c628 */
static int console2_disp(void *w)
{
    if (((int (*)())player_status_test_flags)(0x80000000) == 0) {
        ulGxSetTextureList(system_ptm);
        if (game_flags & 0x40) {
            if ((console_flag & 1) == 0)
                ((void (*)(void *, float))console2_score_disp)(w, 2.2222223f); /* steer(cast) */
        }
        if ((console_flag & 2) == 0)
            console2_bar_graph_disp(w, 2.2222223f);
        if ((console_flag & 4) == 0)
            ((void (*)(void *, float))console2_cells_disp)(w, 2.2222223f); /* steer(cast) */
        if ((console_flag & 8) == 0)
            return ((int (*)(void *, float))console2_boss_gauge_disp)(w, 2.2222223f); /* steer(cast) */
    }
}

/* 0x0010c718 */
static int console2_task_create(void *parent)
{
    return task_create(parent, &console2_desc, 36);
}

/* pcolp: window backings, pcol: windows. */
/* 0x0010c740 */
void console_set_stage_colors(int col_a, int col_b)
{
    pcolp = col_a;
    pcol = col_b;
}

/* 0x0010c758 */
int console_get_pcolp(void)
{
    return pcolp;
}

/* 0x0010c768 */
int console_get_pcol(void)
{
    return pcol;
}

/* Level picks the windows shown. Returns the previous one. */
/* 0x0010c778 */
int console_set_disp_level(int level)
{
    int old = console_disp_level;
    console_disp_level = level;
    return old;
}

/* 0x0010c788 */
int console_get_disp_level(void)
{
    return console_disp_level;
}

/* 0x0010c798 */
void console_cell_pos_right(int n, float *px, float *py)
{
    float x = screen_layout.right - (*(ConLayView *)&console_windows).x1; /* steer(cast) */
    float y = screen_layout.bottom - (*(ConLayView *)&console_windows).y1; /* steer(cast) */
    *px = x + n * 40.0f;
    *py = y;
}

/* 0x0010c7e8 */
void console_cell_pos_left(int n, float *px, float *py)
{
    float x = screen_layout.left + console_bar_window.x;
    float y = screen_layout.bottom - console_bar_window.y;
    *px = x + n * 20.0f;
    *py = y;
}

/* Returns the previous setting. */
/* 0x0010c838 */
int console2_swap_boss_on(int on)
{
    int old = boss_on;
    boss_on = on;
    return old;
}

/* Pixels. */
/* 0x0010c848 */
void console2_set_boss_width(float v)
{
    boss_width = v;
}

/* Remaining life, 0..1. */
/* 0x0010c858 */
void console2_set_boss_rate(float v)
{
    boss_rate = v;
}

/* Slid-out positions. */
/* 0x0010c868 */
static void console_layout_reset(void)
{
    int i;
    Console2CellsDispConLay *win = console_windows;

    for (i = 3; i != 0; i--) {
        win->x = win->out_x;
        win->y = win->out_y;
        win->state = 0;
        win++;
    }
    console_bar_window.x = console_bar_window.out_x;
    console_bar_window.y = console_bar_window.out_y;
    console_bar_window.state = 0;
}

/* At most 12 px per call; 1 once there. */
/* 0x0010c8b8 */
static int console_slide_step(float *pos, float target)
{
    float cur = *pos;
    int arrived = 0;

    if (target < cur) {
        cur -= 12.0f;
        if (cur <= target) {
            cur = target;
            arrived = 1;
        }
    } else if (cur < target) {
        cur += 12.0f;
        if (target <= cur) {
            cur = target;
            arrived = 1;
        }
    } else
        arrived = 1;
    *pos = cur;
    return arrived;
}

/* state 2 while moving, 1 once in. */
/* 0x0010c920 */
static void console_window_slide_in(ConWin *w)
{
    int n;

    if (w->state != 1) {
        w->state = 2;
        n = 0;
        if (console_slide_step(&w->x, w->in_x))
            n = 1;
        if (console_slide_step(&w->y, w->in_y))
            n++;
        if (n == 2)
            w->state = 1;
    }
}

/* state 2 while moving, 0 once out. */
/* 0x0010c9a0 */
static void console_window_slide_out(ConWin *w)
{
    int n;
    if (w->state == 0)
        return;
    w->state = 2;
    n = 0;
    if (console_slide_step(&w->x, w->out_x) != 0)
        n = 1;
    if (console_slide_step(&w->y, w->out_y) != 0)
        n++;
    if (n == 2)
        w->state = 0;
}

/* In while its level <= console_level. Bar graph also slides out at the player's maximum level. */
/* 0x0010ca10 */
static void console_windows_update(void)
{
    int i;
    int is_max;
    ConLayLevelView *w = console_windows;

    for (i = 3; i != 0; i--) {
        if (w->level <= console_level)
            console_window_slide_in(w);
        else
            console_window_slide_out(w);
        w++;
    }
    is_max = player_status_is_max_level();
    w = &console_bar_window;
    if (w->level <= console_level && is_max == 0)
        console_window_slide_in(w);
    else
        console_window_slide_out(w);
}

/* In by 0.1 to 1, out by 0.02 to 0. */
/* 0x0010cad8 */
static float console_fade_step(float v, int up)
{
    if (up) {
        v += 0.1f;
        if (v >= 1.0f)
            v = 1.0f;
    } else {
        v -= 0.02f;
        if (v < 0.0f)
            v = 0.0f;
    }
    return v;
}
