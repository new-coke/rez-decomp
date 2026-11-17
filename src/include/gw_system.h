/* Debug GUI start-up, frame passes, info windows. */

#ifndef REZ_GW_SYSTEM_H
#define REZ_GW_SYSTEM_H

struct GwWin;

extern int gw_input_flags;
extern int gw_input_keys[];

void gw_next_window_pos(int *x, int *y);
int gw_change_ss_mode(int mode);
int gw_change_tex_filter(int filter);
void gw_get_cell_scale(float *width, float *height);
int gw_get_pad_port(void);
void gw_get_desktop_rect(int *rect);
void gw_get_screen_size(int *width, int *height);
int gw_get_font(void);
int gw_set_font(int font);
int gw_get_cell_size(int *cw, int *ch);
struct GwWin *gw_info_window_create(int x, int y);
int gw_update(void);
int gw_input_update(void);
void gw_exit(void);
void gw_init(int *user_conf);

#endif /* REZ_GW_SYSTEM_H */
