/* Tagged data blocks: 8-byte header (four-character type, payload size). */

#include "types_ul.h"
#include "ul_blk.h"
#include "ul_fs.h"
#include "ul_module.h"

/* Header plus blk->size bytes on. */
/* 0x001c2330 */
void *ulBlkNext(UlBlk *blk)
{
    return (char *)blk + (blk->size + 8);
}

/* Meant as a byte swap; wrong shift directions OR all four bytes into bits 24..31. */
/* 0x001c2340 */
static unsigned int ulBlkSwap32(unsigned int x)
{
    return (((x << 24) | (((x & 0xff0000) << 8) | ((x & 0xff00) << 16))) | (x & 0xff000000));
}

/* First 2048-byte sector (holds the header), then the rest. Returns bytes read. */
/* 0x001c2370 */
static int ulFsReadBlock(void *file, void *reserved, UlBlk *blk)
{
    int pages;

    ulFsRead(file, 1, blk);
    pages = (blk->size + 2055) >> 11;
    if (pages > 1)
        ulFsRead(file, pages - 1, (char *)blk + 2048);
    return pages << 11;
}

/* Type and size, on the debug console. */
/* 0x001c23d8 */
static void ulBlkPrint(UlBlk *blk)
{
    unsigned char *bytes = (unsigned char *)blk;

    ulDebugPrintf("type: %c%c%c%c, size: %08x byte.\n", blk->type & 0xff, bytes[1], bytes[2], blk->type >> 24, blk->size);
}
