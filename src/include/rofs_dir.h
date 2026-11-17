/* ROFS directories. */

#ifndef REZ_ROFS_DIR_H
#define REZ_ROFS_DIR_H

#include "types_cri.h"

int rofs_read_dir(RofsVol *vol, char *buf, int lsn, RofsDir *dir, int size);
int RFDIR_SearchName(const char *fname, RofsDir *dir, int reserved);
int RFDIR_SearchPath(void *vol, void *buf, void *ptloc, void *ptsize, void *path);

#endif /* REZ_ROFS_DIR_H */
