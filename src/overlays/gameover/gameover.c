/*
 * Game over screen: types a message with a blinking cursor while "game over" fades in, then the
 * result message. A button press skips the wait.
 */

#include "types_common.h"
#include "task.h"
#include "color.h"
#include "fade.h"
#include "fog.h"
#include "libc.h"
#include "pad.h"
#include "result_message.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "system.h"
#include "ul_gx_prim.h"

/* no null test, unlike system.h's */
#undef TASK_F_END
#define TASK_END(w) do { FINISH(w); } while (0)

/* 32 bytes */
typedef struct GameoverWork {
    int delay;                  /* frames before the next character */
    int len;                    /* characters shown, up to 75 */
    int blink;                  /* 0..59: cursor shows below 31 */
    float fade;                 /* 0x0c: "game over" fades in over 240 frames */
    int wait;                   /* 0x10: frames before the screen may end; a press clears it */
    int state;                  /* 0x14 */
    void *msg;                  /* 0x18: result message task */
    int clear;                  /* 0x1c: clear the screen on the next draw */
} GameoverWork;

typedef struct GameoverVtx {
    float x, y, q;
    unsigned int color;
} GameoverVtx;

extern Pad *pad_main;
extern unsigned int pad_decide_mask;
extern GameState game_state;
extern ScreenCfg screen_cfg;
extern float screen_layout[];

extern void font_set(int id);

/* 75 characters */
extern char message[];

/* by language */
extern char *gameover_texts[];

extern struct TaskDesc gameover_desc;

void *gameover_create(void *parent);

/* 0x00af6700 */
void *module_entry(void *parent)
{
    return gameover_create(parent);
}

/* steer(cast) */
/* 0x00af6720 */
void gameover_init(GameoverWork *w)
{
    system_set_frame_rate(1);
    ((void (*)(unsigned int))fog_req_color)(COL_BLACK);
    BackColReq(COL_BLACK);
    beat_clock_request_tempo(120.0f);
    fog_set_enable(0);
    w->wait = 300;
    pad_input_enable(0);
    fade_start_out(0);
    w->clear = 1;
    w->msg = result_message_disp_create(w, 0);
}

/* 0x00af67a8 */
void gameover_release(void)
{
}

/* 0x00af67b0 */
void gameover_exec(GameoverWork *w)
{
    if (w->delay <= 0) {
        w->delay = 1;
        if ((unsigned int)w->len < 75) {
            w->blink = 0;
            w->len++;
            SoundRhythmSe_Common(5);
        }
    } else {
        w->delay--;
    }
    if (++w->blink >= 60)
        w->blink = 0;
    w->fade += 0.004166667f;
    if (1.0f <= w->fade)
        w->fade = 1.0f;
    if (pad_main->menu.trig & ((pad_cancel_mask | pad_decide_mask) | 8))
        w->wait = 0;
    if (w->wait > 0) {
        w->wait--;
    } else if (result_message_is_done(w->msg)) {
        switch (w->state) {
        case 0:
            fade_start(0, 1, 30);
            w->state++;
            break;
        case 1:
            if (fade_is_idle())
                TASK_END(w);
            break;
        }
    }
}

/* 0x00af6908 */
void gameover_disp(GameoverWork *w)
{
    GameoverVtx v[4];
    char *str;
    int n;
    void *fbuf;
    float x, y;
    int lang;
    float tx, ty;
    ScreenCfg *scr;

    if (w->clear) {
        w->clear = 0;
        v[0].y = v[2].y = v[0].x = v[1].x = 0.0f;
        v[2].x = v[3].x = screen_layout[0];
        v[1].y = v[3].y = screen_layout[1];
        v[0].q = v[1].q = v[2].q = v[3].q = 3.3333333f;
        v[0].color = v[1].color = v[2].color = v[3].color = COL_BLACK;
        ulGxBeginStrip(0);
        ulGxAddStrip(v, 4);
        ulGxEndStrip();
        return;
    }
    str = message;
    n = w->len;
    if (n > 0) {
        font_set(2);
        fbuf = font_begin_current(RGBA(0x5e, 0x33, 0x11, 0x80));
        x = screen_layout[6] + 0.0f;
        y = screen_layout[8] + 0.0f;
        do {
            if (*str == '\n') {
                y += 19.0f;
                x = screen_layout[6] + 0.0f;
            } else {
                fbuf = font_put_char_scaled(fbuf, x, y, 2.2222223f, *str, 1.0f);
                x += 12.0f;
            }
            n--;
            str++;
        } while (n);
        if (w->blink < 31)
            fbuf = font_put_char_scaled(fbuf, x, y, 2.2222223f, 127, 1.0f);
        font_end_current(fbuf);
    }
    lang = game_state.lang;
    str = gameover_texts[lang];
    scr = &screen_cfg;
    tx = scr->cx - (float)((strlen(str) - 1) * 36) * 0.5f;
    ty = screen_cfg.cy - 21.0f;
    n = color_blend(COL_BLACK, RGBA(0x5e, 0x33, 0x11, 0x80), w->fade);
    font_set(3);
    font_print(tx, ty, 2.2222223f, n, str);
}

/* 0x00af6be0 */
void *gameover_create(void *parent)
{
    return task_create(parent, &gameover_desc, 32);
}
