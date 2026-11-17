/* Debug GUI scroll bars. */

#ifndef REZ_GW_SCROLLBAR_H
#define REZ_GW_SCROLLBAR_H

struct GwSb;
struct GwRange;
struct GwWin;
struct GwBtn;

int gw_scrollbar_get_orient(struct GwSb *sb);
int gw_scrollbar_get_track(struct GwSb *sb);
void gw_scrollbar_set_enabled(struct GwSb *sb, int on);
void gw_range_set(float *rng, float val);
float gw_scrollbar_step_up(struct GwSb *sb);
float gw_scrollbar_step_down(struct GwSb *sb);
float gw_scrollbar_set_max(struct GwSb *sb, float val);
float gw_scrollbar_get_val(struct GwSb *sb);
float gw_scrollbar_set_val(struct GwSb *sb, float val);
int gw_scrollbar_set_pos(struct GwSb *sb, int val);
void gw_scrollbar_wheel(struct GwSb *sb);
void gw_scrollbar_keys(struct GwSb *sb);
struct GwSb *gw_scrollbar_create(int orient, struct GwRange *init, int x, int y, int width, int height, struct GwWin *parent);

#endif /* REZ_GW_SCROLLBAR_H */
