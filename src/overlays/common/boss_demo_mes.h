/* Boss briefing text. */

#ifndef REZ_COMMON_BOSS_DEMO_MES_H
#define REZ_COMMON_BOSS_DEMO_MES_H

#include "types_overlays.h"

char *boss_name_subst(char *src, int level);
void boss_demo_mes2_exec(BossDemoMes2Work *w);
BossDemoMes2Work *boss_demo_mes2_create(void *parent, char **lines, int n, int delay);
void boss_demo_mes2_set_end(BossDemoMes2Work *w, int quit);
void boss_demo_mes_center(BossDemoMesWork *w);
void boss_demo_mes_init(BossDemoMesWork *w);
void boss_demo_mes_release(BossDemoMesWork *w);
void boss_demo_mes_exec_close(BossDemoMesWork *w);
void boss_demo_mes_exec_type(BossDemoMesWork *w);
void boss_demo_mes_exec(BossDemoMesWork *w);
void boss_demo_mes_disp(void);
BossDemoMesWork *boss_demo_mes_create(void *parent, char *name);
BossDemoMes2Work *boss_demo_mes2_start(void *parent, char *name, int kind);

#endif /* REZ_COMMON_BOSS_DEMO_MES_H */
