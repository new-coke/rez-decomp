/* Disc directory cache for the 23 ROFS volumes, read on first use. */

#include "debug.h"
#include "libc.h"
#include "rofs_if.h"
#include "dir_read.h"

typedef struct DirCache {
    char *name;
    char *buf;
    int loaded;
} DirCache;

typedef struct DirReadRofsDir {
    int name;
    int buf;
    int loaded;
} DirReadRofsDir;

extern char rofs_dir_bufs[];
extern DirReadRofsDir rofs_dirs[];

/* "VOL:<name>" into a 6240-byte slot of rofs_dir_bufs, once; retries until it succeeds. */
/* 0x001118c8 */
static void ROFS_DirRead(int i)
{
    DirCache *dir = &rofs_dirs[i];
    char buf[64];

    if (dir->loaded == 0) {
        sprintf(buf, "VOL:%s", dir->name);
        while (ROFS_LoadDir(buf, rofs_dir_bufs + i * 6240, 32) != 0)
            ;
        dir->buf = rofs_dir_bufs + i * 6240;
        if (dir->name != 0)
            DEBUG_PRINTF("ROFS_DirRead : %s\n", dir->name);
        dir->loaded = 1;
    }
}

/* 0x00111998 */
void rofs_dir_read_all(void)
{
    int i = 0;

    do {
        ROFS_DirRead(i);
        i++;
    } while (i < 23);
}

/* Reads it first if needed. */
/* 0x001119d0 */
int rofs_dir_entry(int i)
{
    DirReadRofsDir *dir = &rofs_dirs[i];

    if (dir->loaded == 0)
        ROFS_DirRead(i);
    return dir->buf;
}

/* 0x00111a20 */
static int rofs_dir_get_name(int i)
{
    return rofs_dirs[i].name;
}
