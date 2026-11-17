/* Area 1 ground waves: faded in when the first map scene starts, reset for the next scene. */

#include "stg01.h"
#include "color.h"
#include "heap.h"
#include "math_util.h"
#include "nj_system.h"
#include "player_status.h"
#include "sound_midiseq.h"
#include "task.h"
#include "land_wave.h"
#include "../common/land_wave.h"

extern int land_wave_path_offsets[];
extern char land_wave_mng_desc[];

extern char land_wave_desc[];

extern struct LandWaveMngWork *land_wave_mng;
extern unsigned char note_levels[10];
extern struct LandWaveWork *land_wave_prev;

/* 0x00af7e80 */
void land_wave_mng_exec_fade_in(LandWaveMngWork *w)
{
    float v = w->fade * 50.0f / w->fade_len;
    float *hd = w->bias;
    float *trl = &w->bias[27];
    int i = 2;

    do {
        *trl-- = v;
        i--;
        *hd++ = v;
    } while (i >= 0);
    if (++w->fade >= w->fade_len)
        w->fade = w->fade_len - 1;
    land_wave_mng_exec(w);
}

/* 0x00af7f30 */
void land_wave_mng_fade_in(int frames)
{
    if (land_wave_mng) {
        land_wave_mng->fade_len = frames;
        land_wave_mng->fade = 0;
        TASK_OF(land_wave_mng)->exec = (void (*)(void *))land_wave_mng_exec_fade_in;
    }
}

/* 0x00af7f58 */
void land_wave_mng_reset(void)
{
    if (land_wave_mng) {
        heap_bzero(land_wave_mng->bias, sizeof land_wave_mng->bias);
        TASK_OF(land_wave_mng)->exec = (void (*)(void *))land_wave_mng_exec;
    }
}
