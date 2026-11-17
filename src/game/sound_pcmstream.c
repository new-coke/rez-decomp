/*
 * PCM streams: "Sound Pcm" streams ADX music from disc via CRI ADXT: a ".adx" file (faded in)
 * or an "STMP" pack entry, optionally held ready to start on a beat. Pausing, volume fades,
 * fade-out on a new request, stall recovery; "PcmStream" window and test variables.
 */

#include "pcmstream.h"
#include "task.h"
#include "types_game.h"
#include "types_gw.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "adxf.h"
#include "adxstm.h"
#include "adxt.h"
#include "adxt_lib.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_window.h"
#include "sjrbf.h"
#include "sound_pcmstream.h"
#include "debugmenu.h"
#include "file.h"
#include "game.h"
#include "heap.h"
#include "map_manager.h"
#include "sound_beat.h"
#include "sound_main.h"
#include "sound_midise.h"
#include "sound_bank.h"
#include "system.h"

typedef struct AdxtErrObj {
    char pad_00[12];
    int mode;                   /* 0x0c */
} AdxtErrObj;

typedef struct StmpEntry {
    char pad_00[4];
    int start;                  /* sector */
    int end;
    char pad_0c[4];
} StmpEntry;

typedef struct StmpHeader {
    char magic[4];              /* "STMP" */
    char pad_04[4];
    int count;
    char pad_0c[4];
    StmpEntry entry[1];
} StmpHeader;

/* Debug menu variable group (44 bytes): definition and its table (gw_varedit_add_table). */
typedef struct GwVarGroupDef {
    GwVarDef def;
    int table;                  /* 0x28 */
} GwVarGroupDef;

/* "REZ-ADXT_ERR[%s]\n" */
extern char str_adxt_err[];

/* "Mode[%d]\n" */
extern char str_adxt_err_mode[];

extern int pcmstream_vol1;

extern PcmStream *pcm_work;
extern char pcm_adxt_work[];
extern int pcmstream_fade;

extern int system_flags;

/* "Sound Pcm" */
extern TaskDesc pcm_desc;

extern void *pcm_debug_win;
/* main mode names: "WAIT", "PREPARE", ... */
extern char *pcm_mode_names[];

/* file state names: "NOP", "OPEN", ... */
extern char *pcm_file_state_names[];

extern char *pcm_adxt_stat_names[];

extern DebugMenu screen_layout;
extern struct GwVarNode *gw_varedit_root;
extern int pcmstream_vardef;
extern int pcmstream_vardefs;
extern int pcmstream_vars_group;
extern PcmEntry pcm_test_entries[];
extern int pcmstream_play_file;
extern int pcmstream_play_index;
extern int pcmstream_play_div;
extern void *system1_work;
extern TaskDesc testplay_desc;
extern int sound_test_bank;
extern int sound_test_prg;
extern int sound_test_note;
extern int sound_test_vardef;
extern GwVarGroupDef sound_test_vardefs[2];

static void pcm_debug_menu_add(void);
static void pcm_debug_menu_remove(void);

/* Prints the message and the handle's mode. */
/* 0x0016ad00 */
static void adxt_error_print(AdxtErrObj *obj, char *msg)
{
    DEBUG_PRINTF(str_adxt_err, msg);
    DEBUG_PRINTF(str_adxt_err_mode, obj->mode);
}

/* -960 for (nearly) 0; pcmstream_vol1 (-60) / v up to 1; -60 * (2 - v) above 1. */
/* 0x0016ad38 */
static int volume_to_attenuation(float v)
{
    int att;

    if (fabsf(v) < FLT_EPSILON)
        return -960;
    if (v > 1.0f)
        att = pcmstream_vol1 * (2.0f - v);
    else
        att = pcmstream_vol1 / v;
    if (att < -960)
        return -960;
    if (att > 0)
        return 0;
    return att;
}

/* 2 KB header buffer; state and name cleared. */
/* 0x0016add0 */
static void pcm_file_init(PcmFile *file)
{
    file->buf = heap_block_tag(game_malloc(2048), "sound_pcmstream.c", 1107);
    file->name_p = file->name;
    file->name[0] = 0;
    file->state = 0;
}

/*
 * ADXT library and a stereo handle; runs while paused; file loader; volume 1.5; debug window
 * and variables.
 */
