/* ROFS public interface. */

#ifndef REZ_ROFS_IF_H
#define REZ_ROFS_IF_H
#include "types_cri.h"

int rofs_if_set(void *cfg);
void rofs_if_finish(void);
int ROFS_AddVolume(const char *vol, void *img, int extra);
int ROFS_DelVolume(const char *vol);
void *ROFS_Open(const char *fname, void *dir);
void rofs_if_exec(void);
int rofs_get_file_size(const char *fname);
int rofs_get_file_size_dir(const char *fname, void *dir);
int rofs_get_sct_len(void);
int ROFS_GetVolumeInf(const char *vol, void *info);
int ROFS_LoadDir(const char *dname, void *buf, int size);
int ROFS_GetDirInf(void *dir, void *inf, int n);
void *ROFS_IsExistFile(const char *fname, void *dir);
int ROFS_GetNumFiles(const char *vol);
void ROFS_EntryErrFunc(void *fn, void *obj);

extern RofsIf *rofs_if;

#endif /* REZ_ROFS_IF_H */
