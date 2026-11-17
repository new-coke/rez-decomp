/* Texture force effect: surface, pass, stage data, parts, presets. */

#ifndef REZ_TEXFORCE_H
#define REZ_TEXFORCE_H

struct TexForce;
struct TfCtrl;
struct Vec3;
struct TfPart;
struct KtfPart;
struct TfScreen;
struct TfScreenTask;

void texforce_surface_create(void);
int texforce_surface_get_addr(void);
int *texforce_get_texture(void);
void texforce_enable(void);
void texforce_disable(void);
void texforce_begin(struct TexForce *w);
void texforce_finish(struct TexForce *w);
void *tfctrl_create(void *parent);
void texforce_draw_screen(void);
void texforce_load_ktf(char *data);
void texforce_unload_ktf(void);
void TextureForce(int id, struct Vec3 *pos);
void tfscreen_select(int i);

#endif /* REZ_TEXFORCE_H */
