/*
 * Earth boss opening demo (sound banks, light and fog, screen wipe, boss camera entrance on the
 * beat), score lock table, arena map and player path, and sound and music helpers.
 */

#include "b_earth.h"
#include "debug.h"
#include "fog.h"
#include "game.h"
#include "gw_varedit.h"
#include "map.h"
#include "pad_vib.h"
#include "player.h"
#include "pnb.h"
#include "screen.h"
#include "screen_lens.h"
#include "sound_bank.h"
#include "sound_beat.h"
#include "sound_midise.h"
#include "stage.h"
#include "task.h"
#include "texforce.h"
#include "wipefade.h"
#include "earth_demo.h"
#include "../common/boss_common.h"
#include "../common/boss_sound.h"

extern BossSe se_table;
extern char earth_1st_demo_files[][8];
extern int earth_1st_demo_light_cols[];
extern unsigned int earth_1st_demo_fog_cols[];
extern float earth_map_level_scales[];
extern int earth_map_level;
extern BossVolDef vol_table[];
extern BossLockDef earth_score_lock_defs[];
extern char earth_score_vardef[], earth_score_vardefs[];
extern int earth_score_vars_group;
extern char earth_1st_demo_desc[], earth_1st_demo_desc_debug[];
extern struct TaskDesc earth_map_desc;
extern int earth_map_model_def[];
extern int earth_map_part_value;
extern int pcm_files[][2];
extern int adx_files[2][2];

/* steer(cast) */
/* 0x00cb16d0 */
void earth_1st_demo_exec(Earth1stDemoWork *w)
{
    Vec3 v;
    BossFog fog;
    CmLightChangeWork *light;
    void *mpb;
    int *cam_mot, *cam_path;
    float t;

    texforce_disable();
    if (w->sub && (TASK_OF(w->sub)->flags & TASK_F_END))
        w->sub = 0;
    switch (w->state) {
    case -2:
        v.z = -10.0f;
        v.x = v.y = 0.0f;
        w->cam = boss_camera_create(w->arg, 1.0f, 0, 0, &v, 89, 120, 120, 480);
        w->mpb[0] = (void *)sound_mpb_load_create_bpms(earth_1st_demo_files[0], 1);
        w->mpb[1] = (void *)sound_mpb_load_create_bpms(earth_1st_demo_files[1], 2);
        w->state++;
    case -1:
        mpb = w->mpb[0];
        if (mpb && (TASK_OF(mpb)->flags & TASK_F_END)) {
            w->mpb[0] = 0;
            mpb = 0;
        }
        if (w->mpb[1] && (TASK_OF(w->mpb[1])->flags & TASK_F_END))
            w->mpb[1] = 0;
        if (!mpb && w->mpb[1] == 0)
            w->state++;
        break;
    case 0:
        light = cm_light_change_create(0.0f, 2.0f);
        if (light)
            cm_light_change_set_target(light, earth_1st_demo_light_cols);
        {
            int i = light_mode_index();

            w->fog_to.back = earth_1st_demo_fog_cols[i * 2];
            w->fog_to.col = earth_1st_demo_fog_cols[i * 2 + 1];
        }
        w->fog_to.near = 70.0f;
        w->fog_to.far = 1500.0f;
        SoundRhythmSe_StgCommonStart(7);
        fog_get(&w->fog_from);
        w->timer0 = w->timer = 180;
        w->state++;
    case 1:
        if (--w->timer <= 0)
            w->state++;
        else if (w->timer == 2)
            stage_end_child();
        else if (w->timer == 30.0)
            w->sub = wipe_start(0, 0, 0);
        if (w->sub && wipe_is_covered(w->sub)) {
            game_pass_count_spd_set(10.0f);
            ((void (*)(void *, int, int))wipe_finish)(w->sub, 120, 255);
        }
        break;
    case 2:
        if (w->cb && !w->cb(w->arg))
            break;
        w->speed = earth_map_get_level_scale();
        w->state++;
        w->timer0 = w->timer = 150;
        w->fog_frames = w->fog_n = 60;
        break;
    case 3:
        if (!w->started && beat_grid_hit(0) && !(beat_clock_get_haku() & 3)) {
            w->started = 1;
            boss_vol_play(w->parent->bgm, 0, 0);
            SoundRhythmSe_StgCommonStop(7);
            SoundRhythmSe_StgCommon(8);
            cam_mot = pnb_get_entry(*(void **)w->arg, 30);
            cam_path = pnb_get_entry(*(void **)w->arg, 31);
            boss_camera_set_motion(w->cam, *cam_mot, (unsigned int *)*cam_path);
            boss_pl_di_create(w, 0, 0);
            peri_act_beat_tick();
        }
        if (--w->timer <= 0 && w->started) {
            w->timer0 = w->timer = 372;
            w->state++;
        }
        break;
    case 4:
        if (--w->timer <= 0)
            w->state++;
        else if (w->timer == 120.0f) {
            if (w->sub)
                wipe_is_done(w->sub);
            w->wave_frames = w->wave_n = 120;
            if (!light_mode_index())
                screen_wave_start();
        } else if (w->timer == 90.0f)
            game_pass_count_spd_set(w->speed);
        if (w->fog_n) {
            w->fog_n--;
            fog_blend(&w->fog_from, &w->fog_to, &fog, (float)w->fog_n / (float)w->fog_frames);
            do {
                BackColReq(fog.back);
                fog_set(fog.col, 4, fog.near, fog.far);
                fog_set_enable((void *)1);
            } while (0);
        }
        if (w->wave_n) {
            w->wave_n--;
            t = 1.0f - (float)w->wave_n / (float)w->wave_frames;
            if (t < 0.25f)
                t *= 4.0f;
            else {
                t = 1.0f - (t - 0.25f) / 0.75f;
                t *= t;
            }
            screen_set_margin(10);
            if (!light_mode_index())
                screen_wave_set_strength(t);
            screen_set_fade_alpha((int)(t * 250.0f));
        }
        break;
    case 5:
        if (!light_mode_index())
            screen_wave_stop();
        screen_set_fade_alpha(0);
        screen_set_margin(0);
        w->state++;
        break;
    case 6:
        if (w->cam && !(TASK_OF(w->cam)->flags & TASK_F_END))
            break;
        w->state++;
    case 7:
        if (beat_grid_hit(0) && !(beat_clock_get_haku() & 3)) {
            boss_vol_play(w->parent->bgm, 1, 0);
            do {
                FINISH(w);
            } while (0);
        }
        break;
    }
    if (w->state >= 4)
        w->frames++;
}

