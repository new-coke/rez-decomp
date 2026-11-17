/*
 * Peripheral LCD: a 48x32 one-bit bitmap (pixels, masked and scaled blits, invert) sent through
 * the Ninja LCD calls. Per-slot event handlers pick the picture.
 */

#include "types_game.h"
#include "nj_system.h"
#include "pad_lcd.h"
#include "sound_beat.h"
#include "backup.h"

typedef struct {
    char pad_00[4];
    int handle;
} PadLcdTarget;

typedef struct {
    int x, y, right, bottom;
} PadLcdRect;

typedef struct {
    char pad_00[4];
    int slot;
    char pad_08[8];
    int kind;
    char pad_14[20];
    int code;
    McSlot *r;
} PadLcdEvent;

extern unsigned char pad_lcd_bitmap[];
extern int pad_lcd_inited;
extern int pad_lcd_result;
extern int (*pad_lcd_handlers[])(void *, PadLcdEvent *, int *);
extern int pad_lcd_counts[];
extern int pad_lcd_codes[];
extern int pad_lcd_img_beat1;
extern int pad_lcd_img_beat0;
extern int pad_lcd_img_once;
extern void bitmap48x32_apply(PadLcdTarget *target, unsigned char *bits);

/* Stored bottom-up and mirrored, like bitmap_get_bit. */
/* 0x00142a60 */
int bitmap48x32_get_pixel(int x, int y)
{
    int row = 31 - y;
    int col = 47 - x;
    unsigned char byte = pad_lcd_bitmap[row * 6 + col / 8];
    int mask = 1 << (7 - col % 8);

    return (byte & mask) != 0;
}

/* val: 0 or 1. */
/* 0x00142ac8 */
void bitmap48x32_set_pixel(int x, int y, unsigned char val)
{
    int row = 31 - y;
    int col = 47 - x;
    unsigned char *dst = pad_lcd_bitmap + row * 6 + col / 8;
    int n = 7 - col % 8;

    *dst = (*dst & ~(1 << n)) | (val << n);
}

/* Bottom-up and mirrored ((0, 0) is the last bit of the last row), MSB first; bpr bytes per row. */
/* 0x00142b40 */
int bitmap_get_bit(unsigned char *bits, int bpr, int height, int x, int y)
{
    int row = height - y - 1;
    int col = bpr * 8 - x - 1;
    unsigned char byte = bits[row * bpr + col / 8];
    int mask = 1 << (7 - col % 8);

    return (byte & mask) != 0;
}

/* Addressed as in bitmap_get_bit. */
/* 0x00142ba0 */
void bitmap_set_bit(unsigned char *bits, int bpr, int height, int x, int y, unsigned char val)
{
    int row = height - y - 1;
    int col = bpr * 8 - x - 1;
    int n = 7 - col % 8;
    unsigned char *dst = bits + row * bpr + col / 8;
    int mask = ~(1 << n);

    *dst = (*dst & mask) | (val << n);
}

/* Turned half a turn unless orientation 1; sent only when the LCD is ready. */
/* steer(cast) */
/* 0x00142c10 */
void bitmap48x32_apply(PadLcdTarget *target, unsigned char *bits)
{
    unsigned char buf[192];
    unsigned char *src;
    int handle = target->handle;
    int x;
    int y;

    if (((int (*)(int))nj_lcd_get_orientation)(handle) == 1) {
        src = bits;
    } else {
        for (y = 0; y < 32; y++) {
            for (x = 0; x < 48; x++)
                ((void (*)(unsigned char *, int, int, int, int, int))bitmap_set_bit)(buf, 6, 32, x, y, bitmap_get_bit(bits, 6, 32, 47 - x, 31 - y)); /* steer(cast) */
        }
        src = buf;
    }
    if (((int (*)(int))nj_lcd_is_ready)(handle))
        ((void (*)(int, unsigned char *))nj_lcd_write)(handle, src);
}

/* 0x00142d08 */
void bitmap48x32_invert(void)
{
    int i;

    for (i = 0; i < 192; i++)
        pad_lcd_bitmap[i] = ~pad_lcd_bitmap[i];
}

/*
 * Nearest-pixel scale of img rect src (default all) into dst (default whole screen), skipping
 * masked pixels; clipped to both.
 */
