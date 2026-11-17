/* Backfire: enemy exhaust glow. */

#ifndef REZ_COMMON_EN_BACKFIRE_H
#define REZ_COMMON_EN_BACKFIRE_H

#include "types_overlays.h"
#include "types_common.h"

int en_backfire_collect_nodes(unsigned int *node, unsigned int mask, NodeRec **out, int num);
void en_backfire_init(EnBackFireWork *w, BackFireLink *lnk);
void en_backfire_release(EnBackFireWork *w);
void en_backfire_exec(EnBackFireWork *w);
void en_backfire_disp(EnBackFireWork *w);
void *en_backfire_create_with_cb(void *parent, BackFireLink *lnk, int clear);
void *en_backfire_create(void *parent, BackFireLink *lnk, int clear);
int en_backfire_set_texture(int tex);
int en_backfire_set_cb(EnBackFireWork *w, int cb);
void en_backfire_beam_init(EnBackFireWork *w, BackFireLink *lnk);
void en_backfire_beam_release(EnBackFireWork *w);
void en_backfire_beam_exec(EnBackFireWork *w);
void en_backfire_beam_draw_band(float *p0, float *p1, unsigned int col, float u0, float u1, float width0, float width1);
void en_backfire_beam_draw(EnBackFireWork *w, Vec3 *p0, Vec3 *p1, unsigned int col, float width);
void en_backfire_beam_disp(EnBackFireWork *w);
void *en_backfire_beam_create_with_cb(void *parent, BackFireLink *lnk);
void *en_backfire_beam_create(void *parent, BackFireLink *lnk);
int en_backfire_beam_set_texture(int tex);
int en_backfire_beam_set_cb(EnBackFireWork *w, int cb);
void en_backfire_rotate_yxz(int *ang);

#endif /* REZ_COMMON_EN_BACKFIRE_H */
