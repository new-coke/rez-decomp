/*
 * Memory card library: watches both ports, sets up the save directory (icon.sys, icons), loads,
 * saves, lists, renames, deletes in the background via a callback; CRC-checked save images.
 */

#include "types_ul.h"
#include "types_gw.h"
#include "libc.h"

/*
 * Results: 0 ok, -200 busy (retry), -100 no usable card or slot != 0, -1000 refused, -4 no file,
 * -2 unformatted. Card numbers: 0 = port 0, 2 = port 1; others map to port -1, slot -1.
 */

static void ulMcLoadComplete(int op, int res);
static void ulMcSaveComplete(int op, int res);
static void ulMcStatusComplete(int op, int res);
static void ulMcCreateComplete(int op, int res);
static void ulMcHandleCompletion(int op, int res);
static long ulMcLoadStart(int port, int slot, int path, int buf, int size);
static long ulMcLoadStartAt(int port, int slot, int path, int buf, int ofs, int size);
static long ulMcSaveStart(int port, int slot, int path, int buf, int size, int attr);
static long ulMcSaveStartAt(int port, int slot, int path, int buf, int ofs, int size, int attr);
static int ulMcRemount(int port, int slot);
static int ulMcPortToCard(int port);
static int mc_scan_drain_events(void);
static int mc_check_format(int port, int slot);

extern int sceMcSync(int mode, int *cmd, int *result);

/*
 * Registered with ulMcInit: save directory, icon.sys (holds the icon file names), icon model and
 * optional copy/delete icons.
 */
typedef struct McSaveDesc {
    char *dev;
    char *base;
    int data0;
    int size0;
    int data1;
    int size1;
    int data2;
    int size2;
} McSaveDesc;

typedef struct McDirSize {
    char pad_00[16];
    int size;
} McDirSize;

/* Ends save-directory setup: marks mounted, restarts ulmc_pending_cmd. */
#define ALL_DONE                                                                         \
    {                                                                                    \
        int slot_ = ulmc_cur_slot;                                                          \
        int port_ = ulmc_cur_port;                                                          \
        unsigned int st_;                                                                \
        int one_ = 1;                                                                    \
        int o_;                                                                          \
        mc_status = 0;                                                                   \
        st_ = ulmc_pending_cmd;                                                                \
        ulmc_cur_cmd = 0;                                                                  \
        /* ulmc_mounted[port_][slot_] = one_, with the entry address added up as an */ \
        /* int                                                                     */ \
        o_ = slot_ * 4;                                                                  \
        o_ += port_ * 16;                                                                \
        o_ += (int)ulmc_mounted;                                                           \
        *(int *)o_ = one_;                                                               \
        switch (st_) {                                                                   \
        case 110:                                                                        \
            ulMcLoadStart(port_, slot_, ulmc_path, ulmc_io_buf, ulmc_io_size);             \
            break;                                                                       \
        case 111:                                                                        \
            ulMcLoadStartAt(port_, slot_, ulmc_path, ulmc_io_buf, ulmc_io_ofs, ulmc_io_size);\
            break;                                                                       \
        case 112:                                                                        \
            ulMcSaveStart(port_, slot_, ulmc_path, ulmc_io_buf, ulmc_io_size, ulmc_file_attr); \
            break;                                                                       \
        case 113:                                                                        \
            ulMcSaveStartAt(port_, slot_, ulmc_path, ulmc_io_buf, ulmc_io_ofs, ulmc_io_size, \
                           ulmc_file_attr);                                                  \
            break;                                                                       \
        }                                                                                \
        ulmc_pending_cmd = 0;                                                                  \
    }


typedef struct McCardInfo {
    char pad_00[32];
    short unk_20; /* cleared by ulMcGetCardInfo */
    short unk_22; /* cleared by ulMcGetCardInfo */
    short nfree;
    short unk_26; /* same value as nfree (ulMcGetCardInfo) */
    short unk_28; /* cleared by ulMcGetCardInfo */
    short unk_2a; /* cleared by ulMcGetCardInfo */
    short cluster_size;
    short unk_2e; /* cleared by ulMcGetCardInfo */
} McCardInfo;

/* ulmc_dir_ent as modification time, size and attributes. */
typedef struct McDirTime {
    char pad_00[9];
    unsigned char sec;
    unsigned char min;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned short year;
    unsigned int size;
    unsigned short status;
} McDirTime;

/* dow: day of the week. */
typedef struct McFileStat {
    unsigned int size;
    short blocks;
    char unk_06; /* cleared by ulMcGetFileStatus */
    unsigned char flag;
    short unk_08; /* cleared by ulMcGetFileStatus */
    short year;
    char month;
    char day;
    char hour;
    char min;
    char sec;
    char dow;
} McFileStat;


typedef struct McMountMcDirSize {
    char pad_00[16];
    unsigned int size;
} McMountMcDirSize;

/* r = size of path in 1 KB blocks (rounded up), or -100/-1000/-4; waits while busy. */
#define MC_FILE_BLOCKS_ULMCGETFILEBLOCKS(r, path, L)                                              \
    {                                                                           \
        int mc_cmd;                                                             \
        int mc_res;                                                             \
        int mc_cnt;                                                             \
        int mc_ret;                                                             \
        int mc_blocks;                                                          \
        pth = (path);                                                           \
        if (slot != 0) {                                                        \
            r = -100;                                                           \
        } else {                                                                \
            do {                                                                \
            } while (0);                                                        \
          L:                                                                    \
            mc_ret = sceMcSync(1, &mc_cmd, &mc_res);                           \
            if (mc_ret == 0)                                                    \
                goto L;                                                         \
            if (mc_ret == 1) {                                                  \
                ulMcHandleCompletion(mc_cmd, mc_res);                           \
                goto L;                                                         \
            }                                                                   \
            mc_ret = ((int (*)(int, int, char *, int, int, McMountMcDirSize *))sceMcGetDir)(port, slot, pth, 0, 1, &ulmc_dir_ent);         \
            if (mc_ret < 0) {                                                   \
                r = -1000;                                                      \
            } else {                                                            \
                mc_dir(pth);                                                    \
                sceMcSync(0, 0, &mc_cnt);                                      \
                if (mc_cnt < -10) {                                             \
                    r = -100;                                                   \
                } else if (mc_cnt == 0) {                                       \
                    r = -4;                                                     \
                } else {                                                        \
                    mc_blocks = mc_cnt;                                         \
                    if (mc_blocks > 0)                                          \
                        mc_blocks = ((*(McMountMcDirSize *)&ulmc_dir_ent).size + 1023) >> 10;             \
                    r = mc_blocks;                                              \
                }                                                               \
            }                                                                   \
        }                                                                       \
    }

/*
 * Save image header, 96 bytes, data follows: CRC-16 of the data, magic 0x550047, data and length.
 */
typedef struct {
    char hdr0[70];
    unsigned short crc;
    char pad_48[4];
    short flag;
    char pad_4e[2];
    void *magic;
    char pad_54[4];
    char *data;
    unsigned int len;
} McImg;



/* As sceMcGetDir fills it, 64 bytes. */
typedef struct {
    char created[8];
    char modified[8];
    unsigned int size;      /* bytes */
    unsigned short attr;
    char pad_16[10];
    char name[32];
} McDirEnt;

extern void (*ulmc_user_callback)();
extern int ulmc_port_mode;
extern int ulmc_cur_port;
extern int ulmc_cur_slot;
extern int ulmc_callback_arg;
extern int ulmc_cur_cmd;
extern int ulmc_load_fd;
extern int ulmc_io_buf;
extern int ulmc_io_ofs;
extern int ulmc_io_size;
extern int ulmc_save_fd;
extern int ulmc_file_attr;
extern int ulmc_file_attr_mask;
extern McDirStat ulmc_dir_ent;
extern char ulmc_path[];
extern int sceMcSeek(int, int, int);
extern int sceMcWrite(int, int, int);
extern int sceMcClose(int);
extern int sceMcGetDir(int, int, char *, int, int, McDirStat *);
extern int sceMcSetFileInfo(int, int, char *, McDirStat *, int);
extern int ulmc_setup_flag;
extern int mc_connected[][4];
extern int ulmc_mounted[][4];
extern int mc_status;
extern int ulmc_create_buf;
extern int ulmc_create_size;
extern int ulmc_create_fd;
extern void ulMcLoadComplete(int, int);
extern void ulMcSaveComplete(int, int);
extern void ulMcStatusComplete(int, int);
extern void ulMcConnectEvent(int, int);
extern void ulMcCreateComplete(int, int);
extern int ulmc_slot_count;
extern void sceMcInit(void);
extern int ulmc_poll_port;
extern int ulmc_pending_cmd;
extern int ulmc_retry;
extern int ulmc_create_flags;
extern int sceMcChdir(int, int, char *, int);
extern char str_ulmc_parent_dir[];
extern int ulmc_list_port;
extern int ulmc_list_slot;
extern int sceOpen(char *, int);
extern int sceMcOpen(int, int, char *, int);
extern int sceMcRead(int, char *, int);
extern int sceWrite(int, char *, int);
extern McSaveDesc *ulmc_save_desc;
extern long sceMcGetInfo(int, int, int, int, int *);
extern char str_ulmc_wildcard[];
extern int sceMcDelete(int, int, int);
extern int sceMcRename(int, int, int, int);
extern int ulmc_sub_callback;
extern int mc_check_format(int p, int s);
extern int sceMcFormat(int a, int b);
extern int ulmc_port_state[][4];
extern int ulmc_scan_index;
extern int mc_scan_drain_events(void);
extern void ulMcPoll(void);
extern int sceMcUnformat(int, int);
extern char str_ulmc_cur_dir[];
extern int sceMcMkdir(int port, int slot, const char *name);
extern long ulMcLoadStart(int port, int slot, int path, int buf, int size);
extern long ulMcLoadStartAt(int port, int slot, int path, int buf, int ofs, int size);
extern int ulMcPortToCard(int port);
extern int ulMcRemount(int port, int slot);
extern long ulMcSaveStart(int port, int slot, int path, int buf, int size, int attr);
extern long ulMcSaveStartAt(int port, int slot, int path, int buf, int ofs, int size, int attr);

/*
 * State 0x6e, or 0x6f from ulmc_io_ofs: open, seek, read ulmc_io_size into ulmc_io_buf, close,
 * callback. Errors end at once; a missing file (-4) reports 0.
 */
/* 0x001810c0 */
static void ulMcLoadComplete(int op, int res)
{
    int st;
    int port;
    int slot;

    if (res < 0)
        goto err;
    switch (op) {
    case 2:
        ulmc_load_fd = res;
        if (ulmc_cur_cmd == 0x6e) {
            sceMcRead(res, ulmc_io_buf, ulmc_io_size);
            st = 0;
            return;
        }
        sceMcSeek(res, ulmc_io_ofs, 0);
        st = 0;
        return;
    case 4:
        sceMcRead(ulmc_load_fd, ulmc_io_buf, ulmc_io_size);
        st = 0;
        return;
    case 5:
        sceMcClose(ulmc_load_fd);
        st = 0;
        return;
    case 3:
        st = ulmc_cur_cmd;
        ulmc_cur_cmd = 0;
        port = ulmc_cur_port;
        slot = ulmc_cur_slot;
        if (st == 1)
            return;
        if (ulmc_user_callback == 0)
            return;
        if (ulmc_port_mode != 0)
            ulmc_user_callback(port, slot, st, res, ulmc_callback_arg);
        else
            ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), st, res, ulmc_callback_arg); /* steer(cast) */
        return;
    }
    return;
  err:
    if (res == -4) {
        st = ulmc_cur_cmd;
        ulmc_cur_cmd = 0;
        port = ulmc_cur_port;
        slot = ulmc_cur_slot;
        if (st == 1)
            return;
        if (ulmc_user_callback == 0)
            return;
        if (ulmc_port_mode == 0)
            ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), st, 0, ulmc_callback_arg); /* steer(cast) */
        else
            ulmc_user_callback(port, slot, st, 0, ulmc_callback_arg);
        return;
    }
    st = ulmc_cur_cmd;
    ulmc_cur_cmd = 0;
    port = ulmc_cur_port;
    slot = ulmc_cur_slot;
    if (st == 1)
        return;
    if (ulmc_user_callback == 0)
        return;
    if (ulmc_port_mode == 0)
        ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), st, res, ulmc_callback_arg); /* steer(cast) */
    else
        ulmc_user_callback(port, slot, st, res, ulmc_callback_arg);
    return;
}

