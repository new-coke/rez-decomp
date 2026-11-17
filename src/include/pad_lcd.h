/* Peripheral LCD bitmaps, LCD event handlers. */

#ifndef REZ_PAD_LCD_H
#define REZ_PAD_LCD_H

#include "types_game.h"

int bitmap48x32_get_pixel(int x, int y);
void bitmap48x32_set_pixel(int x, int y, unsigned char val);
int bitmap_get_bit(unsigned char *bits, int bpr, int height, int x, int y);
void bitmap_set_bit(unsigned char *bits, int bpr, int height, int x, int y, unsigned char val);
void bitmap48x32_invert(void);
void bitmap48x32_blit_masked(int x, int y, PadLcdImage *img);
void bitmap48x32_blit_masked_inv(int x, int y, PadLcdImage *img);
int pad_lcd_beat_anim(int bitmap, int ev);
int pad_lcd_show_once(int bitmap, int ev, int *count);
int pad_lcd_beat_anim_repeat(int bitmap, int ev);

#endif /* REZ_PAD_LCD_H */
