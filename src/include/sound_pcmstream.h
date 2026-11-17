/* ADX stream player: load, play, cue start, stop, volume. */

#ifndef REZ_SOUND_PCMSTREAM_H
#define REZ_SOUND_PCMSTREAM_H

struct AdxtErrObj;
struct PcmFile;
struct PcmStream;

void pcm_task_init(struct PcmStream *w);
void pcm_file_exec(struct PcmStream *w, struct PcmFile *file);
void pcm_check_adxt(struct PcmStream *w);
void pcm_task_create(void *parent);
void pcm_play(void *entry, int arg);
void pcm_play_index(void *entry, int arg, int index);
void pcm_load(void *entry, int arg);
void pcm_load_index(void *entry, int arg, int index);
int pcm_is_ready(void);
int pcm_is_playing(void);
void pcm_start(void);
void pcm_stop(void);
void pcm_set_volume(int frames, float vol);
void sound_test_menu_add(void);

#endif /* REZ_SOUND_PCMSTREAM_H */
