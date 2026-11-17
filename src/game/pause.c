/*
 * Pause menu. "Pause1": Start pauses and runs the menu (vibration, up/down inversion, volumes,
 * resume, quit), warnings for a disconnected controller or new vibrator. "Pause2" draws it and
 * the quit blackout.
 */

#include "types_game.h"
#include "color.h"
#include "debug.h"
#include "libc.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "pause.h"
#include "billboard.h"
#include "config.h"
#include "fade.h"
#include "float_bits.h"
#include "game.h"
#include "node_draw_state.h"
#include "pad.h"
#include "peri_vib.h"
#include "sound_main.h"
#include "sound_midise.h"
#include "system.h"
#include "task.h"
#include "warning.h"

typedef struct Pause1Work {
    int state;
    int next;
} Pause1Work;

typedef struct Pause1WorkView {
    int state;
    int next;
    int sel;
    char pad_0c[4];
    int redraw;
    int vib_cnt;
} Pause1WorkView;

typedef struct Pause1State3ExecPause1Work {
    int state;
    int next;
    int sel;                /* +8 */
} Pause1State3ExecPause1Work;

/* steer(alias): the code that matched reads PadData through these member types; one view
 * shared by those sites (src/game/pause.c). */
typedef struct PadDataView {
    char pad_00[104];
    unsigned int raw_on;
    unsigned int flags;
} PadDataView;

typedef struct PadSystemPauseView {
    char pad_00[48];
    PadDataView *pad;
} PadSystemPauseView;

typedef struct Pause1State5ExecPause1Work {
    int state;
    int next;
    char pad_08[8];
    int redraw;
} Pause1State5ExecPause1Work;

#ifdef VERSION_US

/* Quit confirmation box. */
#define WARNING_QUIT_CONFIRM 21

#else

#define WARNING_QUIT_CONFIRM 18

#endif /* !VERSION_US */

typedef struct GameSysView {
    char pad_00[3064];
    int lang;               /* +3064 language index */
    char pad_bfc[28];
    int tex;                /* +3096 */
    int sprites;            /* +3100 */
} GameSysView;

typedef struct PauseVtx {
    float x, y, z;
    unsigned color;
} PauseVtx;

typedef struct Pause2TaskDispPause1Work {
    char pad_00[16];
    int redraw;
} Pause2TaskDispPause1Work;

/* steer(alias): the code that matched reads ScreenGeom through these member types; one view
 * shared by those sites (src/game/pause.c). */
typedef struct ScreenGeomView {
    float w;
    float h;
} ScreenGeomView;

typedef struct Pause1GetSelectionPause1Work {
    char pad_00[8];
    int sel;
} Pause1GetSelectionPause1Work;

extern void font_set(int id);
extern Pad *pad_main;
extern char *pause_vibration_names0[];
extern char *pause_vibration_names1[];
extern PadSystem pad_system;
extern char *pause_updown_names0[];
extern char *pause_updown_names1[];
extern Pause1WorkView *pause1_work;
extern unsigned int system_flags;
extern PauseItem pause_items[];
extern unsigned int pad_decide_mask;
extern char str_pause_decide[];
extern char str_pause_pause[];
extern void *pause2_work;
extern ScreenCfg screen_cfg;
extern void *system_ptm;
extern GameSysView game_state;
extern ScreenGeomView screen_layout;
extern char *system1_work;
extern char *scr_root;
extern struct TaskDesc pause1_desc;
extern struct TaskDesc pause2_desc;

static int pause1_get_selection(void);
static void pause2_set_state(int state);

/* Text centred on highlight sprite 18, drawn behind when draw is set. Returns the sprite width. */
/* 0x0014d048 */
static float pause_draw_label(float x, float y, float z, int draw, char *str)
{
    int tex;
    void *uv;
    float width;
    float height;
    int color = game_menu_color;
    float tx;
    float ty;

    sprite_get(18, &tex, &uv, &width, &height);
    if (draw != 0) {
        tx = x;
        ty = y - floorf(height * 0.5f);
        tex_rect_draw_uv(tex, COL_WHITE, uv, tx, ty, z, width, height);
    }
    z = float_add_bits16(1, z);
    tx = x + floorf(width * 0.5f - (float)(line_length(str) * 12) * 0.5f);
    ty = y - floorf(9.5f) - 1.0f;
    font_set(2);
    font_print(tx, ty, z, color, str);
    return width;
}

