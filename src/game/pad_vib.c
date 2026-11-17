/*
 * Vibration cues: peri_act.pak actions on the controller (Trance Vibrator: own table) on the beat
 * and on game events. Per stage, an action record per scene row: one per beat of the bar, the
 * button, and events (player death, boss break, ...).
 */

#include "types_game.h"
#include "mathdef.h"
#include "libc.h"
#include "pad_vib.h"
#include "callback.h"
#include "config.h"
#include "game.h"
#include "map_manager.h"
#include "pad.h"
#include "peri_act.h"
#include "peri_vib.h"
#include "sound_beat.h"

typedef struct {
    char pad_00[6];
    short life;
    char pad_08[16];
    char *name;
} PeriActEventObj;

typedef struct {
    PeriActEventObj *obj;
} PeriActEventArg;

typedef struct {
    int channel;
    int count;
    int dur;
    int val;
} PeriActVib;

/* Header words, then one action table per action number (1-based). */
typedef struct {
    void *pack;
    int count;
    int *tables[1];
} PeriActPack;

typedef struct {
    int channel;
    int count;
    int dur0;
    int val0;
    int dur1;
    int val1;
    int dur2;
    int val2;
    int dur3;
    int val3;
} PeriActPattern;

extern int peri_act_pulse_on;
extern int peri_act_row;
extern char *peri_act_rec;
extern int peri_act_pack;
extern int peri_act_port;
extern PadSystem pad_system;
extern PeriActVib peri_act_vibs[];
extern PeriActPattern peri_act_patterns[];
extern char peri_act_pak_file[];
extern PeriActRecView peri_act_recs[];
extern int peri_act_rec_idx;
extern int peri_act_lsn_opening;
extern int peri_act_lsn_indexed;
extern int peri_act_lsn_if_less;
extern int peri_act_lsn_scene;
extern int peri_act_lsn_vibration;
extern int peri_act_lsn_pattern;
extern int peri_act_lsn_play0;
extern int peri_act_lsn_play2;
extern unsigned int stage_exit_flags;
extern PeriActRecView peri_act_recs_alt[];
extern void peri_act_play_network_opening(PeriActEventArg *ev);

/* At 0.8 of the previous beat: the record's action for this scene row and beat of the bar. */
/* 0x00143a20 */
void peri_act_beat_pulse(void)
{
    int n;

    if (beat_grid_hit(0))
        peri_act_pulse_on = 0;
    if (peri_act_pulse_on == 0) {
        if (beat_clock_get_phase() > 0.8f) {
            peri_act_pulse_on = 1;
            n = 0;
            switch (beat_clock_get_haku() % 4) {
            case 0:
                n = *(int *)(peri_act_rec + peri_act_row * 4 + 0);
                break;
            case 1:
                n = *(int *)(peri_act_rec + peri_act_row * 4 + 76);
                break;
            case 2:
                n = *(int *)(peri_act_rec + peri_act_row * 4 + 152);
                break;
            case 3:
                n = *(int *)(peri_act_rec + peri_act_row * 4 + 228);
                break;
            }
            if (n > 0)
                peri_act_play_pack_entry(peri_act_pack, n - 1, peri_act_port, 0);
        }
    }
}

/* While button 4 is held. */
/* 0x00143b98 */
void peri_act_play_button(void)
{
    int port = peri_act_port;
    int n;

    if (pad_system.pad[port]->menu.on & PAD_A) {
        n = *(int *)(peri_act_rec + peri_act_row * 4 + 380);
        if (n > 0)
            peri_act_play_pack_entry(peri_act_pack, n - 1, port, 0);
    }
}

/* Listener: opening action for a live "Network Opening" object. */
/* 0x00143c00 */
void peri_act_play_network_opening(PeriActEventArg *ev)
{
    int n;

    if (strcmp(ev->obj->name, "Network Opening") == 0) {
        if (ev->obj->life > 0) {
            n = (*(struct { char pad[456] ; int act ; } * *)&peri_act_rec)->act; /* steer(cast) */
            if (n > 0)
                peri_act_play_pack_entry(peri_act_pack, n - 1, peri_act_port, 0);
        }
    }
}

/* Listener: the record's action for the current scene row and this event. */
/* 0x00143c80 */
void peri_act_play_indexed(void)
{
    int n = *(int *)(peri_act_rec + peri_act_row * 4 + 304);

    if (n > 0)
        peri_act_play_pack_entry(peri_act_pack, n - 1, peri_act_port, 0);
}

/* Listener: act[1] when pair->b drops below pair->a. */
/* 0x00143cc8 */
void peri_act_play_if_less(Pair *pair)
{
    int n;

    if (pair->b < pair->a) {
        n = (*(PeriActRec * *)&peri_act_rec)->act[1]; /* steer(cast) */
        if (n > 0)
            peri_act_play_pack_entry(peri_act_pack, n - 1, peri_act_port, 0);
    }
}

