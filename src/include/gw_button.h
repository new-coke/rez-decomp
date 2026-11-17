/* Debug GUI buttons. */

#ifndef REZ_GW_BUTTON_H
#define REZ_GW_BUTTON_H

#include "types_gw.h"

struct GwWin;
struct GwBtn;

void gw_toggle_button_set(struct GwBtn *btn, int on);
int gw_toggle_button_get(struct GwBtn *btn);
struct GwBtn *gw_toggle_button_create(GwBtnFn cb, char *name, int x, int y, int width, int height, struct GwWin *parent);
struct GwBtn *gw_icon_button_create(GwBtnFn cb, int icon, float scale, int x, int y, int width, int height, struct GwWin *parent);
void gw_button_set_callback(struct GwBtn *btn, GwBtnFn cb);
void gw_button_set_enabled(struct GwBtn *btn, int enable);
void gw_icon_button_update(struct GwBtn *btn);
struct GwBtn *gw_push_button_create(GwBtnFn cb, char *name, int x, int y, int width, int height, struct GwWin *parent);
void gw_titlebar_button_pos(struct GwWin *btn, struct GwWin *parent, int *px, int *py);
struct GwBtn *gw_close_box_create(struct GwWin *parent);
struct GwBtn *gw_maximize_box_create(struct GwWin *parent);
struct GwBtn *gw_minimize_box_create(struct GwWin *parent);

#endif /* REZ_GW_BUTTON_H */
