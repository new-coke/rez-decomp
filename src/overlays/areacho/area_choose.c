/*
 * Area choose screen: each move changes the background, the music pattern and tempo; deciding
 * flies the camera in and starts the stage; cancel asks with a warning box.
 */

#include "areacho.h"
#include "task.h"
#include "mathdef.h"
#include "camera.h"
#include "console.h"
#include "debug.h"
#include "fade.h"
#include "game.h"
#include "libc.h"
#include "model.h"
#include "module.h"
#include "node_draw_state.h"
#include "pad.h"
#include "pnb.h"
#include "render.h"
#include "sequence.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "ul_gx.h"
#include "warning.h"
#include "ac_beat.h"
#include "ac_bg.h"
#include "ac_player.h"
#include "../common/demo_camera.h"

extern unsigned char kbd_state[];

/* 984 bytes */
typedef struct AreaChoose {
    void *models;
    void *textures;             /* 0x04 */
    int state;                  /* 0x08 */
    int sel;                    /* 0x0c */
    unsigned int max;           /* 0x10: the last area open */
    int se;                   /* 0x14: the playing preview note, -1 = none */
    DemoCamera *cam;                 /* 0x18 */
    AcBg *bg;                   /* 0x1c */
    void *beat;                /* 0x20 */
    AcPlayer *player;               /* 0x24 */
    ModelPart icons[5];            /* 0x28 */
    ModelPart marks[5];            /* 0x1f4 */
    char pad_3c0[20];
    int saved;                  /* 0x3d4: console_set_disp_level */
} AreaChoose;

extern unsigned char *area_choose_titles[];
extern char area_choose_textures[];
extern char area_choose_models[];
extern float area_tempos[];
extern struct TaskDesc area_choose_desc;
extern PadSystem pad_system;
extern unsigned int pad_decide_mask;
extern unsigned int system_flags;
extern unsigned char *stage_login_names[][11];
void *area_choose_create(void *parent);

/* 0x00ca1ca8 */
void *module_entry(void *parent)
{
    return area_choose_create(parent);
}

/* 0x00ca1cc8 */
void area_choose_load(AreaChoose *w)
{
    w->textures = model_create_timed(area_choose_textures, 0x910000);
    w->models = model_pnb_register(area_choose_models);
}

/* 0x00ca1d10 */
void area_choose_unload(AreaChoose *w)
{
    model_list_remove(w->textures);
    model_free_all_buffers_checked(w->models);
}

/* steer(cast) */
/* 0x00ca1d40 */
void area_choose_icons_create(AreaChoose *w)
{
    ModelPart *p;
    unsigned int i;
    int root_idx, clip_idx, root;

    camera_set_far(10000.0f);
    p = w->icons;
    for (i = 0; i <= w->max; i++) {
        dc_object_init(p, *((struct DcNode **)pnb_get_entry(w->models, i + 4)), 0, 0);
        p++;
    }
    for (; i < 4; i++) {
        dc_object_init(p, *((struct DcNode **)pnb_get_entry(w->models, i + 8)), 0, 0);
        p++;
    }
    p = w->marks;
    root_idx = 12;
    i = 5;
    clip_idx = 13;
    for (; i != 0; i--) {
        root = *((int *)pnb_get_entry(w->models, root_idx));
        root_idx += 2;
        dc_object_init(p, root, *((struct DcClip **)pnb_get_entry(w->models, clip_idx)), 0);
        clip_idx += 2;
        p++;
    }
}

/* 0x00ca1e68 */
void area_choose_icons_release(AreaChoose *w)
{
    ModelPart *p;
    int n;

    p = w->icons;
    for (n = 5; n != 0; n--) {
        dc_object_release(p);
        p++;
    }
    p = w->marks;
    for (n = 5; n != 0; n--) {
        dc_object_release(p);
        p++;
    }
}

/* 0x00ca1ed8 */
void area_choose_init(AreaChoose *w)
{
    unsigned int i;
    int sel;

    sel = module_get_clear_flag(4);
    fade_start(0, 1, 0);
    w->saved = console_set_disp_level(0);
    music_change(1);
    system_set_frame_rate(1);
    for (i = 0; i < 5 && game_state.setting.by.area_open[i]; i++)
        w->max = i;
    w->sel = sel < (int)w->max ? sel : (int)w->max;
    area_choose_load(w);
    area_choose_icons_create(w);
    game_task_request(module_get_current());
    w->se = -1;
}

/* 0x00ca1fa8 */
void area_choose_release(AreaChoose *w)
{
    if (w->se >= 0)
        se_note_off(w->se);
    area_choose_icons_release(w);
    area_choose_unload(w);
    console_set_disp_level(w->saved);
    fade_start(0, 1, 0);
}

