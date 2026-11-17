/*
 * Ending module: picks the ending type from the clear flags (debug key override); staff roll alone
 * (types 0, 1, 5) or model scene first. Ends after the closing message or START.
 */

#include "ending_shared.h"
#include "color.h"
#include "debug.h"
#include "fade.h"
#include "file.h"
#include "fog.h"
#include "model.h"
#include "module.h"
#include "pad.h"
#include "sound_midise.h"
#include "sound_pcmstream.h"
#include "system.h"
#include "ending.h"
#include "end_butterfly.h"
#include "end_circle.h"
#include "end_model.h"
#include "end_staff_roll.h"

typedef struct Game {
    char pad_00[24];
    int flags;                  /* 0x18 */
} Game;

extern Game *pad_main;
extern GameState game_state;

extern struct TaskDesc ending_desc;
extern int ending_load_state;
extern int ending_type;
extern int ending_end;
extern int ending_scene_stopped;

/* 0x00afac30 */
void *module_entry(void *parent)
{
    return ending_create(parent);
}

/* steer(cast) */
/* 0x00afac50 */
void ending_start(Ending *w)
{
    float base_fps = FPS;
    float cur_fps;
    void *tex;

    frame_scale = base_fps / game_state.fps;
    time_scale = game_state.fps / base_fps;
    end_butterfly_vars_init();
    system_set_frame_rate(1);
    pad_input_enable(0);
    fade_start_out((int)(time_scale * 2.0f * base_fps));
    switch (ending_type_get()) {
    case 0:
    case 1:
    case 5:
        fog_set(COL_BLACK, 4, 300.0f, 1000.0f);
        fog_set_enable((void *)1);
        do {
            ((void (*)(unsigned int))BackColReq)(COL_BLACK);
            w->cam = 0;
            w->mdl = 0;
            w->roll = end_staff_roll_create(w);
        } while (0);
        break;
    default:
        tex = model_load_timed((char *)ending_files[3], 0xd000);
        w->models = model_pnb_load((char *)ending_files[0]);
        do { w->textures = tex; } while (0);
        fog_set(RGBA(0x5f, 0x5f, 0x5f, 0x80), 4, 300.0f, 1000.0f);
        fog_set_enable((void *)1);
        BackColReq(RGBA(0x5f, 0x5f, 0x5f, 0x80));
        do {
            w->cam = demo_camera_create(w);
            w->mdl = end_model_create(w, 0);
            w->roll = 0;
        } while (0);
        break;
    }
    ending_end = 0;
    ending_scene_stopped = 0;
    w->state = 0;
}

/* 0x00afadf8 */
void ending_release(Ending *w)
{
    if (ending_load_state == 2) {
        switch (ending_type) {
        case 0:
        case 1:
        case 5:
            file_request_cancel(ending_files[2]);
            break;
        default:
            file_request_cancel(ending_files[3]);
            file_request_cancel(ending_files[0]);
            break;
        }
    }
    end_butterfly_vars_release();
    pcm_stop();
    model_unload_timed(w->textures);
    model_pnb_unload(w->models);
    do { w->models = 0; } while (0);
    w->textures = 0;
    ending_end = 0;
}

/* 0x00afaeb0 */
void ending_exec_wait(Ending *w)
{
    switch (w->state) {
    case 0:
        if (ending_end || (((Pad *)pad_main)->menu.trig & PAD_START))
            w->state = 100;
        break;
    case 100:
        fade_start(0, 1, (int)(time_scale * 2.0f * FPS));
        pcm_stop();
        se_stg_common_clear();
        w->state++;
        break;
    case 101:
        if (fade_is_idle() == 1) {
            FINISH_IF(w);
            w->state++;
        }
        break;
    }
}

/* 0x00afafb0 */
void ending_disp(void)
{
}

/* 0x00afafb8 */
void ending_exec(Ending *w)
{
    switch (ending_load_state) {
    case 0:
        if (file_request_all_done()) {
            DEBUG_PRINTF("Ending Start File End\n");
            ending_load_state = 1;
        }
        break;
    case 1:
        DEBUG_PRINTF("Ending Data Load Start\n");
        ending_type = module_get_clear_flag(1);
        DEBUG_PRINTF("EndingType : %d\n", ending_type);
        if (debug_flags_test(8)) {
            ending_type = 4;
            if (pad_keyboard_key_down(30))
                ending_type = 0;
            if (pad_keyboard_key_down(31))
                ending_type = 1;
            if (pad_keyboard_key_down(32))
                ending_type = 2;
            if (pad_keyboard_key_down(33))
                ending_type = 3;
            if (pad_keyboard_key_down(34))
                ending_type = 4;
            if (pad_keyboard_key_down(35))
                ending_type = 5;
        }
        switch (ending_type) {
        case 0:
        case 1:
        case 5:
            file_request_simple(ending_files[2], 0);
            break;
        default:
            file_request_simple(ending_files[0], 0);
            file_request_simple(ending_files[3], 0);
            break;
        }
        ending_load_state = 2;
        break;
    case 2:
        if (file_request_all_done()) {
            DEBUG_PRINTF("Ending Data Load End\n");
            ending_start(w);
            TASK_OF(w)->exec = (void (*)(void *))ending_exec_wait;
            ending_load_state = 3;
            TASK_OF(w)->disp = (void (*)(void *))ending_disp;
        }
        break;
    }
}

/* 0x00afb1a0 */
void *ending_create(void *parent)
{
    ending_load_state = 0;
    return task_create(parent, &ending_desc, 24);
}

/* 0x00afb1d0 */
int ending_type_get(void)
{
    return ending_type;
}

/* 0x00afb1e0 */
void ending_stop_scene(Ending *w)
{
    if (w->cam) {
        FINISH(w->cam);
        FINISH_IF(w->mdl);
    }
    ending_scene_stopped = 1;
}

/* 0x00afb220 */
void ending_set_end(void)
{
    ending_end = 1;
}