/* steer(cast) */
/* 0x0016ae20 */
void pcm_task_init(PcmStream *w)
{
    int atten;

    w->work_buf = pcm_adxt_work;
    sjrbf_lib_init();
    adxstm_set_retry_limit(32);
    ((void (*)(void (*)(void), void *))adxt_set_error_cb)(adxt_error_print, w);
    w->adxt = adxt_create(2, w->work_buf, 0x4b1e4);
    adxt_set_recover_mode(w->adxt, 1);
    w->adxt->sj->vtbl->fn[5](w->adxt->sj);
    w->mode = 0;
    w->request = 0;
    w->stop = 0;
    w->running = 0;
    pcm_work = w;
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    pcm_file_init(&w->file);
    atten = volume_to_attenuation(1.5f);
    w->cur_volume = atten;
    w->volume = atten;
    pcm_debug_menu_add();
}

/* Closes the file if open. */
/* 0x0016aee8 */
static void pcm_file_release(PcmFile *file)
{
    heap_free(file->buf, 0);
    if (file->fh)
        file_handle_close(file->fh);
}

/* 0x0016af28 */
static void pcm_task_release(PcmStream *w)
{
    sjrbf_lib_finish();
    adxt_destroy(w->adxt);
    pcm_file_release(&w->file);
    pcm_debug_menu_remove();
    pcm_work = 0;
}

/*
 * ".adx": streamed whole, faded in; else first sector read and, for "STMP", the entry at the
 * index (0 out of range) seeked to. The ADXT stream starts paused; the player unpauses it.
 */
/* steer(cast) */
/* 0x0016af70 */
void pcm_file_exec(PcmStream *w, PcmFile *file)
{
    StmpHeader *hdr;
    StmpEntry *ent;
    int idx;
    int start;
    int size;
    int len;

    switch (file->state) {
    case 0:
        break;
    case 1:
        if (file->fh)
            file_handle_close(file->fh);
        file->fh = file_handle_open(&file->path);
        if (file->fh) {
            file->state = 4;
        } else {
            ERROR_PRINTF("PcmStream File Not Open : %s\n", file->name_p);
            file->state = 0;
        }
        break;
    case 2:
        file_handle_close(file->fh);
        file->fh = 0;
        file->state = 0;
        break;
    case 3:
        break;
    case 4:
        if (strstr(file->name_p, ".adx")) {
            len = ADXF_GetFsizeSct(file->fh);
            file->state = 6;
            w->length = len;
            w->fade_in_on = 1;
            w->fade_in = pcmstream_fade;
        } else {
            w->fade_in_on = 0;
            w->fade_in = 0;
            if (file_handle_read(file->fh, 1, file->buf) > 0)
                file->state = 5;
        }
        break;
    case 5:
        if (file_handle_is_idle(file->fh)) {
            if (fourcc_equal(file->buf, "STMP")) {
                hdr = file->buf;
                idx = pcm_work->index;
                if (hdr->count < idx) {
                    ERROR_PRINTF("PcmStream Index Over : %3d %s\n", idx, file->name_p);
                    hdr = file->buf;
                    idx = 0;
                }
                ent = &hdr->entry[idx];
                start = ent->start;
                w->length = ent->end - start;
                size = w->length << 11;
            } else {
                start = 0;
                w->length = ADXF_GetFsizeSct(file->fh);
                size = w->length << 11;
            }
            if (file_handle_seek(file->fh, start, 0)) {
                file->size = size;
                file->state = 6;
            }
        }
        break;
    case 6:
        w->adxt->sj->vtbl->fn[5](w->adxt->sj);
        adxt_pause(w->adxt, 1);
        if (w->fade_in_on)
            adxt_set_out_vol(w->adxt, -960);
        else
            adxt_set_out_vol(w->adxt, w->volume);
        adxt_set_loop_flag(w->adxt, 0);
        ADXF_ReadSj32(file->fh, w->length, w->adxt->sj);
        adxt_start_sj(w->adxt, w->adxt->sj);
        DEBUG_PRINTF("***ADXT Stream Start[%d][%s]***\n", w->count, file->name_p);
        file->state = 7;
        break;
    case 7:
        break;
    }
}

/*
 * Stops a stream stuck decoding its header or with play time frozen for 120 frames, negative time,
 * ADXT error or end of data. Resumes after a pause.
 */
