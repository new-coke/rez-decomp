/*
 * Score attack area select: area list over the selected area's background and high score.
 * Moving plays the area's preview and pattern; deciding starts the stage.
 */

#include "areasel.h"
#include "task.h"
#include "camera.h"
#include "color.h"
#include "console.h"
#include "debug.h"
#include "fade.h"
#include "game.h"
#include "model.h"
#include "module.h"
#include "nj_strip.h"
#include "node_draw_state.h"
#include "pad.h"
#include "render.h"
#include "sequence.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "ul_gx.h"
#include "as_bg.h"
#include "../common/demo_camera.h"

extern PadSystem pad_system;
extern unsigned int pad_decide_mask;
extern unsigned int system_flags;

/* 36 bytes */
typedef struct AreaSelect {
    void *models;               /* 0x00 */
    void *textures;             /* 0x04 */
    int step;                   /* 0x08 */
    DemoCamera *cam;                 /* 0x0c */
    AsBg *bg;                   /* 0x10 */
    int se;                     /* 0x14: SE note playing, -1 = none */
    int area;                   /* 0x18: selected area */
    int saved;                  /* 0x1c: value returned by console_set_disp_level(0), restored at release */
    char pad_20[4];
} AreaSelect;

typedef struct AsLogArg {       /* logio_create argument */
    unsigned char *text;
    int color;                  /* 0x04 */
    int t_in, t_hold, t_out;                /* 0x08 */
} AsLogArg;

extern char area_select_textures[];

extern char area_select_models[];

/* area name sprites and cursor */
extern float sprite_uvs[][4];

extern float sprite_sizes[][2];

/* BPM per area */
extern float area_tempos[];

extern struct TaskDesc area_select_desc;

extern unsigned char *stage_login_names[][11];

void area_select_load(AreaSelect *w);
void area_select_unload(AreaSelect *w);
void *area_select_create(void *parent);
void area_select_draw_sprite(Vec3 *pos, int i);

/* 0x00ca1598 */
void *module_entry(void *parent)
{
    return area_select_create(parent);
}

/* 0x00ca15b8 */
void area_select_load(AreaSelect *w)
{
    w->textures = model_create_timed(area_select_textures, 0x920000);
    w->models = model_pnb_register(area_select_models);
}

/* 0x00ca1600 */
void area_select_unload(AreaSelect *w)
{
    model_list_remove(w->textures);
    model_free_all_buffers_checked(w->models);
}

/* 0x00ca1630 */
void area_select_init(AreaSelect *w)
{
    int sel = module_get_clear_flag(5);

    camera_set_far(10000.0f);
    fade_start(0, 1, 0);
    w->saved = console_set_disp_level(0);
    music_change(1);
    system_set_frame_rate(1);
    w->area = sel < 5 ? sel : 4;
    area_select_load(w);
    game_task_request((void *)module_get_current());
    w->se = -1;
}

/* 0x00ca16d0 */
void area_select_release(AreaSelect *w)
{
    if (w->se >= 0)
        se_note_off(w->se);
    area_select_unload(w);
    console_set_disp_level(w->saved);
    fade_start(0, 1, 0);
}

