/*
 * Beat clock: "Sound Beat" counts video frames against the tempo, keeping beat and bar position and
 * four grids (beat, half, quarter, eighth) with a hit flag per step. Also the "MIDI_Beat" window.
 */

#include "beat_clock.h"
#include "task.h"
#include "mathdef.h"
#include "libc.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_window.h"
#include "nj_strip.h"
#include "sound_beat.h"
#include "debugmenu.h"
#include "float_bits.h"
#include "sound_main.h"

typedef struct {
    float x;
    float y;
    float z;
    int color;
} BeatDbgVertex;

extern struct {
    int pad0;
    int pad1;
    int fps;                    /* 50 (PAL) or 60; set by the TV-mode selection */
} game_state;

/* requested tempo */
extern float midi_beat_bpm;
extern int beat_clock_work;
/* "Sound Beat" */
extern const TaskDesc beat_clock_desc;

extern BeatGrid beat_grid[4];

/* "bpm %8.3f(%8.3f)" */
extern char str_beat_bpm_fmt[];

/* "Haku %5d div %d" */
extern char str_beat_haku_fmt[];

extern char str_beat_rest_fmt[];

extern void *beat_debug_win;

/* debug menu (initially "Info") */

/* "MIDI_Beat" */
extern char str_beat_midi_beat[];

extern struct {
    int pad[6];
    float x;                    /* 0x18 */
    int pad1c;
    float y;                    /* 0x20 */
} screen_layout;

extern struct GwVarNode *gw_varedit_root;

extern int midi_beat_vardef;
extern int midi_beat_vardefs;
extern int midi_beat_vars_group;

#ifndef VERSION_US

extern void beat_clock_init(BeatClock *w);
extern void beat_clock_disp(BeatClock *w);
extern void beat_clock_release(void);

#endif /* !VERSION_US */

#ifndef VERSION_US

const TaskDesc beat_clock_desc = {
    beat_clock_init, beat_clock_tick, beat_clock_disp, beat_clock_release, "Sound Beat"
};

#endif /* !VERSION_US */

static void beat_clock_init(BeatClock *w);
static void beat_clock_release(void);
static void beat_clock_disp(BeatClock *w);
static void beat_debug_register(void);
static void beat_debug_unregister(void);

/*
 * Frames per beat = fps * 60 / bpm; half, quarter, eighth for the other grids. Counted per video
 * frame, so 50/60 Hz dependent.
 */
/* 0x00164fe0 */
static void beat_clock_set_tempo(BeatClock *clk)
{
    float fps = game_state.fps;
    float t;

    clk->bpm = midi_beat_bpm;
    t = (double)fps * 60.0 / clk->bpm;
    clk->interval[0] = t;
    clk->interval[1] = t * 0.5f;
    clk->interval[2] = t * 0.5f * 0.5f;
    clk->interval[3] = t * 0.5f * 0.5f * 0.5f;
}

/* 0x00165078 */
static void beat_grid_set_hit(int hit)
{
    int i;

    for (i = 3; i >= 0; i--)
        beat_grid[i].hit = hit;
}

/* Sets every grid's hit flag; registers the debug window and variables. */
/* 0x001650b0 */
static void beat_clock_init(BeatClock *w)
{
    (*(BeatClock * *)&beat_clock_work) = w; /* steer(cast) */
    w->bpm = 0;
    w->div = 0;
    w->interval[0] = 0;
    w->interval[1] = 0;
    w->interval[2] = 0;
    w->interval[3] = 0;
    w->rest[0] = 0;
    w->rest[1] = 0;
    w->rest[2] = 0;
    w->rest[3] = 0;
    w->ratio[0] = 0;
    w->ratio[1] = 0;
    w->ratio[2] = 0;
    w->ratio[3] = 0;
    w->bar = 0;
    w->haku = 0;
    beat_grid_set_hit(1);
    beat_debug_register();
}

/* 0x00165118 */
static void beat_clock_release(void)
{
    beat_debug_unregister();
    beat_clock_work = 0;
}

/*
 * Per video frame. New beat: counts it, restarts grids, takes a changed tempo; else steps the
 * eighth counter, raising hit flags of grids starting a step. Then positions and the bar.
 */