/*
 * State 0x70/0x71: open, seek, write, close. With ulmc_file_attr bit 3, then rewrites the entry's
 * attributes (ulmc_file_attr under ulmc_file_attr_mask, max 0x80f).
 */
/* 0x00181298 */
static void ulMcSaveComplete(int op, int res)
{
    int st;
    int port;
    int slot;
    unsigned int mask;

    if (res < 0)
        goto err;
    switch (op) {
    case 2:
        ulmc_save_fd = res;
        if (ulmc_cur_cmd == 0x70) {
            sceMcWrite(res, ulmc_io_buf, ulmc_io_size);
            return;
        }
        sceMcSeek(res, ulmc_io_ofs, 0);
        return;
    case 4:
        sceMcWrite(ulmc_save_fd, ulmc_io_buf, ulmc_io_size);
        return;
    case 6:
        sceMcClose(ulmc_save_fd);
        return;
    case 3:
        if (ulmc_file_attr & 8) {
            sceMcGetDir(ulmc_cur_port, ulmc_cur_slot, ulmc_path, 0, 1, &ulmc_dir_ent);
            return;
        }
        goto end;
    case 13:
        mask = ulmc_file_attr_mask & 0x80f;
        ulmc_dir_ent.status = (ulmc_dir_ent.status & ~mask) | (*(unsigned short *)&ulmc_file_attr & mask);
        sceMcSetFileInfo(ulmc_cur_port, ulmc_cur_slot, ulmc_path, &ulmc_dir_ent, 4);
        return;
    case 14:
      end:
        st = ulmc_cur_cmd;
        ulmc_cur_cmd = 0;
        port = ulmc_cur_port;
        slot = ulmc_cur_slot;
        if (st == 1)
            return;
        if (ulmc_user_callback == 0)
            return;
        if (ulmc_port_mode == 0)
            ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), st, res, ulmc_callback_arg); /* steer(cast) */
        else
            ulmc_user_callback(port, slot, st, res, ulmc_callback_arg);
        return;
    }
    return;
  err:
    st = ulmc_cur_cmd;
    ulmc_cur_cmd = 0;
    port = ulmc_cur_port;
    slot = ulmc_cur_slot;
    if (st == 1)
        return;
    if (ulmc_user_callback == 0)
        return;
    if (ulmc_port_mode == 0)
        ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), st, res, ulmc_callback_arg); /* steer(cast) */
    else
        ulmc_user_callback(port, slot, st, res, ulmc_callback_arg);
}

/*
 * State 0x72, from mc_get_dir_request: merges ulmc_file_attr under ulmc_file_attr_mask (max
 * 0x80f) into the entry and writes it back.
 */
/* 0x001814b0 */
static void ulMcStatusComplete(int op, int res)
{
    int st;
    int port;
    int slot;
    unsigned int mask;

    if (res < 0)
        goto err;
    switch (op) {
    case 13:
        mask = ulmc_file_attr_mask & 0x80f;
        ulmc_dir_ent.status = (ulmc_dir_ent.status & ~mask) | (ulmc_file_attr & mask);
        sceMcSetFileInfo(ulmc_cur_port, ulmc_cur_slot, ulmc_path, &ulmc_dir_ent, 4);
        return;
    case 14:
        st = ulmc_cur_cmd;
        ulmc_cur_cmd = 0;
        port = ulmc_cur_port;
        slot = ulmc_cur_slot;
        if (st == 1)
            return;
        if (ulmc_user_callback == 0)
            return;
        if (ulmc_port_mode == 0)
            ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), st, res, ulmc_callback_arg); /* steer(cast) */
        else
            ulmc_user_callback(port, slot, st, res, ulmc_callback_arg);
        return;
    }
    return;
  err:
    st = ulmc_cur_cmd;
    ulmc_cur_cmd = 0;
    port = ulmc_cur_port;
    slot = ulmc_cur_slot;
    if (st == 1)
        return;
    if (ulmc_user_callback == 0)
        return;
    if (ulmc_port_mode == 0)
        ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), st, res, ulmc_callback_arg); /* steer(cast) */
    else
        ulmc_user_callback(port, slot, st, res, ulmc_callback_arg);
}

/*
 * State 0x73, type in ulmc_file_attr. New type 2 (any with ulmc_port_mode): event 100, then 101
 * (card-number mode: only with auto mount). PS1/PDA logged, others illegal. < -10: event 102.
 */
/* 0x001815f0 */
void ulMcConnectEvent(int op, int res)
{
    int port;
    int slot;
    int *flag;
    int cb;
    int off;
    int one;

    port = ulmc_cur_port;
    slot = ulmc_cur_slot;
    off = slot * 4 + port * 16;
    flag = (int *)((char *)mc_connected + off);
    ulmc_cur_cmd = 0;
    if (*flag == 0) {
        one = 1;
        if (ulmc_file_attr == one)
            printf("libulmc: connec PS1 port[%d] slot[%d] result[%d]\n", port, slot, res);
        if (ulmc_file_attr == 3)
            printf("libulmc: connec PDA port[%d] slot[%d] result[%d]\n", port, slot, res);
        if (res > -11) {
            if (ulmc_port_mode != 0 || ulmc_file_attr == 2) {
                printf("libulmc: connect port[%d] slot[%d] type[%d]\n", port, slot, ulmc_file_attr);
                *flag = one;
                *(int *)((char *)ulmc_mounted + off) = 0;
                if (ulmc_user_callback) {
                    if (ulmc_port_mode == 0) {
                        cb = ((int (*)(int, int))ulMcPortToCard)(port, slot); /* steer(cast) */
                        (*(int (*(*))())&ulmc_user_callback)(cb, 100, res, ulmc_callback_arg);
                        if (ulmc_setup_flag == 0)
                            (ulmc_user_callback)(cb, 101, res, ulmc_callback_arg);
                    } else {
                        (ulmc_user_callback)(port, slot, 100, ulmc_file_attr, ulmc_callback_arg);
                        (ulmc_user_callback)(port, slot, 101, res, ulmc_callback_arg);
                    }
                }
            } else {
                printf("libulmc: connect illegal-type[%d] port[%d] slot[%d]\n", ulmc_file_attr, port,
                       slot);
            }
        }
    } else if (res < -10) {
        printf("libulmc: disconnect port[%d] slot[%d]\n", ulmc_cur_port, ulmc_cur_slot);
        *flag = 0;
        *(int *)((char *)ulmc_mounted + off) = 0;
        if (ulmc_user_callback) {
            if (ulmc_port_mode == 0) {
                cb = ((int (*)(int, int))ulMcPortToCard)(port, slot); /* steer(cast) */
                (ulmc_user_callback)(cb, 102, res, ulmc_callback_arg);
            } else {
                (ulmc_user_callback)(port, slot, 102, res, ulmc_callback_arg);
            }
        }
    }
}

/*
 * State 0x74: enters ulmc_save_desc->dev (creating it), checks icon.sys (964 bytes), icon, copy and
 * delete icons; recreates missing/wrong-size ones (except loading: ends with 0). ulmc_create_flags.
 */
/* 0x00181860 */
static void ulMcCreateComplete(int op, int res)
{
    McSaveDesc *cfg;
    unsigned int flags;
    unsigned int st;
    char *name;
    int found;
    unsigned int nf;
    int ad;
    int sz;

    if (res >= 0) {
        switch (op) {
        case 11:
            sceMcChdir(ulmc_cur_port, ulmc_cur_slot, ulmc_save_desc->dev, 0);
            found = 0;
            break;
        case 13:
            cfg = ulmc_save_desc;
            found = 0;
            if (res > 0) {
                flags = *(volatile unsigned int *)&ulmc_create_flags; /* steer(volatile) */
                if (!(flags & 1)) {
                    if ((*(McDirSize *)&ulmc_dir_ent).size != 964) /* steer(cast) */
                        goto chk;
                    nf = flags | 1;
                    found = 1;
                } else if (!(flags & 2)) {
                    if ((*(McDirSize *)&ulmc_dir_ent).size != cfg->size0) /* steer(cast) */
                        goto chk;
                    nf = flags | 2;
                    found = 1;
                } else if (!(flags & 4)) {
                    if ((*(McDirSize *)&ulmc_dir_ent).size != cfg->size1) /* steer(cast) */
                        goto chk;
                    nf = flags | 4;
                    found = 1;
                } else {
                    if (flags & 8)
                        goto chk;
                    if ((*(McDirSize *)&ulmc_dir_ent).size != cfg->size2) /* steer(cast) */
                        goto chk;
                    nf = flags | 8;
                    found = 1;
                }
                ulmc_create_flags = nf;
            }
        chk:
            nf = ulmc_create_flags;
            {
                unsigned int pending;
                if (!found && !((pending = ulmc_pending_cmd) > 0x6f || pending < 0x6e)) {
                    ulmc_cur_cmd = 0;
                    ulmc_pending_cmd = 0;
                    mc_status = 0;
                    {
                        int port = ulmc_cur_port, slot = ulmc_cur_slot;
                        if (pending == 1)
                            return;
                        if (ulmc_user_callback == 0)
                            return;
                        if (ulmc_port_mode == 0)
                            ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), pending, 0, ulmc_callback_arg); /* steer(cast) */
                        else
                            ulmc_user_callback(port, slot, pending, 0, ulmc_callback_arg);
                    }
                    return;
                }
            }
            if (!(nf & 1)) {
                sceMcOpen(ulmc_cur_port, ulmc_cur_slot, "icon.sys", 514);
                ulmc_create_size = 964;
                ulmc_create_buf = (int)cfg->base;
                return;
            }
            if (!(nf & 2)) {
                name = cfg->base + 260;
                if (found) {
                    sceMcGetDir(ulmc_cur_port, ulmc_cur_slot, name, 0, 1, &ulmc_dir_ent);
                    found = 0;
                    return;
                }
                sceMcOpen(ulmc_cur_port, ulmc_cur_slot, name, 514);
                ad = cfg->data0;
                sz = cfg->size0;
                goto set;
            }
            if (!(nf & 4)) {
                if (cfg->data1 != 0) {
                    name = cfg->base + 324;
                    if (found) {
                        sceMcGetDir(ulmc_cur_port, ulmc_cur_slot, name, 0, 1, &ulmc_dir_ent);
                        found = 0;
                        return;
                    }
                    sceMcOpen(ulmc_cur_port, ulmc_cur_slot, name, 514);
                    ad = cfg->data1;
                    sz = cfg->size1;
                    goto set;
                }
                nf |= 4;
                ulmc_create_flags = nf;
            }
            if (!(nf & 8)) {
                if (cfg->data2 != 0) {
                    name = cfg->base + 388;
                    if (found) {
                        sceMcGetDir(ulmc_cur_port, ulmc_cur_slot, name, 0, 1, &ulmc_dir_ent);
                        found = 0;
                        return;
                    }
                    sceMcOpen(ulmc_cur_port, ulmc_cur_slot, name, 514);
                    ad = cfg->data2;
                    sz = cfg->size2;
                set:
                    ulmc_create_size = sz;
                    ulmc_create_buf = ad;
                    return;
                }
                nf |= 8;
                ulmc_create_flags = nf;
            }
            ALL_DONE;
            return;
        case 3:
        case 12: {
            McSaveDesc *desc = ulmc_save_desc;
            unsigned int fl;
            fl = ulmc_create_flags;
            if (!(fl & 1)) {
                name = "icon.sys";
                sceMcGetDir(ulmc_cur_port, ulmc_cur_slot, name, 0, 1, &ulmc_dir_ent);
                found = 0;
                return;
            }
            if (!(fl & 2)) {
                name = desc->base + 260;
                sceMcGetDir(ulmc_cur_port, ulmc_cur_slot, name, 0, 1, &ulmc_dir_ent);
                found = 0;
                return;
            }
            if (!(fl & 4)) {
                if (desc->data1 != 0) {
                    name = desc->base + 324;
                    sceMcGetDir(ulmc_cur_port, ulmc_cur_slot, name, 0, 1, &ulmc_dir_ent);
                    found = 0;
                    return;
                }
                fl |= 4;
                ulmc_create_flags = fl;
            }
            if (!(fl & 8)) {
                if (desc->data2 != 0) {
                    name = desc->base + 388;
                    sceMcGetDir(ulmc_cur_port, ulmc_cur_slot, name, 0, 1, &ulmc_dir_ent);
                    found = 0;
                    return;
                }
                fl |= 8;
                ulmc_create_flags = fl;
            }
            ALL_DONE;
            return;
        }
        case 2:
            ulmc_create_fd = res;
            sceMcWrite(res, ulmc_create_buf, ulmc_create_size);
            found = 0;
            return;
        case 6:
            sceMcClose(ulmc_create_fd);
            found = 0;
            return;
        }
    } else {
        st = ulmc_pending_cmd;
        mc_status = res;
        ulmc_cur_cmd = 0;
        if (st == 0)
            return;
        ulmc_pending_cmd = 0;
        {
            int port = ulmc_cur_port, slot = ulmc_cur_slot;
            if (st == 1)
                return;
            if (ulmc_user_callback == 0)
                return;
            if (ulmc_port_mode == 0)
                ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), st, res, ulmc_callback_arg); /* steer(cast) */
            else
                ulmc_user_callback(port, slot, st, res, ulmc_callback_arg);
        }
    }
}