/* steer(cast) */
/* 0x00ca1720 */
void area_select_exec(AreaSelect *w)
{
    int sel;
    AsLogArg arg;
    GameState *gs;
#ifdef VERSION_US
    unsigned int trig;
#endif

    switch (w->step) {
    case 0:
        w->cam = demo_camera_create(w);
        w->bg = as_bg_create(w, w->area);
        w->step++;
        break;
    case 1:
        midiseq_mute_request(w->area);
        beat_clock_request_tempo(area_tempos[w->area]);
        w->step++;
        break;
    case 2:
        if (seqload_idle()) {
            midiseq_play();
            w->se = 12;
            ((void (*)(int, int, int))se_note_on)(5, 12, 0);
            fade_start_out(30);
            pad_input_enable(0);
            w->step = 100;
        }
        break;
    case 100:
        sel = w->area;
        if (pad_system.pad[0]->menu.trig & PAD_UP)
            sel--;
        if (pad_system.pad[0]->menu.trig & PAD_DOWN)
            sel++;
        if (sel < 0)
            sel = 0;
        if (sel > 4)
            sel = 4;
        if (w->area != sel && game_state.setting.by.area_table[sel]) {
            w->area = sel;
            as_bg_set_area(w->bg, sel);
            if (w->se >= 0)
                se_note_off(w->se);
            w->se = sel + 13;
            ((void (*)(int, int, int))se_note_on)(5, sel + 13, 0);
            midiseq_mute_request(sel);
            beat_clock_request_tempo(area_tempos[sel]);
        }
#ifdef VERSION_US
        trig = pad_system.pad[0]->menu.trig;
        if (trig & pad_decide_mask) {
            SoundRhythmSe_Common(8);
            midiseq_mute_request(5);
            if (w->se >= 0) {
                se_note_off(w->se);
                w->se = -1;
            }
            w->step = 200;
        } else if (trig & pad_cancel_mask) {
            SoundRhythmSe_Common(6);
            w->step = 300;
        }
        break;
#else
        if (pad_system.pad[0]->menu.trig & pad_decide_mask) {
            SoundRhythmSe_Common(8);
            midiseq_mute_request(5);
            if (w->se >= 0) {
                se_note_off(w->se);
                w->se = -1;
            }
            w->step = 200;
        }
        if (pad_system.pad[0]->menu.trig & pad_cancel_mask) {
            SoundRhythmSe_Common(6);
            w->step = 300;
        }
        break;
#endif
    case 200:
        if (system_flags & SYS_DEBUG) {
            if (debug_flags_test(8) && (pad_system.pad[0]->menu.on & PAD_LEFT))
                w->area = 10;
            if (pad_system.pad[0]->menu.on & PAD_RIGHT)
                w->area = 5;
        }
        pad_input_disable(0);
        stage_next_set(w->area);
        module_stage_setup(w);
        module_set_clear_flag(5, w->area);
        demo_camera_play(w->cam);
        w->step++;
        break;
    case 201:
        if (demo_camera_get_remaining(w->cam) <= 150.0f) {
            gs = &game_state;
            arg.text = stage_login_names[gs->lang][w->area];
            arg.color = gs->menu_color;
            arg.t_in = 15;
            arg.t_hold = 120;
            arg.t_out = 15;
            logio_create(w, &arg);
            w->step++;
        }
        break;
    case 202:
        if (demo_camera_get_remaining(w->cam) <= 5.0f) {
            fade_start(0, 1, 5);
            w->step++;
        }
        break;
    case 203:
        if (demo_camera_is_done(w->cam))
            FINISH_IF(w);
        break;
    case 300:
        midiseq_mute_request(5);
        pad_input_disable(0);
        fade_start(0, 1, 30);
        w->step++;
        break;
    case 301:
        if (fade_is_idle() == 1) {
            module_set_clear_flag(0, 3);
            sound_sequence_set_next(2);
            FINISH_IF(w);
            w->step++;
        }
        break;
    }
}

/* 0x00ca1ba8 */
void area_select_disp(AreaSelect *w)
{
    Vec3 pos;
    int i;
    int n;
    GameState *gs;

    RenderSetCurrentDisplayList(6);
    ulGxSetTextureList(w->textures);
    ulGxSelectTexture(ulGxGetTextureList(), 0);
    draw_set_fc_mode(0x2000);
    pos.x = -205.0f;
    pos.y = 85.0f;
    pos.z = -416.0f;
    gs = &game_state;
    n = 0;
    for (i = 0; i < 5U; i++, n++) {
        if (gs->setting.by.area_table[i])
            area_select_draw_sprite(&pos, n);
        pos.y -= 56.0f;
    }
    pos.y = 85.0f - (float)w->area * 56.0f;
    area_select_draw_sprite(&pos, 5);
}

/* 0x00ca1cc0 */
void *area_select_create(void *parent)
{
    return task_create(parent, &area_select_desc, 36);
}

/* 0x00ca1ce8 */
void area_select_draw_sprite(Vec3 *pos, int i)
{
    AsVtx vtx[4];
    float hw, hh, u_left, v_top, u_right, v_bottom;

    hw = sprite_sizes[i][0] * 0.5f;
    hh = sprite_sizes[i][1] * 0.5f;
    u_left = sprite_uvs[i][0];
    v_top = sprite_uvs[i][1];
    u_right = sprite_uvs[i][2];
    v_bottom = sprite_uvs[i][3];
    vtx[1].x = vtx[0].x = pos->x - hw;
    vtx[3].x = vtx[2].x = pos->x + hw;
    vtx[2].y = vtx[0].y = pos->y + hh;
    vtx[3].y = vtx[1].y = pos->y - hh;
    vtx[3].z = vtx[2].z = vtx[1].z = vtx[0].z = pos->z;
    vtx[1].u = vtx[0].u = u_left;
    vtx[3].u = vtx[2].u = u_right;
    vtx[2].v = vtx[0].v = v_top;
    vtx[3].v = vtx[1].v = v_bottom;
    vtx[3].color = vtx[2].color = vtx[1].color = vtx[0].color = COL_WHITE;
    nj_clip_tex_strip_begin(1);
    nj_clip_tex_strip_vertices(vtx, 4);
    nj_clip_tex_strip_end();
}
