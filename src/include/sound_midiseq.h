/* Music sequencer: play, stop, mute tables, track switches, song load. */

#ifndef REZ_SOUND_MIDISEQ_H
#define REZ_SOUND_MIDISEQ_H

struct Midiseq;
struct MidiseqEvent;
struct MidiseqTrack;
struct MidiseqWork;
struct MidiseqMute;
struct SeqLoad;

void midiseq_set_volume(int id, float vol);
void midiseq_pcm_table_clear(void);
void midiseq_mute_update(struct Midiseq *seq);
void *midiseq_task_create(void *parent);
void midiseq_play(void);
void midiseq_stop(void);
void midiseq_set_sub(unsigned char *data);
void midiseq_clear_sub(void);
void midiseq_mute_request(int idx);
int midiseq_track_enable(int track, int on);
void midiseq_track_volume(int track, int vol);
unsigned char *midiseq_get_pulse_levels(void);
void music_change(int id);
int seqload_idle(void);

#endif /* REZ_SOUND_MIDISEQ_H */
