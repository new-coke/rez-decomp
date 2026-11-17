/* On-screen debug text console: character grid drawn as sprites from a 16x16-glyph font sheet. */

#include "types_ul.h"
#include "sdk_compat.h"
#include <stdarg.h>
#include "libc.h"
#include "ul_console.h"
#include "ps_displist.h"
#include "ps_gs.h"
#include "ps_pkt.h"
#include "ul_gx.h"
#include "ul_texture.h"

extern UlConsole *ulConsoleCtx;
extern UlGxBig ulGxState;
extern void *ulConsoleFont;
extern char ulConsoleBuf[];
extern int ulConsoleReady;
extern char ulConsoleFontData[];

/*
 * Colour, then texture and screen corners. Sheet is 16 glyphs wide, 8 texels each. Returns the end.
 */
/* 0x001b8da0 */
static int *ulConsoleBuildGlyph(int *dst, int ch, int x, int y, int z, int width, int height)
{
    UlConsole *k = ulConsoleCtx;
    int row = ch / 16;
    int u = (ch - row * 16) << 7;
    int v = row << 7;

    dst[0] = k->col_r;
    dst[1] = k->col_g;
    dst[2] = k->col_b;
    dst[3] = k->col_a;
    dst += 4;
    dst[0] = u;
    dst[1] = v;
    dst += 4;
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
    dst[3] = 0;
    dst += 4;
    dst[0] = u + 128;
    dst[1] = v + 128;
    dst += 4;
    dst[0] = x + width;
    dst[1] = y + height;
    dst[2] = z;
    dst[3] = 0;
    return dst + 4;
}

/*
 * One sprite batch per row, glyphs for non-empty cells; depth from the console scale. Once a frame.
 */
/* 0x001b8e40 */
void ulConsoleDraw(void)
{
    UlGxGifTag tag;
    UlGxBig *gx = &ulGxState;
    void *pkt;
    UlGxVifPkt *dl;
    UlGxGifTag *gt;
    int *wr;
    char *cell;
    int x_start;
    int y;
    int z;
    int width;
    int height;
    int x;
    int row;
    int col;
    int n;

    *(u_long128 *)&tag = REZ_QWORD_ZERO(u_long128);
    tag.EOP = 1;
    tag.PRE = 1;
    tag.PRIM = 0x156;
    tag.FLG = 0;
    tag.NREG = 5;
    tag.REGS0 = 1;
    tag.REGS1 = 3;
    tag.REGS2 = 4;
    tag.REGS3 = 3;
    tag.REGS4 = 4;
    ulGxSetTexFilter(0);
    ulGxSetTexFunc(0);
    ulGxSetAlphaBlend(4, 5);
    ulGxSetTexture(ulConsoleFont);
    width = (int)((float)ulConsoleCtx->cw * gx->scale_x * 16.0f);
    height = (int)((float)ulConsoleCtx->ch * gx->scale_y * 16.0f);
    x_start = (int)((0.0f * gx->scale_x + gx->offset_x) * 16.0f);
    y = (int)((0.0f * gx->scale_y + gx->offset_y) * 16.0f);
    z = (int)(ulConsoleCtx->inv * gx->scale_z * 16.0f);
    pkt = psGsGetDisplayList();
    dl = psDisplayListOpenRefPkt(pkt);
    psVifPktOpenGifPkt(dl);
    psDisplayListFlushGsRegs(pkt, dl);
    cell = ulConsoleCtx->cells;
    for (row = 0; row < ulConsoleCtx->rows; row++, y += height) {
        gt = psVifPktAddGifTag(dl, &tag);
        wr = (int *)(gt + 1);
        n = 0;
        x = x_start;
        for (col = 0; col < ulConsoleCtx->cols; col++) {
            int c = *cell++;

            if (c > 0) {
                wr = ulConsoleBuildGlyph(wr, c, x, y, z, width, height);
                n++;
            }
            x += width;
        }
        if (n > 0) {
            dl->cur += n * 80;
            dl->nqw += n * 5;
            gt->NLOOP = n;
        }
        psVifPktCountGifQwords(dl);
    }
    psVifPktCloseGifPkt(dl);
    psDisplayListCloseRefPkt(pkt);
}

