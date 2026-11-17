/* Boss rush score screen. */

#ifndef REZ_STG21_BR_SCORE_H
#define REZ_STG21_BR_SCORE_H

#include "stg21.h"

void br_score_draw_number(int num, float x, float y, float z);
void br_score_count_area(BrScore *w);
void br_score_count_total(BrScore *w);
void br_score_exec(BrScore *w);
void br_score_draw_total(BrScore *w);
void br_score_disp(BrScore *w);
void br_score_init(BrScore *w);
void br_score_release(BrScore *w);
BrScore *br_score_create(void *parent);
void br_score_open(BrScore *w, int area);
int br_score_is_idle(BrScore *w);
void br_score_bg_draw_rects(int idx, unsigned int color);
void br_score_bg_exec(BrScoreBg *w);
void br_score_bg_disp(BrScoreBg *w);
void *br_score_bg_create(void *parent);
void br_score_bg_set_last(BrScoreBg *w, int area);

#endif /* REZ_STG21_BR_SCORE_H */
