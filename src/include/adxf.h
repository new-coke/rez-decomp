/* ADX file access: handles, AFS partitions, ROFS device. */

#ifndef REZ_ADXF_H
#define REZ_ADXF_H

#include "cri.h"

struct SjObj;

void adxf_lib_init(void);
void adxf_lib_finish(void);
void adxf_rofs_add_device(void);
void adxf_rofs_del_device(void);
int adxf_rofs_add_volume(const char *vol, const char *fname);
void adxf_rofs_set_def_volume(const char *vol);
AdxfObj *ADXF_Open(const char *fname, void *dir);
void adxf_close_handle(void *obj);
int ADXF_ReadSj32(AdxfObj *adxf, int nsct, struct SjObj *sj);
int ADXF_ReadNw32(void *obj, int nsct, void *buf);
int ADXF_ReadNw(AdxfObj *adxf, int nsct, void *buf);
void adxf_exec_server(void);
int ADXF_Seek(AdxfObj *adxf, int pos, int type);
int ADXF_GetFsizeSct(AdxfObj *adxf);
int ADXF_GetStat(void *obj);
int adxf_get_pt_file_info_dir(int ptid, int flid, char *fname, void **dir, int *ofst, int *nsct);

#endif /* REZ_ADXF_H */