/* 0x0016b1c8 */
void pcm_check_adxt(PcmStream *w)
{
    int stat;
    int old;
    int now;

    w->running = 0;
    w->frames++;
    stat = w->adxt_stat;
    if (stat == 1) {
        w->dec_err++;
        if ((float)w->dec_err >= 120.0f) {
            DEBUG_PRINTF("=========================\n");
            DEBUG_PRINTF("ADXT Status Err [DECINFO]\n");
            DEBUG_PRINTF("=========================\n");
            w->stop = stat;
        }
    }
    old = w->time;
    now = adxt_get_time_csec(w->adxt);
    w->time = now;
    if (old >= now) {
        w->time_err++;
        if ((float)w->time_err >= 120.0f) {
            int err = adxt_get_error(w->adxt);
            DEBUG_PRINTF("=========================\n");
            DEBUG_PRINTF("ADXT Time Err [2.s][%d]\n", err);
            DEBUG_PRINTF("=========================\n");
            w->stop = 1;
        }
    } else {
        w->time_err = 0;
    }
    if (w->time < 0) {
        int err = adxt_get_error(w->adxt);
        DEBUG_PRINTF("=========================\n");
        DEBUG_PRINTF("ADXT Time Err [-][%d]\n", err);
        DEBUG_PRINTF("=========================\n");
        w->stop = 1;
    }
    {
    int err = adxt_get_error(w->adxt);
    if (err) {
        DEBUG_PRINTF("=========================\n");
        DEBUG_PRINTF("ADXT Status Err [%d]\n", err);
        DEBUG_PRINTF("=========================\n");
        w->stop = 1;
    }
    }
    if (w->pause) {
        w->pause = 0;
        adxt_pause(w->adxt, 0);
        do {
            if (sound_sd_err)
                ERROR_PRINTF("%s (E) : %08x\n", "sdPstmPlay", sound_sd_err);
        } while (0);
    } else {
        w->running = 1;
    }
    w->adxt_stat = adxt_get_stat(w->adxt);
    if (adxt_is_play_end(w->adxt) == 1)
        w->stop = 1;
}

/* Closes the file, stops the handle, back to WAIT. */
/* steer(cast) */
/* 0x0016b3e8 */
static void pcm_stream_stop(PcmStream *w)
{
    DEBUG_PRINTF("***ADXT Stream Stop[%d]***\n", w->count);
    if (w->file.fh)
        file_handle_close(w->file.fh);
    w->file.fh = 0;
    adxt_stop(w->adxt);
    ((void (*)(Adxt *))adxt_stream_stop_hook)(w->adxt);
    w->adxt->sj->vtbl->fn[5](w->adxt->sj);
    w->running = 0;
    w->file.state = 0;
    w->mode = 0;
    w->adxt_stat = 5;
}

/*
 * WAIT .. PLAYWAIT: loads on request, unpauses once started, pauses with the game, counts the
 * fade-in down, fades out on a new request, stops at end or failure.
 */
/* 0x0016b470 */
static void pcm_mode_exec(PcmStream *w)
{
    switch (w->mode) {
    case 0:
        if (w->request) {
            w->request = 0;
            w->mode = 1;
        }
        w->stop = 0;
        w->dec_err = 0;
        w->unk_870 = 0;
        w->time = 0;
        w->unk_888 = 0;
        w->fade_out = 0;
        w->fade_in = 0;
        break;
    case 1:
        if (w->file.state == 0) {
            w->file.state = 1;
            w->mode = 2;
        }
        break;
    case 2:
        if (w->file.state == 7)
            w->mode = 3;
        else if (w->file.state == 0)
            w->mode = 0;
        break;
    case 3:
        w->mode = 4;
        break;
    case 4:
    case 5:
        if (w->start) {
            adxt_pause(w->adxt, 0);
            if (sound_sd_err)
                ERROR_PRINTF("%s (E) : %08x\n", "sdPstmPlay", sound_sd_err);
            w->mode = 6;
            w->pause = 0;
            w->frames = 0;
            w->time_err = 0;
            w->adxt_stat = 0;
        } else if (w->stop == 1) {
            pcm_stream_stop(w);
        }
        break;
    case 6:
        if (system_flags & (SYS_PAUSE_KBD | SYS_PAUSED)) {
            if (w->pause == 0) {
                w->pause = 1;
                adxt_pause(w->adxt, 1);
            }
        } else {
            pcm_check_adxt(w);
        }
        if (w->fade_in_on) {
            if (w->fade_in)
                w->fade_in--;
        }
        if (w->request == 1) {
            w->fade_out++;
            w->running = 0;
            if (w->fade_out >= pcmstream_fade)
                w->stop = 1;
        }
        if (w->adxt_stat == 5 || (w->running == 1 && w->adxt_stat == 0) || w->stop == 1)
            pcm_stream_stop(w);
        break;
    }
}

/*
 * Linear volume fade; while playing, quadratic fade-out (new request) or linear fade-in (".adx")
 * to -400 over pcmstream_fade frames.
 */
