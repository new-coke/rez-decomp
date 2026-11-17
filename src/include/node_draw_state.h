/* Model draw state setters with save/restore. */

#ifndef REZ_NODE_DRAW_STATE_H
#define REZ_NODE_DRAW_STATE_H

struct DrawColor4;

/* draw_set_override_flags bits */
#define DRAW_COLOR_SET      0x10        /* material colour replaced by the override colour */
#define DRAW_COLOR_ADD      0x20        /* override colour added to the material colour */
#define DRAW_STRIP_OVERRIDE 0x800       /* strip flags masked and replaced */

void model_render_state_init(void);
int draw_set_override_flags(int flags);
void draw_set_strip_flags(int flags, int mask);
void draw_push_strip_flags(void);
void draw_pop_strip_flags(void);
void draw_set_override_color(float *rgba);
void draw_push_override_color(void);
void draw_pop_override_color(void);
void draw_set_blend(int src, int dst);
void draw_push_blend(void);
void draw_pop_blend(void);
int draw_set_test_mode(int mode);
int draw_set_fc_mode(int mode);
int draw_set_ztest_mode(int mode);

#endif /* REZ_NODE_DRAW_STATE_H */
