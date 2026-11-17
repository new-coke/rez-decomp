/* Sector read driver: disc and host reads. */

#ifndef REZ_SRD_H
#define REZ_SRD_H

#include "types_cri.h"

extern int srd_check_drive;
extern volatile int srd_check_locked;
extern volatile int srd_exec_busy;
extern int srd_exec_locked;
extern SrdObj srd_req;
extern volatile int srd_ver_str;
extern volatile int srd_wait_count;

int srd_req_cd_read(int lsn, int nsct, void *buf, SrdMode *mode);
int srd_req_host_read(int fd, void *buf, int size);
int srd_get_stat(void);
void srd_wait(void);
void srd_cancel(void);
int srd_is_busy(void);
void srd_exec_server(void);
void srd_lock(void);
void srd_unlock(void);

#endif /* REZ_SRD_H */
