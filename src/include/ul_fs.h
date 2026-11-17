/* File access: paths, directories, device choice, reads, IOP modules. */

#ifndef REZ_UL_FS_H
#define REZ_UL_FS_H
#include "types_gw.h"

struct FsSlot;
struct FsCtx;

int ulFsLoadModule(char *name, int arglen, int args);
int ulFsMakePath(char *name, char *out);
int ulFsRead(struct FsSlot *file, int nsct, void *buf);
int ulFsChangeDir(char *dir);
void ulFsSetRootDir(char *dir);
int ulFsSetDevice(int dev);
void ulFsInit(int n, struct FsCtx *ctx, int nent, int *ent);

extern unsigned char _ctype_[];        /* newlib: (_ctype_ + 1)[c]; entry 0 is EOF */

#endif /* REZ_UL_FS_H */