/*
 * op = library function number. Callback hears every request but op 1, then ulmc_cur_cmd steps.
 * Result < -10 on a connected card = pulled out: disconnected, event 102.
 */
/* 0x00181e08 */
static void ulMcHandleCompletion(int op, int res)
{
    int port = ulmc_cur_port;
    int slot = ulmc_cur_slot;

    if (op != 1 && ulmc_user_callback) {
        if (ulmc_port_mode == 0)
            ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), op, res, ulmc_callback_arg); /* steer(cast) */
        else
            ulmc_user_callback(port, slot, op, res, ulmc_callback_arg);
    }
    switch (ulmc_cur_cmd) {
    case 0x6e:
    case 0x6f:
        ulMcLoadComplete(op, res);
        break;
    case 0x70:
    case 0x71:
        ulMcSaveComplete(op, res);
        break;
    case 0x72:
        ulMcStatusComplete(op, res);
        break;
    case 0x73:
        ulMcConnectEvent(op, res);
        break;
    case 0x74:
        ulMcCreateComplete(op, res);
        break;
    }
    if (mc_connected[ulmc_cur_port][ulmc_cur_slot] != 0 && res < -10) {
        printf("libulmc: port[%d] slot[%d] op[%d] is cancel by disconnect\n", ulmc_cur_port,
               ulmc_cur_slot, op);
        mc_connected[ulmc_cur_port][ulmc_cur_slot] = 0;
        if (ulmc_user_callback) {
            if (ulmc_port_mode == 0)
                ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(ulmc_cur_port, ulmc_cur_slot), 0x66, 0, ulmc_callback_arg); /* steer(cast) */
            else
                ulmc_user_callback(ulmc_cur_port, ulmc_cur_slot, 0x66, 0, ulmc_callback_arg);
        }
    }
}

/* Ports are enabled afterwards with mc_slot_mark_used. */
/* 0x00181fc0 */
void ulMcInit(int save_desc)
{
    printf("libulmc: ulMcInit\n");
    sceMcInit();
    ulmc_save_desc = save_desc;
    ulmc_slot_count = 0;
    memset(ulmc_port_state, 0, 32);
    memset(mc_connected, 0, 32);
    memset(ulmc_mounted, 0, 32);
}

/* -200 while a request runs. */
/* 0x00182038 */
static int ulMcExit(void)
{
    int cmd;
    int res;
    int state;

    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        return -200;
    }
    if (state == 0)
        return -200;
    memset(ulmc_port_state, 0, 32);
    memset(mc_connected, 0, 32);
    memset(ulmc_mounted, 0, 32);
    printf("libulmc: ulMcExit\n");
    return 0;
}

/*
 * Per frame: handles a finished request, or when idle starts a card check (0x73) on the next
 * enabled port. Type asked only while no card is known connected.
 */
/* 0x001820d8 */
void ulMcPoll(void)
{
    int n = 0;
    int cmd;
    int res;
    int state;
    unsigned int port;
    int connected;

    if (ulmc_cur_cmd == 0) {
        state = sceMcSync(1, &cmd, &res);
        if (state == 1) {
            ulMcHandleCompletion(cmd, res);
        } else if (state != 0) {
            for (port = 0; port < 2; port++) {
                if (((int *)&ulmc_port_state)[port * 4] != 0) { /* steer(cast) */
                    connected = ((int *)&mc_connected)[port * 4]; /* steer(cast) */
                    if (n != ulmc_poll_port) {
                        n++;
                    } else {
                        ulmc_poll_port = (n + 1) % ulmc_slot_count;
                        if (connected != 0) {
                            if (sceMcGetInfo(port, 0, 0, 0, 0) != 0)
                                continue;
                        } else {
                            if (sceMcGetInfo(port, 0, &ulmc_file_attr, 0, 0) != 0)
                                continue;
                        }
                        ulmc_cur_port = port;
                        ulmc_cur_slot = 0;
                        ulmc_cur_cmd = 0x73;
                        return;
                    }
                }
            }
        }
    }
}

/*
 * State 0x6e. ulmc_retry (public wrappers) runs ulMcRemount first and parks the load in
 * ulmc_pending_cmd. 0 started, -200 busy, or the error (also to ulmc_user_callback).
 */
/* 0x00182200 */
static long ulMcLoadStart(int port, int slot, int path, int buf, int size)
{
    long ret;
    long req;
    long state;
    int cmd;
    int res;
    int open_cmd;
    int open_res;

again0:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again0;
    }
    if (state == 0)
        return -200;
    strcpy(ulmc_path, path);
    ulmc_io_buf = buf;
    ulmc_io_size = size;
    if (ulmc_retry) {
        ulmc_retry = 0;
        ulmc_pending_cmd = 0x6e;
        ret = ulMcRemount(port, slot);
        if (ret != 0) {
            if (ulmc_user_callback) {
                if (ulmc_port_mode == 0)
                    ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), 0x6e, ret, ulmc_callback_arg); /* steer(cast) */
                else
                    ulmc_user_callback(port, slot, 0x6e, ret, ulmc_callback_arg);
            }
            return ret;
        }
        return 0;
    }
    ulmc_cur_cmd = 0x6e;
again1:
    state = sceMcSync(1, &open_cmd, &open_res);
    if (state == 1) {
        ulMcHandleCompletion(open_cmd, open_res);
        goto again1;
    }
    if (state == 0) {
        ret = -200;
    } else {
        req = sceMcOpen(port, slot, path, 1);
        if (req == 0) {
            ulmc_cur_port = port;
            ulmc_cur_slot = slot;
        }
        ret = req;
    }
    if (ret == 0)
        goto ok;
    ulmc_cur_cmd = 0;
    if (ulmc_user_callback) {
        if (ulmc_port_mode == 0)
            ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), 0x6e, ret, ulmc_callback_arg); /* steer(cast) */
        else
            ulmc_user_callback(port, slot, 0x6e, ret, ulmc_callback_arg);
    }
    return ret;
ok:
    ulmc_cur_port = port;
    ulmc_cur_slot = slot;
    return 0;
}

/* From byte offset ofs (state 0x6f). */
/* 0x001823e0 */
static long ulMcLoadStartAt(int port, int slot, int path, int buf, int ofs, int size)
{
    long ret;
    long req;
    long state;
    int cmd;
    int res;
    int open_cmd;
    int open_res;

again0:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again0;
    }
    if (state == 0)
        return -200;
    strcpy(ulmc_path, path);
    ulmc_io_buf = buf;
    ulmc_io_size = size;
    ulmc_io_ofs = ofs;
    if (ulmc_retry) {
        ulmc_retry = 0;
        ulmc_pending_cmd = 0x6f;
        ret = ulMcRemount(port, slot);
        if (ret != 0) {
            if (ulmc_user_callback) {
                if (ulmc_port_mode == 0)
                    ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), 0x6f, ret, ulmc_callback_arg); /* steer(cast) */
                else
                    ulmc_user_callback(port, slot, 0x6f, ret, ulmc_callback_arg);
            }
            return ret;
        }
        return 0;
    }
    ulmc_cur_cmd = 0x6f;
again1:
    state = sceMcSync(1, &open_cmd, &open_res);
    if (state == 1) {
        ulMcHandleCompletion(open_cmd, open_res);
        goto again1;
    }
    if (state == 0) {
        ret = -200;
    } else {
        req = sceMcOpen(port, slot, path, 1);
        if (req == 0) {
            ulmc_cur_port = port;
            ulmc_cur_slot = slot;
        }
        ret = req;
    }
    if (ret == 0)
        goto ok;
    ulmc_cur_cmd = 0;
    if (ulmc_user_callback) {
        if (ulmc_port_mode == 0)
            ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), 0x6f, ret, ulmc_callback_arg); /* steer(cast) */
        else
            ulmc_user_callback(port, slot, 0x6f, ret, ulmc_callback_arg);
    }
    return ret;
ok:
    ulmc_cur_port = port;
    ulmc_cur_slot = slot;
    return 0;
}

/* State 0x70; creates the file if needed, then applies attr (see ulMcSaveComplete). */
/* 0x001825d0 */
static long ulMcSaveStart(int port, int slot, int path, int buf, int size, int attr)
{
    long ret;
    long req;
    long state;
    int cmd;
    int res;
    int open_cmd;
    int open_res;

again0:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again0;
    }
    if (state == 0)
        return -200;
    strcpy(ulmc_path, path);
    ulmc_io_buf = buf;
    ulmc_io_size = size;
    ulmc_file_attr = attr;
    if (ulmc_retry) {
        ulmc_retry = 0;
        ulmc_pending_cmd = 0x70;
        ret = ulMcRemount(port, slot);
        if (ret != 0) {
            if (ulmc_user_callback) {
                if (ulmc_port_mode == 0)
                    ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), 0x70, ret, ulmc_callback_arg); /* steer(cast) */
                else
                    ulmc_user_callback(port, slot, 0x70, ret, ulmc_callback_arg);
            }
            return ret;
        }
        return 0;
    }
    ulmc_cur_cmd = 0x70;
again1:
    state = sceMcSync(1, &open_cmd, &open_res);
    if (state == 1) {
        ulMcHandleCompletion(open_cmd, open_res);
        goto again1;
    }
    if (state == 0) {
        ret = -200;
    } else {
        req = sceMcOpen(port, slot, path, 0x202);
        if (req == 0) {
            ulmc_cur_port = port;
            ulmc_cur_slot = slot;
        }
        ret = req;
    }
    if (ret == 0)
        goto ok;
    ulmc_cur_cmd = 0;
    if (ulmc_user_callback) {
        if (ulmc_port_mode == 0)
            ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), 0x70, ret, ulmc_callback_arg); /* steer(cast) */
        else
            ulmc_user_callback(port, slot, 0x70, ret, ulmc_callback_arg);
    }
    return ret;
