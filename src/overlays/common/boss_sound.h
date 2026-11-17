/* Boss music and sound. */

#ifndef REZ_COMMON_BOSS_SOUND_H
#define REZ_COMMON_BOSS_SOUND_H

#include "types_overlays.h"

void se_grid_set(int vol);
void cm_sound_bpm_exec(CmSoundBpmWork *w);
void cm_sound_bpm_release(CmSoundBpmWork *w);
CmSoundBpmWork *cm_sound_bpm_create(void *parent, int curve, float from, float delta, float secs);
int se_table_play(int owner, BossSe *tab, int id, int lock, int pos);
void se_table_reset(BossSe *se);
void se_table_stop(BossSe *se);
void cm_bgm_sound_exec(CmBgmSoundWork *w);
void cm_bgm_sound_release(CmBgmSoundWork *w);
CmBgmSoundWork *cm_bgm_sound_create(void *parent, int *def, int arg);
void cm_bgm_sound_man_release(BgmMan *man);
void cm_bgm_sound_man_exec(BgmMan *man);
void sound_debug_toggle(void);
void cm_bgm_sound_man_end_children(BgmMan *man);
void cm_bgm_sound_man_end(BgmMan *man);
void cm_bgm_sound_set_arg(CmBgmSoundWork *w, int vol);
int vol_table_play(BgmMan *man, BossVolDef *tab, int id, int arg);
void vol_table_stop(BgmMan *man, BossVolDef *tab, int id);
void boss_stream_load(int entry, int pcm_arg);
void boss_stream_start(void);
void boss_stream_load_index(int entry, int pcm_arg, int idx);
void cm_bgm_sound_man_set_stream(int pcm);
BgmMan *cm_bgm_sound_man_create(void *parent, void (*cb)(BgmMan *man));
void cm_bgm_sound_sub_release(CmBgmSoundSubWork *w);
void cm_bgm_sound_sub_exec(CmBgmSoundSubWork *w);
void cm_bgm_sound_sub_create(BgmMan *man, int cb, int cb_arg, int slot, int n, int useidx, int idx, int seq_idx, int on);
void cm_bgm_sound_sub_create_idle(BgmMan *man, int cb, int cb_arg, int slot, int n);
void cm_bgm_sound_sub_start(BgmMan *man, int cb, int cb_arg, int slot, int n, int idx, int seq_idx);
void cm_bgm_sound_man_add_file(BgmMan *man, void *file, int pcm);
void cm_se_laser_exec(CmSeLaserWork *w);
void cm_se_laser_release(CmSeLaserWork *w);
CmSeLaserWork *cm_se_laser_create(void *file);
void cm_se_laser_set_cb(CmSeLaserWork *w, void (*done)(int arg), int arg);
void sound_log_clear(void);
void sound_log_print(int x, int y);
void sound_log_entry(BossLogEnt *ent, int idx, unsigned int state);
void sound_log_add(char *line);
void sound_beat_bars_draw(int hit, float bar);
void cm_bgm_sound_man_disp(BgmMan *w);

#endif /* REZ_COMMON_BOSS_SOUND_H */
