/* Projection, clip range, clear colour, debug console, model overrides. */

#ifndef REZ_NJ_VIEW_H
#define REZ_NJ_VIEW_H

struct NjViewDesc;

extern float nj_cnk_override_color[4];

void nj_system_reset(void);
void nj_set_clear_color(unsigned int col);
void nj_view_exit(void);
void nj_profile_get_timing(void);
void njSetPerspective(int ang);
void njSetAspect(float sx, float sy);
void nj_console_close(void);
void nj_cnk_set_override_mode(int mode);
void nj_cnk_set_override_flags(int flags);
void nj_cnk_set_override_strip_flags(int and_mask, int or_bits);
void nj_cnk_set_override_color(float *col);

#endif /* REZ_NJ_VIEW_H */