ok:
    ulmc_cur_port = port;
    ulmc_cur_slot = slot;
    return 0;
}

/* At byte offset ofs (state 0x71). */
/* 0x001827c0 */
static long ulMcSaveStartAt(int port, int slot, int path, int buf, int ofs, int size, int attr)
{
    long ret;
    long req;
    long state;
    int cmd;
    int res;
    int open_cmd;
    int open_res;

again0:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again0;
    }
    if (state == 0)
        return -200;
    strcpy(ulmc_path, path);
    ulmc_io_buf = buf;
    ulmc_io_size = size;
    ulmc_file_attr = attr;
    ulmc_io_ofs = ofs;
    if (ulmc_retry) {
        ulmc_retry = 0;
        ulmc_pending_cmd = 0x71;
        ret = ulMcRemount(port, slot);
        if (ret != 0) {
            if (ulmc_user_callback) {
                if (ulmc_port_mode == 0)
                    ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), 0x71, ret, ulmc_callback_arg); /* steer(cast) */
                else
                    ulmc_user_callback(port, slot, 0x71, ret, ulmc_callback_arg);
            }
            return ret;
        }
        return 0;
    }
    ulmc_cur_cmd = 0x71;
again1:
    state = sceMcSync(1, &open_cmd, &open_res);
    if (state == 1) {
        ulMcHandleCompletion(open_cmd, open_res);
        goto again1;
    }
    if (state == 0) {
        ret = -200;
    } else {
        req = sceMcOpen(port, slot, path, 0x202);
        if (req == 0) {
            ulmc_cur_port = port;
            ulmc_cur_slot = slot;
        }
        ret = req;
    }
    if (ret == 0)
        goto ok;
    ulmc_cur_cmd = 0;
    if (ulmc_user_callback) {
        if (ulmc_port_mode == 0)
            ulmc_user_callback(((int (*)(int, int))ulMcPortToCard)(port, slot), 0x71, ret, ulmc_callback_arg); /* steer(cast) */
        else
            ulmc_user_callback(port, slot, 0x71, ret, ulmc_callback_arg);
    }
    return ret;
ok:
    ulmc_cur_port = port;
    ulmc_cur_slot = slot;
    return 0;
}

static inline int mc_cd(int port, int slot, char *path)
{
    int cmd;
    int res;
    int state;
    int req;

  L:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto L;
    }
    if (state == 0)
        return -200;
    req = sceMcChdir(port, slot, path, 0);
    if (req == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return req;
}

static inline int mc_mk(int port, int slot, char *path)
{
    int cmd;
    int res;
    int state;
    int req;

  L:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto L;
    }
    if (state == 0)
        return -200;
    req = sceMcMkdir(port, slot, path);
    if (req == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return req;
}

/*
 * State 0x74: back to the parent, marks unmounted, enters or creates ulmc_save_desc->dev.
 * 0 started, -100 slot != 0, -200 busy, or the error.
 */
/* 0x001829c0 */
static int ulMcRemount(int port, int slot)
{
    int cd_res;
    int state;
    int ret;
    int cmd;
    int mk_cmd;
    int req;
    int cd_cmd;
    int ok;
    int dir_res;
    int mk_res;
    int res;
    char *path;
    int dir_cmd;
    int n;

    if (slot != 0)
        return -100;
  l0:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto l0;
    }
    if (state == 0)
        return -200;
    ulmc_cur_cmd = 0x74;
    ulmc_create_flags = 0;
    sceMcChdir(port, slot, str_ulmc_parent_dir, 0);
    sceMcSync(0, 0, 0);
    ulmc_mounted[port][slot] = 0;
    path = *(*(char * * *)&ulmc_save_desc); /* steer(cast) */
    do {
    } while (0);
  l1:
    do {
        state = sceMcSync(1, &dir_cmd, &dir_res);
    } while (0);
    if (state == 0)
        goto l1;
    if (state == 1) {
        ulMcHandleCompletion(dir_cmd, dir_res);
        goto l1;
    }
    state = sceMcGetDir(port, slot, path, 0, 1, &ulmc_dir_ent);
    if (state < 0) {
        ok = 0;
        mc_status = -1000;
    } else {
        sceMcSync(0, 0, &n);
        if (n > 0) {
            mc_status = 0;
            ok = 1;
        } else {
            if (n == 0)
                mc_status = -4;
            else if (n < -10)
                mc_status = -100;
            else
                mc_status = n;
            ok = 0;
        }
    }
    if (ok) {
        ret = mc_cd(port, slot, *(*(char * * *)&ulmc_save_desc)); /* steer(cast) */
    } else {
        ret = mc_status;
        if (ret != -4) {
            ulmc_cur_cmd = 0;
            return ret;
        }
        ret = mc_mk(port, slot, *(*(char * * *)&ulmc_save_desc)); /* steer(cast) */
    }
    if (ret != 0) {
        ulmc_cur_cmd = 0;
        return ret;
    }
    return 0;
}

static inline void mc_drain(void)
{
    int state;
    int cmd;
    int res;

loop:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (state == 0);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto loop;
    }
}

static inline int mc_get_first(char *pat, char *buf)
{
    int n;

    mc_drain();
    if (sceMcGetDir(0, 0, pat, 0, 1, &ulmc_dir_ent) < 0)
        return 0;
    sceMcSync(0, 0, &n);
    if (n < 1)
        return 0;
    ulmc_list_port = 0;
    ulmc_list_slot = 0;
    strcpy(buf, (*(McDirName *)&ulmc_dir_ent).name); /* steer(cast) */
    return 1;
}

static inline int mc_get_next(char *pat, char *buf)
{
    int n;

    mc_drain();
    if (sceMcGetDir(ulmc_list_port, ulmc_list_slot, pat, 1, 1, &ulmc_dir_ent) < 0)
        return 0;
    sceMcSync(0, 0, &n);
    if (n < 1)
        return 0;
    strcpy(buf, (*(McDirName *)&ulmc_dir_ent).name); /* steer(cast) */
    return 1;
}

/* Debug: lists the current directory of port 0's card; the arguments are only printed. */
/* 0x00182c60 */
static int ulMcDumpFileList(int port, int slot, char *path)
{
    char buf[256];

    printf("ulMcDumpFileList:\n");
    printf("port[%d] slot[%d] %s\n", port, slot, path);
    if (mc_get_first("*", buf)) {
        printf("%s\n", buf);
        while (mc_get_next("*", buf))
            printf("%s\n", buf);
    } else {
        printf("flist not found.\n");
    }
    return 0;
}

static inline void mc_read(int fd, char *buf, int n)
{
    int cmd;
    int res;
    int state;

  L:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto L;
    }
    if (state != 0)
        sceMcRead(fd, buf, n);
}

static inline void mc_close(int fd)
{
    int cmd;
    int res;
    int state;

  L:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto L;
    }
    if (state != 0)
        sceMcClose(fd);
}

/*
 * Debug: copies path to host0: in 256-byte pieces.
 * Bug: the host fd is passed to sceMcClose.
 */
/* 0x00182e78 */
static int ulMcDumpFileToHost(int port, int slot, char *path)
{
    char buf[256];
    int cmd;
    int res;
    int card_fd;
    int open_cmd;
    int open_res;
    int size_cmd;
    int size_res;
    int n;
    int cnt;
    int nread;
    int ok;
    int remaining;
    int state;
    int fd;
    int req;

    do {
    } while (0);
  l0:
    state = sceMcSync(1, &cmd, &res);
    if (state == 0)
        goto l0;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto l0;
    }
    state = sceMcGetDir(port, slot, path, 0, 1, &ulmc_dir_ent);
    if (state < 0) {
        ok = 0;
        mc_status = -1000;
    } else {
        sceMcSync(0, 0, &n);
        if (n > 0) {
            mc_status = 0;
            ok = 1;
        } else {
            if (n == 0)
                mc_status = -4;
            else if (n < -10)
                mc_status = -100;
            else
                mc_status = n;
            ok = 0;
        }
    }
    if (!ok)
        return mc_status;
    strcpy(buf, "host0:");
    strcat(buf, path);
    fd = sceOpen(buf, 0x602);
    if (fd >= 0) {
      l1:
        state = sceMcSync(1, &open_cmd, &open_res);
        if (state == 1) {
            ulMcHandleCompletion(open_cmd, open_res);
            goto l1;
        }
        if (state == 0) {
            req = -200;
        } else {
            req = sceMcOpen(port, slot, path, 1);
            if (req == 0) {
                ulmc_cur_port = port;
                ulmc_cur_slot = slot;
            }
        }
        if (req != 0)
            return 0;
        sceMcSync(0, 0, &card_fd);
        if (slot != 0) {
            remaining = -100;
        } else {
            do {
            } while (0);
          l2:
            state = sceMcSync(1, &size_cmd, &size_res);
            if (state == 0)
                goto l2;
            if (state == 1) {
                ulMcHandleCompletion(size_cmd, size_res);
                goto l2;
            }
            state = sceMcGetDir(port, slot, path, 0, 1, &ulmc_dir_ent);
            if (state < 0) {
                remaining = -1000;
            } else {
                sceMcSync(0, 0, &cnt);
                if (cnt >= -10) {
                    if (cnt == 0) {
                        remaining = -4;
                    } else {
                        int x = cnt;
                        if (x > 0)
                            x = (*(McDirSize *)&ulmc_dir_ent).size; /* steer(cast) */
                        remaining = x;
                    }
                } else
                    remaining = -100;
            }
        }
        while (remaining > 0) {
            sceMcSync(0, 0, 0);
            mc_read(card_fd, buf, 256);
            sceMcSync(0, 0, &nread);
            sceWrite(fd, buf, nread);
            remaining -= nread;
        }
        mc_close(card_fd);
        sceMcClose(fd);
        printf("dump [%s] to host\n", path);
    }
    return 0;
}

/*
 * nfree and its copy: free clusters or -2 unformatted; cluster_size 1024, rest cleared.
 * Card 0 only (else -100). 0, -200 busy, or the card check's error.
 */
/* 0x00183240 */
static int ulMcGetCardInfo(unsigned int which, McCardInfo *info)
{
    int slot;
    int free_clusters;
    int format;
    int probe_cmd;
    int res;
    int port;
    int st;
    int cnt;
    int free_res;
    int n;
    int val;
    int cmd;
    int free_cmd;
    int probe_res;
    long state;
    int ret;

  l0:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto l0;
    }
    if (state == 0)
        return -200;
    switch (which) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        port = -1;
        break;
    }
    switch (which) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        slot = -1;
        break;
    }
    if (which != 0) {
        mc_status = -100;
        ret = -100;
    } else {
        do {
        } while (0);
      l1:
        state = sceMcSync(1, &probe_cmd, &probe_res);
        if (state == 0)
            goto l1;
        if (state == 1) {
            ulMcHandleCompletion(probe_cmd, probe_res);
            goto l1;
        }
        if (sceMcGetInfo(port, which, 0, 0, &st) < 0) {
            mc_status = -1000;
            ret = -1000;
        } else {
            sceMcSync(0, 0, &n);
            if (n >= -10)
                mc_status = (st == 1) ? 0 : -2;
            else
                mc_status = -100;
            ret = mc_status;
        }
    }
    if (ret != 0)
        return ret;
    info->unk_20 = 0;
    info->unk_22 = 0;
    if (slot != 0) {
        val = -100;
    } else {
        do {
        } while (0);
      l2:
        state = sceMcSync(1, &free_cmd, &free_res);
        if (state == 0)
            goto l2;
        if (state == 1) {
            ulMcHandleCompletion(free_cmd, free_res);
            goto l2;
        }
        state = sceMcGetInfo(port, slot, 0, &free_clusters, &format);
        if (state < 0) {
            val = -1000;
        } else {
            sceMcSync(0, 0, &cnt);
            if (cnt < -10)
                val = -100;
            else
                val = (format == 1) ? free_clusters : -2;
        }
    }
    info->nfree = val;
    info->unk_28 = 0;
    info->unk_2a = 0;
    info->unk_26 = val;
    info->cluster_size = 0x400;
    info->unk_2e = 0;
    return 0;
}