/* Frame sprite 4, n segments (sprites 0..n-1). */
/* 0x0014d198 */
static void pause_draw_bar(float x, float y, float z, int n)
{
    int tex;
    void *uv;
    float width;
    float height;
    int color = game_menu_color;
    int i;

    y = y + 8.0f;
    sprite_get(4, &tex, &uv, &width, &height);
    tex_rect_draw_uv(tex, color, uv, x, y - height, z, width, height);
    if (n > 0) {
        i = 0;
        do {
            sprite_get(i++, &tex, &uv, &width, &height);
            tex_rect_draw_uv(tex, color, uv, x, y - height, z, width, height);
            x = x + width;
        } while (--n != 0);
    }
}

/* State 4. */
/* 0x0014d2b8 */
static void pause1_enter_resume(Pause1Work *w)
{
    w->state = 0;
    w->next = 4;
}

/* State 5. */
/* 0x0014d2c8 */
static void pause1_enter_quit(Pause1Work *w)
{
    w->state = 0;
    w->next = 5;
}

/* 0x0014d2d8 */
static void pause_vibration_enter(void)
{
}

/* Left off, right on. */
/* 0x0014d2e0 */
static void pause_vibration_input(void)
{
    int nv = 0;
    int cur = ConfigGetVibration();
    Pad *pad = pad_main;

    if (!(pad->raw.trig & PAD_LEFT))
        nv = cur;
    if (pad->raw.trig & PAD_RIGHT)
        nv = 1;
    if (cur != nv) {
        SoundRhythmSe_Common(0);
        if (nv != 0)
            ConfigSetVibration(1);
        else
            ConfigSetVibration(0);
    }
}

/* Setting highlighted. */
/* 0x0014d358 */
static float pause_vibration_draw(float x, float y, float z)
{
    int setting = ConfigGetVibration();
    int lang = game_lang;
    float width;

    x = x + 201.0f;
    width = pause_draw_label(x, y, z, setting == 0, pause_vibration_names0[lang]);
    return pause_draw_label(x + width, y, z, setting, pause_vibration_names1[lang]);
}

/* 0x0014d408 */
static void pause_updown_enter(void)
{
}

/* Left normal, right inverted. */
/* 0x0014d410 */
static void pause_updown_input(void)
{
    int old = ConfigGetUpdown();
    PadSystem *pad_sys = &pad_system;

    if (pad_sys->pad[0]->raw.trig & PAD_LEFT)
        ConfigSetUpdown(0);
    if (pad_sys->pad[0]->raw.trig & PAD_RIGHT)
        ConfigSetUpdown(1);
    if (old != ConfigGetUpdown())
        SoundRhythmSe_Common(0);
}

/* Setting highlighted. */
/* 0x0014d4a0 */
static float pause_updown_draw(float x, float y, float z)
{
    int setting = ConfigGetUpdown();
    int lang = game_lang;
    float width;

    x = x + 201.0f;
    width = pause_draw_label(x, y, z, setting == 0, pause_updown_names0[lang]);
    return pause_draw_label(x + width, y, z, setting, pause_updown_names1[lang]);
}

/* 0x0014d550 */
static void pause_volseq_enter(void)
{
}

/* Cursor sound only when it changes (the config clamps). */
/* 0x0014d558 */
static void pause_volseq_input(void)
{
    int cur = ConfigGetVolSeq();
    int nv = cur - 1;
    Pad *pad = pad_main;

    if (!(pad->raw.trig & PAD_LEFT))
        nv = cur;
    if (pad->raw.trig & PAD_RIGHT)
        nv = nv + 1;
    if (nv != cur) {
        ConfigSetVolSeq(nv);
        if (ConfigGetVolSeq() != cur)
            SoundRhythmSe_Common(0);
    }
}

/* 0x0014d5d0 */
static void pause_volseq_draw(float x, float y, float z)
{
    int n = ConfigGetVolSeq();

    pause_draw_bar(x + 231.0f, y, z, n);
}

/* 0x0014d628 */
static void pause_vol_se_enter(void)
{
}

