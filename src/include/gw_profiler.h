/* Debug GUI profiler. */

#ifndef REZ_GW_PROFILER_H
#define REZ_GW_PROFILER_H
#include "types_gw.h"

struct GwProf;
struct GwProfGroup;
struct GwProfEntry;
struct GwWin;

struct GwProfGroup *gw_profiler_group_create(char *name);
struct GwProfEntry *gw_profiler_begin(struct GwProfGroup *grp, int color);
void gw_profiler_end(struct GwProfGroup *grp, struct GwProfEntry *entry);

extern GwProf *gw_profiler_main;
/* Built-in groups, pending-mark stack. */
extern GwProfGroup *gw_prof_vblank;
extern GwProfGroup *gw_prof_maple;
extern GwProfGroup *gw_prof_render;
extern GwProfGroup *gw_prof_dma;
extern int gw_profiler_mark_cnt;                  /* pending marks */
extern GwProfEntry *gw_profiler_marks[8];
extern int gw_profiler_timing[];                /* nj_profile_get_timing; render times from [1] */

#endif /* REZ_GW_PROFILER_H */