/* steer(cast) */
/* 0x00ca2000 */
void area_choose_exec(AreaChoose *w)
{
    ModelPart *p;
    int i, sel, lo, max;
    int arg[5];

    switch (w->state) {
    case 0:
        w->cam = demo_camera_create(w);
        w->bg = ac_bg_create(w, w->sel);
        w->beat = ac_beat_create(w);
        w->player = ac_player_create(w);
        w->state++;
        break;
    case 1:
        midiseq_mute_request(w->sel);
        beat_clock_request_tempo(area_tempos[w->sel]);
        w->state++;
        break;
    case 2:
        if (seqload_idle()) {
            midiseq_play();
            w->se = 12;
            ((void (*)(int, int, int))se_note_on)(5, 12, 0);
            fade_start_out(30);
            pad_input_enable(0);
            w->state = 100;
        }
        break;
    case 100:
        if (system_flags & SYS_DEBUG && kbd_state[5] == 19) {
            area_choose_icons_release(w);
            w->max = 4;
            area_choose_icons_create(w);
        }
        sel = w->sel;
        max = w->max;
        if (pad_system.pad[0]->menu.trig & PAD_LEFT)
            sel--;
        if (pad_system.pad[0]->menu.trig & PAD_RIGHT)
            sel++;
        lo = sel > -1 ? sel : 0;
        if (lo < max)
            sel = lo;
        else
            sel = max;
        if (w->sel != sel) {
            w->sel = sel;
            ac_bg_set_area(w->bg, sel);
            if (w->se >= 0)
                se_note_off(w->se);
            w->se = sel + 13;
            ((void (*)(int, int, int))se_note_on)(5, sel + 13, 0);
            midiseq_mute_request(sel);
            beat_clock_request_tempo(area_tempos[sel]);
        }
        if (pad_system.pad[0]->menu.trig & pad_decide_mask) {
            SoundRhythmSe_Common(8);
            midiseq_mute_request(5);
            if (w->se >= 0) {
                se_note_off(w->se);
                w->se = -1;
            }
            w->state = 200;
        }
#ifdef VERSION_US
        else
#endif
        if (pad_system.pad[0]->menu.trig & pad_cancel_mask) {
            SoundRhythmSe_Common(6);
            w->state = 300;
        }
        break;
    case 200:
        if (system_flags & SYS_DEBUG) {
            if (debug_flags_test(8) && pad_system.pad[0]->menu.on & PAD_UP)
                w->sel = 10;
            if (pad_system.pad[0]->menu.on & PAD_DOWN)
                w->sel = 5;
        }
        pad_input_disable(0);
        stage_next_set(w->sel);
        module_stage_setup(w);
        module_set_clear_flag(4, w->sel);
        demo_camera_play(w->cam);
        ac_player_play(w->player);
        w->state++;
        break;
    case 201:
        if (demo_camera_get_remaining(w->cam) <= 150.0f) {
            arg[0] = (int)stage_login_names[game_state.lang][w->sel];
            arg[1] = game_state.menu_color;
            arg[2] = 15;
            arg[4] = 15;
            arg[3] = 120;
            logio_create(w, arg);
            w->state++;
        }
        break;
    case 202:
        if (demo_camera_get_remaining(w->cam) <= 5.0f) {
            fade_start(0, 1, 5);
            w->state++;
        }
        break;
    case 203:
        if (demo_camera_is_done(w->cam))
            FINISH_IF(w);
        break;
    case 300:
#ifdef VERSION_US
        if (warning_request(23, 0))
#else
        if (warning_request(20, 0))
#endif
            w->state++;
        break;
    case 301:
        if (warning_is_done()) {
            if (warning_get_result()) {
                midiseq_mute_request(5);
                pad_input_disable(0);
                fade_start(0, 1, 30);
                w->state++;
            } else {
                w->state = 100;
            }
        }
        break;
    case 302:
        if (fade_is_idle() == 1) {
            module_set_clear_flag(0, 2);
            sound_sequence_set_next(2);
            FINISH_IF(w);
            w->state++;
        }
        break;
    }
    for (i = 0, p = w->marks; i < 5; i++, p++) {
        p->speed = i == w->sel ? 1.0f : -1.0f;
        do {
            dc_object_advance_time(p);
        } while (0);
    }
}

/* 0x00ca2550 */
void area_choose_disp(AreaChoose *w)
{
    Vec3 pos, step;
    ModelPart *p;
    int n, lang, col, prev_dl;
    float eps;

    lang = game_state.lang;
    ulGxSetTextureList(w->textures);
    p = w->icons;
    for (n = 5; n != 0; n--) {
        dc_object_draw(p);
        p++;
    }
    eps = FLT_EPSILON;
    p = w->marks;
    for (n = 5; n != 0; n--) {
        if (!(fabsf(p->frame) < eps))
            dc_object_draw(p);
        p++;
    }
    col = game_state.menu_color;
    draw_set_fc_mode(8192);
    pos.x = -280.0f;
    pos.y = 170.0f;
    pos.z = -416.5f;
    step.x = 12.0f;
    *(int *)&step.y = 0;
    *(int *)&step.z = 0;
    prev_dl = RenderSetCurrentDisplayList(6);
    font_set(2);
    font_print3d(area_choose_titles[lang], &pos, &step, 0, 0, col);
    RenderSetCurrentDisplayList(prev_dl);
}

/* 0x00ca26b8 */
void *area_choose_create(void *parent)
{
    return task_create(parent, &area_choose_desc, 984);
}
