/* Model break-up effect, Area 5. */

#ifndef REZ_STG05_EF_MDL_BRK_H
#define REZ_STG05_EF_MDL_BRK_H

#include "stg05.h"

struct EfMdlBrkWork;

void ef_mdl_brk_init(struct EfMdlBrkWork *w, EfMdlBrkArg *prm);
void ef_mdl_brk_init_relative(struct EfMdlBrkWork *w, EfMdlBrkArg *prm);
void ef_mdl_brk_release(struct EfMdlBrkWork *w);
void ef_mdl_brk_exec(struct EfMdlBrkWork *w);
void ef_mdl_brk_disp(struct EfMdlBrkWork *w);
void *ef_mdl_brk_create(void *w, void *prm);
void *ef_mdl_brk_start(void *w, EfMdlBrkArg *prm);
void *ef_mdl_brk_create_relative(void *w, void *prm);
void *ef_mdl_brk_start_relative(void *w, EfMdlBrkArg *prm);
void ef_mdl_brk_rotate_yxz(int *ang);

#endif /* REZ_STG05_EF_MDL_BRK_H */
