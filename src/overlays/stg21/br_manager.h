/* Boss rush manager. */

#ifndef REZ_STG21_BR_MANAGER_H
#define REZ_STG21_BR_MANAGER_H

struct BrManager;

void br_manager_vars_add(void);
void br_manager_vars_remove(void);
void br_manager_exec(struct BrManager *w);
void br_manager_init(struct BrManager *w);
void br_manager_release(struct BrManager *w);
void *br_manager_create(void *parent);
int boss_level_get(int boss);

#endif /* REZ_STG21_BR_MANAGER_H */
