/*
 * Boss music and sound: the BPM task's beat grid, rhythm sound-effect tables, BGM tasks that
 * switch sequencer tracks and fade volume, the BGM manager (PCM streams, sequence switch on a
 * bar line), the looping laser sound and a debug sound log.
 */

#include "types_common.h"
#include "color.h"

#include "mathdef.h"
#include "types_overlays.h"

#include "debug.h"
#include "enemy_status.h"
#include "file.h"
#include "heap.h"
#include "libc.h"
#include "nj_strip.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "sound_pcmstream.h"
#include "ul_math.h"
#include "task.h"
#include "boss_sound.h"

BgmMan *cm_bgm_sound_man_create(void *parent, void (*cb)(BgmMan *b));

extern BgmMan *cm_bgm_sound_man;
extern char *sound_log_states[8];
extern char *vol_phase_names[8];
extern struct TaskDesc cm_sound_bpm_desc;
extern struct TaskDesc cm_bgm_sound_desc;
extern char str_midiseq_mute[];
extern struct TaskDesc cm_bgm_sound_man_desc;
extern struct TaskDesc cm_bgm_sound_sub_desc;
extern struct TaskDesc cm_se_laser_desc;
extern char str_err2[];
extern char str_null[];
extern char str_s[];
extern char str_log_entry[];
extern char str_log_err2[];
extern char str_vol_task[];
extern char str_mssub_task[];
extern char sound_log[16][128];
extern int se_grid;
extern int sound_disp_x;
extern int sound_disp_y;
extern int sound_log_head;
extern int sound_log_calls;
extern int sound_debug;
extern char str_s_d[];
extern char str_s_d_err[];

/* 0x00cc27d8 b_earth, 0x00cd4d18 b_eden, 0x00cc1380 b_mars, 0x00cc5ab0 b_uranus, 0x00cc87e8 b_venus */
void se_grid_set(int vol)
{
    se_grid = vol;
}

/* 0x00cc27e8 b_earth, 0x00cd4d28 b_eden, 0x00cc1390 b_mars, 0x00cc5ac0 b_uranus, 0x00cc87f8 b_venus */
void cm_sound_bpm_exec(CmSoundBpmWork *w)
{
    int n = enemy_status_get_ticks();
    float t, rate;
    float sc[2];

    for (; n; n--) {
        if (--w->count <= 0)
            if (w)
                FINISH(w);
    }
    t = (float)w->count / w->total;
    rate = 1.0f - t;
    switch (w->curve) {
    case 1:
        ulSinCosf(rate * PI, &sc[0], &sc[1]);
        rate = sc[1] * -0.5f + 0.5f;
        break;
    case 2:
        rate = rate * rate;
        break;
    case 3:
        rate = (1.0f - rate) * (rate - 1.0f) + 1.0f;
        break;
    }
    w->cur = w->to * rate + w->from * (1.0f - rate);
    beat_clock_request_tempo(w->cur);
}

/* 0x00cc2930 b_earth, 0x00cd4e70 b_eden, 0x00cc14d8 b_mars, 0x00cc5c08 b_uranus, 0x00cc8940 b_venus */
void cm_sound_bpm_release(CmSoundBpmWork *w)
{
    beat_clock_request_tempo(w->to);
}

/* 0x00cc2938 b_earth, 0x00cd4e78 b_eden, 0x00cc14e0 b_mars, 0x00cc5c10 b_uranus, 0x00cc8948 b_venus */
CmSoundBpmWork *cm_sound_bpm_create(void *parent, int curve, float from, float delta, float secs)
{
    CmSoundBpmWork *w = task_create(parent, &cm_sound_bpm_desc, 28);

    if (from <= 0.0f)
        w->from = beat_clock_get_bpm();
    else
        w->from = from;
    w->cur = w->from;
    if (curve < 0) {
        w->to = w->from + delta;
        curve &= 0xffff;
    } else {
        w->to = delta;
    }
    w->curve = curve;
    w->unk_00 = 0;
    w->count = w->total = secs * FPS;
    return w;
}

