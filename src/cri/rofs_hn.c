/* ROFS file handles: files or sector ranges, sector seeks, device reads, waits with a watchdog. */

#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "rofs_hn.h"
#include "adx_error.h"
#include "cvfs.h"
#include "dvci.h"
#include "lsc.h"
#include "rofs_if.h"
#include "rofs_mai.h"
#include "rofs_pfs.h"
#include "dtx.h"

typedef struct {
    int nent;
    int nmax;
    int unk_08; /* cleared by RFHN_CreateDirhn */
} RofsDirHn;

/* Runs device and ROFS servers until done or no progress (-201); 0 when done, else the error. */
/* 0x001de730 */
int RFHN_WaitCompleteReq(RofsHn *hn)
{
    rofs_reset_watchdog();
    do {
        hn->work->io.dev->exec_server();
        rofs_if_exec();
        rofs_delay_short();
        switch (((unsigned int (*)(RofsHn *))ROFS_GetStat)(hn)) { /* steer(cast) */
        case 1:
            return 0;
        case 0:
            return -204;
        case 2:
            break;
        case 3:
            return hn->err;
        default:
            return -1000;
        }
    } while (rofs_check_watchdog(hn->vol->fh) == 1);
    return -201;
}

/* Nothing transferred. */
/* 0x001de7e0 */
static void rofs_init_hn(RofsHn *hn, int idx, int lsn, int size, RofsVol *vol)
{
    if (hn != 0) {
        hn->used = 1;
        hn->idx = idx;
        hn->lsn = lsn;
        hn->sct_ofs = 0;
        hn->fsize = size;
        hn->fsct = (size + 2047) >> 11;
        hn->vol = vol;
        hn->nsct = 0;
        hn->pos = 0;
        hn->ntr = 0;
        hn->rqsct = 0;
        hn->rdbuf = 0;
        hn->stat = 0;
        hn->trstat = 0;
        hn->err = 0;
    }
}

/* Mounted volume; 0 on failure. */
/* 0x001de838 */
RofsHn *RFHN_CreateHnG(RofsHn *hn, int idx, int lsn, int size, RofsVol *vol)
{
    if (hn == 0)
        return 0;
    if (vol == 0)
        return 0;
    if (hn->used != 0)
        return 0;
    if (vol->fh == 0)
        return 0;
    rofs_init_hn(hn, idx, lsn, size, vol);
    return hn;
}

/* Unless reading; 0 on failure. */
/* 0x001de890 */
RofsHn *RFHN_ReopenHn(RofsHn *hn, int idx, int lsn, int size, RofsVol *vol)
{
    if (hn == 0 || hn->stat == 1)
        return 0;
    rofs_init_hn(hn, idx, lsn, size, vol);
    return hn;
}

/* Empty record for n entries; 0 on bad arguments. */
/* 0x001de8d8 */
RofsDirHn *RFHN_CreateDirhn(RofsDirHn *rec, int n)
{
    if (rec == 0 || n < 0)
        return 0;
    memset(rec, 0, 72);
    rec->nmax = n;
    rec->nent = 0;
    rec->unk_08 = 0;
    return rec;
}

/* Any handle reading image id (hn's own when 0); one reading with no volume stalls the walk. */
/* 0x001de938 */
int rofs_is_volume_busy(RofsHn *hn, int id)
{
    RofsWork *work;
    RofsHn *other;
    int i;

    if (hn == 0)
        return 0;
    if (id == 0)
        id = hn->vol->fh;
    work = hn->work;
    other = work->hns;
    for (i = 0; i < work->nhn; i++) {
        if (other->stat == 1) {
            if (other->vol == 0)
                continue;
            if (other->vol->fh == id)
                return 1;
        }
        other++;
    }
    return 0;
}

/* 0x001de9b8 */
int rofs_load_dir_simple(int x)
{
    return ROFS_LoadDir(x, 0, 0);
}

/* Stops a transfer in progress. */
/* 0x001de9d8 */
void rofs_close(RofsHn *hn)
{
    if (hn != 0 && hn->used != 0) {
        if (hn->trstat == 2)
            ROFS_StopTr(hn);
        hn->used = 0;
        hn->stat = 0;
    }
}

