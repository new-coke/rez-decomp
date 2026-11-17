/* Area 2 break-up effects. */

#ifndef REZ_STG02_EN_SHIVA_BREAK_H
#define REZ_STG02_EN_SHIVA_BREAK_H

#include "types_overlays.h"
#include "types_common.h"

struct EnShivaBreakWork;
struct EnShivaBreak;

void en_shiva_break_shatter(struct EnShivaBreakWork *w);
Shatter *en_shiva_break_spawn_plain(void *obj, Vec3 *pos, int *ang, int kind);
Shatter *en_shiva_break_spawn_tuned(void *obj, Vec3 *pos, int *ang, int kind);
void en_shiva_break_shatter_part(struct EnShivaBreakWork *w, int i);
void en_shiva_break_exec(struct EnShivaBreakWork *w);
void en_shiva_break_toggle_parts(char *w);
void en_shiva_break_disp(struct EnShivaBreakWork *w);
void en_shiva_break_init(struct EnShivaBreakWork *w);
void en_shiva_break_release(struct EnShivaBreak *w);
void *en_shiva_break_spawn(void *obj, Vec3 *pos, int *ang, unsigned int kind);
void en_shiva_break_rotate_yxz(int *ang);

#endif /* REZ_STG02_EN_SHIVA_BREAK_H */
