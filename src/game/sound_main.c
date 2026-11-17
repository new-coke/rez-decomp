/*
 * Sound top level: starts the sound library and tasks at boot, rescues frozen sound DMA tasks,
 * applies stereo and the SE and music volumes (option levels, debug variables, muting).
 */

#include "task.h"
#include "mathdef.h"
#include "color.h"
#include "se_low.h"
#include "debug.h"
#include "adxt.h"
#include "gw_varedit.h"
#include "nj_system.h"
#include "ulsnd.h"
#include "sound_main.h"
#include "config.h"
#include "sound_bank.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "sound_midiseq.h"
#include "sound_pcmstream.h"

typedef struct SoundTask {
    void *dma_load;             /* 0x00 "DMALoad" task */
} SoundTask;

#define MAX(a, b) ((a) >= (b) ? (a) : (b))

extern struct GwVarNode *gw_varedit_root;
extern int sound_vardef;
extern int sound_vardefs;
extern int sound_varedit;
extern void *system1_work;
extern void *scr_root;
extern int sound_kprj_file[];

extern void *sound_task_work;

/* "DMALoad" task descriptor */
extern TaskDesc sound_dma_load_desc;

/* "Sound" task descriptor */
extern int sound_desc;

/* "SoundEnd" task descriptor */
extern int sound_end_desc;

/* "SoundStg" task descriptor */
extern int sound_stg_desc;
/* sound on (cleared while the volume is off) */
extern int sound_on;

/* volume for each of the five option levels */
extern float sound_volume_levels[5];

extern int sound_st;

extern int model_wire_buf;

/* SE volume (variable editor) */
extern float sound_vse;

/* sequence volume (variable editor) */
extern float sound_vseq;

static void *sound_task_create(void *parent);

/* 0x00165be0 */
void sound_vsync(void)
{
}

/*
 * Once at boot: library, "kprj.pml" project, sound test menu, variables, beat clock, "Sound" task
 * with rhythm SE, MIDI sequence and PCM stream tasks, stereo option.
 */
/* steer(cast) */
/* 0x00165be8 */
void sound_init(void)
{
    void *snd;

    sound_bank_debug_init();
    sound_mlt_load_file(sound_kprj_file);
    se_stg_common_clear();
    sound_test_menu_add();
    sound_varedit = ((int (*)(struct GwVarNode **, int, int *))gw_varedit_add)(&gw_varedit_root, 0, &sound_vardef);
    gw_varedit_add_table(&gw_varedit_root, sound_varedit, &sound_vardefs);
    beat_clock_create(system1_work);
    snd = sound_task_create(scr_root);
    se_rhythm_create(snd);
    midiseq_task_create(snd);
    pcm_task_create(snd);
    sound_set_stereo(ConfigGetStereo() != 0);
}

/* Creates "DMALoad", under which bank loads queue their DMA tasks. */
/* 0x00165c98 */
static void sound_task_init(SoundTask *w)
{
    sound_task_work = w;
    w->dma_load = task_create(w, &sound_dma_load_desc, 0);
}

/* 0x00165cd8 */
static void sound_task_release(void *w)
{
    sound_task_work = 0;
}

/*
 * If every DMA task up to the first ending one sleeps, prints "sdDMA Task All Freeze! : RESCUE"
 * and wakes the first, so the queue cannot stall.
 */
/* 0x00165ce8 */
static void sound_task_exec(void **work)
{
    SndTask *child = SND_TASK(*work);
    void *cur;
    int all = 1;
    int col;

    cur = SND_WORK(child->child);
    if (cur == 0)
        return;
    for (; cur; cur = SND_WORK(SND_TASK(cur)->next)) {
        if (SND_TASK(cur)->flags & 1)
            break;
        if (!(SND_TASK(cur)->flags & TASK_F_SUSPENDED)) {
            all = 0;
            break;
        }
    }
    if (all) {
        cur = SND_WORK(SND_TASK(*work)->child);
        if (cur && !(SND_TASK(cur)->flags & 1)) {
            do {
                col = debug_color_set(RGBA(0x80, 0, 0, 0x80));
                DEBUG_PRINTF("sdDMA Task All Freeze! : RESCUE\n");
                debug_color_set(col);
            } while (0);
            SND_TASK(cur)->flags &= ~2U;
        }
    }
}

