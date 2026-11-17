/* Boss movie panel. */

#ifndef REZ_COMMON_BOSS_TEXTURE_ANIME_H
#define REZ_COMMON_BOSS_TEXTURE_ANIME_H

#include "types_overlays.h"

void tex_still_create(void);
void tex_still_free(void);
void tex_anime_keep_frame(void);
void tex_anime_surface_create(CmTexAnimeWork *w);
void tex_anime_upload(CmTexAnimeWork *w, void *data);
void tex_anime_surface_free(CmTexAnimeWork *w);
void tex_anime_select_frame(CmTexAnimeWork *w);
void tex_anime_select(void);
void cm_tex_anime_draw(CmTexAnimeWork *w);
void cm_tex_anime_exec_end(CmTexAnimeWork *w);
void cm_tex_anime_exec(CmTexAnimeWork *w);
void cm_tex_anime_disp(CmTexAnimeWork *w);
void cm_tex_anime_release(CmTexAnimeWork *w);
void cm_tex_anime_exec_open(CmTexAnimeWork *w);
CmTexAnimeWork *cm_tex_anime_create(void *parent, int vol, char *name, int arg, int fsize, int nbuf, int interval);
void tex_anime_set_play(int on);
void tex_anime_set_visible(int on);
void tex_anime_set_color(unsigned int color);
void tex_anime_set_rect(float x, float y, float width, float height);
void tex_anime_stop(void);
void tex_anime_finish(void);
void tex_anime_set_limit(int limit);
void tex_anime_set_once(void);
void cm_tex_anime_color_exec(CmTexAnimeColorWork *w);
CmTexAnimeColorWork *cm_tex_anime_color_create(CmTexAnimeWork *w, unsigned int from, unsigned int to, int delay, int frames);
CmTexAnimeWork *tex_anime_start(void *parent, int *file, int arg, int limit);
void tex_anime_fade_in_out(void);

#endif /* REZ_COMMON_BOSS_TEXTURE_ANIME_H */