/* 0x0016b688 */
static void pcm_update_volume(PcmStream *w)
{
    int atten;
    int base;

    if (w->fade) {
        w->fade--;
        if (w->fade == 0) {
            base = w->volume;
            w->cur_volume = base;
            atten = base;
        } else {
            base = w->volume;
            atten = base + (w->cur_volume - base) * w->fade / w->fade_len;
        }
    } else {
        atten = w->volume;
        base = atten;
    }
    if (w->mode == 6) {
        if (w->request == 1 && w->fade_out > 0) {
            float t;

            t = (float)w->fade_out / (float)pcmstream_fade;
            t = t * t;
            atten = t * -400.0f + base * (1.0f - t);
        } else if (w->fade_in_on == 1 && w->fade_in > 0) {
            float t;

            t = (float)w->fade_in / (float)pcmstream_fade;
            atten = t * -400.0f + base * (1.0f - t);
        }
    }
    adxt_set_out_vol(w->adxt, atten);
}

/* Volume, file loader, then the mode machine. */
/* 0x0016b7c0 */
static void pcm_task_exec(PcmStream *w)
{
    w->count = (w->count + 1) % 60;
    pcm_update_volume(w);
    pcm_file_exec(w, &w->file);
    pcm_mode_exec(w);
}

/* 0x0016b818 */
void pcm_task_create(void *parent)
{
    task_create(parent, &pcm_desc, sizeof(PcmStream));
}

/* Starts as soon as it is ready. */
/* 0x0016b828 */
void pcm_play(void *entry, int arg)
{
    PcmStream *w = pcm_work;

    if (w) {
        file_desc_copy(&w->file.path, w->file.name, entry);
        w->arg = arg;
        w->request = 1;
        w->start = 1;
        w->index = 0;
    }
}

/* STMP index; negative means 0. */
/* 0x0016b880 */
void pcm_play_index(void *entry, int arg, int index)
{
    PcmStream *w = pcm_work;

    if (w) {
        file_desc_copy(&w->file.path, w->file.name, entry);
        w->arg = arg;
        w->request = 1;
        w->start = 1;
        if (index >= 0)
            w->index = index;
        else
            w->index = 0;
    }
}

/* Waits in READY until pcm_start. */
/* 0x0016b8f0 */
void pcm_load(void *entry, int arg)
{
    PcmStream *w = pcm_work;

    if (w) {
        file_desc_copy(&w->file.path, w->file.name, entry);
        w->arg = arg;
        w->request = 1;
        w->start = 0;
        w->index = 0;
    }
}

/* STMP index; negative means 0. */
/* 0x0016b948 */
void pcm_load_index(void *entry, int arg, int index)
{
    PcmStream *w = pcm_work;

    if (w) {
        file_desc_copy(&w->file.path, w->file.name, entry);
        w->arg = arg;
        w->request = 1;
        w->start = 0;
        if (index >= 0)
            w->index = index;
        else
            w->index = 0;
    }
}

/* READY or PLAY; 1 without a player. */
/* 0x0016b9b8 */
int pcm_is_ready(void)
{
    PcmStream *w = pcm_work;

    if (w)
        return w->mode - 4 < 2;
    return 1;
}

/* PLAY or PLAYWAIT; 0 without a player. */
/* 0x0016b9e0 */
int pcm_is_playing(void)
{
    int playing = 0;
    PcmStream *w = pcm_work;

    if (w == 0)
        return playing;
    playing = w->mode >= 5;
    return playing;
}

/* Loaded with pcm_load. */
/* 0x0016ba08 */
void pcm_start(void)
{
    PcmStream *w = pcm_work;

    if (w)
        w->start = 1;
}

/* 0x0016ba28 */
void pcm_stop(void)
{
    PcmStream *w = pcm_work;

    if (w)
        w->stop = 1;
}

/* frames 0: at once; else a linear fade. */
/* 0x0016ba48 */
void pcm_set_volume(int frames, float vol)
{
    PcmStream *w = pcm_work;
    int atten;

    if (w) {
        if (frames) {
            w->fade_len = frames;
            w->cur_volume = w->volume;
            w->fade = frames;
            w->volume = volume_to_attenuation(vol);
        } else {
            w->fade_len = 0;
            w->fade = 0;
            atten = volume_to_attenuation(vol);
            w->cur_volume = atten;
            w->volume = atten;
            adxt_set_out_vol(w->adxt, atten);
        }
    }
}

