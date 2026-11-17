/* Venus boss hex shell wall. */

#ifndef REZ_B_VENUS_VENUS_SHELL_H
#define REZ_B_VENUS_VENUS_SHELL_H

#include "types_common.h"
#include "types_overlays.h"

struct VenusLineVtx;
struct VenusVecPair;
struct VenusPiece;
struct VenusHwallWork;
struct VenusHwallBWork;
struct VenusShellBWork;

void venus_shell_vars_add(void);
void venus_shell_vars_remove(void);
unsigned int venus_hwall_wall_col(int i, float alpha);
void venus_hwall_cell_pos(int i, int j, int n, int ny, Vec3 *out, int *ang, float extra_r);
void venus_hwall_pos_cell(Vec3 *pos, int n, int *i, int *j);
int venus_hwall_cell_dir(int x, int y, int cx, int cy, int nx, int ny);
void venus_hwall_hex_pts(Vec3 *v);
void venus_hwall_hex_lines(BossV4 *verts);
void venus_hwall_cells_draw(void *work);
int venus_hwall_grid_build(int nx, int ny, Vec3 *out);
void venus_hwall_lines_draw(struct VenusLineVtx *v, int n);
int venus_hwall_grid_line_num(int nx, int ny);
struct VenusVecPair *venus_hwall_strip_pairs(struct VenusVecPair *pair, Vec3 *src, int n);
struct VenusVecPair *venus_hwall_seg_pairs(struct VenusVecPair *pair, Vec3 *src, int n);
void venus_hwall_grid_pairs(struct VenusVecPair *pair, Vec3 *src, int nx, int ny);
int venus_hwall_cell_step(short *grid, int *px, int *py, int nc, int nr, int dir);
void venus_hwall_cell_add(struct VenusHwallWork *w, int i, int j);
void venus_hwall_cell_break(struct VenusHwallWork *w, char *cell);
int venus_hwall_cell_move(struct VenusHwallWork *w, signed char *cell, int dir);
int venus_hwall_cells_slide(int *work);
void venus_hwall_cell_pose(char *w);
void venus_hwall_lines_partial(Vec3 *strip, Vec3 *out, float t);
int venus_hwall_in_front(struct VenusHwallWork *w, signed char *cell, int len);
int venus_hwall_cell_start_attack(int owner, int *grp);
int venus_hwall_cells_attack(int *work);
int venus_hwall_pick_action(struct VenusHwallWork *w);
int venus_hwall_cells_hit(int *work);
void venus_hwall_exec(void *work);
void venus_hwall_disp(void *w);
void venus_hwall_disp_appear(void *work);
int venus_hwall_appear_step(void *work);
void venus_hwall_exec_appear(void *w);
void venus_hwall_appear_start(struct VenusHwallWork *w);
void venus_hwall_release(struct VenusHwallWork *w);
void *venus_hwall_create(void *parent, Vec3 *pos, int *parts);
void venus_hwall_b_piece_init(int pts, struct VenusPiece *piece, int idx, float spin, float spd, float spd_rnd, float sec, float sec_rnd);
int venus_hwall_b_pieces_build(int pts, struct VenusPiece *piece, int nx, int ny, float spin, float spd, float spd_rnd, float sec, float sec_rnd);
void venus_hwall_b_exec(struct VenusHwallBWork *w);
void venus_hwall_b_disp(struct VenusHwallBWork *w);
void venus_hwall_b_release(struct VenusHwallBWork *w);
int *venus_hwall_b_create(int parent, int pts, int nx, int ny);
void venus_hwall_shatter(int parent, char *wall);
void venus_shell_b_exec(struct VenusShellBWork *w);
void venus_shell_b_disp(struct VenusShellBWork *w);
int *venus_shell_b_create(int *w, Vec3 *pos, Vec3 *ang, int model);
void venus_shell_rotate_yxz(int *ang);
void boss_se_play(int id, int lock, int pos);
void boss_vol_play(int id, int arg);
void boss_vol_stop(int id);
void boss_bgm_exit_cb(BgmMan *bgm);
BgmMan *boss_bgm_start(void *parent);
void boss_adx_request(void);
void boss_bgm_switch(int cb, int cb_arg, int idx);
void venus_cue_cb(void);
void venus_cue(int idx, int id, int delay);
void venus_cue_fn_cb(void);
void venus_cue_set(int idx, int id, void (*fn)(int arg), int arg);
void boss_pcm_load(int i);
void boss_pcm_start(void);

#endif /* REZ_B_VENUS_VENUS_SHELL_H */
