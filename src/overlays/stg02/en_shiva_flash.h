/* Shiva's muzzle flash, Area 2. */

#ifndef REZ_STG02_EN_SHIVA_FLASH_H
#define REZ_STG02_EN_SHIVA_FLASH_H

#include "types_common.h"

struct EnShivaFlashWork;
struct EnShivaFlash;

float en_shiva_flash_curve(float t);
void en_shiva_flash_exec(struct EnShivaFlashWork *w);
void en_shiva_flash_disp(struct EnShivaFlash *w);
void en_shiva_flash_init(int *w, int pos);
void en_shiva_flash_release(void);
void *en_shiva_flash_create(void *parent, Vec3 *pos);

#endif /* REZ_STG02_EN_SHIVA_FLASH_H */
