/* Screen saver, screen wave, lens test. */

#ifndef REZ_SCREEN_LENS_H
#define REZ_SCREEN_LENS_H

struct ScreenLensWork;
struct ScrSaverTask;
struct FishRec;
struct ScreenWaveWork;
struct ScrWvUvWork;
struct ScrWvUvArg;

void screen_lens_toggle(void);
void scr_saver_setup(void);
void *scr_saver_create(int fish_parent);
void screen_wave_start(void);
void screen_wave_stop(void);
void screen_wave_set_strength(float x);

#endif /* REZ_SCREEN_LENS_H */
