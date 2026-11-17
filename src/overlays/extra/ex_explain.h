/* Beyond menu: hint bar. */

#ifndef REZ_EXTRA_EX_EXPLAIN_H
#define REZ_EXTRA_EX_EXPLAIN_H

#include "extra_shared.h"

void ex_explain_draw_strip(int strip, float x, float y, float z);
void ex_explain_init(void);
void ex_explain_release(void);
void ex_explain_exec(void);
void ex_explain_disp(ExExplain *w);
void *ex_explain_create(void *parent);
void ex_explain_set(ExExplain *w, int hint, int adjust, int accept);

#endif /* REZ_EXTRA_EX_EXPLAIN_H */
