/*
 * Boss rush: starters for each boss (Earth, Mars, Venus, Uranus, Eden): music, tempo and scene,
 * then the fight at its level. Debug flag 4 with the button held: boss and level select instead.
 */

#include "stg21.h"
#include "boss_start.h"
#include "color.h"
#include "debug.h"
#include "fog.h"
#include "lockon.h"
#include "sound_bank.h"
#include "sound_beat.h"
#include "sound_midiseq.h"
#include "texforce.h"
#include "br_manager.h"

extern int debug_boss;
extern int debug_level;
extern char *boss_names[];
extern struct TaskDesc boss_boot_debug_desc;
extern struct TaskDesc boss_boot_earth_desc;
extern int debug_boss_ids[];
extern struct TaskDesc boss_boot_mars_desc;
extern struct TaskDesc boss_boot_venus_desc;
extern struct TaskDesc boss_boot_uranus_desc;
extern struct TaskDesc boss_boot_eden_desc;
extern char eden_bpm_files[2][8];

/* 0x00af6930 */
void boss_boot_earth_exec(int *w)
{
    int prm[2];

    if (--*w > 0)
        return;
    prm[0] = 0;
    prm[1] = boss_level_get(0);
    boss_start_create(prm);
    FINISH_IF(w);
}

/* 0x00af6988 */
void *boss_boot_earth_create(void *parent)
{
    int *w;

    if (debug_flags_test(4) && ((BrPad *)pad_main)->held & 4)
        w = task_create(parent, &boss_boot_debug_desc, 4);
    else
        w = task_create(parent, &boss_boot_earth_desc, 4);
    w[0] = 0;
    music_change(2);
    beat_clock_request_tempo(140.0f);
    midiseq_mute_request(39);
    midiseq_play();
    texforce_disable();
    return w;
}

/* Arrow keys pick boss and level, Enter starts. */
/* 0x00af6a38 */
void boss_boot_debug_exec(int *w)
{
    int prm[2];

    switch (kbd_key) {
    case 80:
        if (--debug_level < 0)
            debug_level = 0;
        break;
    case 79:
        if (++debug_level >= 4)
            debug_level = 3;
        break;
    case 81:
        if (++debug_boss >= 5)
            debug_boss = 4;
        break;
    case 82:
        if (--debug_boss < 0)
            debug_boss = 0;
        break;
    case 40:
        prm[0] = debug_boss_ids[debug_boss];
        prm[1] = debug_level;
        boss_start_create(prm);
        FINISH_IF(w);
        debug_level = 0;
        debug_boss = 0;
        break;
    }
}

/* 0x00af6b70 */
void boss_boot_debug_disp(void)
{
    debug_set_origin(30, 10);
    debug_printf("****Boss Select****");
    debug_newline();
    debug_printf("U_D:  Boss Select");
    debug_newline();
    debug_printf("L_R:  Level Select");
    debug_newline();
    debug_printf("ENTER:GO Boss");
    debug_newline();
    debug_newline();
    debug_printf("Select...    %s", boss_names[debug_boss]);
    debug_newline();
    debug_printf("level...     %2d", debug_level);
}

/* 0x00af6c28 */
void boss_boot_mars_exec(int *w)
{
    int prm[2];

    if (--*w > 0)
        return;
    prm[0] = 1;
    prm[1] = boss_level_get(1);
    boss_start_create(prm);
    FINISH_IF(w);
}

/* 0x00af6c80 */
void *boss_boot_mars_create(void *parent)
{
    int *w = task_create(parent, &boss_boot_mars_desc, 4);

    w[0] = 0;
    music_change(3);
    beat_clock_request_tempo(145.0f);
    midiseq_mute_request(39);
    midiseq_play();
    texforce_disable();
    return w;
}

/* 0x00af6ce8 */
void boss_boot_venus_exec(int *w)
{
    int prm[2];

    if (--*w > 0)
        return;
    prm[0] = 2;
    prm[1] = boss_level_get(2);
    boss_start_create(prm);
    FINISH_IF(w);
}

/* 0x00af6d40 */
void *boss_boot_venus_create(void *parent)
{
    int *w = task_create(parent, &boss_boot_venus_desc, 4);

    w[0] = 0;
    music_change(4);
    beat_clock_request_tempo(138.0f);
    midiseq_mute_request(39);
    midiseq_play();
    texforce_disable();
    return w;
}

/* 0x00af6da8 */
void boss_boot_uranus_exec(int *w)
{
    int prm[2];

    if (--*w > 0)
        return;
    prm[0] = 3;
    prm[1] = boss_level_get(3);
    boss_start_create(prm);
    FINISH_IF(w);
}

/* 0x00af6e00 */
void *boss_boot_uranus_create(void *parent)
{
    int *w = task_create(parent, &boss_boot_uranus_desc, 4);

    w[0] = 0;
    music_change(5);
    beat_clock_request_tempo(140.0f);
    midiseq_mute_request(39);
    midiseq_play();
    texforce_disable();
    return w;
}

/* 0x00af6e68 */
void boss_boot_eden_exec(int *w)
{
    int prm[2];

    if (--*w > 0)
        return;
    prm[0] = 4;
    prm[1] = boss_level_get(4);
    boss_start_create(prm);
    FINISH_IF(w);
}

/* 0x00af6ec0 */
void *boss_boot_eden_create(void *parent)
{
    int *w = task_create(parent, &boss_boot_eden_desc, 4);

    w[0] = 0;
    fog_set(RGBA(0x05, 0, 0, 0x80), 4, 200.0f, 2000.0f);
    BackColReq(RGBA(0x05, 0, 0, 0x80));
    lockon_set_far(2000.0f);
    sound_mpb_load_create_bpms(eden_bpm_files[0], 1);
    sound_mpb_load_create_bpms(eden_bpm_files[1], 2);
    music_change(7);
    beat_clock_request_tempo(140.0f);
    midiseq_mute_request(39);
    midiseq_play();
    texforce_disable();
    return w;
}
