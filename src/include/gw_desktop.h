/* Debug GUI root window and menus. */

#ifndef REZ_GW_DESKTOP_H
#define REZ_GW_DESKTOP_H

#include "types_gw.h"

struct GwItem;
struct GwPop;
struct GwWin;

struct GwPop *gw_popup_menu_open(struct GwItem *list, char *name, int x, int y);
struct GwItem *gw_desktop_get_menu(void);
void gw_desktop_set_left_click(GwFn cb);
void gw_desktop_init(int width, int height);
void gw_desktop_shutdown(void);

#endif /* REZ_GW_DESKTOP_H */