/* 0x00142d40 */
void bitmap48x32_blit_scaled(PadLcdRect *dst, PadLcdImage *img, PadLcdRect *src)
{
    int ax, ay, ar, ab;
    int bx, by, br, bb;
    int bw, bh;
    int aw, ah;
    int x, y, sx, sy;

    if (dst == 0) {
        ay = 0;
        ax = 0;
        ar = 48;
        ab = 32;
    } else {
        ax = dst->x;
        ay = dst->y;
        ar = dst->right;
        ab = dst->bottom;
    }
    if (src == 0) {
        by = 0;
        bx = 0;
        br = img->w;
        bb = img->h;
    } else {
        bx = src->x;
        by = src->y;
        br = src->right;
        bb = src->bottom;
    }
    bw = br - bx;
    bh = bb - by;
    aw = ar - ax;
    ah = ab - ay;
    if (aw <= 0 || ah <= 0 || bw <= 0 || bh <= 0)
        return;
    for (y = ay; y < ab; y++) {
        sy = bh * (y - ay) / ah + by;
        if (sy >= 0 && sy < img->h && y >= 0 && y < 32) {
            for (x = ax; x < ar; x++) {
                sx = bw * (x - ax) / aw + bx;
                if (sx >= 0 && sx < img->w && x >= 0 && x < 48) {
                    if (!bitmap_get_bit(img->mask, img->bpr, img->h, sx, sy))
                        ((void (*)(unsigned char *, int, int, int, int, int))bitmap_set_bit)(pad_lcd_bitmap, 6, 32, x, y, /* steer(cast) */
                                     bitmap_get_bit(img->color, img->bpr, img->h, sx, sy));
                }
            }
        }
    }
}

static inline int imax(int val, int lo)
{
    return val < lo ? lo : val;
}

/* pixel = (pixel & mask) | colour; byte at a time, shifted path off byte boundaries; clipped. */
/* 0x00142f90 */
void bitmap48x32_blit_masked(int x, int y, PadLcdImage *img)
{
    int col_start;
    int sh;
    int ox = 48 - img->bpr * 8;
    int oy = 32 - img->h;

    x = ox - x;
    y = oy - y;
    if (x >= 0) {
        col_start = x / 8;
        sh = x % 8;
    } else {
        col_start = ~((-x - 1) / 8);
        sh = (8 - -x % 8) % 8;
    }
    if (sh == 0) {
        int row;
        int col;

        for (row = imax(y, 0); row < (y + img->h > 32 ? 32 : y + img->h); row++) {
            for (col = imax(col_start, 0); col < (col_start + img->bpr > 6 ? 6 : col_start + img->bpr); col++) {
                int k = (row - y) * img->bpr + (col - col_start);
                unsigned char *mp = img->mask + k;
                unsigned char *cp = img->color + k;

                pad_lcd_bitmap[row * 6 + col] &= *mp;
                pad_lcd_bitmap[row * 6 + col] |= *cp;
            }
        }
    } else {
        int row;
        int col;

        for (row = y > -1 ? y : 0; row < (y + img->h > 32 ? 32 : y + img->h); row++) {
            int k;

            col = col_start;
            k = (row - y) * img->bpr;
            if ((unsigned int)col < 6) {
                unsigned char mask = (0xff << (8 - sh)) | (img->mask[k] >> sh);

                pad_lcd_bitmap[row * 6 + col] &= mask;
                pad_lcd_bitmap[row * 6 + col] |= img->color[k] >> sh;
            }
            for (col = col_start + 1; col < col_start + img->bpr; col++) {
                int j = (row - y) * img->bpr + col - 1;

                k = j - col_start;
                if ((unsigned int)col < 6) {
                                        unsigned char cur_mask = img->mask[k];
                    unsigned char next_mask = img->mask[k + 1];
                    unsigned char mask = (next_mask >> sh) | (cur_mask << (8 - sh));

                    pad_lcd_bitmap[row * 6 + col] &= mask;
                    pad_lcd_bitmap[row * 6 + col] |= img->color[k] << (8 - sh);
                    pad_lcd_bitmap[row * 6 + col] |= img->color[k + 1] >> sh;
                }
            }
            col = col_start + img->bpr;
            k = (row - y) * img->bpr + img->bpr - 1;
            if ((unsigned int)col < 6) {
                unsigned char mask = (0xff >> sh) | (img->mask[k] << (8 - sh));

                pad_lcd_bitmap[row * 6 + col] &= mask;
                pad_lcd_bitmap[row * 6 + col] |= img->color[k] << (8 - sh);
            }
        }
    }
}