static inline void mc_mount(int port, int slot)
{
    char buf[48];

    buf[0] = '.';
    buf[1] = '.';
    buf[2] = '/';
    strcpy(buf + 3, *(*(char * * *)&ulmc_save_desc)); /* steer(cast) */
    mc_drain();
    sceMcChdir(port, slot, buf, 0);
    sceMcSync(0, 0, 0);
}

static inline McDirTime *mc_lookup(int port, int slot, char *path)
{
    int n;

    mc_drain();
    if (sceMcGetDir(port, slot, path, 0, 1, &ulmc_dir_ent) < 0)
        return 0;
    sceMcSync(0, 0, &n);
    if (n < 1)
        return 0;
    ulmc_list_port = port;
    ulmc_list_slot = slot;
    return &ulmc_dir_ent;
}

static inline int mc_probe(int port, int slot)
{
    int cmd;
    int res;
    int st;
    int n;
    int state;

    do {
    } while (0);
  lp:
    do {
        do {
            state = sceMcSync(1, &cmd, &res);
        } while (0);
    } while (0);
    if (state == 0)
        goto lp;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto lp;
    }
    if (sceMcGetInfo(port, slot, 0, 0, &st) < 0) {
        mc_status = -1000;
        return -1000;
    }
    sceMcSync(0, 0, &n);
    if (n >= -10)
        mc_status = (st == 1) ? 0 : -2;
    else
        mc_status = -100;
    return mc_status;
}

static inline void mc_mount_ulMcGetFileBlocks(int port, int slot)
{
    char buf[48];
    int cmd;
    int res;
    int state;

    buf[0] = '.';
    buf[1] = '.';
    buf[2] = '/';
    strcpy(buf + 3, ulmc_save_desc->dev);
    do {
    } while (0);
  lm:
    state = sceMcSync(1, &cmd, &res);
    if (state == 0)
        goto lm;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto lm;
    }
    sceMcChdir(port, slot, buf, 0);
    sceMcSync(0, 0, 0);
    ulmc_mounted[port][slot] = 1;
}

static inline void mc_mount_ulMcLoadFile(int port, int slot)
{
    char buf[48];
    int cmd;
    int res;
    int state;

    buf[0] = '.';
    buf[1] = '.';
    buf[2] = '/';
    strcpy(buf + 3, *(*(char * * *)&ulmc_save_desc)); /* steer(cast) */
    do {
    } while (0);
  lm:
    state = sceMcSync(1, &cmd, &res);
    if (state == 0)
        goto lm;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto lm;
    }
    sceMcChdir(port, slot, buf, 0);
    sceMcSync(0, 0, 0);
    ulmc_mounted[port][slot] = 1;
}

static inline void mc_mount_ulMcLoadFile2(int port, int slot)
{
    char buf[48];
    int cmd;
    int res;
    int state;

    buf[0] = '.';
    buf[1] = '.';
    buf[2] = '/';
    strcpy(buf + 3, *(*(char * * *)&ulmc_save_desc)); /* steer(cast) */
    do {
    } while (0);
  lm:
    state = sceMcSync(1, &cmd, &res);
    if (state == 0)
        goto lm;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto lm;
    }
    sceMcChdir(port, slot, buf, 0);
    sceMcSync(0, 0, 0);
    ulmc_mounted[port][slot] = 1;
}

static inline void mc_mount_ulMcFindFirst(int port, int slot)
{
    char buf[48];
    int cmd;
    int res;
    int state;

    buf[0] = '.';
    buf[1] = '.';
    buf[2] = '/';
    strcpy(buf + 3, *(*(char * * *)&ulmc_save_desc)); /* steer(cast) */
    do {
    } while (0);
  lm:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (0);
    if (state == 0)
        goto lm;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto lm;
    }
    sceMcChdir(port, slot, buf, 0);
    sceMcSync(0, 0, 0);
    ulmc_mounted[port][slot] = 1;
}

static inline int mc_probe_ulMcFindFirst(int port, int slot)
{
    int cmd;
    int res;
    int st;
    int n;
    int state;

    do {
    } while (0);
  lp:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (0);
    if (state == 0)
        goto lp;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto lp;
    }
    if (sceMcGetInfo(port, slot, 0, 0, &st) < 0) {
        mc_status = -1000;
        return -1000;
    }
    sceMcSync(0, 0, &n);
    if (n >= -10)
        mc_status = (st == 1) ? 0 : -2;
    else
        mc_status = -100;
    return mc_status;
}

static inline void mc_mount_ulMcDelete(int port, int slot)
{
    char buf[48];
    int cmd;
    int res;
    int state;

    buf[0] = '.';
    buf[1] = '.';
    buf[2] = '/';
    strcpy(buf + 3, *(*(char * * *)&ulmc_save_desc)); /* steer(cast) */
    do {
    } while (0);
  lm:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (0);
    if (state == 0)
        goto lm;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto lm;
    }
    sceMcChdir(port, slot, buf, 0);
    sceMcSync(0, 0, 0);
    ulmc_mounted[port][slot] = 1;
}

static inline void mc_mount_ulMcRename(int port, int slot)
{
    char buf[48];
    int cmd;
    int res;
    int state;

    buf[0] = '.';
    buf[1] = '.';
    buf[2] = '/';
    strcpy(buf + 3, *(*(char * * *)&ulmc_save_desc)); /* steer(cast) */
    do {
    } while (0);
  lm:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (0);
    if (state == 0)
        goto lm;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto lm;
    }
    sceMcChdir(port, slot, buf, 0);
    sceMcSync(0, 0, 0);
    ulmc_mounted[port][slot] = 1;
}

static inline void mc_mount_ulMcSetFileAttr(int port, int slot)
{
    char buf[48];
    int cmd;
    int res;
    int state;

    buf[0] = '.';
    buf[1] = '.';
    buf[2] = '/';
    strcpy(buf + 3, *(*(char * * *)&ulmc_save_desc)); /* steer(cast) */
    do {
    } while (0);
  lm:
    state = sceMcSync(1, &cmd, &res);
    if (state == 0)
        goto lm;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto lm;
    }
    sceMcChdir(port, slot, buf, 0);
    sceMcSync(0, 0, 0);
    ulmc_mounted[port][slot] = 1;
}

/*
 * Size (bytes, KB), flag (0xff with attribute bit 3), date; dow by Zeller's congruence. Card 0
 * only. 0, -4 missing, -200 busy, or the check's error.
 */
/* 0x001834d8 */
static int ulMcGetFileStatus(unsigned int which, char *path, McFileStat *out)
{
    McDirTime *ent;
    int cmd;
    int res;
    int ret;
    int port;
    int slot;
    int y;
    int month;
    int day;

  l0:
    ret = sceMcSync(1, &cmd, &res);
    if (ret == 1) {
        ulMcHandleCompletion(cmd, res);
        goto l0;
    }
    if (ret == 0)
        return -200;
    switch (which) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        port = -1;
        break;
    }
    switch (which) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 5:
    case 7:
        slot = -1;
        break;
    default:
        slot = -1;
        break;
    }
    if (which != 0) {
        mc_status = -100;
        ret = -100;
    } else
        ret = mc_probe(port, which);
    if (ret != 0)
        return ret;
    if (ulmc_mounted[port][slot] == 0) {
        mc_mount(port, slot);
        ulmc_mounted[port][slot] = 1;
    }
    ent = mc_lookup(port, slot, path);
    if (ent == 0)
        return -4;
    out->size = ent->size;
    out->blocks = (ent->size + 1023) >> 10;
    out->unk_06 = 0;
    out->flag = (ent->status & 8) ? 0xff : 0;
    out->unk_08 = 0;
    out->year = ent->year;
    out->month = ent->month;
    out->day = ent->day;
    out->hour = ent->hour;
    out->min = ent->min;
    out->sec = ent->sec;
    month = ent->month;
    day = ent->day;
    y = ent->year;
    if (month < 3) {
        y--;
        month += 12;
    }
    out->dow = (y + y / 4 - y / 100 + y / 400 + (month * 13 + 8) / 5 + day) % 7;
    return 0;
}

/* Into a local buffer nothing reads. */
static inline void mc_dir(char *path)
{
    char dir[256];
    char *slash;
    int len;
    char *dst = dir;

    slash = strrchr(path, '/');
    if (slash) {
        len = slash - path;
        memcpy(dst, path, len);
        dst[len] = 0;
    } else {
        dst[0] = '.';
        dst[1] = 0;
    }
}

/*
 * KB of path plus icon.sys and icons plus 4 (5 with copy and delete icons). 0 if missing,
 * -200 busy, or a lookup error.
 */
/* 0x00183928 */
int ulMcGetFileBlocks(unsigned int which, char *path)
{
    int port;
    int slot;
    int size;
    int blocks;
    char *pth;

    {
        int cmd;
        int res;
        int dir_cmd;
        int dir_res;
        int n;
        int x;
        int ret;

      l0:
        ret = sceMcSync(1, &cmd, &res);
        if (ret == 1) {
            ulMcHandleCompletion(cmd, res);
            goto l0;
        }
        if (ret == 0)
            return -200;
        switch (which) {
        case 0:
            port = 0;
            break;
        case 2:
            port = 1;
            break;
        case 1:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        default:
            port = -1;
            break;
        }
        switch (which) {
        case 0:
        case 2:
            slot = 0;
            break;
        case 1:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        default:
            slot = -1;
            break;
        }
        if (ulmc_mounted[port][slot] == 0)
            mc_mount_ulMcGetFileBlocks(port, slot);
        if (slot != 0) {
            size = -100;
        } else {
            do {
            } while (0);
          l1:
            ret = sceMcSync(1, &dir_cmd, &dir_res);
            if (ret == 0)
                goto l1;
            if (ret == 1) {
                ulMcHandleCompletion(dir_cmd, dir_res);
                goto l1;
            }
            ret = sceMcGetDir(port, slot, path, 0, 1, &ulmc_dir_ent);
            if (ret < 0) {
                size = -1000;
            } else {
                mc_dir(path);
                sceMcSync(0, 0, &n);
                if (n >= -10) {
                    if (n == 0) {
                        size = -4;
                    } else {
                        x = n;
                        if (x > 0)
                            x = ((*(McMountMcDirSize *)&ulmc_dir_ent).size + 1023) >> 10; /* steer(cast) */
                        size = x;
                    }
                } else
                    size = -100;
            }
        }
    }
    if (size == -4)
        return 0;
    if (size < 0)
        goto done;
    if (ulmc_save_desc == 0)
        goto done;
    {
        MC_FILE_BLOCKS_ULMCGETFILEBLOCKS(blocks, "icon.sys", l2);
        if (blocks >= 0)
            size += blocks;
        MC_FILE_BLOCKS_ULMCGETFILEBLOCKS(blocks, ulmc_save_desc->base + 260, l3);
        if (blocks >= 0)
            size += blocks;
        if (ulmc_save_desc->data1) {
            MC_FILE_BLOCKS_ULMCGETFILEBLOCKS(blocks, ulmc_save_desc->base + 324, l4);
            if (blocks >= 0)
                size += blocks;
        }
        if (ulmc_save_desc->data2) {
            MC_FILE_BLOCKS_ULMCGETFILEBLOCKS(blocks, ulmc_save_desc->base + 388, l5);
            if (blocks >= 0)
                size += blocks;
        }
    }
    {
        int ret = size + 4;
        size = ret + (ulmc_save_desc->data1 && ulmc_save_desc->data2);
    }
  done:
    return size;
}