/* Sectors (whence 0 start, 1 current, 2 end) within the file; -101 bad handle, -205 bad seek. */
/* 0x001dea28 */
int ROFS_Seek(RofsHn *hn, int ofst, unsigned int whence)
{
    int size;
    int pos;

    if (hn == 0)
        return rofs_error("rofs_hn.c", 286, "ROFS_Seek", -101);
    if (hn->used != 1)
        return rofs_error("rofs_hn.c", 289, "ROFS_Seek", -101);
    size = hn->fsct;
    switch (whence) {
    case 0:
        pos = ofst;
        break;
    case 1:
        pos = hn->sct_ofs + ofst;
        break;
    case 2:
        pos = size + ofst;
        break;
    default:
        return rofs_error("rofs_hn.c", 308, "ROFS_Seek", -205);
    }
    if (pos < 0 || size < pos)
        return rofs_error("rofs_hn.c", 313, "ROFS_Seek", -205);
    hn->sct_ofs = pos;
    return pos;
}

/* Sectors; -101 bad handle. */
/* 0x001deb00 */
int ROFS_Tell(RofsHn *hn)
{
    if (hn == 0)
        return rofs_error("rofs_hn.c", 329, "ROFS_Tell", -101);
    if (hn->used != 1)
        return rofs_error("rofs_hn.c", 332, "ROFS_Tell", -101);
    return hn->sct_ofs;
}

/* To the end at most; 0 at EOF, -101 bad handle, -200 busy, -204 device refused. */
/* 0x001deb68 */
int ROFS_ReqRd(RofsHn *hn, int nsct, void *buf)
{
    short used;
    int lsn;
    int end;
    int ret;

    if (hn == 0)
        return rofs_error("rofs_hn.c", 349, "ROFS_ReqRd", -101);
    used = hn->used;
    if (used != 1)
        return rofs_error("rofs_hn.c", 352, "ROFS_ReqRd", -101);
    if (hn->stat == used || rofs_is_volume_busy(hn, 0) == used)
        return -200;
    lsn = hn->lsn + hn->sct_ofs;
    end = hn->lsn + hn->fsct;
    if (lsn >= end) {
        hn->trstat = used;
        return 0;
    }
    if (end < lsn + nsct)
        nsct = end - lsn;
    if (nsct <= 0) {
        hn->trstat = used;
        return 0;
    }
    ret = hn->work->io.pfs->get_stat(hn->vol->unk_10, hn->vol->fh, lsn, nsct, buf);
    if (ret) {
        hn->rdbuf = buf;
        hn->trstat = 2;
        hn->stat = used;
        hn->pos = 0;
        hn->ntr = 0;
        hn->rqsct = nsct;
        hn->nsct = nsct;
    } else {
        hn->trstat = used;
    }
    return ret ? nsct : -204;
}

/* 0 idle, 1 done, 2 reading, 3 error (or bad handle). */
/* 0x001decb0 */
int ROFS_GetStat(RofsHn *hn)
{
    if (hn == 0) {
        rofs_error("rofs_hn.c", 425, "ROFS_GetStat", -101);
        return 3;
    }
    if (hn->used == 0) {
        rofs_error("rofs_hn.c", 429, "ROFS_GetStat", -101);
        return 3;
    }
    return hn->trstat;
}

/* Bytes of the current read; -101 bad handle. */
/* 0x001ded10 */
int ROFS_GetNumTr(RofsHn *hn)
{
    if (hn == 0)
        return rofs_error("rofs_hn.c", 444, "ROFS_GetNumTr", -101);
    if (hn->used == 0)
        return rofs_error("rofs_hn.c", 447, "ROFS_GetNumTr", -101);
    return (hn->pos << 11) + hn->ntr;
}

/* Cancels an in-flight device read. */
/* 0x001ded80 */
void ROFS_StopTr(RofsHn *hn)
{
    short st;

    if (hn == 0) {
        rofs_error("rofs_hn.c", 461, "ROFS_StopTr", -101);
        return;
    }
    if (hn->used == 0) {
        rofs_error("rofs_hn.c", 465, "ROFS_StopTr", -101);
        return;
    }
    st = hn->trstat;
    if (st != 2) {
        hn->trstat = 0;
    } else {
        if (hn->stat == 1)
            hn->work->io.pfs->open(hn->vol->fh);
        hn->stat = st;
    }
}
