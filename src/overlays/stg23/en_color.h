/* Trance Mission enemy colours. */

#ifndef REZ_STG23_EN_COLOR_H
#define REZ_STG23_EN_COLOR_H

#include "types_common.h"

void en_color_vars_register(void);
void en_color_vars_unregister(void);
void en_color_body_rgba(float *rgba);
int en_color_wire_at(Vec3 *pos);

#endif /* REZ_STG23_EN_COLOR_H */