/* 0x00cc2a08 b_earth, 0x00cd4f48 b_eden, 0x00cc15b0 b_mars, 0x00cc5ce0 b_uranus, 0x00cc8a18 b_venus */
int se_table_play(int owner, BossSe *tab, int id, int lock, int pos)
{
    BossSe *se = &tab[id];
    BossSe *start_se;
    int res, lo, hi;

    if (se->id != id) {
        for (se = tab; se->id != -1; se++) {
            if (se->id == id)
                break;
        }
        if (se->id == -1)
            return 5;
    }
    /* the lock and pos terms are always 0 */
    switch (se->kind + ((lock & 1) >> 1) + ((pos & 1) >> 1)) {
    case 0:
        res = 0;
        SoundRhythmSe_Boss(se->ch, se->note - 12, se_grid);
        break;
    case 1:
        res = 1;
        SoundRhythmSe_Boss(se->ch, se->note - 12, se->arg);
        break;
    case 2:
        if (!se->arg) {
            res = 2;
            SoundRhythmSe_BossStart(se->ch, se->note - 12, se_grid);
        } else {
            res = 6;
        }
        se->arg++;
        break;
    case 3:
        start_se = &tab[se->arg];
        if (start_se->arg > 0 && --start_se->arg == 0) {
            res = 3;
            SoundRhythmSe_BossStop(start_se->ch, start_se->note - 12, se_grid);
        } else {
            res = 6;
        }
        break;
    case 4:
        res = 4;
        lo = se->arg & 0xffff;
        hi = (unsigned int)se->arg >> 16;
        SoundRhythmSe_Boss(se->ch, lo + (int)(RAND_FLOAT_A() * (hi - lo + 1)) - 12,
                      se_grid);
        break;
    default:
        return 5;
    }
    if (sound_debug)
        sound_log_entry((BossLogEnt *)se, 0, res);
    return res;
}

/* 0x00cc2c00 b_earth, 0x00cd5140 b_eden, 0x00cc17a8 b_mars, 0x00cc5ed8 b_uranus, 0x00cc8c10 b_venus */
void se_table_reset(BossSe *se)
{
    for (; se->id != -1; se++) {
        if (se->kind == 2)
            se->arg = 0;
    }
}

/* 0x00cc2c38 b_earth, 0x00cd5178 b_eden, 0x00cc17e0 b_mars, 0x00cc5f10 b_uranus, 0x00cc8c48 b_venus */
void se_table_stop(BossSe *se)
{
    for (; se->id != -1; se++) {
        if (se->kind == 2)
            SoundRhythmSe_BossStop(se->ch, se->note - 12, se_grid);
    }
}

/* 0x00cc2cb8 b_earth, 0x00cd51f8 b_eden, 0x00cc1860 b_mars, 0x00cc5f90 b_uranus, 0x00cc8cc8 b_venus */
void cm_bgm_sound_exec(CmBgmSoundWork *w)
{
    int n;
    int vol;
    int *pair;

    for (n = enemy_status_get_ticks(); n != 0; n--) {
        switch (w->phase) {
        case 0:
            if (--w->count > 0)
                break;
            midiseq_track_enable(w->def[2], 1);
            midiseq_track_volume(w->def[2], w->vol_from);
            w->phase++;
            w->value = w->vol_last = w->vol_from;
            w->count = w->total = ((float *)w->def)[4] * FPS;
            break;
        case 1:
            w->value = w->vol_from + (w->vol_to - w->vol_from) * (1.0f - (float)w->count / w->total);
            if (fabsf(w->value - w->vol_last) > 5.0f) {
                w->vol_last = w->value;
                midiseq_track_volume(w->def[2], w->value);
            }
            if (--w->count > 0)
                break;
            w->phase++;
            w->count = w->total = ((float *)w->def)[5] * FPS;
            break;
        case 2:
            if (w->count > 0) {
                if (--w->count > 0)
                    break;
                w->phase = 3;
                w->count = w->total = ((float *)w->def)[6] * FPS;
                break;
            }
            w->value = w->vol_to;
            if (fabsf(w->value - w->vol_last) > 5.0f) {
                w->vol_last = w->value;
                midiseq_track_volume(w->def[2], w->value);
            }
            break;
        case 3:
            w->count--;
            vol = w->vol_to - (w->vol_to - w->vol_from) * (1.0f - (float)w->count / w->total);
            w->value = vol;
            if (fabsf(vol - w->vol_last) > 5.0f) {
                w->vol_last = vol;
                midiseq_track_volume(w->def[2], vol);
            }
            if (w->count > 0)
                break;
            FINISH_IF(w);
            break;
        }
    }
    pair = (int *)w->arg;
    if (pair && w->phase == 2) {
        int full = pair[0], left = pair[1];
        if (left <= 0) {
            FINISH_IF(w);
            return;
        }
        w->vol_to = w->def[7] + (w->def[8] - w->def[7]) * ((float)left / full);
    }
}

