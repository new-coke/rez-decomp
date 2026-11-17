/* Layer-change lock-on target and its letter effect. */

#ifndef REZ_CHANGE_SYMBOL_H
#define REZ_CHANGE_SYMBOL_H

struct Vec3;

void change_symbol_params_register(void);
void change_symbol_model_draw(void);
void cs_eff_release(void);
int cs_eff_create(void *parent);
void cs_eff_set_state(int *p, int state);
void cs_eff_set_origin(char *w, struct Vec3 *pos);
void change_symbol_draw_line_quad(float fa, float fb, float *vtx, float *start, float *end);
void change_symbol_update_weights(char *w, int mode);
void change_symbol_follow_view(char *w);
void change_symbol_release(char *w);
void *change_symbol_create(int spawn);
void change_symbol_rotate_yxz(int *p);

#endif /* REZ_CHANGE_SYMBOL_H */
