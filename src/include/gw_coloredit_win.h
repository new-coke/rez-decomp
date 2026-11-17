/* Debug GUI colour editor window. */

#ifndef REZ_GW_COLOREDIT_WIN_H
#define REZ_GW_COLOREDIT_WIN_H

#include "types_gw.h"

struct GwWin;

void gw_coloredit_set_change_callback(struct GwWin *win, GwColorFn cb);
unsigned gw_coloredit_get_color(struct GwWin *win);
struct GwWin *gw_coloredit_window_create(int mode, unsigned color, int x, int y, int width, int height, struct GwWin *parent);

#endif /* REZ_GW_COLOREDIT_WIN_H */
