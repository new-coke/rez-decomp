/*
 * Warning boxes: a framed, centred message box: one-button warning, two-choice select box, or an
 * input-less info box closed by its caller.
 */

#include "tvset.h"
#include "color.h"
#include "debug.h"
#include "libc.h"
#include "ul_gx.h"
#include "warning.h"
#include "billboard.h"
#include "float_bits.h"
#include "node_draw_state.h"
#include "sound_midise.h"
#include "system.h"
#include "task.h"

typedef struct WarnUV {
    float u0;
    float v0;
    float u1;
    float v1;
} WarnUV;

extern TvGame game_state;
extern char **warning_texts[];
extern void font_set(int a);
extern Warning *warning_work;
extern unsigned int system_flags;
extern int warning_result;
extern unsigned int pad_decide_mask;
extern TvSys pad_system;
extern float screen_cfg[4];
extern int warning_lines[][4];
extern int system_ptm;
extern int warning_desc;

extern void *scr_root;

/* 15-pixel rows of a 512x512 page, as a 512x22 sprite; lines from 30 moved by auto_load_drive. */
/* 0x00173170 */
static void warning_draw_line(int n, float x, float y, float z)
{
    WarnUV uv;
    int old;

    if (n >= 30)
        n += auto_load_drive;
    uv.u0 = 0;
    uv.v0 = n * 15.0f * (1.0f / 512);
    uv.u1 = 0.64453125f;
    uv.v1 = (n + 1) * 15.0f * (1.0f / 512);
    ulGxSetTexFilter(1);
    old = draw_set_test_mode(0x18000);
    tex_rect_draw_uv(19, COL_WHITE, &uv, x, y, z, 512.0f, 22.0f);
    draw_set_test_mode(old);
}

/* Lines 19 units apart; '#' and '$' -> the language's pair from "1!1!2!2!3!3!4!4!". */
/* 0x00173260 */
static void warning_draw_text(float x, float y, float z, int n, int tbl)
{
    char buf[256];
    char *src;
    char *dst;
    char ch;
    char hash_ch;
    char dollar_ch;
    int font;

    font = game_state.font;
    src = warning_texts[tbl][n];
    hash_ch = *("1!1!2!2!3!3!4!4!" + game_state.lang * 2);
    dollar_ch = *("1!1!2!2!3!3!4!4!" + game_state.lang * 2 + 1);
    font_set(2);
    while (*src) {
        dst = buf;
        while (*src != 0 && *src != '\n') {
            ch = *src++;
            if (ch == '#')
                ch = hash_ch;
            else if (ch == '$')
                ch = dollar_ch;
            *dst++ = ch;
        }
        *dst = 0;
        font_print(x, y, z, font, buf);
        src++;
        y += 19.0f;
    }
}

/* Centred, whole pixels; frame: selection frame (sprite 18) underneath. */
/* 0x001733c0 */
static void warning_draw_icon(int id, int frame, float x, float y, float z)
{
    int col;
    int tex;
    void *uv;
    float width;
    float height;
    float px;
    float py;

    col = game_state.font;
    if (frame) {
        sprite_get(18, &tex, &uv, &width, &height);
        px = x - floorf(width * 0.5f);
        py = y - floorf(height * 0.5f);
        tex_rect_draw_uv(tex, col, uv, px, py, z, width, height);
    }
    z = float_add_bits16(1, z);
    sprite_get(id, &tex, &uv, &width, &height);
    px = x - floorf(width * 0.5f);
    py = y - floorf(height * 0.5f);
    tex_rect_draw_uv(tex, col, uv, px, py, z, width, height);
}

/* Runs while paused (task flag 8); display list 13. */
/* 0x00173530 */
static void warning_set(Warning *w, unsigned int id, int sel)
{
    w->id = id;
    TV_TASK(w)->flags |= 8;
    warning_work = w;
    w->sel = sel;
    task_set_dlist(w, 13);
}

/* 0x00173558 */
static void warning_release(Warning *w)
{
    warning_work = 0;
}

/*
 * System flag 4 cancels (result 0). Select boxes (19..31): pad moves the choice, decide stores
 * it. Info boxes (32 on): no input.
 */
