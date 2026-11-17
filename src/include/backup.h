/* Memory-card save, load and format tasks. */

#ifndef REZ_BACKUP_H
#define REZ_BACKUP_H
#include "types_common.h"

struct SaveWork;
struct LoadWork;
struct FormatWork;
struct CheckVmsWork;

extern FileReq filereq_work;
extern McSlot mc_slots[];

int filereq_is_free(void);
void filereq_clear(void);
int filereq_is_idle(void);
int filereq_get_state(void);
void *filereq_get_buf(void);
int filereq_get_result(void);
void filereq_set(int cmd, int drive, const char *name, void *buf, int size, int flag);
void filereq_set_path(int drive, const char *name, void *buf);
void filereq_set_drive(int drive);
int backup_mc_callback(int port, int op, int status, int param);
void *BackupSaveTaskSet(void);
void *BackupLoadTaskSet(void *owner);
void *BackupFormatTaskSet(void *owner);
void backup_checkvms_task_init(struct CheckVmsWork *w);
void backup_checkvms_task_exec(struct CheckVmsWork *w);

#endif /* REZ_BACKUP_H */
