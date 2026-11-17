/* Seraphim damage pieces, Area 5. */

#ifndef REZ_STG05_EN_SERAPHIM_DMG_H
#define REZ_STG05_EN_SERAPHIM_DMG_H

#include "stg05.h"

struct EnSeraphimDmgWork;

void en_seraphim_dmg_vars_register(void);
void en_seraphim_dmg_vars_unregister(void);
void en_seraphim_dmg_init(struct EnSeraphimDmgWork *w, EnSeraphimDmgArg *prm);
void en_seraphim_dmg_release(struct EnSeraphimDmgWork *w);
void en_seraphim_dmg_exec(struct EnSeraphimDmgWork *w);
void en_seraphim_dmg_disp(struct EnSeraphimDmgWork *w);
void *en_seraphim_dmg_create(void *w, void *prm);
void en_seraphim_dmg_rotate_yxz(int *ang);

#endif /* REZ_STG05_EN_SERAPHIM_DMG_H */
