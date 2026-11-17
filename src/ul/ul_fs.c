/*
 * File layer over CRI ADXF: cdrom0: or host0: paths from root and current directory, open-file
 * slots, 2048-byte sector reads, IOP module loads.
 */

#include "types_ul.h"
#include "libc.h"
#include "ul_fs.h"
#include "adxf.h"
#include "libkernl_loadfile.h"
#include "ul_module.h"

extern FsCtx *ulFsCtx;

/* name resolved like a file path. */
/* 0x001b71f0 */
int ulFsLoadModule(char *name, int arglen, int args)
{
    char path[256];
    int ret;

    ulFsMakePath(name, path);
    ret = sceSifLoadModule(path, arglen, args);
    if (ret >= 0)
        ulDebugPrintf("Load IOP module [%s]\n", path);
    else
        ulDebugPrintf("module load error! [%s]\n", path);
    return ret;
}

/* Every '/' and '\\' -> sep. */
/* 0x001b7268 */
static char *ulFsReplaceSeparator(char *str, char sep)
{
    char *p;

    p = str;
    while (*p) {
        if (*p == '/' || *p == '\\')
            *p = sep;
        p++;
    }
    return str;
}

/* In place. */
/* 0x001b72b8 */
static char *ulFsToUpper(char *str)
{
    char *p;
    int c;
    int upper;

    p = str;
    while (*p) {
        c = *p;
            upper = c;
        if ((_ctype_ + 1)[c] & 2)
            upper = c - 32;
        *p = upper;
        p++;
    }
    return str;
}

/* Becomes the next one handed out. */
/* 0x001b7310 */
static void ulFsFreeSlot(FsSlot *slot)
{
    FsCtx *fs = ulFsCtx;

    if (slot->used) {
        fs->next = slot->idx;
        fs->nopen--;
        memset(slot, 0, 44);
    }
}

/*
 * Hands out ctx->next and finds the next free (after it, then from the start); 0 when full.
 * Bug: the wrapped search tests the slot handed out, so a wrapped table reports full early.
 */
/* 0x001b7348 */
static FsSlotView *ulFsAllocSlot(void)
{
    FsCtx *ctx;
    char (*slots)[44];
    int max;
    FsSlotView *slot;
    int i;
    int n;

    ctx = ulFsCtx;
    if (ctx->next < 0)
        return 0;
    slots = (char (*)[44])ctx->slot; /* steer(cast) */
    slot = (FsSlotView *)slots[ctx->next];
    slot->used = 1;
    ctx->nopen = ctx->nopen + 1;
    n = ctx->next;
    max = ctx->max;
    i = n + 1;
    if (i < max) {
        char (*p)[44] = &slots[i];
        do {
            if (!(*p)[0]) {
                ctx->next = i;
                return slot;
            }
            i++;
            p++;
        } while (i < max);
    }
    if (n > 0) {
        i = 0;
        do {
            do {
                do {
                    if (!slot->used) {
                        ctx->next = i;
                        return slot;
                    }
                    i++;
                } while (i < n);
            } while (0);
        } while (0);
    }
    ctx->next = -1;
    return slot;
}

/*
 * Root + current dir + name; "DVD:"/"HST:" picks the device. Disc: upper case, '\\', ";1". -1 for
 * an unknown device. Bug: the first ";1" is appended before out is written, and lost.
 */
/* 0x001b73f8 */
int ulFsMakePath(char *name, char *out)
{
    char buf[256];
    int dev;

    dev = ulFsCtx->dev;
    if (strncmp(name, "DVD:", 4) == 0) {
        dev = 0;
        name += 4;
    } else if (strncmp(name, "HST:", 4) == 0) {
        dev = 1;
        name += 4;
    }
    switch (dev) {
    case 0:
        strcpy(buf, "");
        if (strlen(ulFsCtx->root))
            strcat(buf, ulFsCtx->root);
        if (strlen(ulFsCtx->cwd)) {
            strcat(buf, "\\");
            strcat(buf, ulFsCtx->cwd);
        }
        if (strlen(name)) {
            strcat(buf, "\\");
            strcat(buf, name);
            strcat(out, ";1");
        }
        ulFsReplaceSeparator(buf, 92);
        ulFsToUpper(buf);
        strcpy(out, "cdrom0:");
        strcat(out, buf);
        strcat(out, ";1");
        break;
    case 1:
        strcpy(out, "host0:");
        if (strlen(ulFsCtx->root))
            strcat(out, ulFsCtx->root);
        if (strlen(ulFsCtx->cwd)) {
            strcat(out, "/");
            strcat(out, ulFsCtx->cwd);
        }
        if (strlen(name)) {
            strcat(out, "/");
            strcat(out, name);
        }
        if (!ulFsCtx->bs_dev || ulFsCtx->bs_dev != dev)
            ulFsReplaceSeparator(out, 47);
        else
            ulFsReplaceSeparator(out, 92);
        break;
    default:
        dev = -1;
        break;
    }
    return dev;
}

