/* ROFS image device, vsync counter. */

#ifndef REZ_ROFS_PFS_H
#define REZ_ROFS_PFS_H

#include "types_cri.h"

extern RofsDev rofs_dcf_dev;

void rofs_delay_short(void);
void rofs_install_count_cb(void);
void rofs_restore_cb(void);
void rofs_set_pfs_flag(int x);
RofsDev *rofs_get_cd_device(void);

extern RofsPfs *rofs_pfs_tbl;              /* image files are read through it */

#endif /* REZ_ROFS_PFS_H */