/* Screen pixels. */
/* 0x001b9180 */
void ulConsoleSetCellSize(int width, int height)
{
    ulConsoleCtx->cw = width;
    ulConsoleCtx->ch = height;
}

/* 0x001b9190 */
static void ulConsoleSetColor(unsigned int col)
{
    UlConsole *k = ulConsoleCtx;

    k->col_r = col & 0xff;
    k->col_g = (col >> 8) & 0xff;
    k->col_b = (col >> 16) & 0xff;
    k->col_a = col >> 24;
}

/* A newline moves the cursor down but the copy continues on the same row. */
/* 0x001b91c0 */
static void ulConsoleWrite(char *dst, const char *src, unsigned int n)
{
    unsigned int i;

    for (i = 0; i < n; i++) {
        char c = *src++;

        if (c == '\n') {
            ulConsoleSetCursor(ulConsoleCtx->x, ulConsoleCtx->y + 1);
        } else {
            *dst++ = c;
        }
    }
}

/* 0x001b9258 */
static void ulConsoleStub(void)
{
}

/* At the cursor, cut off at the end of the row. */
/* 0x001b9260 */
void ulConsolePuts(char *str)
{
    UlConsole *k;
    int len = strlen(str);
    int n;

    k = ulConsoleCtx;
    n = len;
    if (k->cols < k->x + len)
        n = k->cols - k->x;
    ulConsoleWrite(k->cur, str, n);
}

/* At the cursor. */
/* 0x001b92a8 */
void ulConsolePrintf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vsprintf(ulConsoleBuf, fmt, ap);
    ulConsolePuts(ulConsoleBuf);
}

/* Column x, row y. */
/* 0x001b9300 */
void ulConsoleSetCursor(int x, int y)
{
    UlConsole *k = ulConsoleCtx;

    k->x = x;
    k->y = y;
    k->cur = (char *)k + y * k->cols * k->bpc + (x * k->bpc + 68);
}

/* Glyph depth = 1 / scale. */
/* 0x001b9338 */
void ulConsoleSetScale(float scale)
{
    ulConsoleCtx->scale = scale;
    ulConsoleCtx->inv = 1.0f / scale;
}

/* 0x001b9360 */
void ulConsoleClear(void)
{
    UlConsole *k = ulConsoleCtx;

    memset(k->cells, 0, k->size);
    ulConsoleSetCursor(0, 0);
}

/* Frees the font texture. */
/* 0x001b9390 */
void ulConsoleExit(void)
{
    if (ulConsoleReady == 0) {
        printf("ulConsoleExit: console is not initialized!\n");
        return;
    }
    if (ulConsoleFont) {
        ulTextureFreeLocked(ulConsoleFont);
        ulConsoleFont = 0;
    }
    ulConsoleReady = 0;
}

/*
 * Cells (cols x rows bytes) follow the header in k. 16x16-pixel grey text; loads the built-in font.
 */
/* 0x001b93e0 */
void ulConsoleInit(UlConsole *k, int cols, int rows)
{
    if (ulConsoleReady) {
        printf("ulConsoleInit: already initialize!\n");
        return;
    }
    ulConsoleReady = 1;
    k->unk_30 = 6720;
    k->cols = cols;
    k->rows = rows;
    k->cur = k->cells;
    k->bpc = 1;
    k->size = cols * rows;
    k->y = 0;
    k->x = 0;
    ulConsoleCtx = k;
    ulConsoleSetScale(0.1f);
    ulConsoleSetCellSize(16, 16);
    ulConsoleSetColor(0x80606060);
    ulGxLock(1);
    ulConsoleFont = ulTextureLoadBlockDirect(ulConsoleFontData);
    ulGxUnlock(1);
}
