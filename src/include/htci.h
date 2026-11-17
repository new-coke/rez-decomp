/* Host file device for cvFs. */

#ifndef REZ_HTCI_H
#define REZ_HTCI_H

#include "types_cri.h"

extern HtciErrFn htci_error_fn;
extern void *htci_error_obj;
extern HtciObj htci_obj_tbl[13];
extern int htci_open_mode;
extern char htci_path_buf[];
extern volatile int htci_ver_str;
extern int htci_vtbl[];

void htci_error(void *hn, const char *msg);
void htci_srd_wait(void);
int htci_srd_wait_fd(int fd);
int htci_get_host_fsize(int fd);

#endif /* REZ_HTCI_H */