/*
 * Runs after the other sound tasks: samples the command count, sends the frame's commands unless
 * the driver is busy.
 */
/* 0x00165dd0 */
static void sound_end_exec(void *work)
{
    sound_host_command_sample();
    if (ulsnd_is_busy() == 0)
        ulsnd_update();
}

/* "SoundEnd" after it, running while paused (task flag 8). */
/* 0x00165e00 */
static void *sound_task_create(void *parent)
{
    void *work;
    void *end;

    work = task_create(parent, &sound_desc, 4);
    if (work) {
        end = task_create_sibling(work, &sound_end_desc, 0);
        SND_TASK(end)->flags |= TASK_F_RUN_PAUSED;
    }
    return work;
}

/* 0x00165e58 */
static void sound_stg_init(void *work)
{
}

/* 0x00165e60 */
static void sound_stg_release(void *work)
{
}

/* 0x00165e68 */
void *sound_stg_create(void *parent)
{
    return task_create(parent, &sound_stg_desc, 4);
}

/* 0x00165e90 */
int sound_get_dma_load(void)
{
    return *(*(int * *)&sound_task_work); /* steer(cast) */
}

/* SE ports, MIDI sequence, PCM stream; sound marked off. */
/* 0x00165ea0 */
void sound_volume_off(void)
{
    sound_on = 0;
    se_ports_set_volume(0.0f, 166);
    midiseq_set_volume(166, 0.0f);
    pcm_set_volume(166, 0.0f);
}

/* SE level on the SE ports; music level on the MIDI sequence and, x 40/19 capped at 2.0, PCM. */
/* 0x00165ee8 */
void sound_volume_on(void)
{
    float v;
    int i;

    sound_on = 1;
    i = ConfigGetVolSe();
    se_ports_set_volume(sound_volume_levels[i], 166);
    v = sound_volume_levels[ConfigGetVolSeq()];
    midiseq_set_volume(166, v);
    v = v * 2.1052632f;
    pcm_set_volume(166, v < 2.0f ? v : 2.0f);
}

/* 0x00165f88 */
int sound_get_varedit(void)
{
    return sound_varedit;
}

/* Driver, ADX streams and system settings. Also the "Stereo" variable callback. */
/* steer(cast) */
/* 0x00165f98 */
void sound_set_stereo(int stereo)
{
    int mono;

    sound_st = stereo;
    if (stereo) {
        ulsnd_set_stereo(1);
        mono = 0;
        ((void (*)(int))adxt_set_output_mono)(0);
    } else {
        ulsnd_set_stereo(0);
        mono = 1;
        ((void (*)(int))adxt_set_output_mono)(1);
    }
    if (((int (*)(void *))nj_sound_get_info)(&model_wire_buf) == 0) {
        ((void (*)(int))nj_sound_set_mono)(mono);
        nj_sound_apply();
    }
}

/* Clamped 0..2; port 0 always, others while sound is on. Also the "Volume SE" callback. */
/* 0x00166018 */
static void sound_set_se_volume(float vol)
{
    float v;

    v = MIN(MAX(vol, 0.0f), 2.0f);
    sound_vse = v;
    se_port0_set_volume(v, 5);
    if (sound_on)
        se_ports_set_volume(v, 5);
}

/*
 * Clamped 0..2; while on: MIDI sequence and, x 40/19 capped at 2.0, PCM. Also the "Volume
 * Sequence" callback.
 */
/* 0x001660a8 */
static void sound_set_seq_volume(float vol)
{
    vol = MIN(MAX(vol, 0.0f), 2.0f);
    sound_vseq = vol;
    if (sound_on) {
        midiseq_set_volume(5, vol);
        vol = vol * 2.1052632f;
        pcm_set_volume(5, vol < 2.0f ? vol : 2.0f);
    }
}

/* level clamped 0..4. */
/* 0x00166170 */
void sound_set_se_volume_level(int level)
{
    if (level < 0)
        level = 0;
    if (level > 4)
        level = 4;
    sound_set_se_volume(sound_volume_levels[level]);
}

/* level clamped 0..4. */
/* 0x001661a0 */
void sound_set_seq_volume_level(int level)
{
    if (level < 0)
        level = 0;
    if (level > 4)
        level = 4;
    sound_set_seq_volume(sound_volume_levels[level]);
}
