/* ROFS file handles. */

#ifndef REZ_ROFS_HN_H
#define REZ_ROFS_HN_H

#include "types_cri.h"

int RFHN_WaitCompleteReq(RofsHn *hn);
RofsHn *RFHN_CreateHnG(RofsHn *hn, int idx, int lsn, int size, RofsVol *vol);
RofsHn *RFHN_ReopenHn(RofsHn *hn, int idx, int lsn, int size, RofsVol *vol);
int rofs_is_volume_busy(RofsHn *hn, int id);
int rofs_load_dir_simple(int x);
void rofs_close(RofsHn *hn);
int ROFS_Seek(RofsHn *hn, int ofst, unsigned int whence);
int ROFS_Tell(RofsHn *hn);
int ROFS_ReqRd(RofsHn *hn, int nsct, void *buf);
int ROFS_GetStat(RofsHn *hn);
int ROFS_GetNumTr(RofsHn *hn);
void ROFS_StopTr(RofsHn *hn);

#endif /* REZ_ROFS_HN_H */
