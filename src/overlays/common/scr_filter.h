/* Screen filter: a disc of light on screen. */

#ifndef REZ_COMMON_SCR_FILTER_H
#define REZ_COMMON_SCR_FILTER_H

#include "types_overlays.h"

void scr_filter_draw_disc(float x, float y, float z, float r, int rot, unsigned int color, int inner, int outer);
void scr_filter_init(ScrFilterWork *w);
void scr_filter_release(void);
void scr_filter_exec(ScrFilterWork *w);
void scr_filter_disp(ScrFilterWork *w);
void *scr_filter_create(void *parent);
void scr_filter_mng_init(ScrFilterWork **mng);
void scr_filter_mng_release(ScrFilterWork **mng);
void scr_filter_mng_exec(ScrFilterWork **mng);
void *scr_filter_mng_create(void *parent);
void scr_filter_fade(void *fader, void *to, int frames);
void scr_filter_rotate_yxz(int *ang);

#endif /* REZ_COMMON_SCR_FILTER_H */