/* As pause_volseq_input. */
/* 0x0014d630 */
static void pause_vol_se_input(void)
{
    int cur = ConfigGetVolSe();
    int nv = cur - 1;
    Pad *pad = pad_main;

    if (!(pad->raw.trig & PAD_LEFT))
        nv = cur;
    if (pad->raw.trig & PAD_RIGHT)
        nv = nv + 1;
    if (nv != cur) {
        ConfigSetVolSe(nv);
        if (ConfigGetVolSe() != cur)
            SoundRhythmSe_Common(0);
    }
}

/* 0x0014d6a8 */
static void pause_vol_se_draw(float x, float y, float z)
{
    int n = ConfigGetVolSe();

    pause_draw_bar(x + 231.0f, y, z, n);
}

/* Returns 1 when, with vibration on, the first vibrator connects. */
/* 0x0014d700 */
static int pause1_update_count(Pause1WorkView *w)
{
    int started = 0;
    int cur = peri_vib_get_count();

    if (ConfigGetVibration() == 0) {
        w->vib_cnt = cur;
    } else {
        int old = w->vib_cnt;

        if (cur < old) {
            w->vib_cnt = cur;
        } else {
            if (old < cur) {
                if (old == 0)
                    started = 1;
            }
            w->vib_cnt = cur;
        }
    }
    return started;
}

/* Vibrator count starts high so connected ones do not count as new. */
/* 0x0014d770 */
static void pause1_task_init(Pause1WorkView *w)
{
    w->state = 0;
    w->next = 0;
    w->sel = 0;
    w->vib_cnt = 100;
    pause1_work = w;
    task_set_dlist(w, 13);
    w->redraw = 0;
}

/* 0x0014d7b8 */
static void pause1_task_release(void)
{
}

/*
 * State 0: pauses (sound off, SYS_PAUSED) for a new vibrator (state 1) or Start (state 3).
 * Not before game type 2 or while SYS_NO_PAUSE, SYS_DOOR_OPEN or SYS_SOFT_RESET blocks it.
 */
/* steer(cast) */
/* 0x0014d7c0 */
static void pause1_check_start(Pause1Work *w)
{
    int btn;
    int upd;

    btn = 0;
    if (pad_main->raw.trig & PAD_START)
        btn = 1;
    pad_is_connected(0);
    upd = pause1_update_count(w);
    if (((unsigned int (*)(void))game_type_get)() < 2)
        return;
    if (upd != 0) {
        if (system_flags & (SYS_NO_PAUSE | SYS_DOOR_OPEN | SYS_SOFT_RESET))
            return;
        sound_volume_off();
        w->next = 1;
        system_flags |= SYS_PAUSED;
        w->state = 0;
    } else {
        if (btn == 0)
            return;
        if (system_flags & (SYS_NO_PAUSE | SYS_DOOR_OPEN | SYS_SOFT_RESET))
            return;
        sound_volume_off();
        w->next = 3;
        system_flags |= SYS_PAUSED;
        w->state = 0;
    }
}

/* Warning 15 (vibrator connected), then the menu. */
/* 0x0014d8a0 */
static void pause1_state1_exec(Pause1Work *w)
{
    switch (w->state) {
    case 0:
        if (warning_request(15, 1))
            w->state = w->state + 1;
        break;
    case 1:
        if (warning_is_done()) {
            w->state = 0;
            w->next = 3;
        }
        break;
    }
}

/* Warning 0 (controller disconnected) until reconnected, then the menu. */
/* 0x0014d918 */
static void pause1_state2_exec(Pause1Work *w)
{
    switch (w->state) {
    case 0:
        if (warning_request(0, 1))
            w->state = w->state + 1;
        break;
    case 1:
        if (pad_is_connected(0)) {
            warning_cancel();
            w->state = 0;
            w->next = 3;
        }
        break;
    }
}

/*
 * Menu. Up/down wrap; Start runs the first item (resume); 0x200 + 0x400 held hides the menu
 * (state 6). New vibrator or lost controller -> their warnings.
 */
