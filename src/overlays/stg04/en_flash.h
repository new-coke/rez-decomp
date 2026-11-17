/* Area 4 muzzle flash. */

#ifndef REZ_STG04_EN_FLASH_H
#define REZ_STG04_EN_FLASH_H

#include "stg04.h"

struct EnFlashWork;

void en_flash_exec(struct EnFlashWork *w);
void en_flash_disp(struct EnFlashWork *w);
void en_flash_init(struct EnFlashWork *w, EnFlashArg *prm);
void en_flash_release(char *w);
void *en_flash_create(void *parent, void *prm);
void *en_flash_spawn(void *parent, int pos);

#endif /* REZ_STG04_EN_FLASH_H */
