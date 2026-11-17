/* Area choose screen: background and record panel. */

#ifndef REZ_AREACHO_AC_BG_H
#define REZ_AREACHO_AC_BG_H

#include "areacho.h"

struct AcObj;

int ac_bg_node_cb(unsigned int *flags, int node_idx, AcBg *w);
void ac_bg_preload_lights(AcBg *w);
AcPart *ac_bg_parts_create(AcBg *w, struct AcObj *ob, int n);
void ac_bg_parts_release(AcPart *p, int n);
void ac_bg_draw_quad(float x, float y, float u, float v, float width, float height);
void ac_bg_init(AcBg *w, int area);
void ac_bg_release(AcBg *w);
void ac_bg_exec(AcBg *w);
void ac_bg_disp(AcBg *w);
void *ac_bg_create(void *parent, int area);
void ac_bg_set_area(AcBg *w, int area);

#endif /* REZ_AREACHO_AC_BG_H */