/* 0x00cc3038 b_earth, 0x00cd5578 b_eden, 0x00cc1be0 b_mars, 0x00cc6310 b_uranus, 0x00cc9048 b_venus */
void cm_bgm_sound_release(CmBgmSoundWork *w)
{
    midiseq_track_enable(w->def[2], 0);
}

/* 0x00cc3048 b_earth, 0x00cd5588 b_eden, 0x00cc1bf0 b_mars, 0x00cc6320 b_uranus, 0x00cc9058 b_venus */
CmBgmSoundWork *cm_bgm_sound_create(void *parent, int *def, int arg)
{
    CmBgmSoundWork *w = task_create(parent, &cm_bgm_sound_desc, 40);

    w->arg = arg;
    w->def = def;
    w->count = w->total = *(float *)&def[3] * FPS;
    w->vol_from = def[7];
    w->vol_to = def[8];
    w->kind = 1;
    return w;
}

/* 0x00cc30c8 b_earth, 0x00cd5608 b_eden, 0x00cc1c70 b_mars, 0x00cc63a0 b_uranus, 0x00cc90d8 b_venus */
void cm_bgm_sound_man_release(BgmMan *man)
{
    BossEnt *ent = man->ent;
    int i;

    ((void (*)(BgmMan *))man->release_cb)(man);
    for (i = 0; i < man->nent; i++, ent++) {
        switch (ent->busy) {
        case 1:
            file_request_cancel(ent->file);
            break;
        case 2:
            heap_free((void *)ent->data, 0);
            break;
        }
    }
    se_laser_bank_clear();
    if (man->loaded)
        heap_free((void *)man->loaded, 0);
    midiseq_clear_sub();
    if (man->pcm > 0)
        pcm_stop();
}

/* 0x00cc31c0 b_earth, 0x00cd5700 b_eden, 0x00cc1d68 b_mars, 0x00cc6498 b_uranus, 0x00cc91d0 b_venus */
void cm_bgm_sound_man_exec(BgmMan *man)
{
    BossEnt *ent;
    int i, busy;

    if (man->nreq) {
        for (ent = man->ent, i = 0; i < man->nent; i++, ent++) {
            busy = ent->busy;
            if (busy != 1)
                continue;
            if (file_request_status(ent->file) != busy)
                continue;
            ent->data = file_load_sync(ent->file, 0, 0);
            ent->busy = 2;
            if (--man->nreq <= 0)
                break;
        }
    }
    switch (man->pcm) {
    case 2:
        man->pcm = 3;
        break;
    case 3:
        if (pcm_is_playing())
            man->pcm++;
        break;
    case 4:
        if (!pcm_is_playing())
            man->pcm = 0;
        break;
    }
}

/* 0x00cc32e8 b_earth, 0x00cd5828 b_eden, 0x00cc1e90 b_mars, 0x00cc65c0 b_uranus, 0x00cc92f8 b_venus */
void sound_debug_toggle(void)
{
    BgmMan *man = cm_bgm_sound_man;

    sound_debug = man->debug ^= 1;
}

/* 0x00cc3308 b_earth, 0x00cd5848 b_eden, 0x00cc1eb0 b_mars, 0x00cc65e0 b_uranus, 0x00cc9318 b_venus */
void cm_bgm_sound_man_end_children(BgmMan *man)
{
    Task *child;
    int *cur, *nxt, *p;

    if (!man)
        man = cm_bgm_sound_man;
    child = (Task *)TASK_OF(man)->child;
    p = child ? (int *)(child + 1) : 0;
    if (p) {
        do {
            Task *sib;

            cur = p;
            sib = (Task *)TASK_OF(cur)->next;
            nxt = sib ? (int *)(sib + 1) : 0;
            p = nxt;
            FINISH_IF(cur);
        } while (nxt);
    }
}

