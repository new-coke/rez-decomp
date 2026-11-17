/* Earth boss opening and arena. */

#ifndef REZ_B_EARTH_EARTH_DEMO_H
#define REZ_B_EARTH_EARTH_DEMO_H

#include "types_overlays.h"

void earth_1st_demo_exec(Earth1stDemoWork *w);
void earth_1st_demo_disp(Earth1stDemoWork *w);
void earth_1st_demo_release(Earth1stDemoWork *w);
Earth1stDemoWork *earth_1st_demo_create(void *parent, int (*cb)(void *arg), void *arg);
void earth_score_vars_add(void);
void earth_score_vars_remove(void);
void earth_map_release(void);
int earth_map_create(void *parent, int n);
float earth_map_get_level_scale(void);
void boss_se_play(int owner, int id, int lock, int pos);
void boss_vol_play(BgmMan *man, int id, int arg);
void boss_vol_play_stage(BgmMan *man, int id, int arg, int stage);
void boss_vol_stop(BgmMan *man, int id);
void boss_pcm_load(BgmMan *man, int i);
void boss_pcm_start(void);
void boss_bgm_exit_cb(BgmMan *bgm);
void boss_se_stop_all(void);
BgmMan *boss_bgm_start(void *parent);
void boss_adx_request(BgmMan *man);
void boss_bgm_switch(BgmMan *man, int cb, int cb_arg, int idx);

#endif /* REZ_B_EARTH_EARTH_DEMO_H */
