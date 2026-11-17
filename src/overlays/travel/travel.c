/*
 * Travelling mode area select: open areas with artist and track over the area's picture; deciding
 * starts it in travelling mode, cancel returns to the title.
 */

#include "travel_shared.h"
#include "task.h"
#include "billboard.h"
#include "color.h"
#include "console.h"
#include "fade.h"
#include "float_bits.h"
#include "game.h"
#include "model.h"
#include "module.h"
#include "pad.h"
#include "pnb.h"
#include "sequence.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "system.h"
#include "ul_gx.h"

extern GameState game_state;
extern PadSystem pad_system;
extern unsigned int pad_decide_mask;

extern void font_set(int id);

extern struct TaskDesc travel_desc;

void *travel_create(void *parent);

/* 28 bytes */
typedef struct TravelWork {
    char pad_00[4];
    void *textures;             /* 0x04: the overlay's model file (travel_textures) */
    int step;                   /* 0x08 */
    int sel;                    /* 0x0c: selected area */
    int max;                    /* 0x10: last area open */
    int se;                     /* 0x14: SE note playing, -1 = none */
    int saved;                  /* 0x18: value returned by console_set_disp_level(0), restored at release */
} TravelWork;

extern unsigned char *artist_label_texts[];
extern unsigned char *track_label_texts[];
extern unsigned char *area_artist_names[];
extern unsigned char *area_track_titles[][2];
extern char travel_textures[];

/* x, y, w, h, and texture coordinates */
extern float travel_quads[][4];

extern float travel_quad_uvs[][4];

extern unsigned char *travel_title_texts[];

extern unsigned char *travel_caption_texts[];

/* BPM per area */
extern float area_tempos[];

extern int area_bg_tex_ids[];

/* 0x00ca0700 */
void *module_entry(void *parent)
{
    return travel_create(parent);
}

/* 0x00ca0720 */
void travel_load(TravelWork *w)
{
    w->textures = model_create_timed(travel_textures, 0x900000);
}

/* 0x00ca0758 */
void travel_unload(TravelWork *w)
{
    model_list_remove(w->textures);
}

/* 0x00ca0760 */
void travel_draw_credits(int stg, int hl, float x, float y, float z)
{
    int lang;
    int col;
    float y_track, dy;
    unsigned char *str;

    col = hl ? COL_WHITE : RGBA(0x50, 0x50, 0x50, 0x80);
    dy = 4.0f;
    x += 12.0f;
    y += dy;
    lang = game_lang;
    font_set(2);
    font_print(x, y, z, col, artist_label_texts[lang]);
    y_track = y + 19.0f;
    font_print(x, y_track, z, col, track_label_texts[lang]);
    x += 116.0f;
    font_print(x, y, z, col, area_artist_names[stg]);
    str = area_track_titles[stg][0];
    font_print(x, y_track - 1.0f, z, col, str);
    y += 38.0f;
    font_print(x, y - dy, z, col, area_track_titles[stg][1]);
}

/* 0x00ca08e0 */
void travel_draw_area(int stg, int sel, float z)
{
    int quad;

    if (sel) {
        quad = stg;
        tex_rect_draw_uv(20, COL_WHITE, travel_quad_uvs[quad], travel_quads[quad][0], travel_quads[quad][1], z, travel_quads[quad][2], travel_quads[quad][3]);
        quad = stg + 4;
        tex_rect_draw_uv(20, COL_WHITE, travel_quad_uvs[quad], travel_quads[quad][0], travel_quads[quad][1], z, travel_quads[quad][2], travel_quads[quad][3]);
    } else {
        quad = stg + 8;
        tex_rect_draw_uv(20, COL_WHITE, travel_quad_uvs[quad], travel_quads[quad][0], travel_quads[quad][1], z, travel_quads[quad][2], travel_quads[quad][3]);
    }
    z = float_add_bits16(1, z);
    travel_draw_credits(stg, sel, travel_quads[stg][0], travel_quads[stg][1], z);
}

/* 0x00ca0a78 */
void travel_draw_title(float z)
{
    int lang;
    float x, y, width, height;

    x = travel_quads[12][0];
    y = travel_quads[12][1];
    width = travel_quads[12][2];
    height = travel_quads[12][3];
    lang = game_lang;
    if (lang == 0) {
        float (*uv)[4] = travel_quad_uvs;

        tex_rect_draw_uv(20, COL_WHITE, uv[12], x, y, z, width, height);
    } else {
        font_set(3);
        font_print(x, y, z, COL_WHITE, travel_title_texts[lang]);
    }
}