/* 0x00cc3368 b_earth, 0x00cd58a8 b_eden, 0x00cc1f10 b_mars, 0x00cc6640 b_uranus, 0x00cc9378 b_venus */
void cm_bgm_sound_man_end(BgmMan *man)
{
    if (!man)
        man = cm_bgm_sound_man;
    do {
        if (man)
            FINISH(man);
    } while (0);
}

/* 0x00cc3398 b_earth, 0x00cd58d8 b_eden, 0x00cc1f40 b_mars, 0x00cc6670 b_uranus, 0x00cc93a8 b_venus */
void cm_bgm_sound_set_arg(CmBgmSoundWork *w, int vol)
{
    w->vol_from = vol;
}

/* 0x00cc33a0 b_earth, 0x00cd58e0 b_eden, 0x00cc1f48 b_mars, 0x00cc6678 b_uranus, 0x00cc93b0 b_venus */
int vol_table_play(BgmMan *man, BossVolDef *tab, int id, int arg)
{
    BossVolDef *ent = &tab[id];
    char buf[64];

    if (!man)
        man = cm_bgm_sound_man;
    if (ent->id != id) {
        for (ent = tab; ent->id != -1; ent++) {
            if (ent->id == id)
                break;
        }
        if (ent->id == -1)
            return 0;
    }
    switch ((unsigned int)ent->kind) {
    case 0:
        cm_bgm_sound_set_arg((CmBgmSoundWork *)man, ent->arg);
        midiseq_mute_request(ent->arg);
        if (sound_debug) {
            sprintf(buf, str_midiseq_mute, ent->arg);
            sound_log_add(buf);
        }
        return 1;
    case 1:
    case 3:
        cm_bgm_sound_create(man, (int *)ent, arg);
        return 1;
    }
    return 0;
}

/* 0x00cc3480 b_earth, 0x00cd59c0 b_eden, 0x00cc2028 b_mars, 0x00cc6758 b_uranus, 0x00cc9490 b_venus */
void vol_table_stop(BgmMan *man, BossVolDef *tab, int id)
{
    BossVolDef *ent = &tab[id];
    Task *child;
    int *p, *nxt;

    if (!man)
        man = cm_bgm_sound_man;
    if (ent->id != id) {
        for (ent = tab; ent->id != -1; ent++) {
            if (ent->id == id)
                break;
        }
        if (ent->id == -1)
            return;
    }
    midiseq_track_enable(ent->arg, 0);
    child = (Task *)TASK_OF(man)->child;
    p = child ? (int *)(child + 1) : 0;
    if (p) {
        do {
            int *cur = p;
            Task *sib = (Task *)TASK_OF(cur)->next;

            nxt = sib ? (int *)(sib + 1) : 0;
            p = nxt;
            if (cur[0] == 1 && cur[8] == (int)ent)
                FINISH(cur);
        } while (nxt);
    }
}

/* 0x00cc3560 b_earth, 0x00cd5aa0 b_eden, 0x00cc2108 b_mars, 0x00cc6838 b_uranus, 0x00cc9570 b_venus */
void boss_stream_load(int entry, int pcm_arg)
{
    BgmMan *man = cm_bgm_sound_man;

    if (!man->pcm) {
        pcm_load(entry, pcm_arg);
        man->pcm = 1;
    }
}

/* 0x00cc35a0 b_earth, 0x00cd5ae0 b_eden, 0x00cc2148 b_mars, 0x00cc6878 b_uranus, 0x00cc95b0 b_venus */
void boss_stream_start(void)
{
    BgmMan *man = cm_bgm_sound_man;

    if (man->pcm == 1) {
        pcm_start();
        man->pcm = 2;
    }
}

/* 0x00cc35e8 b_earth, 0x00cd5b28 b_eden, 0x00cc2190 b_mars, 0x00cc68c0 b_uranus, 0x00cc95f8 b_venus */
void boss_stream_load_index(int entry, int pcm_arg, int idx)
{
    BgmMan *man = cm_bgm_sound_man;

    if (!man->pcm) {
        pcm_load_index(entry, pcm_arg, idx);
        man->pcm = 1;
    }
}

