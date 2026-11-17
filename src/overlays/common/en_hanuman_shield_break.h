/* Piece shot off Hanuman's shield. */

#ifndef REZ_COMMON_EN_HANUMAN_SHIELD_BREAK_H
#define REZ_COMMON_EN_HANUMAN_SHIELD_BREAK_H

#include "types_overlays.h"
#include "types_common.h"

void en_hanuman_shield_break_exec(EnHanumanShieldBreakWork *w);
void en_hanuman_shield_break_disp(EnHanumanShieldBreakWork *w);
void en_hanuman_shield_break_init(EnHanumanShieldBreakWork *w);
void en_hanuman_shield_break_release(char *w);
EnHanumanShieldBreakWork *en_hanuman_shield_break_create(void *parent, Vec3 *pos, int *ang, Vec3 *v, int part);
void en_hanuman_shield_break_rotate_yxz(int *ang);

#endif /* REZ_COMMON_EN_HANUMAN_SHIELD_BREAK_H */