/* 0x0014d9a0 */
static void pause1_state3_exec(Pause1State3ExecPause1Work *w)
{
    PauseItem *item;
    int state;
    PauseItem *it;
    int old;
    int n;
    PadDataView *pad;

    state = w->state;
    switch (state) {
    case 0: {
        int i;
        w->sel = 0;
        system_flags = (system_flags | SYS_NO_VIBRATION) & ~SYS_NO_SCREEN_SAVER;
        for (i = 6, item = pause_items; i != 0; i--, item++) {
            if (item->enter != 0)
                item->enter(w);
        }
        pause2_set_state(1);
        fade_start_alpha(0.5f, 0, 12);
        w->state = w->state + 1;
        break;
        }
    case 1:
        if (fade_is_idle() == 1)
            w->state = w->state + 1;
        break;
    case 2:
        break;
    }
    pad = (*(PadSystemPauseView *)&pad_system).pad; /* steer(cast) */
    old = w->sel;
    n = old;
    if (pad->flags & PAD_UP) {
        n = old - 1;
        if (n < 0)
            n = 5;
    }
    if (pad->flags & PAD_DOWN) {
        n = n + 1;
        if (!(n < 6))
            n = 0;
    }
    if (old != n) {
        w->sel = n;
        SoundRhythmSe_Common(0);
        old = w->sel;
    }
    it = &pause_items[old];
    if (it->input != 0)
        it->input(w);
    if ((*(PadSystemPauseView *)&pad_system).pad->flags & pad_decide_mask) { /* steer(cast) */
        DEBUG_PRINTF(str_pause_decide);
        if (it->decide != 0)
            it->decide(w);
    }
    if ((*(PadSystemPauseView *)&pad_system).pad->flags & PAD_START) { /* steer(cast) */
        DEBUG_PRINTF(str_pause_pause);
        it = pause_items;
        if (it->decide != 0)
            it->decide(w);
    }
    if (((*(PadSystemPauseView *)&pad_system).pad->raw_on & 0x600) == 0x600) { /* steer(cast) */
        system_flags |= SYS_NO_SCREEN_SAVER;
        fade_reset();
        w->next = 6;
    }
    if (w->next != 3)
        pause2_set_state(0);
    if (pause1_update_count(w) != 0) {
        pause2_set_state(0);
        w->next = 1;
        w->state = 0;
    }
    if (pad_is_connected(0) == 0) {
        pause2_set_state(0);
        w->state = 0;
        w->next = 2;
    }
}

/* Hides the menu, fades the dimming, then sound on, unpause, state 0. */
/* 0x0014dc38 */
static void pause1_state4_exec(Pause1Work *w)
{
    switch (w->state) {
    case 0:
        fade_start_alpha(0.5f, 0, -12);
        pause2_set_state(0);
        w->state = w->state + 1;
    case 1:
        if (fade_is_idle() == 1) {
            sound_volume_on();
            w->next = 0;
            system_flags &= ~SYS_NO_VIBRATION;
            system_flags &= ~SYS_PAUSED;
            system_flags |= SYS_NO_SCREEN_SAVER;
        }
        break;
    }
}

/*
 * Quit confirm (WARNING_QUIT_CONFIRM). No: menu. Yes: shot sounds off, a few black frames,
 * GameExit(3), no further pausing, unpause.
 */
/* 0x0014dce8 */
static void pause1_state5_exec(Pause1State5ExecPause1Work *w)
{
    switch (w->state) {
    case 0:
        if (warning_request(WARNING_QUIT_CONFIRM, 0))
            w->state = w->state + 1;
        break;
    case 1:
        if (warning_is_done()) {
            if (warning_get_result()) {
                SoundMidiSeEnable(0);
                SoundMidiSeEnableLaser(0);
                w->redraw = 1;
                w->state = w->state + 1;
            } else {
                w->state = 0;
                w->next = 3;
            }
        }
        break;
    case 2:
    case 3:
    case 4:
        w->state = w->state + 1;
        w->redraw = 1;
        break;
    case 5:
        w->redraw = 1;
        GameExit(3);
        system_pause_disable();
        sound_volume_on();
        w->next = 0;
        system_flags &= ~SYS_NO_VIBRATION;
        system_flags &= ~SYS_PAUSED;
        system_flags |= SYS_NO_SCREEN_SAVER;
        break;
    }
}

/* Menu hidden: any button shows it. */
/* 0x0014de00 */
static void pause1_state6_exec(Pause1Work *w)
{
    if (pad_main->raw.trig != 0) {
        system_flags &= ~SYS_NO_SCREEN_SAVER;
        pause2_set_state(1);
        fade_start_alpha(0.5f, 0, 12);
        w->next = 3;
    }
}