/* 0x00cc3628 b_earth, 0x00cd5b68 b_eden, 0x00cc21d0 b_mars, 0x00cc6900 b_uranus, 0x00cc9638 b_venus */
void cm_bgm_sound_man_set_stream(int pcm)
{
    cm_bgm_sound_man->pcm = pcm;
}

/* 0x00cc3638 b_earth, 0x00cd5b78 b_eden, 0x00cc21e0 b_mars, 0x00cc6910 b_uranus, 0x00cc9648 b_venus */
BgmMan *cm_bgm_sound_man_create(void *parent, void (*cb)(BgmMan *man))
{
    BgmMan *man = task_create(parent, &cm_bgm_sound_man_desc, 564);

    man->release_cb = (int)cb;
    cm_bgm_sound_man = man;
    sound_debug = 0;
    sound_log_clear();
    return man;
}

/* 0x00cc3690 b_earth, 0x00cd5bd0 b_eden, 0x00cc2238 b_mars, 0x00cc6968 b_uranus, 0x00cc96a0 b_venus */
void cm_bgm_sound_sub_release(CmBgmSoundSubWork *w)
{
    if (w->on && w->started && !w->boss->pcm)
        pcm_stop();
}

/* 0x00cc36c8 b_earth, 0x00cd5c08 b_eden, 0x00cc2270 b_mars, 0x00cc69a0 b_uranus, 0x00cc96d8 b_venus */
void cm_bgm_sound_sub_exec(CmBgmSoundSubWork *w)
{
    char buf[64];
    void *seq;

    switch (w->phase) {
    case 0:
        if (!beat_grid_hit(0))
            break;
        if (beat_clock_get_haku() & 3)
            break;
        w->n--;
        if (w->on) {
            if (w->n == 1) {
                if (w->idx >= 0)
                    pcm_load_index(w->ent->pcm, 1, w->idx);
                else
                    pcm_load(w->ent->pcm, 1);
            }
            w->started = 1;
        }
        if (w->n > 0)
            break;
        w->phase++;
    case 1:
        if (!beat_grid_hit(0))
            break;
        if (beat_clock_get_haku() & 3)
            break;
        if (w->seq_idx >= 0) {
            seq = PackFileGet((void *)w->ent->data, w->seq_idx, 0);
            if (seq) {
                midiseq_set_sub(seq);
                if (sound_debug) {
                    sprintf(buf, str_s_d, ((char **)w->ent->file)[1], w->seq_idx);
                    sound_log_add(buf);
                }
            } else if (sound_debug) {
                sprintf(buf, str_s_d_err, ((char **)w->ent->file)[1], w->seq_idx);
                sound_log_add(buf);
            }
        } else {
            midiseq_set_sub((void *)w->ent->data);
            if (sound_debug)
                sound_log_add(((char **)w->ent->file)[1]);
        }
        w->phase++;
        break;
    case 2:
        if (!beat_grid_hit(0))
            break;
        if (beat_clock_get_haku() & 3)
            break;
        if (w->cb)
            ((void (*)(int))w->cb)(w->cb_arg);
        if (w->on) {
            pcm_start();
            if (sound_debug) {
                if (w->idx >= 0) {
                    sprintf(buf, str_s_d, ((char **)w->ent->pcm)[1], w->idx);
                    sound_log_add(buf);
                } else {
                    sound_log_add(((char **)w->ent->pcm)[1]);
                }
            }
            w->phase++;
            break;
        }
        FINISH_IF(w);
        break;
    case 3:
        do {
            if (!w->boss->pcm && pcm_is_playing())
                break;
            if (w)
                FINISH(w);
        } while (0);
        break;
    }
}

