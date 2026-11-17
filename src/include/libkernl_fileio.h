/* File I/O through the IOP file server. */

#ifndef REZ_LIBKERNL_FILEIO_H
#define REZ_LIBKERNL_FILEIO_H

struct SlotEntry;

extern char libkernl_ver_tag[];

int sceFsReset(void);
int sceOpen(const char *name, int flags, ...);
int sceClose(int fd);
int sceLseek(int fd, int offset, int whence);
int sceRead(int fd, void *buf, int size);
int sceWrite(int fd, char *buf, int size);
int sceIoctl(int fd, int cmd, void *argp);

#endif /* REZ_LIBKERNL_FILEIO_H */
