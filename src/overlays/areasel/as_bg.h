/* Score attack area select: background and label panel. */

#ifndef REZ_AREASEL_AS_BG_H
#define REZ_AREASEL_AS_BG_H

#include "areasel.h"
#include "types_overlays.h"

struct AsObj;

void as_bg_preload_lights(AsBg *w);
Part *as_bg_parts_create(AsBg *w, struct AsObj *ob, int n);
void as_bg_parts_release(Part *p, int n);
int as_bg_node_cb(unsigned int *flags, int node_idx, AsBg *w);
void as_bg_init(AsBg *w, int area);
void as_bg_release(AsBg *w);
void as_bg_exec(AsBg *w);
void as_bg_disp(AsBg *w);
void *as_bg_create(void *parent, int area);
void as_bg_set_area(AsBg *w, int area);

#endif /* REZ_AREASEL_AS_BG_H */
