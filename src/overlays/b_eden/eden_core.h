/* Eden boss core. */

#ifndef REZ_B_EDEN_EDEN_CORE_H
#define REZ_B_EDEN_EDEN_CORE_H

struct EdenCoreWork;
struct EdenCoreShineWork;

void eden_core_vars_add(void);
void eden_core_vars_remove(void);
void eden_core_shot_fire(struct EdenCoreWork *w);
void eden_core_exec(struct EdenCoreWork *w);
void eden_core_hit_check(struct EdenCoreWork *w);
void eden_core_glow_draw(struct EdenCoreWork *w);
void eden_core_disp(struct EdenCoreWork *w);
void eden_core_release(struct EdenCoreWork *w);
int *eden_core_create(void *parent, char *src);
void eden_core_open(struct EdenCoreWork *w, int mode);
int eden_core_get_state(struct EdenCoreWork *w);
void eden_core_set_hits(struct EdenCoreWork *w);
void eden_core_shine_exec(struct EdenCoreShineWork *shine);
void eden_core_shine_disp(struct EdenCoreShineWork *w);
void eden_core_shine_release(struct EdenCoreShineWork *w);
int *eden_core_shine_create(void *parent, int arg);
void eden_core_shine_set_req(int *shine, int req);
int eden_core_shine_get_pos(int core);
void eden_core_shine_reset(int phase);
void eden_core_shine_set_mode(int *shine, int mode, int sub);
int eden_core_shine_get_mode(int *shine);
int eden_core_shine_get_hits(int *shine);
void eden_core_shine_clear_hits(int *shine);
void eden_core_shine_set_active(int on);

#endif /* REZ_B_EDEN_EDEN_CORE_H */