/* 0x00cb1d48 */
void earth_1st_demo_disp(Earth1stDemoWork *w)
{
}

/* 0x00cb1d50 */
void earth_1st_demo_release(Earth1stDemoWork *w)
{
    if (!light_mode_index())
        screen_wave_stop();
    screen_set_fade_alpha(0);
    screen_set_margin(0);
    player_swap_fullrange(0);
    FINISH_IF(w->sub);
    SoundRhythmSe_StgCommonStop(7);
}

/* 0x00cb1db8 */
Earth1stDemoWork *earth_1st_demo_create(void *parent, int (*cb)(void *arg), void *arg)
{
    Earth1stDemoWork *w;

    if (debug_flags_test(4))
        w = task_create(parent, earth_1st_demo_desc_debug, 104);
    else
        w = task_create(parent, earth_1st_demo_desc, 104);
    w->cb = cb;
    w->arg = arg;
    w->parent = parent;
    w->state = -2;
    return w;
}

/* steer(cast) */
/* 0x00cb1e48 */
void earth_score_vars_add(void)
{
    lock_table_set(earth_score_lock_defs, 11);
    earth_score_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, earth_score_vardef);
    gw_varedit_add_table(&gw_varedit_root, earth_score_vars_group, earth_score_vardefs);
}

/* 0x00cb1ea8 */
void earth_score_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)earth_score_vardef);
}

/* 0x00cb1ec0 */
void earth_map_release(void)
{
    player_cell_move_clear();
    player_swap_fullrange(1);
}

/* 0x00cb1ee0 */
int earth_map_create(void *parent, int n)
{
    void **map;
    int map_task, i;
    int *def;
    int *cell_from, *cell_to, *pass_a, *pass_b, *pass_c, *pass_d;

    map = task_create_inherit(parent, &earth_map_desc, 8);
    def = earth_map_model_def;
    map_task = map_task_create_mode2(map, def);
    for (i = 0; i < def[1]; i++)
        map_part_set_color(map_task, i, earth_map_part_value);
    cell_from = pnb_get_entry(*map, 29);
    cell_to = pnb_get_entry(*map, 29);
    pass_a = pnb_get_entry(*map, 28);
    pass_b = pnb_get_entry(*map, 28);
    pass_c = pnb_get_entry(*map, 29);
    pass_d = pnb_get_entry(*map, 29);
    player_cell_move_start(*cell_from, *cell_to, *pass_a, *pass_b, *pass_c, *pass_d);
    if (n >= 5)
        n = 4;
    earth_map_level = n;
    return map_task;
}

/* 0x00cb2040 */
float earth_map_get_level_scale(void)
{
    return earth_map_level_scales[earth_map_level];
}

/* 0x00cb2060 */
void boss_se_play(int owner, int id, int lock, int pos)
{
    se_table_play(owner, &se_table, id, lock, pos);
}

/* 0x00cb2080 */
void boss_vol_play(BgmMan *man, int id, int arg)
{
    vol_table_play(man, vol_table, id, arg);
}

/* 0x00cb2098 */
void boss_vol_play_stage(BgmMan *man, int id, int arg, int stage)
{
    vol_table_play(man, vol_table, id + stage % 3 * 3, arg);
}

/* 0x00cb20c8 */
void boss_vol_stop(BgmMan *man, int id)
{
    vol_table_stop(man, vol_table, id);
}

/* 0x00cb20d8 */
void boss_pcm_load(BgmMan *man, int i)
{
    boss_stream_load((int)pcm_files[i], 1);
}

/* 0x00cb20f0 */
void boss_pcm_start(void)
{
    boss_stream_start();
}

/* 0x00cb20f8 */
void boss_bgm_exit_cb(BgmMan *bgm)
{
    se_table_stop(&se_table);
}

/* 0x00cb2108 */
void boss_se_stop_all(void)
{
    se_table_stop(&se_table);
}

/* 0x00cb2118 */
BgmMan *boss_bgm_start(void *parent)
{
    se_table_reset(&se_table);
    return cm_bgm_sound_man_create(parent, boss_bgm_exit_cb);
}

/* 0x00cb2158 */
void boss_adx_request(BgmMan *man)
{
    cm_bgm_sound_man_add_file(man, adx_files[0], (int)adx_files[1]);
}

/* 0x00cb2168 */
void boss_bgm_switch(BgmMan *man, int cb, int cb_arg, int idx)
{
    cm_bgm_sound_sub_start(man, cb, cb_arg, 0, 0, idx, idx);
}
