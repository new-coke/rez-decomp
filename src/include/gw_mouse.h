/* Debug GUI pointer. */

#ifndef REZ_GW_MOUSE_H
#define REZ_GW_MOUSE_H
#include "types_gw.h"

struct GwWin;
struct GwMouse;

extern int gw_grabbed_win;
extern int gw_mouse_stat;

int gw_win_mouse_local(struct GwWin *win, int *rx, int *ry);
int gw_mouse_get_wheel(void);
int gw_mouse_get_dclick(void);
int gw_mouse_get_repeat(void);
int gw_mouse_get_release(void);
int gw_mouse_get_press(void);
int gw_mouse_get_on(void);
int gw_mouse_get_cursor(void);
int gw_mouse_get_stat(void);
int gw_mouse_moved(void);
int gw_mouse_get_move(int *dx, int *dy);
void gw_mouse_get_prev_pos(int *x, int *y);
void gw_mouse_get_pos(int *x, int *y);
void gw_mouse_read_pad_move(struct GwMouse *ms, unsigned char *pad);
void gw_mouse_update(struct GwMouse *ms);
void gw_mouse_draw(struct GwMouse *ms, float scale);
void gw_mouse_init(int pad_port, int width, int height);
struct GwWin *gw_mouse_info_create(int x, int y);

extern GwMouse gw_mouse_state;

#endif /* REZ_GW_MOUSE_H */