/* system_flags bit 2 forces resume. */
/* 0x0014de70 */
static void pause1_task_exec(Pause1Work *w)
{
    if (system_flags & SYS_SOFT_RESET)
        pause1_enter_resume(w);
    switch (w->next) {
    case 0:
        pause1_check_start(w);
        break;
    case 1:
        pause1_state1_exec(w);
        break;
    case 2:
        pause1_state2_exec(w);
        break;
    case 3:
        pause1_state3_exec(w);
        break;
    case 4:
        pause1_state4_exec(w);
        break;
    case 5:
        pause1_state5_exec(w);
        break;
    case 6:
        pause1_state6_exec(w);
        break;
    }
}

/* 0x0014df60 */
static void pause2_task_init(void *w)
{
    pause2_work = w;
    task_set_dlist(w, 13);
}

/* Translucent 446 x 360 panel, centred; title, frame, cursor, item names and their drawing. */
/* steer(cast) */
/* 0x0014df70 */
static void pause2_draw_menu(int *w)
{
    float z = 3.3333333f;
    float y = 368.0f;
    int col;
    PauseItem *it;
    int i;
    int lang;

    {
        void *tex_list = system_ptm;

        *w = *w + 1;
        ulGxSetTextureList(tex_list);
    }
    draw_set_blend(4, 5);
    rect_draw_outline(screen_cfg.cx - 223.0f, screen_cfg.cy - 180.0f, z, 446.0f, 360.0f, game_state.tex, 0x5a000000);
    col = game_state.sprites;
    z = float_add_bits16(2, z);
    sprite_draw(screen_cfg.cx, 40.0f, z, col, 5, 1, 0);
    sprite_draw(126.0f, y, z, col, 10, 0, 0);
    sprite_draw(410.0f, y, z, col, 14, 0, 0);
    {
        PauseItem *sel = &pause_items[pause1_get_selection()];

        sprite_draw(sel->x, sel->y, z, col, 20, 2, 1);
    }
    z = float_add_bits16(1, z);
    font_set(2);
    lang = game_state.lang;
    for (i = 0; i < 6; i++) {
        it = &pause_items[i];
        {
            float tx = it->x;
            float ty = it->y - floorf(9.5f);

            font_print(tx, ty, z, col, it->names[lang]);
        }
        if (it->draw)
            it->draw(pause1_work, it->x, it->y, z);
    }
}

/* Menu while shown; black screen on frames "Pause1" asks (redraw). */
/* 0x0014e190 */
static void pause2_task_disp(Pause2Work *w)
{
    switch (w->state) {
    case 0:
        break;
    case 1:
        pause2_draw_menu(w);
        break;
    }
    if (pause1_work->redraw != 0) {
        PauseVtx vtx[4];
        unsigned col = COL_BLACK;
        float bottom = screen_layout.h;
        float right = screen_layout.w;
        float left = 0.0f;
        float z = 3.3333333f;
        vtx[0].y = vtx[2].y = vtx[0].x = vtx[1].x = left;
        vtx[2].x = vtx[3].x = right;
        vtx[1].y = vtx[3].y = bottom;
        vtx[0].z = vtx[1].z = vtx[2].z = vtx[3].z = z;
        vtx[0].color = vtx[1].color = vtx[2].color = vtx[3].color = col;
        ulGxBeginStrip(0);
        ulGxAddStrip(vtx, 4);
        ulGxEndStrip();
        pause1_work->redraw = 0;
    }
}

/* 0x0014e260 */
static int pause1_get_selection(void)
{
    return pause1_work->sel;
}

/* 1 shows, 0 hides. */
/* 0x0014e270 */
static void pause2_set_state(int state)
{
    (*(Pause2Work * *)&pause2_work)->state = state; /* steer(cast) */
}

/* "Pause2" (drawing) goes next to the screen root task. */
/* 0x0014e280 */
void pause_task_create(void)
{
    char *work;
    char *root;

    work = task_create(system1_work, &pause1_desc, 24);
    root = scr_root;
    TASK_FLAGS_OF(work) |= TASK_F_RUN_PAUSED;
    work = task_create_sibling(root, &pause2_desc, 8);
    TASK_FLAGS_OF(work) |= TASK_F_RUN_PAUSED;
}

/* 0x0014e2e8 */
void *pause2_get_task(void)
{
    return pause2_work;
}