/* 0x0016bac0 */
static void pcm_debug_win_set(void *win)
{
    pcm_debug_win = win;
    menu_item_set_value(info_menu_name, "PcmStream", 1);
}

/* 0x0016bae0 */
static void pcm_debug_close(void)
{
    pcm_debug_win = 0;
    menu_item_set_value(info_menu_name, "PcmStream", 0);
}

/* Mode, loader state, ADXT status, time counters, file name and size. */
/* 0x0016bb00 */
static void pcm_debug_disp(void *win)
{
    PcmStream *w = pcm_work;
    int cw;
    int ch;
    int y;

    if (w) {
        gw_get_cell_size(&cw, &ch);
        gw_win_printf(win, 4, 0, "pcmstream %s", pcm_mode_names[w->mode]);
        y = ch;
        gw_win_printf(win, 4, y, "MainMode  %s", pcm_file_state_names[w->file.state]);
        y += ch;
        gw_win_printf(win, 4, y, "ADXT Status %s", pcm_adxt_stat_names[w->adxt_stat + 2]);
        y += ch;
        gw_win_printf(win, 4, y, "%4d %08x %6d", w->time, w->time_err * 2, w->frames);
        y += ch;
        gw_win_printf(win, 4, y, "FileName : %s", w->file.name_p);
        gw_win_printf(win, 4, y + ch, "FileSize : %08x", w->file.size);
    }
}

/* Opens at 21x7 cells. */
/* 0x0016bc60 */
static void pcm_debug_open(void)
{
    int cw;
    int ch;
    void *win;

    if (pcm_debug_win) {
        gw_window_close(pcm_debug_win);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    win = gw_window_open("PcmStream", 95, screen_layout.x + 4.0f, screen_layout.y + 4.0f,
                       cw * 21, ch * 7 + 4, 0);
    if (win) {
        pcm_debug_win_set(win);
        gw_window_set_handlers(win, 0, pcm_debug_disp, pcm_debug_close);
    }
}

/* Info menu item; stream test variables in the sound entry. */
/* 0x0016bd38 */
static void pcm_debug_menu_add(void)
{
    int n;
    int group;

    menu_item_add(info_menu_name, "PcmStream", pcm_debug_open, 0, 0);
    n = sound_get_varedit();
    group = gw_varedit_add(&gw_varedit_root, n, &pcmstream_vardef);
    pcmstream_vars_group = group;
    gw_varedit_add_table(&gw_varedit_root, group, &pcmstream_vardefs);
}

/* 0x0016bdb0 */
static void pcm_debug_menu_remove(void)
{
    int parent;

    menu_item_remove(info_menu_name, "PcmStream");
    parent = sound_get_varedit();
    gw_varedit_remove(&gw_varedit_root, parent, pcmstream_vars_group);
}

/* pcmstream_play_file ("s_title1.adx" or "s_s01_c.adp") at pcmstream_play_index. */
/* 0x0016bdf8 */
static void pcm_test_play(void)
{
    pcm_play_index(&pcm_test_entries[pcmstream_play_file], 1, pcmstream_play_index);
}

/* As pcm_test_play, without starting. */
/* 0x0016be20 */
static void pcm_test_load(void)
{
    pcm_load_index(&pcm_test_entries[pcmstream_play_file], 1, pcmstream_play_index);
}

/* Starts the loaded stream on the next step of grid pcmstream_play_div, then ends. */
/* 0x0016be48 */
static void testplay_exec(void *work)
{
    if (beat_grid_hit(pcmstream_play_div)) {
        pcm_start();
        if (work)
            TASK_OF(work)->flags |= TASK_F_END;
    }
}

/* Under the system task. */
/* 0x0016be98 */
static void testplay_create(void)
{
    task_create(system1_work, &testplay_desc, 0);
}

/* 0x0016beb0 */
static void sound_test_bank_select(void)
{
}

/* Selected bank and program. */
/* 0x0016beb8 */
static void sound_test_midi_play(void)
{
    se_direct_note_on(sound_test_bank, sound_test_prg, sound_test_note);
}

/* With its two variable groups. */
/* 0x0016bed8 */
void sound_test_menu_add(void)
{
    struct GwVarNode *group;
    GwVarGroupDef *item;
    GwVarGroupDef *end;

    group = gw_varedit_add(&gw_varedit_root, game_varedit_get(), &sound_test_vardef);
    item = sound_test_vardefs;
    end = item + 2;
    do {
        gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, group, item), item->table);
        item++;
    } while (item < end);
}

/* 0x0016bf70 */
static void sound_test_menu_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, game_varedit_get(), &sound_test_vardef);
}
