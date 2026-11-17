/* Venus boss particle effects. */

#ifndef REZ_B_VENUS_VENUS_PTCL_H
#define REZ_B_VENUS_VENUS_PTCL_H

#include "types_overlays.h"
#include "types_common.h"
#include "b_venus.h"

struct VenusDustWork;

void venus_ptcl_vars_add(void);
void venus_ptcl_vars_remove(void);
CmPtclWork *venus_ptcl_create(void *parent, Vec3 *follow, Vec3 *dirsrc, int tex);
void venus_ptcl_ex_exec(VenusTaskList *w);
int *venus_ptcl_ex_create(void *parent, Vec3 *pos, int n);
void venus_ptcl_ex_hide(VenusTaskList *w, int k);
void venus_sp_draw_exec(int *w);
void venus_sp_draw_disp(char *w);
int *venus_sp_draw_create(VenusFbManWork *parent, VenusFbManPod *pos, int tex, unsigned int color, int follow, float sec, float scl);
void venus_dust_respawn(char *dust, char *rec);
void venus_dust_exec(struct VenusDustWork *w);
void venus_dust_disp(struct VenusDustWork *w);
void venus_dust_release(struct VenusDustWork *w);
VenusDustWork *venus_dust_create(void *parent, int arg);
void venus_ptcl_rotate_yxz(int *ang);

#endif /* REZ_B_VENUS_VENUS_PTCL_H */
