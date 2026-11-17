/* Screen geometry, viewport, capture texture, mosaic, fade overlay. */

#ifndef REZ_SCREEN_H
#define REZ_SCREEN_H

extern float screen_cx;
extern int screen_effect_flags;
extern float screen_top;

void screen_init(void);
void screen_menu_init(void);
void screen_set_size(float width, float height);
int screen_get_pass_cnt(void);
int screen_swap_render(int on);
int screen_swap_render_back(int on);
void screen_set_fade_alpha(int val);
void screen_set_fade_color(int col);
void screen_set_margin(int val);
float screen_get_mosaic_scale(void);
void screen_frame_hook(void);
void screen_redraw(void);
int screen_get_effect_mode(void);
void screen_apply_scale_frame(void);
void screen_enable(void);
void screen_update(void);
char *screen_get_tex_list(void);
int screen_get_surface_addr(void);

#endif /* REZ_SCREEN_H */
