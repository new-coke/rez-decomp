/* Eden boss small effects. */

#ifndef REZ_B_EDEN_EDEN_EFFECT_H
#define REZ_B_EDEN_EDEN_EFFECT_H

#include "types_common.h"

struct EdenShineWork;
struct EdenFlashWork;
struct EdenScreenWork;
struct EdenScreenManWork;

void eden_effect_vars_add(void);
void eden_effect_vars_remove(void);
void eden_effect_dshine_exec(struct EdenShineWork *w);
void eden_effect_dshine_disp(struct EdenShineWork *w);
int *eden_effect_dshine_create(void *parent, int arg);
void eden_effect_dshine2_exec(struct EdenShineWork *w);
void eden_effect_dshine2_disp(struct EdenShineWork *w);
int *eden_effect_dshine2_create(void *parent, int arg);
void eden_effect_dshine_scale(struct EdenShineWork *w, float k);
void eden_effect_dshine_ex_exec(struct EdenShineWork *w);
void eden_effect_dshine_ex_disp(struct EdenShineWork *w);
int *eden_effect_dshine_ex_create(void *parent, int arg, int frm, float scl);
void eden_effect_flash_quad_draw(Vec3 *pos, int *ang, int rx, float scl_z, unsigned int col, float scl_xy);
void eden_effect_bflash_exec(struct EdenFlashWork *w);
void eden_effect_bflash_disp(struct EdenFlashWork *w);
int *eden_effect_bflash_create(void *parent, int arg, int ang);
int *eden_effect_bflash2_create(void *parent, int arg, int ang);
void eden_effect_screen_exec(struct EdenScreenWork *w);
void eden_effect_screen_disp(struct EdenScreenWork *w);
int eden_effect_screen_create(void *parent);
void eden_effect_screen_man_exec(struct EdenScreenManWork *w);
void eden_effect_screen_man_release(struct EdenScreenManWork *w);
int *eden_effect_screen_man_create(void *parent);
void eden_effect_screen_man_end(struct EdenScreenManWork *w);
void eden_effect_rotate_yxz(int *ang);

#endif /* REZ_B_EDEN_EDEN_EFFECT_H */