/*
 * Listener: one steady vibration per channel until the next bar line (bar = 240 / bpm s), or the
 * one after when over a quarter of the bar has gone.
 */
/* 0x00143d10 */
void peri_act_vibration_start(void)
{
    int n = *(int *)(peri_act_rec + 460);
    int dur;
    float bar;
    float bar_end;
    float fd;
    int *src;
    int cnt;
    int i;

    if (n != 0) {
        dur = (int)(240.0f / beat_clock_get_bpm() * FPS);
        bar = beat_clock_get_bar();
        bar_end = 0.25f;
        if (bar <= bar_end)
            bar_end = 1.0f;
        else
            bar_end = 2.0f;
        fd = (float)dur;
        do { /* steer(barrier) */
        } while (0);
        peri_act_vibs[0].dur = peri_act_vibs[1].dur = peri_act_vibs[2].dur = (int)((bar_end - bar) * fd);
        /* Always 0: never resets the port. */
        if ((n & 1) >> 1)
            peri_act_port = 0;
        src = (*(PeriActPack * *)&peri_act_pack)->tables[n - 1]; /* steer(cast) */
        cnt = *src++;
        for (i = 0; i < cnt; i++) {
            peri_act_vibs[i].channel = src[0];
            peri_act_vibs[i].count = 1;
            src += 3;
            peri_act_vibs[i].val = *src++;
            peri_act_queue_table(peri_act_port, 0, &peri_act_vibs[i].channel);
            src += 2;
        }
    }
}

/*
 * Listener: four-step ramp per channel from a quarter of the way between the entry's strengths
 * up to the second, over 0.2, 0.2, 0.1, 0.5 bars.
 */
/* 0x00143e70 */
void peri_act_pattern_start(void)
{
    int n = *(int *)(peri_act_rec + 460);
    int dur;
    float fd;
    int *src;
    int cnt;
    int i;
    int len;
    int start;
    int base;

    if (n != 0) {
        i = 0;
        dur = (int)(240.0f / beat_clock_get_bpm() * FPS);
        src = (*(PeriActPack * *)&peri_act_pack)->tables[n - 1]; /* steer(cast) */
        cnt = *src++;
        if (cnt > 0) {
            fd = (float)dur;
            for (; i < cnt; i++) {
                peri_act_patterns[i].channel = src[0];
                peri_act_patterns[i].count = 4;
                src += 3;
                start = src[0];
                src += 2;
                len = src[0] - start;
                base = (int)(fd * 0.2f);
                peri_act_patterns[i].val0 = start + len / 4;
                peri_act_patterns[i].val1 = start + len / 2;
                peri_act_patterns[i].val2 = start + len * 3 / 4;
                peri_act_patterns[i].val3 = start + len;
                peri_act_patterns[i].dur0 = base;
                peri_act_patterns[i].dur1 = base;
                peri_act_patterns[i].dur2 = (int)(fd * 0.1f);
                peri_act_patterns[i].dur3 = (int)(fd * 0.5f);
                peri_act_queue_table(peri_act_port, 0, &peri_act_patterns[i].channel);
                src += 1;
            }
        }
    }
}

/* Listener: act[0]. */
/* 0x00144068 */
void peri_act_play_symbol_end(void)
{
    int n = (*(PeriActRec * *)&peri_act_rec)->act[0]; /* steer(cast) */

    if (n > 0)
        peri_act_play_pack_entry(peri_act_pack, n - 1, peri_act_port, 0);
}

/* Listener: moves the scene row forward to a new scene (never back). */
/* 0x001440a0 */
void peri_act_sync_scene(int *p)
{
    if (map_manager_scene_flag(p[0]) == 0)
        peri_act_row = (peri_act_row < map_manager_get_scene_id(p[0]) + 1) ? map_manager_get_scene_id(p[0]) + 1 : peri_act_row;
}

/* Listener. */
/* 0x00144118 */
void peri_act_play_player_dead(void)
{
    int n = (*(PeriActRec * *)&peri_act_rec)->act[2]; /* steer(cast) */

    if (n > 0)
        peri_act_play_pack_entry(peri_act_pack, n - 1, peri_act_port, 0);
}

/* steer(cast) */
/* 0x00144150 */
void peri_act_pak_open(void)
{
    peri_act_pack = ((int (*)(char *))peri_act_pack_load)(peri_act_pak_file);
}

/* 0x00144178 */
void peri_act_pak_close(void)
{
    peri_act_pack_free(peri_act_pack);
    peri_act_pack = 0;
}

