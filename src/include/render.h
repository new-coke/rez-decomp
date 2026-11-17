/* Frame renderer: display lists, start/flip, GS kick, interrupt. */

#ifndef REZ_RENDER_H
#define REZ_RENDER_H

int render_get_draw_slot(void);
void render_init_all(void);
void render_init(void);
void render_vars_register(void);
int render_submit_frame(int mode, int wait);
void render_flip(void);
int RenderSetCurrentDisplayList(unsigned list);
float render_get_packet_load(void);
float render_debug_gauges(float x, float y, float z, float width, float height);
void render_kick_display_list(void);
int render_gs_interrupt(void);
float render_get_frame_scale(void);
void render_set_frame_regs(void);
void render_fill_screen_background(void);

#endif /* REZ_RENDER_H */
