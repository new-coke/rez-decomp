/* Sound start-up, sound tasks, stereo and volume. */

#ifndef REZ_SOUND_MAIN_H
#define REZ_SOUND_MAIN_H

struct SoundTask;

void sound_vsync(void);
void sound_init(void);
void *sound_stg_create(void *parent);
int sound_get_dma_load(void);
void sound_volume_off(void);
void sound_volume_on(void);
int sound_get_varedit(void);
void sound_set_stereo(int stereo);
void sound_set_se_volume_level(int level);
void sound_set_seq_volume_level(int level);

#endif /* REZ_SOUND_MAIN_H */