/*
 * From the parent of the current directory (leaves the card unmounted). 0 found, -4, -100, -1000
 * or the library's result, also in mc_status.
 */
/* 0x001840d0 */
int ulMcFindFile(unsigned int which, char *path)
{
    int cmd;
    int res;
    int dir_cmd;
    int dir_res;
    int n;
    int port;
    int slot;
    int ok;
    long state;
    long dir_state;

    switch (which) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        port = -1;
        break;
    }
    switch (which) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        slot = -1;
        break;
    }
    for (;;) {
        state = sceMcSync(1, &cmd, &res);
        if (state == 1) {
            ulMcHandleCompletion(cmd, res);
            continue;
        }
        if (state != 0)
            break;
    }
    sceMcChdir(port, slot, "..", 0);
    sceMcSync(0, 0, 0);
    ulmc_mounted[port][slot] = 0;
    do { } while (0);
l1:
    do { } while (0);
    dir_state = sceMcSync(1, &dir_cmd, &dir_res);
    if (dir_state == 0)
        goto l1;
    if (dir_state == 1) {
        ulMcHandleCompletion(dir_cmd, dir_res);
        goto l1;
    }
    state = sceMcGetDir(port, slot, path, 0, 1, &ulmc_dir_ent);
    if (state < 0) {
        ok = 0;
        mc_status = -1000;
    } else {
        sceMcSync(0, 0, &n);
        if (n > 0) {
            mc_status = 0;
            ok = 1;
        } else {
            if (n == 0)
                mc_status = -4;
            else if (n < -10)
                mc_status = -100;
            else
                mc_status = n;
            ok = 0;
        }
    }
    if (ok)
        return 0;
    return mc_status;
}

/*
 * size in KB, 0 = whole file. Sets ulmc_retry. Bug: a failed size lookup's error is used as the
 * size. -200 busy, else 0; outcome via the callback.
 */
/* 0x001842f0 */
int ulMcLoadFile(unsigned int which, char *path, int buf, int size)
{
    int cmd;
    int res;
    int n;
    int port;
    int slot;
    int state;
    int ret;

    switch (which) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        port = -1;
        break;
    default:
        port = -1;
        break;
    }
    switch (which) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        slot = -1;
        break;
    default:
        slot = -1;
        break;
    }
    if (ulmc_mounted[port][slot] == 0)
        mc_mount_ulMcLoadFile(port, slot);
    if (size == 0) {
        if (slot != 0) {
            size = -100;
        } else {
            do {
            } while (0);
      l0:
            state = sceMcSync(1, &cmd, &res);
            if (state == 0)
                goto l0;
            if (state == 1) {
                ulMcHandleCompletion(cmd, res);
                goto l0;
            }
            state = sceMcGetDir(port, slot, path, 0, 1, &ulmc_dir_ent);
            if (state < 0) {
                size = -1000;
            } else {
                mc_dir(path);
                sceMcSync(0, 0, &n);
                if (n >= -10) {
                    if (n == 0) {
                        size = -4;
                    } else {
                        int x = n;
                        if (x > 0)
                            x = ((*(McMountMcDirSize *)&ulmc_dir_ent).size + 1023) >> 10; /* steer(cast) */
                        size = x;
                    }
                } else
                    size = -100;
            }
        }
    }
    ulmc_retry = 1;
    ret = ((int (*)(int, int, char *, int, int))ulMcLoadStart)(port, slot, path, buf, size << 10); /* steer(cast) */
    if (ret != -200)
        return (ret & 1) >> 1;    /* always 0 */
    return -200;
}

/* From blocks KB into the file. */
/* 0x001845b0 */
int ulMcLoadFileAt(unsigned int which, char *path, int buf, int blocks, int size)
{
    int cmd;
    int res;
    int n;
    int port;
    int slot;
    int state;
    int ret;

    switch (which) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        port = -1;
        break;
    default:
        port = -1;
        break;
    }
    switch (which) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        slot = -1;
        break;
    default:
        slot = -1;
        break;
    }
    if (ulmc_mounted[port][slot] == 0)
        mc_mount_ulMcLoadFile2(port, slot);
    if (size == 0) {
        if (slot != 0) {
            size = -100;
        } else {
            do {
            } while (0);
      l0:
            state = sceMcSync(1, &cmd, &res);
            if (state == 0)
                goto l0;
            if (state == 1) {
                ulMcHandleCompletion(cmd, res);
                goto l0;
            }
            state = sceMcGetDir(port, slot, path, 0, 1, &ulmc_dir_ent);
            if (state < 0) {
                size = -1000;
            } else {
                mc_dir(path);
                sceMcSync(0, 0, &n);
                if (n >= -10) {
                    if (n == 0) {
                        size = -4;
                    } else {
                        int x = n;
                        if (x > 0)
                            x = ((*(McMountMcDirSize *)&ulmc_dir_ent).size + 1023) >> 10; /* steer(cast) */
                        size = x;
                    }
                } else
                    size = -100;
            }
        }
    }
    ulmc_retry = 1;
    ret = ((int (*)(int, int, char *, int, int, int))ulMcLoadStartAt)(port, slot, path, buf, blocks << 10, size << 10); /* steer(cast) */
    if (ret != -200)
        return (ret & 1) >> 1;    /* always 0 */
    return -200;
}

static inline int c17_probe(int port, int slot)
{
    int cmd;
    int res;
    int st;
    int n;
    long state;

    if (slot != 0) {
        mc_status = -100;
        return -100;
    }
    do {
    } while (0);
  L:
    state = sceMcSync(1, &cmd, &res);
    if (state == 0)
        goto L;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto L;
    }
    state = sceMcGetInfo(port, slot, 0, 0, &st);
    if (state < 0) {
        mc_status = -1000;
        return -1000;
    }
    sceMcSync(0, 0, &n);
    if (n >= -10)
        mc_status = (st == 1) ? 0 : -2;
    else
        mc_status = -100;
    return mc_status;
}

/* Tries twice. 0, -2 unformatted, -100 no usable card, -1000 refused; also in mc_status. */
/* 0x00184890 */
int ulMcProbe(unsigned int which)
{
    int port;
    int slot;
    int ret;

    switch (which) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        port = -1;
        break;
    }
    switch (which) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        slot = -1;
        break;
    }
    ret = c17_probe(port, slot);
    if (ret == 0)
        return 0;
    return c17_probe(port, slot);
}

/* Free 1 KB clusters, -2 unformatted, -100 no usable card, -1000 refused. */
/* 0x00184ad0 */
int ulMcGetFree(unsigned int card)
{
    int cmd;
    int res;
    int free_clusters;
    int format;
    int cnt;
    int port;
    int slot;
    int state;
    int ret;

    switch (card) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        port = -1;
        break;
    }
    switch (card) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        slot = -1;
        break;
    }
    if (slot != 0) {
        ret = -100;
    } else {
        do {
        } while (0);
      L:
        state = sceMcSync(1, &cmd, &res);
        if (state == 0)
            goto L;
        if (state == 1) {
            ulMcHandleCompletion(cmd, res);
            goto L;
        }
        if (sceMcGetInfo(port, slot, 0, &free_clusters, &format) < 0) {
            ret = -1000;
        } else {
            sceMcSync(0, 0, &cnt);
            if (cnt < -10)
                ret = -100;
            else
#ifdef VERSION_US
                ret = format == 1 ? free_clusters : -2;
#else
                ret = format == 1 ? free_clusters : cnt;
#endif
        }
    }
    return ret;
}

/* First name into out; mc_find_next continues. 0, -4 empty, or the card check's error. */
/* 0x00184c20 */
static int ulMcFindFirst(unsigned int which, char *out)
{
    int cmd;
    int res;
    int cnt;
    int port;
    int slot;
    int ok;
    int ret;
    int state;
    char *pat;

    switch (which) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        port = -1;
        break;
    }
    switch (which) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        slot = -1;
        break;
    }
    if (slot != 0) {
        mc_status = -100;
        ret = -100;
    } else
        ret = mc_probe_ulMcFindFirst(port, slot);
    if (ret != 0)
        return ret;
    if (ulmc_mounted[port][slot] == 0)
        mc_mount_ulMcFindFirst(port, slot);
    pat = str_ulmc_wildcard;
    do {
    } while (0);
  l2:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (0);
    if (state == 0)
        goto l2;
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto l2;
    }
    state = sceMcGetDir(port, slot, pat, 0, 1, &ulmc_dir_ent);
    if (state < 0 || (sceMcSync(0, 0, &cnt), cnt < 1)) {
        ok = 0;
    } else {
        ulmc_list_port = port;
        ulmc_list_slot = slot;
        strcpy(out, (*(McDirName *)&ulmc_dir_ent).name); /* steer(cast) */
        ok = 1;
    }
    return ok ? 0 : -4;
}

/* 0 started, -200 busy, or the error. */
/* 0x00184f38 */
static int ulMcDelete(unsigned int which, int path)
{
    int port;
    int slot;
    int state;
    int res;
    int ret;
    int cmd;

    switch (which) {
    case 0:
        do {
            port = 0;
        } while (0);
        break;
    case 2:
        do {
            port = 1;
        } while (0);
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        port = -1;
        break;
    }
    switch (which) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        slot = -1;
        break;
    }
    if (ulmc_mounted[port][slot] == 0)
        mc_mount_ulMcDelete(port, slot);
again1:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again1;
    }
    if (state == 0)
        return -200;
    ret = sceMcDelete(port, slot, path);
    if (ret == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return ret;
}

/* 0 started, -200 busy, or the error. */
/* 0x00185130 */
static int ulMcRename(unsigned int which, int old_name, int new_name)
{
    int port;
    int slot;
    int state;
    int res;
    int ret;
    int cmd;

    switch (which) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        port = -1;
        break;
    default:
        port = -1;
        break;
    }
    switch (which) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        slot = -1;
        break;
    }
    if (ulmc_mounted[port][slot] == 0)
        mc_mount_ulMcRename(port, slot);
again1:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again1;
    }
    if (state == 0)
        return -200;
    ret = sceMcRename(port, slot, old_name, new_name);
    if (ret == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return ret;
}

/*
 * flag 0xff sets attribute bit 3, else clears; other bits are whatever ulmc_dir_ent last held.
 * Request errors are not returned.
 */
/* 0x00185328 */
static long ulMcSetFileAttr(unsigned int which, int path, int reserved, unsigned char flag)
{
    int port;
    int slot;
    long ret;

    switch (which) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        port = -1;
        break;
    }
    switch (which) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
        slot = -1;
        break;
    default:
        slot = -1;
        break;
    }
    if (ulmc_mounted[port][slot] == 0)
        mc_mount_ulMcSetFileAttr(port, slot);
    if (flag == 0xff)
        ulmc_dir_ent.status = ulmc_dir_ent.status | 8;
    else
        ulmc_dir_ent.status = ulmc_dir_ent.status & 0xfff7;
    ret = ((long (*)(int, int, int, McDirStat *, int))sceMcSetFileInfo)(port, slot, path, &ulmc_dir_ent, 4); /* steer(cast) */
    if (ret != -200)
        return 0;
    return -200;
}

/* Port 0 -> card 0, 1 -> 2, else -1. */
/* 0x00185508 */
static int ulMcPortToCard(int port)
{
    switch (port) {
    case 0:
        return 0;
    case 1:
        return 2;
    }
    return -1;
}

/* Card 0 -> port 0, 2 -> 1, else -1. */
/* 0x00185530 */
int ulMcCardToPort(unsigned int card)
{
    switch (card) {
    case 0:
        return 0;
    case 2:
        return 1;
    case 1:
    case 3:
    case 5:
    case 7:
        return -1;
    }
    return -1;
}

