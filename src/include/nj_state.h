/* Texture clamp and shading modes. */

#ifndef REZ_NJ_STATE_H
#define REZ_NJ_STATE_H

void nj_state_set0(void);
void nj_state_set1(void);
void nj_state_set2(void);
void nj_state_set3(void);
void nj_state_set4(void);
void nj_state_set5(void);
void nj_state_set6(void);
void nj_state_set7(void);
void nj_texture_flip_mode(void);
void nj_state_set_colors(void);
void njTextureClampMode(int mode);
void njTextureShadingMode(int mode);

#endif /* REZ_NJ_STATE_H */