/* 0x00cc3958 b_earth, 0x00cd5e98 b_eden, 0x00cc2500 b_mars, 0x00cc6c30 b_uranus, 0x00cc9968 b_venus */
void cm_bgm_sound_sub_create(BgmMan *man, int cb, int cb_arg, int slot, int n, int useidx, int idx, int seq_idx, int on)
{
    CmBgmSoundSubWork *w;
    BossEnt *ent;

    if (!man)
        man = cm_bgm_sound_man;
    w = task_create(man, &cm_bgm_sound_sub_desc, 44);
    w->ent = ent = &man->ent[slot];
    w->boss = man;
    w->cb = cb;
    w->cb_arg = cb_arg;
    w->state = 2;
    w->n = n;
    w->on = on;
    if (on) {
        if (!useidx) {
            w->idx = -1;
            w->seq_idx = -1;
            if (n < 2) {
                pcm_load(ent->pcm, 1);
                w->started = 1;
            }
        } else {
            w->seq_idx = seq_idx;
            w->idx = idx;
            if (n < 2) {
                pcm_load_index(ent->pcm, 1, idx);
                w->started = 1;
            }
        }
    }
}

/* 0x00cc3a80 b_earth, 0x00cd5fc0 b_eden, 0x00cc2628 b_mars, 0x00cc6d58 b_uranus, 0x00cc9a90 b_venus */
void cm_bgm_sound_sub_create_idle(BgmMan *man, int cb, int cb_arg, int slot, int n)
{
    cm_bgm_sound_sub_create(man, cb, cb_arg, slot, n, 1, 0, 0, 0);
}

/* 0x00cc3aa8 b_earth, 0x00cd5fe8 b_eden, 0x00cc2650 b_mars, 0x00cc6d80 b_uranus, 0x00cc9ab8 b_venus */
void cm_bgm_sound_sub_start(BgmMan *man, int cb, int cb_arg, int slot, int n, int idx, int seq_idx)
{
    cm_bgm_sound_sub_create(man, cb, cb_arg, slot, n, 1, idx, seq_idx, 1);
}

/* 0x00cc3ad8 b_earth, 0x00cd6018 b_eden, 0x00cc2680 b_mars, 0x00cc6db0 b_uranus, 0x00cc9ae8 b_venus */
void cm_bgm_sound_man_add_file(BgmMan *man, void *file, int pcm)
{
    BossEnt *ent;

    if (!man)
        man = cm_bgm_sound_man;
    ent = &man->ent[man->nent];
    ent->busy = 1;
    ent->pcm = pcm;
    ent->file = file;
    file_request_simple(file, 0);
    man->nent++;
    man->nreq++;
}

/* 0x00cc3b50 b_earth, 0x00cd6090 b_eden, 0x00cc26f8 b_mars, 0x00cc6e28 b_uranus, 0x00cc9b60 b_venus */
void cm_se_laser_exec(CmSeLaserWork *w)
{
    int step = w->step;

    switch (step) {
    case 0:
        w->step = 1;
        break;
    case 1:
        w->step = 2;
        file_request_simple(w->file, 0);
        w->state = step;
        break;
    case 2:
        if (file_request_status(w->file) && beat_grid_hit(0)) {
            w->boss->loaded = file_load_sync(w->file, 0, 0);
            se_laser_bank_set(w->boss->loaded);
            w->state = step;
            if (w->done)
                w->done(w->arg);
            do {
                if (w)
                    FINISH(w);
            } while (0);
        }
        break;
    }
}

/* 0x00cc3c40 b_earth, 0x00cd6180 b_eden, 0x00cc27e8 b_mars, 0x00cc6f18 b_uranus, 0x00cc9c50 b_venus */
void cm_se_laser_release(CmSeLaserWork *w)
{
    if (w->state == 1)
        file_request_cancel(w->file);
}

/* 0x00cc3c60 b_earth, 0x00cd61a0 b_eden, 0x00cc2808 b_mars, 0x00cc6f38 b_uranus, 0x00cc9c70 b_venus */
CmSeLaserWork *cm_se_laser_create(void *file)
{
    BgmMan *man = cm_bgm_sound_man;
    CmSeLaserWork *w = task_create(man, &cm_se_laser_desc, 28);
    int old_bank = man->loaded;

    w->file = file;
    w->unk_00 = 3;
    w->boss = man;
    if (old_bank)
        heap_free((void *)old_bank, 0);
    w->boss->loaded = 0;
    se_laser_bank_clear();
    return w;
}

/* 0x00cc3ce0 b_earth, 0x00cd6220 b_eden, 0x00cc2888 b_mars, 0x00cc6fb8 b_uranus, 0x00cc9cf0 b_venus */
void cm_se_laser_set_cb(CmSeLaserWork *w, void (*done)(int arg), int arg)
{
    w->done = done;
    w->arg = arg;
}

