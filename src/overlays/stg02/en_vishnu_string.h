/* Vishnu's strings, Area 2. */

#ifndef REZ_STG02_EN_VISHNU_STRING_H
#define REZ_STG02_EN_VISHNU_STRING_H

#include "stg02.h"
#include "types_common.h"

struct EnVishnuStringWork;

void en_vishnu_string_vars_register(void);
void en_vishnu_string_vars_unregister(void);
void en_vishnu_string_init_points(void *work);
void en_vishnu_string_move(struct EnVishnuStringWork *w);
void en_vishnu_string_reverse(void *w);
void en_vishnu_string_exec_normal(void *work);
void en_vishnu_string_exec(void *w);
void en_vishnu_string_disp(char *w);
void en_vishnu_string_init(EnVishnuStringView *w, int owner, int hand, int idx, Vec3 *pos);
void en_vishnu_string_release(void);
EnVishnuStringView *en_vishnu_string_create(void *parent, int hand, int idx, Vec3 *pos);
void en_vishnu_string_detach(int *w);
float en_vishnu_string_max_len(void);
float en_vishnu_string_get_rebound(void);

#endif /* REZ_STG02_EN_VISHNU_STRING_H */