/* 0 for cards 0 and 2, else -1. */
/* 0x00185578 */
int ulMcCardToSlot(unsigned int card)
{
    switch (card) {
    case 0:
    case 2:
        return 0;
    case 1:
    case 3:
    case 5:
    case 7:
        return -1;
    }
    return -1;
}

/* auto_mount 0: no event 101 after 100. Calls cb if given; the masks are unused. */
/* 0x001855b8 */
int ulMcSetup(int port_mask, int slot_mask, int auto_mount, void (*cb)(void))
{
    ulmc_setup_flag = auto_mount == 0;
    if (cb)
        cb();
    return 0;
}

/* 0 idle, -200 busy. */
/* 0x001855e8 */
int ulMcDrainMessages(void)
{
    int cmd;
    int res;
    int state;

loop:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto loop;
    }
    return state == -1 ? 0 : -200;
}

/* blocks KB, then attr; sets ulmc_retry. x unused. -200 busy, else 0; outcome via the callback. */
/* 0x00185648 */
int ulMcSaveFile(unsigned int card, int path, int buf, int blocks, void *x, int attr)
{
    int port;
    int slot;
    int ret;

    ulmc_retry = 1;
    switch (card) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        port = -1;
        break;
    }
    switch (card) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    default:
        slot = -1;
        break;
    }
    ret = ((int (*)(int, int, int, int, int, int))ulMcSaveStart)(port, slot, path, buf, blocks << 10, attr); /* steer(cast) */
    if (ret != -200)
        return (ret & 1) >> 1;    /* always 0 */
    return -200;
}

/* Card-number form; mc_set_status_callback is the port/slot form. */
/* 0x00185700 */
void ulMcSetCallback(int cb)
{
    ulmc_user_callback = cb;
    ulmc_port_mode = 0;
}

/* Never called by the library. */
/* 0x00185710 */
void ulMcSetSubCallback(int cb)
{
    ulmc_sub_callback = cb;
}

/* Present and formatted, checked up to twice. */
/* 0x00185718 */
static int ulMcCheckCard(unsigned int card)
{
    int port;
    int slot;

    switch (card) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 5:
    case 7:
    default:
        port = -1;
    }
    switch (card) {
    case 0:
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 5:
    case 7:
    default:
        slot = -1;
    }
    if (mc_check_format(port, slot) == 0)
        return 1;
    return mc_check_format(port, slot) == 0;
}

/* -1 idle, 0 busy. */
/* 0x001857d0 */
int mc_drain_events(void)
{
    int cmd;
    int res;
    int state;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    return state;
}

/* Only reports event 101; file functions enter the save directory themselves. */
/* 0x00185828 */
int mc_mount_request(unsigned int card)
{
    void (*cb)() = ulmc_user_callback;
    int port;
    int slot;

    if (cb) {
        if (ulmc_port_mode == 0) {
            cb(card, 101, 0, ulmc_callback_arg);
        } else {
            switch (card) {
            case 0:
                port = 0;
                break;
            case 2:
                port = 1;
                break;
            case 1:
            case 3:
            case 5:
            case 7:
            default:
                port = -1;
            }
            switch (card) {
            case 0:
            case 2:
                slot = 0;
                break;
            case 1:
            case 3:
            case 5:
            case 7:
            default:
                slot = -1;
            }
            cb(port, slot, 101, 0, ulmc_callback_arg);
        }
    }
    return 0;
}

/* 0x001858e8 */
static int ulmc_return_zero(void)
{
    return 0;
}

/* At KB offset ofs_blocks, no attributes. */
/* 0x001858f0 */
static int ulMcSaveFileAt(unsigned int id, int path, int buf, int ofs_blocks, int blocks)
{
    int port;
    int slot;
    int ret;

    ulmc_retry = 1;
    switch (id) {
    case 0:
        port = 0;
        break;
    case 2:
        port = 1;
        break;
    case 1:
    case 3:
    case 5:
    case 7:
    default:
        port = -1;
    }
    switch (id) {
    case 0:
        slot = 0;
        break;
    case 2:
        slot = 0;
        break;
    case 1:
    case 3:
    case 5:
    case 7:
    default:
        slot = -1;
    }
    ret = ((int (*)(int, int, int, int, int, int, int))ulMcSaveStartAt)(port, slot, path, buf, ofs_blocks << 10, blocks << 10, 0); /* steer(cast) */
    if (ret != -200)
        return (ret & 1) >> 1;    /* always 0 */
    return -200;
}

