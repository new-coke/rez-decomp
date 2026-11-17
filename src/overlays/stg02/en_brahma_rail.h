/* Brahma's rail, Area 2. */

#ifndef REZ_STG02_EN_BRAHMA_RAIL_H
#define REZ_STG02_EN_BRAHMA_RAIL_H

#include "types_common.h"

struct EnBrahmaRailWork;

void en_brahma_rail_vars_register(void);
void en_brahma_rail_vars_unregister(void);
void en_brahma_rail_add_point(struct EnBrahmaRailWork *w);
void en_brahma_rail_state_lay(struct EnBrahmaRailWork *w);
void en_brahma_rail_exec(int *w);
int en_brahma_rail_test_point(void *wp, int i);
int en_brahma_rail_find_visible(void *w, int lo, int hi, int *out);
int en_brahma_rail_find_edge(char *w, int dir, int lo, int hi);
int en_brahma_rail_calc_span(struct EnBrahmaRailWork *w, int *lo, int *hi);
void en_brahma_rail_disp_ahead(struct EnBrahmaRailWork *w);
void en_brahma_rail_disp_line(struct EnBrahmaRailWork *w);
void en_brahma_rail_disp(int *w);
void en_brahma_rail_init(char *w, float x, float y, int rotz, int kind);
void en_brahma_rail_release(void);
void *en_brahma_rail_create(void *parent, int rotz, int kind, float x, float y);
void en_brahma_rail_leave(int *w);
int en_brahma_rail_sample(void *wp, float t, Vec3 *pos, int *ang);
void en_brahma_rail_rotate_yxz(int *ang);

#endif /* REZ_STG02_EN_BRAHMA_RAIL_H */
