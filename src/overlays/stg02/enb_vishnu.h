/* Vishnu's laser, Area 2. */

#ifndef REZ_STG02_ENB_VISHNU_H
#define REZ_STG02_ENB_VISHNU_H

#include "stg02.h"

struct EnbVishnuWork;

void enb_vishnu_vars_register(void);
void enb_vishnu_vars_unregister(void);
int enb_vishnu_vars_init(int *prm);
int enb_vishnu_vars_release(void);
void enb_vishnu_hit_check(int *w);
void enb_vishnu_hit_player(unsigned int *w);
void enb_vishnu_pulse(unsigned int *w);
void enb_vishnu_state_extend(struct EnbVishnuWork *w);
void enb_vishnu_state_reflect(struct EnbVishnuWork *w);
void enb_vishnu_state_fade(struct EnbVishnuWork *w);
void enb_vishnu_state_return(struct EnbVishnuWork *w);
void enb_vishnu_exec(struct EnbVishnuWork *w);
void enb_vishnu_disp(struct EnbVishnuWork *w);
void enb_vishnu_init(int *work);
void enb_vishnu_release(char *w);
void *enb_vishnu_create(void *parent);
void enb_vishnu_detach(EnbVishnu *w);

#endif /* REZ_STG02_ENB_VISHNU_H */
