/* Muzzle flash of the Hunter and the Chaser. */

#ifndef REZ_COMMON_EN_KAWASEMI_FLASH_H
#define REZ_COMMON_EN_KAWASEMI_FLASH_H

#include "types_overlays.h"

void en_kawasemi_flash_exec(EnKawasemiFlashWork *w);
void en_kawasemi_flash_disp(EnKawasemiFlashWork *w);
void en_kawasemi_flash_init(void *work, void *pt, int mode);
void en_kawasemi_flash_release(EnKawasemiFlashWork *w);
void *en_kawasemi_flash_create(void *parent, void *pos, int mode);

#endif /* REZ_COMMON_EN_KAWASEMI_FLASH_H */
