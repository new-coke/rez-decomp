/* Score ranking screen: a board. */

#ifndef REZ_RANKING_RANKING_BOX_H
#define REZ_RANKING_RANKING_BOX_H

#include "ranking.h"

int box_node_cb(unsigned int *flags, int node_idx, Box *w);
void box_init(Box *w, int page, int table);
void box_release(Box *w);
void box_exec(Box *w);
void box_disp(Box *w);
Box *box_create(void *parent, int page, int table);

#endif /* REZ_RANKING_RANKING_BOX_H */