/* 0x00165140 */
void beat_clock_tick(BeatClock *w)
{
    int threshold;

    w->rest[0] += 1.0f;
    w->rest[1] += 1.0f;
    w->rest[2] += 1.0f;
    w->rest[3] += 1.0f;

    if (w->interval[0] <= w->rest[0]) {
        w->rest[1] = 0.0f;
        w->rest[0] -= w->interval[0];
        w->haku++;
        w->rest[2] = 0.0f;
        w->rest[3] = 0.0f;
        w->div = 0;
        if (midi_beat_bpm != w->bpm)
            beat_clock_set_tempo(w);
        beat_grid_set_hit(1);
    } else {
        threshold = w->interval[0] * (float)(w->div + 1) * 0.125f;
        beat_grid_set_hit(0);
        if (threshold <= w->rest[0]) {
            w->div++;
            if ((w->div & 3) == 0) {
                w->rest[1] = 0.0f;
                beat_grid[1].hit = 1;
            }
            if ((w->div & 1) == 0) {
                beat_grid[2].hit = 1;
                w->rest[2] = 0.0f;
            }
            beat_grid[3].hit = 1;
            w->rest[3] = 0.0f;
        }
    }

    /* Every grid tests interval[0] for zero, not its own interval. */
    if (!(fabsf(w->interval[0]) < FLT_EPSILON)) {
        w->ratio[0] = w->rest[0] / w->interval[0];
        beat_grid[0].remain = 1.0f - w->ratio[0];
    }
    if (!(fabsf(w->interval[0]) < FLT_EPSILON)) {
        w->ratio[1] = w->rest[1] / w->interval[1];
        beat_grid[1].remain = 1.0f - w->ratio[1];
    }
    if (!(fabsf(w->interval[0]) < FLT_EPSILON)) {
        w->ratio[2] = w->rest[2] / w->interval[2];
        beat_grid[2].remain = 1.0f - w->ratio[2];
    }
    if (!(fabsf(w->interval[0]) < FLT_EPSILON)) {
        w->ratio[3] = w->rest[3] / w->interval[3];
        beat_grid[3].remain = 1.0f - w->ratio[3];
    }
    w->bar = (float)(w->haku % 4) * 0.25f + w->ratio[0] * 0.25f;
}

/* 0x001653f8 */
static void beat_clock_disp(BeatClock *w)
{
}

/* Unless a clock exists; requests 140 bpm. Returns the work, or 0. */
/* 0x00165400 */
void *beat_clock_create(void *parent)
{
    void *w;

    if (beat_clock_work == 0) {
        w = task_create(parent, &beat_clock_desc, sizeof(BeatClock));
        beat_clock_request_tempo(140.0f);
        return w;
    }
    return 0;
}

/* Counters, positions and bar cleared; requested tempo taken; every grid's hit flag set. */
/* 0x00165458 */
void beat_clock_reset(void)
{
    BeatClock *clk = beat_clock_work;

    if (clk != 0) {
        clk->div = 0;
        clk->haku = 0;
        clk->rest[0] = 0;
        clk->rest[1] = 0;
        clk->rest[2] = 0;
        clk->rest[3] = 0;
        beat_clock_set_tempo(clk);
        clk->ratio[0] = 0;
        clk->ratio[1] = 0;
        clk->ratio[2] = 0;
        clk->ratio[3] = 0;
        clk->bar = 0;
        beat_grid_set_hit(1);
    }
}

/* Taken over on the next beat. */
/* 0x001654c8 */
void beat_clock_request_tempo(float bpm)
{
    if (beat_clock_work != 0)
        midi_beat_bpm = bpm;
}

/* 0x001654e8 */
float beat_grid_get_remain(int i)
{
    if (beat_clock_work == 0)
        return 0.0f;
    return beat_grid[i].remain;
}

/* 0 without a clock. */
/* 0x00165518 */
int beat_clock_get_haku(void)
{
    int haku = 0;

    if (beat_clock_work != 0)
        haku = (*(BeatClock * *)&beat_clock_work)->haku; /* steer(cast) */
    return haku;
}

/* Frames per beat; 0.0 without a clock. */
/* 0x00165538 */
float beat_clock_get_interval(void)
{
    float interval = 0.0f;

    if (beat_clock_work != 0)
        interval = (*(BeatClock * *)&beat_clock_work)->interval[0]; /* steer(cast) */
    return interval;
}

/* Whole frames; 0 without a clock. */
/* 0x00165558 */
int beat_clock_get_frames_to_beat(void)
{
    if (beat_clock_work != 0)
        return (*(BeatClock * *)&beat_clock_work)->interval[0] - (*(BeatClock * *)&beat_clock_work)->rest[0]; /* steer(cast) */
    return 0;
}

/* 0.0 without a clock. */
/* 0x00165590 */
float beat_clock_get_bpm(void)
{
    float bpm = 0.0f;

    if (beat_clock_work != 0)
        bpm = (*(BeatClock * *)&beat_clock_work)->bpm; /* steer(cast) */
    return bpm;
}

/* 0.0 without a clock. */
/* 0x001655b0 */
static float beat_clock_get_requested_tempo(void)
{
    if (beat_clock_work != 0)
        return midi_beat_bpm;
    return 0.0f;
}

/* 0x001655d8 */
int beat_grid_hit(int grid)
{
    if (beat_clock_work != 0)
        return beat_grid[grid].hit;
    return 0;
}

/* 0..1 within the beat; 0.0 without a clock. */
/* 0x00165608 */
float beat_clock_get_phase(void)
{
    float phase = 0.0f;

    if (beat_clock_work != 0)
        phase = (*(BeatClock * *)&beat_clock_work)->ratio[0]; /* steer(cast) */
    return phase;
}

