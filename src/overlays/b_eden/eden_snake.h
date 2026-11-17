/* Eden boss snake form and its variants. */

#ifndef REZ_B_EDEN_EDEN_SNAKE_H
#define REZ_B_EDEN_EDEN_SNAKE_H

#include "b_eden.h"
#include "types_common.h"
#include "types_overlays.h"

struct EdenSnakeSeg;
struct EdenRing;
struct EdenSnakeWork;
struct EdenSnakeHumanWork;
struct EdenHumanModel;
struct EdenSnakeOrbitWork;
struct EdenSoundBeatWork;

void eden_snake_vars_add(void);
void eden_snake_vars_remove(void);
void eden_snake_ring_place(struct EdenSnakeWork *w, struct EdenSnakeSeg *src);
void eden_snake_body_follow(struct EdenSnakeWork *w);
void eden_snake_head_move(struct EdenSnakeWork *snake);
void eden_snake_place(struct EdenSnakeWork *w);
void eden_snake_exec(struct EdenSnakeWork *w);
void eden_snake_release(struct EdenSnakeWork *w);
void eden_snake_disp(struct EdenSnakeWork *w);
int *eden_snake_create(void *parent, int *arg);
void eden_snake_get_dims(struct EdenSnakeWork *w, int *nrow, int *ncell);
void eden_snake_cell_get(struct EdenSnakeWork *w, int i, int j, EdenSnakeCell **cell, int **vec);
void eden_snake_fire(struct EdenSnakeWork *w, int on, int cnt);
void eden_snake_human_tree_points(void *node, int mtx, int *cur, int *idx);
void eden_snake_human_model_draw(struct EdenHumanModel *w, Vec3 *pos, int *ang, int pts, int mtx);
void eden_snake_human_circle_move(struct EdenSnakeHumanWork *w, float rad, float da, int ang, float k);
void eden_snake_human_exec(struct EdenSnakeHumanWork *w);
void eden_snake_human_release(struct EdenSnakeHumanWork *w);
void eden_snake_human_disp(struct EdenSnakeHumanWork *w);
char *eden_snake_human_create(void *parent, int arg, int kind);
void eden_snake_human_point_get(struct EdenSnakeHumanWork *w, int i, EdenHumanPt **pt, char **vec);
int eden_snake_human_is_done(struct EdenSnakeHumanWork *w);
void eden_snake_orbit_ring_points(struct EdenRing *ring, Vec3 *out, int *ang, int n);
void eden_snake_orbit_update(struct EdenSnakeOrbitWork *orbit);
void eden_snake_orbit_exec(struct EdenSnakeOrbitWork *w);
void eden_snake_orbit_release(void);
void eden_snake_orbit_disp(struct EdenSnakeOrbitWork *w);
char *eden_snake_orbit_create(void *parent, int arg);
void eden_snake_orbit_point_get(struct EdenSnakeOrbitWork *w, int i, char **pt, char **vec);
void eden_snake_orbit_set_end(struct EdenSnakeOrbitWork *w);
void eden_snake_orbit_fire(struct EdenSnakeOrbitWork *w);
void eden_snake_rotate_yxz(int *ang);
void boss_se_play(int id, int lock, int pos);
void boss_vol_play(int id, int arg);
void boss_vol_stop(int id);
void boss_bgm_exit_cb(BgmMan *bgm);
BgmMan *boss_bgm_start(void *parent);
void boss_adx_request(void);
void boss_bgm_switch(int cb, int cb_arg, int idx);
void eden_sound_bgm_start_cb(void);
void eden_sound_bgm_start(int i);
void eden_sound_seq_load_cb(int arg);
void eden_sound_seq_load(void);
void eden_sound_tempo_set(int i);
void eden_sound_beat_exec(struct EdenSoundBeatWork *w);
int *eden_sound_beat_create(void *parent);
CmTexAnimeWork *eden_tex_anime_create(int i);
void eden_tex_anime_set_visible(int on);
void eden_tex_anime_set_play(int on);
void eden_tex_anime_stop(void);

#endif /* REZ_B_EDEN_EDEN_SNAKE_H */