/* 0x00173568 */
static void warning_exec(Warning *w)
{
    int old;
    int sel;
    unsigned int trig;

    if (system_flags & SYS_SOFT_RESET) {
        warning_result = 0;
        TV_TASK_END(w);
        return;
    }
#ifdef VERSION_US
    if (w->id < 32)
    if (w->id >= 19) {
#else
    if (w->id >= 16) {
#endif
        sel = w->sel;
        old = sel;
        trig = pad_system.port_rec->trig;
        if (trig & 0x40)
            sel = 1;
        if (trig & 0x80)
            sel = 0;
        if (old != sel) {
            w->sel = sel;
            SoundRhythmSe_Common(0);
        }
    }
#ifdef VERSION_US
    if (w->id < 32 && (pad_system.port_rec->trig & pad_decide_mask)) {
#else
    if (pad_system.port_rec->trig & pad_decide_mask) {
#endif
        warning_result = w->sel;
        SoundRhythmSe_Common(1);
#ifdef VERSION_US
        do {
            TV_TASK(w)->flags |= 1;
        } while (0);
#else
        TV_TASK(w)->flags |= 1;
#endif
    }
}

/*
 * 556x184 frame, centred; message (pre-rendered warning_lines[id] for table 0, else table text);
 * buttons: two choices (selected framed), one, or none.
 */
/* 0x00173650 */
static void warning_disp(Warning *w)
{
    int tbl;
    int *line;
    int i;
    float x, y, z, height;

    tbl = game_state.msg;
    draw_set_blend(4, 5);
    z = 3.3333333f;
    height = 184.0f;
    x = screen_cfg[2] - 278.0f;
    y = screen_cfg[3] - 92.0f;
    rect_draw_outline(x, y, z, 556.0f, height, game_state.tex, 0x64000000);
    x += 22.0f;
    y += 22.0f;
    z = float_add_bits16(2, z);
    ulGxSetTextureList(system_ptm);
    if (tbl == 0) {
        line = warning_lines[w->id];
        for (i = 4; i != 0; i--) {
            if (*line >= 0)
                warning_draw_line(*line, x, y, z);
            y += 22.0f;
            line++;
        }
    } else {
        warning_draw_text(x, y, z, w->id, tbl);
    }
    x = screen_cfg[2];
    y = screen_cfg[3] + height * 0.5f - 42.0f;
#ifdef VERSION_US
    if (w->id < 32) {
        if (w->id >= 19) {
            x -= 62.0f;
            warning_draw_icon(7, w->sel, x, y, z);
            warning_draw_icon(8, w->sel == 0, x + 124.0f, y, z);
        } else {
            warning_draw_icon(9, 1, x, y, z);
        }
    }
#else
    if (w->id >= 16) {
        x -= 62.0f;
        warning_draw_icon(7, w->sel, x, y, z);
        warning_draw_icon(8, w->sel == 0, x + 124.0f, y, z);
    } else {
        warning_draw_icon(9, 1, x, y, z);
    }
#endif
}

/* Sibling of parent. */
/* 0x001738b0 */
static Warning *warning_create(void *parent, unsigned int id, int sel)
{
    Warning *w = task_create_sibling(parent, &warning_desc, 8);

    if (w)
        warning_set(w, id, sel);
    return w;
}

/* Warning (below 19), select (19..31) or info (32 on), unless one is open. 1 if opened. */
/* 0x00173910 */
int warning_request(unsigned int id, int sel)
{
    if (warning_work && !(TV_TASK(warning_work)->flags & 1)) {
        DEBUG_PRINTF("Warning Cancel Another One! : %d\n", id);
        return 0;
    }
    if (warning_create(scr_root, id, sel)) {
#ifdef VERSION_US
        if (id >= 32)
            DEBUG_PRINTF("Info    Set! : %d\n", id - 32);
        else if (id >= 19)
            DEBUG_PRINTF("Select  Set! : %d\n", id - 19);
        else
            DEBUG_PRINTF("Warning Set! : %d\n", id);
#else
        if (id >= 16)
            DEBUG_PRINTF("Select  Set! : %d\n", id - 16);
        else
            DEBUG_PRINTF("Warning Set! : %d\n", id);
#endif
        return 1;
    }
    ERROR_PRINTF("Warning Error! : %d\n", id);
    return 0;
}

/* Unless already closing. */
/* 0x001739e8 */
void warning_cancel(void)
{
    Warning *w = warning_work;

    if (w && !(TV_TASK(w)->flags & 1))
        TV_TASK_END(w);
}

/* 1 when none is open or the current one is closing. */
/* 0x00173a18 */
int warning_is_done(void)
{
    int done = 0;

    if (warning_work == 0 || (TV_TASK(warning_work)->flags & 1))
        done = 1;
    return done;
}

/* 0 when cancelled. */
/* 0x00173a48 */
int warning_get_result(void)
{
    return warning_result;
}
