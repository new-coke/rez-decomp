/* IOP reboot, module loading and state. */

#ifndef REZ_LIBULIOP_H
#define REZ_LIBULIOP_H
#include "types_gw.h"

int uliop_reboot_iop(char *img, int dvd, int load_mem2mb);
int uliop_set_path(char *path);
int uliop_init(void);

extern UlIop uliop_work;

#endif /* REZ_LIBULIOP_H */
