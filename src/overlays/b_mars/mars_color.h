/* Mars boss body colouring. */

#ifndef REZ_B_MARS_MARS_COLOR_H
#define REZ_B_MARS_MARS_COLOR_H

#include "types_overlays.h"

struct MarsVccManWork;
struct MarsPtWork;

void mars_vcc_keys_build(void);
short mars_vcc_collect(void ***w, char **out);
short mars_vcc_count(void ***w);
void mars_vcc_man_release(struct MarsVccManWork *w);
void mars_vcc_man_exec(struct MarsVccManWork *w);
char *mars_vcc_man_create(void *parent, void **obj, int pos, int ang);
void mars_vcc_set_alpha(float alpha);
void mars_vcc_man_set_alpha(struct MarsVccManWork *w, float alpha);
void mars_vcc_exec(struct MarsPtWork *w);
void *mars_vcc_create(void *parent, void *pos, int col, float x, float y);
void *mars_vcc_start(void *parent, void *pos, int mode);
CmCchangeWork *mars_vcc_fade_create(void *parent);
void mars_color_rotate_yxz(int *ang);

#endif /* REZ_B_MARS_MARS_COLOR_H */
