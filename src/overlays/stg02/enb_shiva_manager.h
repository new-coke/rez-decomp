/* Shiva missile manager, Area 2. */

#ifndef REZ_STG02_ENB_SHIVA_MANAGER_H
#define REZ_STG02_ENB_SHIVA_MANAGER_H

#include "types_common.h"

struct EnbShivaManagerWork;

int enb_shiva_manager_find_free(void);
int enb_shiva_manager_find_used(int i);
void enb_shiva_manager_notify(int i, int code);
int enb_shiva_manager_add(int owner, Vec3 *pos, int cb);
void enb_shiva_manager_set_pos(int i, Vec3 *pos);
void enb_shiva_manager_burst(int i);
void enb_shiva_manager_remove(void *idx);
void enb_shiva_manager_exec(struct EnbShivaManagerWork *w);
void enb_shiva_manager_disp(struct EnbShivaManagerWork *w);
void enb_shiva_manager_init(struct EnbShivaManagerWork *w);
void enb_shiva_manager_release(char *w);
void *enb_shiva_manager_create(void *parent);

#endif /* REZ_STG02_ENB_SHIVA_MANAGER_H */