/* 96-byte header + len bytes, magic 0x550047, CRC-16. Returns KB, rounded up. */
/* 0x001859b8 */
int mc_build_save_image(McImg *dst, McImg *src)
{
    char *body;
    unsigned int i;
    int bit;
    int crc;
    char *p;
    unsigned int n;

    *(McImgBytes *)dst = *(McImgBytes *)src;
    body = (char *)dst + 96;
    memcpy(body, src->data, src->len);
    dst->magic = (void *)0x550047;
    n = src->len;
    crc = 0;
    p = body;
    for (i = 0; i < n; i++) {
        crc ^= *p++ << 8;
        for (bit = 7; bit >= 0; bit--) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    dst->crc = crc;
    return (src->len + 1119) >> 10;
}

/*
 * KB for image (data + 96-byte header) and, with a save descriptor, icon.sys, icons and 4 (5 with
 * copy and delete icons). kind, reserved unused.
 */
/* 0x00185b28 */
int mc_calc_save_blocks(int kind, int reserved, unsigned int len)
{
    McSaveDesc *desc = ulmc_save_desc;
    int n = (len + 1119) >> 10;
    int total;

    if (desc) {
        n = n + 1;
        n += (desc->size0 + 1023) / 1024;
        if (desc->data1)
            n += (desc->size1 + 1023) / 1024;
        if (desc->data2)
            n += (desc->size2 + 1023) / 1024;
        total = n + 4;
        n = total + (desc->data1 && desc->data2);
    }
    return n;
}

/* dst->data points behind the header. -1011 bad magic, -1010 CRC mismatch (stored 0 unchecked). */
/* 0x00185bc8 */
int mc_unpack_save_image(McImg *dst, McImg *src)
{
    unsigned int i;
    int bit;
    int crc;
    char *p;
    unsigned int want;

    *(McImgBytes *)dst = *(McImgBytes *)src;
    dst->flag = 0;
    dst->data = (char *)src + 96;
    if (dst->magic != 0x550047)
        return -1011;
    want = src->crc;
    if (want == 0)
        return 0;
    crc = 0;
    p = dst->data;
    for (i = 0; i < dst->len; i++) {
        crc ^= *p++ << 8;
        for (bit = 7; bit >= 0; bit--) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return want == (crc & 0xffff) ? 0 : -1010;
}

/* Passed to every callback. */
/* 0x00185d20 */
static void mc_set_callback_arg(int user_arg)
{
    ulmc_callback_arg = user_arg;
}

/* Reports event 101, as mc_mount_request. */
/* 0x00185d28 */
static int mc_report_status(unsigned int card)
{
    void (*cb)() = ulmc_user_callback;
    int port;
    int slot;

    if (cb) {
        if (ulmc_port_mode == 0) {
            cb(card, 101, 0, ulmc_callback_arg);
        } else {
            switch (card) {
            case 0:
                port = 0;
                break;
            case 2:
                port = 1;
                break;
            case 1:
            case 3:
            case 5:
            case 7:
            default:
                port = -1;
            }
            switch (card) {
            case 0:
            case 2:
                slot = 0;
                break;
            case 1:
            case 3:
            case 5:
            case 7:
            default:
                slot = -1;
            }
            cb(port, slot, 101, 0, ulmc_callback_arg);
        }
    }
    return 0;
}

/* Same as mc_report_status. */
/* 0x00185de8 */
static int mc_report_status_2(unsigned int card)
{
    void (*cb)() = ulmc_user_callback;
    int port;
    int slot;

    if (cb) {
        if (ulmc_port_mode == 0) {
            cb(card, 101, 0, ulmc_callback_arg);
        } else {
            switch (card) {
            case 0:
                port = 0;
                break;
            case 2:
                port = 1;
                break;
            case 1:
            case 3:
            case 5:
            case 7:
            default:
                port = -1;
            }
            switch (card) {
            case 0:
            case 2:
                slot = 0;
                break;
            case 1:
            case 3:
            case 5:
            case 7:
            default:
                slot = -1;
            }
            cb(port, slot, 101, 0, ulmc_callback_arg);
        }
    }
    return 0;
}

/* 0x00185ea8 */
int mc_unmount(void)
{
    return 0;
}

/* 0, or -4 when done; which unused. */
/* 0x00185eb0 */
static int mc_find_next(int which, char *dst)
{
    int cmd;
    int res;
    int state;
    int cnt;
    int ok;
    char *work = str_ulmc_wildcard;
    char *buf;

again:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (state == 0);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    buf = ((char *)&ulmc_dir_ent); /* steer(cast) */
    if (sceMcGetDir(ulmc_list_port, ulmc_list_slot, work, 1, 1, buf) < 0)
        goto no;
    sceMcSync(0, 0, &cnt);
    if (cnt > 0)
        goto yes;
no:
    ok = 0;
    goto done;
yes:
    strcpy(dst, buf + 32);
    ok = 1;
done:
    return ok ? 0 : -4;
}

/* 0 started, -200 busy, or the error. */
/* 0x00185f88 */
int mc_format(unsigned int card)
{
    int cmd;
    int res;
    int mapped_port;
    int mapped_slot;
    int state;
    int rc;

    switch (card) {
    case 0:
        mapped_port = 0;
        break;
    case 2:
        mapped_port = 1;
        break;
    case 1:
    case 3:
    case 5:
    case 7:
        mapped_port = -1;
        break;
    default:
        mapped_port = -1;
    }
    switch (card) {
    case 0:
    case 2:
        mapped_slot = 0;
        break;
    case 1:
    case 3:
    case 5:
    case 7:
    default:
        mapped_slot = -1;
    }
    {
        int port = mapped_port;
        int slot = mapped_slot;
again:
        state = sceMcSync(1, &cmd, &res);
        if (state == 1) {
            ulMcHandleCompletion(cmd, res);
            goto again;
        }
        if (state == 0)
            return -200;
        rc = sceMcFormat(port, slot);
        if (rc == 0) {
            ulmc_cur_port = port;
            ulmc_cur_slot = slot;
        }
        return rc;
    }
}

/* CRC-16/CCITT, polynomial 0x1021, initial 0. */
/* 0x00186078 */
static unsigned int mc_crc16(char *buf, unsigned int n)
{
    unsigned int i;
    int crc;
    int bit;

    crc = 0;
    for (i = 0; i < n; i++) {
        crc ^= *buf++ << 8;
        for (bit = 7; bit >= 0; bit--) {
            if (crc & 0x8000)
                crc = (crc << 1) ^ 0x1021;
            else
                crc <<= 1;
        }
    }
    return crc & 0xffff;
}

/* First call per port/slot counts it in ulmc_slot_count, marks it disconnected and unmounted. */
/* 0x001860d8 */
void mc_slot_mark_used(int port, int slot)
{
    if (ulmc_port_state[port][slot] == 0) {
        ulmc_slot_count = ulmc_slot_count + 1;
        ulmc_port_state[port][slot] = 1;
        mc_connected[port][slot] = 0;
        ulmc_mounted[port][slot] = 0;
    }
}

/* 0x00186138 */
void mc_poll(void)
{
    int cmd;
    int res;

    if (sceMcSync(1, &cmd, &res) == 1)
        ulMcHandleCompletion(cmd, res);
}

/* When idle, ulMcPoll for the next port; 1 once every enabled port had a turn. */
/* 0x00186178 */
int mc_scan_step(void)
{
    if (mc_scan_drain_events()) {
        if (ulmc_scan_index >= ulmc_slot_count) {
            ulmc_scan_index = 0;
            return 1;
        }
        ulmc_scan_index = ulmc_scan_index + 1;
        ulMcPoll();
    }
    return 0;
}

/* Same as mc_drain_events. */
/* 0x001861c8 */
static int mc_scan_drain_events(void)
{
    int cmd;
    int res;
    int state;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    return state;
}

/* Port/slot form. */
/* 0x00186220 */
static void mc_set_status_callback(void (*cb)(), int user_arg)
{
    ulmc_user_callback = cb;
    ulmc_callback_arg = user_arg;
    ulmc_port_mode = 1;
}

/* 0x00186238 */
static void mc_set_sub_callback(int cb)
{
    ulmc_sub_callback = cb;
}

/*
 * This and the requests after it: drain finished requests, -200 busy, else start and record
 * ulmc_cur_port/slot. 0 started, or the error.
 */
/* 0x00186240 */
static int mc_format_port(int port, int slot)
{
    int cmd;
    int res;
    int state;
    int rc;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    rc = sceMcFormat(port, slot);
    if (rc == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return rc;
}

/* 0x001862d0 */
static int mc_unformat_request(int port, int slot)
{
    int cmd;
    int res;
    int state;
    int rc;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    rc = sceMcUnformat(port, slot);
    if (rc == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return rc;
}

/* 0x00186360 */
static int mc_mkdir_request(int port, int slot, int path)
{
    int cmd;
    int res;
    int state;
    int rc;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    rc = sceMcMkdir(port, slot, path);
    if (rc == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return rc;
}

/* 0x00186400 */
static int mc_chdir_request(int port, int slot, int path)
{
    int cmd;
    int res;
    int state;
    int rc;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    rc = sceMcChdir(port, slot, path, 0);
    if (rc == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return rc;
}

/* To str_ulmc_cur_dir; cur_dir is the library's last argument. */
/* 0x001864a8 */
static int mc_chdir_cur_request(int port, int slot, int cur_dir)
{
    int cmd;
    int res;
    int state;
    int rc;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    rc = sceMcChdir(port, slot, str_ulmc_cur_dir, cur_dir);
    if (rc == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return rc;
}

/* 0x00186550 */
static int mc_open_request(int port, int slot, int path, int mode)
{
    int cmd;
    int res;
    int state;
    int rc;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    rc = sceMcOpen(port, slot, path, mode);
    if (rc == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return rc;
}

/* 0x00186600 */
static int mc_close_request(int fd)
{
    int cmd;
    int res;
    int state;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    return sceMcClose(fd);
}

/* 0x00186670 */
static int mc_rename_request(int port, int slot, int old_name, int new_name)
{
    int cmd;
    int res;
    int state;
    int rc;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    rc = sceMcRename(port, slot, old_name, new_name);
    if (rc == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return rc;
}

/* 0x00186720 */
static int mc_delete_request(int port, int slot, int path)
{
    int cmd;
    int res;
    int state;
    int rc;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    rc = sceMcDelete(port, slot, path);
    if (rc == 0) {
        ulmc_cur_port = port;
        ulmc_cur_slot = slot;
    }
    return rc;
}

/* 0x001867c0 */
static int mc_read_request(int fd, int buf, int size)
{
    int cmd;
    int res;
    int state;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    return sceMcRead(fd, buf, size);
}

/* 0x00186850 */
static int mc_write_request(int fd, int buf, int size)
{
    int cmd;
    int res;
    int state;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    return sceMcWrite(fd, buf, size);
}

/* 0x001868e0 */
static int mc_seek_request(int fd, int ofs, int origin)
{
    int cmd;
    int res;
    int state;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    return sceMcSeek(fd, ofs, origin);
}

/* State 0x72: reads the entry; ulMcStatusComplete writes attr under attr_mask. */
/* 0x00186970 */
static int mc_get_dir_request(int port, int slot, char *path, int attr, int attr_mask)
{
    int cmd;
    int res;
    int state;
    int rc;

again:
    state = sceMcSync(1, &cmd, &res);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (state == 0)
        return -200;
    ulmc_cur_cmd = 114;
    ulmc_file_attr = attr;
    ulmc_file_attr_mask = attr_mask;
    if (path != ulmc_path)
        strcpy(ulmc_path, path);
    rc = sceMcGetDir(port, slot, path, 0, 1, ((char *)&ulmc_dir_ent));
    if (rc != 0) {
        ulmc_cur_cmd = 0;
        return rc;
    }
    ulmc_cur_port = port;
    ulmc_cur_slot = slot;
    return 0;
}

/* Into ulmc_dir_ent, remembering the card for mc_dir_next_entry; 0 if none. Waits while busy. */
/* 0x00186a68 */
static char *mc_dir_first_entry(int port, int slot, int path)
{
    int cmd;
    int res;
    int state;
    int cnt;
    char *buf;

again:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (state == 0);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    buf = ((char *)&ulmc_dir_ent); /* steer(cast) */
    if (sceMcGetDir(port, slot, path, 0, 1, buf) < 0)
        return 0;
    sceMcSync(0, 0, &cnt);
    if (cnt <= 0)
        return 0;
    ulmc_list_port = port;
    ulmc_list_slot = slot;
    return buf;
}

/* 0 if none. */
/* 0x00186b40 */
static char *mc_dir_next_entry(int pattern)
{
    int cmd;
    int res;
    int state;
    int cnt;
    char *buf;

again:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (state == 0);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    buf = ((char *)&ulmc_dir_ent); /* steer(cast) */
    if (sceMcGetDir(ulmc_list_port, ulmc_list_slot, pattern, 1, 1, buf) < 0)
        return 0;
    sceMcSync(0, 0, &cnt);
    return cnt > 0 ? buf : 0;
}

/* Name into dst; 1, or 0 if none. */
/* 0x00186bf8 */
static int mc_dir_first(int port, int slot, int path, char *dst)
{
    int cmd;
    int res;
    int state;
    int cnt;
    char *buf;

again:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (state == 0);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    buf = ((char *)&ulmc_dir_ent); /* steer(cast) */
    if (sceMcGetDir(port, slot, path, 0, 1, buf) < 0)
        return 0;
    sceMcSync(0, 0, &cnt);
    if (cnt <= 0)
        return 0;
    ulmc_list_port = port;
    ulmc_list_slot = slot;
    strcpy(dst, buf + 32);
    return 1;
}

/* Name into dst; 1, or 0 if none. */
/* 0x00186ce8 */
static int mc_dir_next(int pattern, char *dst)
{
    int cmd;
    int res;
    int state;
    int cnt;
    char *buf;

again:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (state == 0);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    buf = ((char *)&ulmc_dir_ent); /* steer(cast) */
    if (sceMcGetDir(ulmc_list_port, ulmc_list_slot, pattern, 1, 1, buf) < 0)
        return 0;
    sceMcSync(0, 0, &cnt);
    if (cnt <= 0)
        return 0;
    strcpy(dst, buf + 32);
    return 1;
}

/* mc_status: 0, -4, -100, -1000 or the library's result. Waits while busy. */
/* 0x00186db8 */
static int mc_file_exists(int port, int slot, int path)
{
    int cmd;
    int res;
    int state;
    int cnt;

again:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (state == 0);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (sceMcGetDir(port, slot, path, 0, 1, ((char *)&ulmc_dir_ent)) < 0) {
        mc_status = -1000;
        return 0;
    }
    sceMcSync(0, 0, &cnt);
    if (cnt > 0) {
        mc_status = 0;
        return 1;
    }
    if (cnt == 0)
        mc_status = -4;
    else if (cnt < -10)
        mc_status = -100;
    else
        mc_status = cnt;
    return 0;
}

/* Set by ulMcConnectEvent; 0 for slot != 0. */
/* 0x00186ec0 */
int mc_is_connected(int port, int slot)
{
    if (slot)
        return 0;
    return ((struct { int w[4] ; } *)&mc_connected)[port].w[0]; /* steer(cast) */
}

/* Checked up to twice. */
/* 0x00186ee8 */
static int mc_is_formatted(int port, int slot)
{
    if (mc_check_format(port, slot) == 0)
        return 1;
    return mc_check_format(port, slot) == 0;
}

/* 0, -2 unformatted, -100 no usable card or slot != 0, -1000 refused; also in mc_status. Waits. */
/* 0x00186f38 */
static int mc_check_format(int port, int slot)
{
    int cmd;
    int res;
    int format;
    int cnt;
    int state;

    if (slot) {
        mc_status = -100;
        return -100;
    }
again:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (state == 0);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (sceMcGetInfo(port, slot, 0, 0, &format) < 0) {
        mc_status = -1000;
        return -1000;
    }
    sceMcSync(0, 0, &cnt);
    if (cnt >= -10)
        mc_status = format == 1 ? 0 : -2;
    else
        mc_status = -100;
    return mc_status;
}

/* Free clusters, -2 unformatted, or -100/-1000 as mc_check_format. Waits while busy. */
/* 0x00187038 */
static int mc_get_info(int port, int slot)
{
    int cmd;
    int res;
    int free_clusters;
    int format;
    int cnt;
    int state;

    if (slot)
        return -100;
again:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (state == 0);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    if (sceMcGetInfo(port, slot, 0, &free_clusters, &format) < 0)
        return -1000;
    sceMcSync(0, 0, &cnt);
    if (cnt < -10)
        return -100;
#ifdef VERSION_US
    return format == 1 ? free_clusters : -2;
#else
    return format == 1 ? free_clusters : cnt;
#endif
}

/* Bytes; -4 missing, or -100/-1000 as mc_check_format. Waits while busy. */
/* 0x00187120 */
static int mc_get_file_size(int port, int slot, int path)
{
    int cmd;
    int res;
    int cnt;
    int state;
    char *buf;

    if (slot)
        return -100;
again:
    do {
        state = sceMcSync(1, &cmd, &res);
    } while (state == 0);
    if (state == 1) {
        ulMcHandleCompletion(cmd, res);
        goto again;
    }
    buf = ((char *)&ulmc_dir_ent); /* steer(cast) */
    if (sceMcGetDir(port, slot, path, 0, 1, buf) < 0)
        return -1000;
    sceMcSync(0, 0, &cnt);
    if (cnt < -10)
        return -100;
    if (cnt == 0)
        return -4;
    return cnt > 0 ? *(int *)(buf + 16) : cnt;
}

/* KB, rounded up; also copies the directory part of path into name, which nothing reads. */
/* 0x00187210 */
static int mc_get_file_kb(int port, int slot, char *path)
{
    char name[256];
    int cmd;
    int res;
    int cnt;
    int state;
    char *slash;
    int n;

    if (slot)
        return -100;
    for (;;) {
        do {
            state = sceMcSync(1, &cmd, &res);
        } while (state == 0);
        if (state == 1) {
            ulMcHandleCompletion(cmd, res);
            continue;
        }
        if (sceMcGetDir(port, slot, path, 0, 1, ((McDirEnt *)&ulmc_dir_ent)) < 0)
            return -1000;
        slash = strrchr(path, 47);
        if (slash) {
            n = slash - path;
            memcpy(name, path, n);
            name[n] = 0;
        } else {
            name[1] = 0;
            name[0] = 46;
        }
        sceMcSync(0, 0, &cnt);
        if (cnt < -10)
            return -100;
        if (cnt == 0)
            return -4;
        return cnt > 0 ? (((McDirEnt *)&ulmc_dir_ent)[0].size + 1023) >> 10 : cnt; /* steer(cast) */
    }
}

/* Left by the last card check or lookup. */
/* 0x00187358 */
static int mc_get_status(void)
{
    return mc_status;
}