/* 0..1 within the 4-beat bar; 0.0 without a clock. */
/* 0x00165628 */
float beat_clock_get_bar(void)
{
    float bar = 0.0f;

    if (beat_clock_work != 0)
        bar = (*(BeatClock * *)&beat_clock_work)->bar; /* steer(cast) */
    return bar;
}

/* Bars of the "MIDI_Beat" window. */
/* 0x00165648 */
static void beat_debug_fill_rect(float x, float y, float z, float width, float height, int color)
{
    BeatDbgVertex vtx[4];

    vtx[3].color = color;
    vtx[2].color = color;
    vtx[1].color = color;
    vtx[0].color = color;
    vtx[3].z = z;
    vtx[2].z = z;
    vtx[1].z = z;
    vtx[0].z = z;
    vtx[2].y = y;
    vtx[0].y = y;
    vtx[3].y = y + height;
    vtx[1].y = y + height;
    vtx[1].x = x;
    vtx[0].x = x;
    vtx[3].x = x + width;
    vtx[2].x = x + width;
    nj_color_strip_begin(0);
    nj_color_strip_vertices(vtx, 4);
    nj_color_strip_end();
}

/*
 * Tempos, six 80-pixel bars (bar, grid steps, beat left), counters, a line per grid. Bars in
 * screen coordinates, text in window coordinates.
 */
/* 0x001656c8 */
static void beat_debug_window(void *w)
{
    BeatClock *clk = beat_clock_work;
    int cw;
    int ch;
    int ox;
    int oy;
    int x;
    int y;
    int i;
    float fx;
    float z;
    float len;

    if (clk == 0)
        return;
    gw_get_cell_size(&cw, &ch);
    len = 80.0f;
    gw_win_printf(w, 4, 0, str_beat_bpm_fmt, clk->bpm, midi_beat_bpm);
    y = ch;
    gw_window_get_abs_pos(w, &ox, &oy);
    y++;
    x = ox + 4;
    fx = x;
    y += oy;
    z = float_add_bits16(1, gw_window_get_scale(w));
    beat_debug_fill_rect(fx, y, z, (1.0f - clk->bar) * len, 4.0f, 0x800000ff);
    y += 5;
    for (i = 0; i < 4; i++) {
        beat_debug_fill_rect(fx, y, z, len * beat_grid[i].remain, 4.0f, 0x80000000);
        y += 5;
    }
    beat_debug_fill_rect(fx, y, z, len * (1.0f - clk->ratio[0]), 4.0f, 0x80ff0000);
    y += 5;
    y -= oy;
    x -= ox;
    y++;
    gw_win_printf(w, x, y, str_beat_haku_fmt, clk->haku, clk->div);
    y += ch;
    gw_win_printf(w, x, y, str_beat_rest_fmt, clk->rest[0], clk->interval[0], clk->ratio[0] * 100.0f);
    y += ch;
    gw_win_printf(w, x, y, str_beat_rest_fmt, clk->rest[1], clk->interval[1], clk->ratio[1] * 100.0f);
    y += ch;
    gw_win_printf(w, x, y, str_beat_rest_fmt, clk->rest[2], clk->interval[2], clk->ratio[2] * 100.0f);
    y += ch;
    gw_win_printf(w, x, y, str_beat_rest_fmt, clk->rest[3], clk->interval[3], clk->ratio[3] * 100.0f);
}

/* 0x00165a08 */
static void beat_debug_window_set(void *w)
{
    beat_debug_win = w;
    menu_item_set_value(info_menu_name, str_beat_midi_beat, 1);
}

/* 0x00165a28 */
static void beat_debug_window_close(void)
{
    beat_debug_win = 0;
    menu_item_set_value(info_menu_name, str_beat_midi_beat, 0);
}

/* 0x00165a48 */
static void beat_debug_menu_toggle(void)
{
    int cw;
    int ch;
    void *w;

    if (beat_debug_win != 0) {
        gw_window_close(beat_debug_win);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    w = gw_window_open(str_beat_midi_beat, 95, screen_layout.x + 4.0f, screen_layout.y + 4.0f,
                     cw * 23, ch * 3 + 30, 0);
    if (w != 0) {
        beat_debug_window_set(w);
        gw_window_set_handlers(w, 0, beat_debug_window, beat_debug_window_close);
    }
}

/* Info menu item; variables in the sound entry. */
/* 0x00165b20 */
static void beat_debug_register(void)
{
    int parent;
    int group;

    menu_item_add(info_menu_name, str_beat_midi_beat, beat_debug_menu_toggle, 0, 0);
    parent = sound_get_varedit();
    group = gw_varedit_add(&gw_varedit_root, parent, &midi_beat_vardef);
    midi_beat_vars_group = group;
    gw_varedit_add_table(&gw_varedit_root, group, &midi_beat_vardefs);
}

/* 0x00165b98 */
static void beat_debug_unregister(void)
{
    int parent;

    menu_item_remove(info_menu_name, str_beat_midi_beat);
    parent = sound_get_varedit();
    gw_varedit_remove(&gw_varedit_root, parent, midi_beat_vars_group);
}