/* 0x00ca0b48 */
void travel_draw_caption(float z)
{
    int lang;
    float x, y, width, height;

    x = travel_quads[13][0];
    y = travel_quads[13][1];
    width = travel_quads[13][2];
    height = travel_quads[13][3];
    lang = game_lang;
    if (lang == 0) {
        float (*uv)[4] = travel_quad_uvs;

        tex_rect_draw_uv(20, COL_WHITE, uv[13], x, y, z, width, height);
    } else {
        y += 4.0f;
        font_set(2);
        font_print(x, y, z, COL_WHITE, travel_caption_texts[lang]);
    }
}

/* 0x00ca0c20 */
void travel_init(TravelWork *w)
{
    unsigned int i;
    int sel;

    sel = module_get_clear_flag(3);
    fade_start(0, 1, 0);
    w->saved = console_set_disp_level(0);
    music_change(1);
    w->sel = sel < 4 ? sel : 3;
    for (i = 0; i < 4 && game_state.setting.by.area_open[i]; i++)
        w->max = i;
    system_set_frame_rate(1);
    travel_load(w);
    game_task_request(module_get_current());
    w->se = -1;
}

/* 0x00ca0ce8 */
void travel_release(TravelWork *w)
{
    if (w->se >= 0)
        se_note_off(w->se);
    travel_unload(w);
    console_set_disp_level(w->saved);
    fade_start(0, 1, 0);
}

/* steer(cast) */
/* 0x00ca0d38 */
void travel_exec(TravelWork *w)
{
    int pick, cand, max;

    switch (w->step) {
    case 0:
        w->step++;
        break;
    case 1:
        midiseq_mute_request(w->sel);
        beat_clock_request_tempo(area_tempos[w->sel]);
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
        pick = w->sel;
        max = w->max;
        if (pad_system.pad[0]->menu.trig & PAD_UP)
            pick--;
        if (pad_system.pad[0]->menu.trig & PAD_DOWN)
            pick++;
        cand = pick > -1 ? pick : 0;
        if (cand < max)
            pick = cand;
        else
            pick = max;
        if (w->sel != pick) {
            w->sel = pick;
            if (w->se >= 0)
                se_note_off(w->se);
            w->se = pick + 13;
            ((void (*)(int, int, int))se_note_on)(5, pick + 13, 0);
            midiseq_mute_request(pick);
            beat_clock_request_tempo(area_tempos[pick]);
        }
        if (pad_system.pad[0]->menu.trig & pad_decide_mask) {
            pad_input_disable(0);
            SoundRhythmSe_Common(8);
            midiseq_mute_request(5);
            if (w->se >= 0) {
                se_note_off(w->se);
                w->se = -1;
            }
            w->step = 200;
        }
#ifdef VERSION_US
        else
#endif
        if (pad_system.pad[0]->menu.trig & pad_cancel_mask) {
            pad_input_disable(0);
            SoundRhythmSe_Common(6);
            midiseq_mute_request(5);
            w->step = 300;
        }
        break;
    case 200:
        stage_next_set(w->sel);
        module_stage_setup(w);
        module_set_clear_flag(3, w->sel);
        w->step++;
        break;
    case 201:
        w->step++;
        break;
    case 202:
        fade_start(0, 1, 120);
        w->step++;
        break;
    case 203:
        if (fade_is_idle() == 1)
            FINISH_IF(w);
        break;
    case 300:
        fade_start(0, 1, 30);
        w->step++;
    case 301:
        if (fade_is_idle() == 1) {
            module_set_clear_flag(0, 1);
            sound_sequence_set_next(2);
            FINISH_IF(w);
            w->step++;
        }
        break;
    }
}

/* 0x00ca1088 */
void travel_disp(TravelWork *w)
{
    float z = 0.002f;
    int i;

    ulGxSetTextureList(w->textures);
    screen_image_draw(area_bg_tex_ids[w->sel], COL_WHITE, z);
    z = float_add_bits16(1, z);
    travel_draw_title(z);
    travel_draw_caption(z);
    z = float_add_bits16(1, z);
    for (i = 0; i <= w->max; i++) {
        ulGxSetTextureList(w->textures);
        travel_draw_area(i, i == w->sel, z);
    }
}

/* 0x00ca1168 */
void *travel_create(void *parent)
{
    return task_create(parent, &travel_desc, 28);
}