/* Record idx at scene row 0; installs the listeners (removed by peri_act_setup). */
/* steer(cast) */
/* 0x001441a8 */
void peri_act_start(int idx)
{
    peri_act_rec_idx = idx;
    peri_act_rec = &peri_act_recs[idx];
    peri_act_row = 0;
    peri_act_lsn_opening = ((int (*)(int, void (*)()))listener_add)(11, peri_act_play_network_opening);
    peri_act_lsn_indexed = ((int (*)(int, void (*)()))listener_add)(4, peri_act_play_indexed);
    peri_act_lsn_if_less = ((int (*)(int, void (*)()))listener_add)(19, peri_act_play_if_less);
    peri_act_lsn_play2 = ((int (*)(int, void (*)()))listener_add)(22, peri_act_play_player_dead);
    peri_act_lsn_scene = ((int (*)(int, void (*)()))listener_add)(24, peri_act_sync_scene);
    peri_act_lsn_vibration = ((int (*)(int, void (*)()))listener_add)(7, peri_act_vibration_start);
    peri_act_lsn_pattern = ((int (*)(int, void (*)()))listener_add)(8, peri_act_pattern_start);
    peri_act_lsn_play0 = ((int (*)(int, void (*)()))listener_add)(9, peri_act_play_symbol_end);
}

/* Stops the cues: record 0 (scene row kept if stage_exit_flags bit 1), listeners removed. */
/* 0x001442a8 */
void peri_act_setup(void)
{
    peri_act_rec = peri_act_recs;
    peri_act_rec_idx = 0;
    if (!(stage_exit_flags & 2))
        peri_act_row = 0;
    listener_remove(11, peri_act_lsn_opening);
    listener_remove(4, peri_act_lsn_indexed);
    listener_remove(19, peri_act_lsn_if_less);
    listener_remove(22, peri_act_lsn_play2);
    listener_remove(24, peri_act_lsn_scene);
    listener_remove(7, peri_act_lsn_vibration);
    listener_remove(8, peri_act_lsn_pattern);
    listener_remove(9, peri_act_lsn_play0);
    peri_act_destroy_all();
}

/* Row 0 also stops every action. */
/* 0x00144370 */
void peri_act_select_reset(int row)
{
    peri_act_rec_idx = 0;
    peri_act_row = row;
    if (row == 0)
        peri_act_destroy_all();
}

/* Trance Vibrator records when one is connected, else the controller's. */
/* 0x00144398 */
void peri_act_update(void)
{
    if (peri_vib_is_connected())
        peri_act_rec = &peri_act_recs_alt[peri_act_rec_idx];
    else
        peri_act_rec = &peri_act_recs[peri_act_rec_idx];
    peri_act_beat_pulse();
    peri_act_play_button();
}

/* 0x001443f8 */
void peri_act_set_port(int port)
{
    peri_act_port = port;
}

/* 0x00144408 */
void peri_act_set_selection(int row)
{
    peri_act_row = row;
}

/* 1-based, 0 = nothing; Trance Vibrator versions are numbered 10 higher. */
/* 0x00144418 */
void peri_act_play_number(int n)
{
    if (n > 0) {
        if (peri_vib_is_connected())
            n += 10;
        peri_act_play_pack_entry(peri_act_pack, n - 1, peri_act_port, 0);
    }
}

/* Scene row 17. */
/* 0x00144478 */
void peri_act_select_boss(void)
{
    peri_act_row = 17;
}

/* Scene row 18; in game type 5, extra type 3, plays its beat action each beat. */
/* 0x00144488 */
void peri_act_beat_tick(void)
{
    int mode;

    peri_act_row = 18;
    if (game_type_get() == 5) {
        mode = ConfigGetExtraType();
        if (mode == 3) {
            if (beat_grid_hit(0)) {
                int n = 0;

                switch (beat_clock_get_haku() % 4) {
                case 0:
                    n = *(int *)(peri_act_rec + peri_act_row * 4 + 0);
                    break;
                case 1:
                    n = *(int *)(peri_act_rec + peri_act_row * 4 + 76);
                    break;
                case 2:
                    n = *(int *)(peri_act_rec + peri_act_row * 4 + 152);
                    break;
                case 3:
                    n = *(int *)(peri_act_rec + peri_act_row * 4 + 228);
                    break;
                }
                if (n > 0)
                    peri_act_play_pack_entry(peri_act_pack, n - 1, peri_act_port, 0);
            }
        }
    }
}

/* 0x001445e8 */
void peri_act_play_boss_break(void)
{
    int n = (*(PeriActRec * *)&peri_act_rec)->act[3]; /* steer(cast) */

    if (n > 0)
        peri_act_play_pack_entry(peri_act_pack, n - 1, peri_act_port, 0);
}

/* 0x00144620 */
void peri_act_play_boss_finish(void)
{
    int n = (*(PeriActRec * *)&peri_act_rec)->act[4]; /* steer(cast) */

    if (n > 0)
        peri_act_play_pack_entry(peri_act_pack, n - 1, peri_act_port, 0);
}