/* 0x00cc3cf0 b_earth, 0x00cd6230 b_eden, 0x00cc2898 b_mars, 0x00cc6fc8 b_uranus, 0x00cc9d00 b_venus */
void sound_log_clear(void)
{
    int i;

    for (i = 0; i < 16; i++)
        sprintf(sound_log[i], str_null);
}

/* 0x00cc3d48 b_earth, 0x00cd6288 b_eden, 0x00cc28f0 b_mars, 0x00cc7020 b_uranus, 0x00cc9d58 b_venus */
void sound_log_print(int x, int y)
{
    int i;

    sound_log_calls++;
    for (i = 0; i < 16; i++) {
        debug_set_origin(x, y++);
        debug_printf(str_s, sound_log[(sound_log_head + i) % 16]);
    }
}

/* 0x00cc3e18 b_earth, 0x00cd6358 b_eden, 0x00cc29c0 b_mars, 0x00cc70f0 b_uranus, 0x00cc9e28 b_venus */
void sound_log_entry(BossLogEnt *ent, int idx, unsigned int state)
{
    ent = &ent[idx];
    if (state >= 7)
        state = 7;
    if (ent->name)
        sprintf(sound_log[sound_log_head], str_log_entry, ent->name,
                     sound_log_states[state], ent->a, ent->unk_08);
    else
        sprintf(sound_log[sound_log_head], str_log_err2, str_err2,
                     sound_log_states[state], sound_log_calls);
    sound_log_head = (sound_log_head + 1) % 16;
}

/* 0x00cc3f10 b_earth, 0x00cd6450 b_eden, 0x00cc2ab8 b_mars, 0x00cc71e8 b_uranus, 0x00cc9f20 b_venus */
void sound_log_add(char *line)
{
    sprintf(sound_log[sound_log_head], line);
    sound_log_head = (sound_log_head + 1) % 16;
}

/* 0x00cc3f78 b_earth, 0x00cd64b8 b_eden, 0x00cc2b20 b_mars, 0x00cc7250 b_uranus, 0x00cc9f88 b_venus */
/* steer(cast) */
void sound_beat_bars_draw(int hit, float bar)
{
    float pts[4];
    float width = 2.0f;

    if (hit)
        width = 2.4f;
    pts[0] = 10.0f;
    pts[1] = pts[3] = 400.0f;
    pts[2] = 110.0f;
    ((void (*)(unsigned int, int, float))nj_flat_strip_begin)(COL_WHITE, 0, width);
    nj_flat_strip_vertices(pts, 2, 1.0f);
    nj_flat_strip_end();
    pts[1] = pts[3] = 392.0f;
    pts[2] = bar * 100.0f + 10.0f;
    ((void (*)(unsigned int, int, float))nj_flat_strip_begin)(RGBA(0, 0, 0x80, 0x80), 0, width);
    nj_flat_strip_vertices(pts, 2, 1.0f);
    nj_flat_strip_end();
}

/* 0x00cc4070 b_earth, 0x00cd65b0 b_eden, 0x00cc2c18 b_mars, 0x00cc7348 b_uranus, 0x00cca080 b_venus */
void cm_bgm_sound_man_disp(BgmMan *w)
{
    int *p;
    Task *child;
    int x, y;
    float bar;

    if (!w->debug || !debug_flags_test(4))
        return;
    child = (Task *)TASK_OF(w)->child;
    x = sound_disp_x;
    y = sound_disp_y;
    for (p = child ? (int *)(child + 1) : 0; p; ) {
        if (p[0] == 1) {
            debug_set_origin(x, y++);
            debug_printf(str_vol_task, *(int *)p[8], vol_phase_names[p[1]], p[4]);
        } else if (p[0] == 2) {
            debug_set_origin(x, y++);
            debug_printf(str_mssub_task);
        }
        {
            Task *sib = (Task *)TASK_OF(p)->next;

            p = sib ? (int *)(sib + 1) : 0;
        }
    }
    sound_log_print(x, y);
    bar = beat_clock_get_bar();
    sound_beat_bars_draw(beat_grid_hit(0), bar);
}
