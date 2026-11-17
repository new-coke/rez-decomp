/* Shared debug-variable group of the stage objects. */

#ifndef REZ_COMMON_ENEMY_VARS_H
#define REZ_COMMON_ENEMY_VARS_H

struct GwVarNode;

struct GwVarNode *enemy_vars_group_acquire(void);
void enemy_vars_group_release(void);

#endif /* REZ_COMMON_ENEMY_VARS_H */
