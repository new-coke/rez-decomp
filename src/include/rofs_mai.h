/* ROFS core: work area, watchdog, sync reads, errors. */

#ifndef REZ_ROFS_MAI_H
#define REZ_ROFS_MAI_H

#include "types_cri.h"

extern char rofs_error_buf[];
extern int rofs_iso_hdr0;
extern int rofs_iso_hdr1;
extern int rofs_iso_hdr2;
extern int rofs_iso_hdr3;
extern CriDevIf rofs_mai_if_tbl;
extern volatile int rofs_vsync_count;
extern RofsWork *rofs_work;

int rofs_if_init(void *cfg);
void rofs_reset_watchdog(void);
int rofs_check_watchdog(void *fh);
int RFMAI_ReadWG(RofsVol *vol, int lsn, int nsct, void *buf);
int rofs_error(const char *msg, int line, const char *file, int err);
void rofs_to_upper(char *s);


#endif /* REZ_ROFS_MAI_H */
