/* CRI virtual file system. */

#ifndef REZ_CVFS_H
#define REZ_CVFS_H

#include "types_cri.h"

extern char cvfs_def_dev_name[];
extern CvFsDev cvfs_dev_tbl[32];
extern void *cvfs_error_obj;
extern CvFsHn cvfs_hn_tbl[40];
extern int cvfs_init_count;
extern char *volatile cvfs_ver_str;

void cvfs_error(const char *msg);
void cvfs_lib_finish(void);
void cvFsAddDev(char *name, CvFsDevIf *(*getif)(void));
void cvFsDelDev(char *name);
void cvFsSetDefDev(char *name);
CvFsHn *cvFsOpen(const char *path, void *dir, int mode);
void cvFsClose(CvFsHn *hn);
int cvFsTell(CvFsHn *hn);
int cvFsSeek(CvFsHn *hn, int ofst, int whence);
int cvFsReqRd(CvFsHn *hn, int nsct, void *buf);
void cvFsStopTr(CvFsHn *hn);
void cvfs_exec_server(void);
int cvFsGetStat(CvFsHn *hn);
int cvFsGetFileSize(const char *path);
int cvFsGetFileSizeEx(const char *path, int dir);
int cvFsGetSctLen(CvFsHn *hn);
int cvFsGetNumTr(CvFsHn *hn);
int cvFsAddVolume(char *dev, char *vol, void *img);
int cvFsDelVolume(char *dev, char *vol);
int cvFsGetVolumeInfo(char *dev, char *vol, void *info);

extern void (*cvfs_error_fn)(void *obj, const char *msg, int arg);

#endif /* REZ_CVFS_H */
