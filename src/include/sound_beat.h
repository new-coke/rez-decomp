/* Beat clock: tempo, beat/bar position, beat grids. */

#ifndef REZ_SOUND_BEAT_H
#define REZ_SOUND_BEAT_H

struct BeatClock;

void beat_clock_tick(struct BeatClock *w);
void *beat_clock_create(void *parent);
void beat_clock_reset(void);
void beat_clock_request_tempo(float bpm);
float beat_grid_get_remain(int i);
int beat_clock_get_haku(void);
float beat_clock_get_interval(void);
int beat_clock_get_frames_to_beat(void);
float beat_clock_get_bpm(void);
int beat_grid_hit(int grid);
float beat_clock_get_phase(void);
float beat_clock_get_bar(void);

#endif /* REZ_SOUND_BEAT_H */
