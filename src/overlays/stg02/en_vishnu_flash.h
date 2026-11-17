/* Vishnu's hand flash, Area 2. */

#ifndef REZ_STG02_EN_VISHNU_FLASH_H
#define REZ_STG02_EN_VISHNU_FLASH_H

#include "types_common.h"
#include "stg02.h"

struct EnVishnuFlashWork;

void en_vishnu_flash_exec(int *w);
void en_vishnu_flash_disp(struct EnVishnuFlashWork *w);
void en_vishnu_flash_init(void *work, Vec3 *pos);
void en_vishnu_flash_release(char *w);
EnVishnuFlashView *en_vishnu_flash_create(void *parent, Vec3 *pos);

#endif /* REZ_STG02_EN_VISHNU_FLASH_H */