/* Inverted: pixel = (pixel & mask) | ~(mask | colour). */
/* 0x00143388 */
void bitmap48x32_blit_masked_inv(int x, int y, PadLcdImage *img)
{
    int col_start;
    int sh;
    int ox = 48 - img->bpr * 8;
    int oy = 32 - img->h;

    x = ox - x;
    y = oy - y;
    if (x >= 0) {
        col_start = x / 8;
        sh = x % 8;
    } else {
        col_start = ~((-x - 1) / 8);
        sh = (8 - -x % 8) % 8;
    }
    if (sh == 0) {
        int row;
        int col;

        for (row = imax(y, 0); row < (y + img->h > 32 ? 32 : y + img->h); row++) {
            for (col = imax(col_start, 0); col < (col_start + img->bpr > 6 ? 6 : col_start + img->bpr); col++) {
                int k = (row - y) * img->bpr + (col - col_start);
                unsigned char *mp = img->mask + k;
                unsigned char *cp = img->color + k;

                pad_lcd_bitmap[row * 6 + col] &= *mp;
                pad_lcd_bitmap[row * 6 + col] |= ~*cp & ~*mp;
            }
        }
    } else {
        int row;
        int col;

        for (row = y > -1 ? y : 0; row < (y + img->h > 32 ? 32 : y + img->h); row++) {
            int k;

            col = col_start;
            k = (row - y) * img->bpr;
            if ((unsigned int)col < 6) {
                unsigned char mask = (0xff << (8 - sh)) | (img->mask[k] >> sh);

                pad_lcd_bitmap[row * 6 + col] &= mask;
                pad_lcd_bitmap[row * 6 + col] |= (~img->color[k] >> sh) & ~mask;
            }
            for (col = col_start + 1; col < col_start + img->bpr; col++) {
                int j = (row - y) * img->bpr + col - 1;

                k = j - col_start;
                if ((unsigned int)col < 6) {
                                        unsigned char cur_mask = img->mask[k];
                    unsigned char next_mask = img->mask[k + 1];
                    unsigned char mask = (next_mask >> sh) | (cur_mask << (8 - sh));

                    pad_lcd_bitmap[row * 6 + col] &= mask;
                    pad_lcd_bitmap[row * 6 + col] |= (~img->color[k] << (8 - sh)) & ~mask;
                    pad_lcd_bitmap[row * 6 + col] |= (~img->color[k + 1] >> sh) & ~mask;
                }
            }
            col = col_start + img->bpr;
            k = (row - y) * img->bpr + img->bpr - 1;
            if ((unsigned int)col < 6) {
                unsigned char mask = (0xff >> sh) | (img->mask[k] << (8 - sh));

                pad_lcd_bitmap[row * 6 + col] &= mask;
                pad_lcd_bitmap[row * 6 + col] |= (~img->color[k] << (8 - sh)) & ~mask;
            }
        }
    }
}

/*
 * Unused slots forget their last code. The first slot in use drives the LCD; a new event on it
 * re-arms the handlers. Handlers get the same-code repeat count; nonzero result stops handling.
 */
/* 0x001437b0 */
void input_event_dispatch(PadLcdEvent *ev)
{
    int i;
    int seen;
    int code;
    McSlot *slot;

    if (pad_lcd_inited == 0) {
        {
            int *p = pad_lcd_counts;
            int *codes = pad_lcd_codes;
            for (i = 0; i < 24; i++) {
                *p++ = 0;
                *codes++ = -1;
            }
        }
        pad_lcd_inited = 1;
    }
    seen = 0;
    slot = mc_slots;
    for (i = 0; i < 24; i++, slot++) {
        if (!(slot->flags & 2)) {
            pad_lcd_counts[i] = 0;
            pad_lcd_codes[i] = -1;
        } else if (!seen) {
            seen = 1;
            if (i == ev->slot)
                pad_lcd_result = 0;
        }
    }
    if (pad_lcd_result == 0) {
        if (ev->kind == 3 || ev->kind == 0) {
            code = ev->code;
            if (pad_lcd_codes[ev->slot] != code)
                pad_lcd_counts[ev->slot] = 0;
            else
                pad_lcd_counts[ev->slot]++;
            pad_lcd_codes[ev->slot] = code;
            pad_lcd_result = pad_lcd_handlers[code](pad_lcd_bitmap, ev, &pad_lcd_counts[ev->slot]);
        }
    }
}

/* Alternates two pictures with the beat count; returns 1 on a beat. */
/* 0x00143960 */
int pad_lcd_beat_anim(int bitmap, int ev)
{
    if (beat_grid_hit(0)) {
        if (beat_clock_get_haku() & 1)
            bitmap48x32_apply(ev, pad_lcd_img_beat1);
        else
            bitmap48x32_apply(ev, pad_lcd_img_beat0);
        return 1;
    }
    return 0;
}

/* Fixed picture on the code's first arrival (returns 1 then). */
/* 0x001439c8 */
int pad_lcd_show_once(int bitmap, int ev, int *count)
{
    if (*count == 0) {
        bitmap48x32_apply(ev, pad_lcd_img_once);
        return 1;
    }
    return 0;
}

/* Beat pictures; never stops handling (returns 0). */
/* 0x00143a00 */
int pad_lcd_beat_anim_repeat(int bitmap, int ev)
{
    pad_lcd_beat_anim(bitmap, ev);
    return 0;
}
