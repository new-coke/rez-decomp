#ifndef REZ_FADE_H
#define REZ_FADE_H

struct FadeWork;

int fade_create(void *parent);
void fade_start(int speed, int mode, int n);
void fade_start_out(int n);
void fade_start_alpha(float t, int speed, int frames);
int fade_is_idle(void);
void fade_reset(void);
struct FadeWork *fade_get_work(void);

#endif /* REZ_FADE_H */