/* 2048-byte sectors. */
/* 0x001b7658 */
static int ulFsGetSizeSct(FsSlot *file)
{
    return ADXF_GetFsizeSct(file->adxf);
}

/* Rounded up to whole sectors. */
/* 0x001b7678 */
static int ulFsGetSize(FsSlot *file)
{
    return ADXF_GetFsizeSct(file->adxf) << 11;
}

/* Waits until ADXF reports it finished. */
/* 0x001b7698 */
int ulFsRead(FsSlot *file, int nsct, void *buf)
{
    int ret;

    ret = ADXF_ReadNw32(file->adxf, nsct, buf);
    while (ADXF_GetStat(file->adxf) != 3)
        ;
    return ret;
}

/* Arguments pass through to ADXF_Seek. */
/* 0x001b76e8 */
static int ulFsSeek(FsSlot *file, int pos, int type)
{
    return ADXF_Seek(file->adxf, pos, type);
}

/* Releases the slot too. */
/* 0x001b7708 */
static int ulFsClose(FsSlot *file)
{
    adxf_close_handle(file->adxf);
    ulFsFreeSlot(file);
    return 0;
}

/* 0 if no slot or the open fails. Slot keeps the resolved device; ADXF_Open gets name itself. */
/* 0x001b7740 */
static FsSlot *ulFsOpen(char *name, int mode)
{
    FsSlot *slot;

    slot = (FsSlot *)ulFsAllocSlot();
    if (!slot)
        return 0;
    slot->dev = ulFsMakePath(name, ulFsCtx->path);
    slot->adxf = ADXF_Open(name, mode);
    if (!slot->adxf) {
        printf("ulFsOpen: file open error ![%s]\n", name);
        ulFsFreeSlot(slot);
        return 0;
    }
    return slot;
}

/* Leading separator replaces, ".." drops the last component, else appended as is (no separator). */
/* 0x001b77d8 */
int ulFsChangeDir(char *dir)
{
    char *cwd;
    char *p;

    cwd = ulFsCtx->cwd;
    if (dir[0] == 92 || dir[0] == 47) {
        strcpy(cwd, dir);
    } else if (strncmp(dir, "..", 2) == 0) {
        p = cwd + strlen(cwd) - 1;
        for (; cwd < p; p--) {
            if (*p == 92 || *p == 47)
                break;
        }
        *p = 0;
    } else {
        strcat(ulFsCtx->cwd, dir);
    }
    ulDebugPrintf("Change directory to [%s]\n", cwd);
    return 0;
}

/* 0x001b78e8 */
void ulFsSetRootDir(char *dir)
{
    strcpy(ulFsCtx->root, dir);
}

/* Device whose host paths ulFsMakePath separates with '\\'. Nothing calls it. */
/* 0x001b78f8 */
static void ulFsSetBackslashDevice(int dev)
{
    ulFsCtx->bs_dev = dev;
}

/* 0 disc, 1 host; returns the previous one. */
/* 0x001b7908 */
int ulFsSetDevice(int dev)
{
    int old;

    old = ulFsCtx->dev;
    ulFsCtx->dev = dev;
    return old;
}

/* 0x001b7918 */
static void ulFsStub(void)
{
}

/* 0x001b7920 */
static void ulFsExit(void)
{
    ulFsCtx = 0;
}

/*
 * n open files, disc device. Bug: ent is only installed if the table pointer is set, which the
 * preceding clear rules out.
 */
/* 0x001b7928 */
void ulFsInit(int n, FsCtx *ctx, int nent, int *ent)
{
    ulFsCtx = ctx;
    if (ctx) {
        memset(ctx, 0, n * sizeof(FsSlot) + 800);
        ulFsCtx->max = n;
        if (ulFsCtx->ent) {
            ulFsCtx->ent = ent;
            memset(ent, 0, nent * 28 + 8);
            *ulFsCtx->ent = nent;
        }
        {
            unsigned char *flags = ulFsCtx->flags;

            *flags = 5;
            flags[1] = 0;
            flags[2] = 0;
        }
        ulFsCtx->dev = 0;
        ulFsCtx->bs_dev = 0;
    }
}
