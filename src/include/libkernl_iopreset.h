/* IOP reboot, reboot-finished poll. */

#ifndef REZ_LIBKERNL_IOPRESET_H
#define REZ_LIBKERNL_IOPRESET_H

int sceSifSyncIop(void);
int sceSifRebootIop(const char *img);

#endif /* REZ_LIBKERNL_IOPRESET_H */
