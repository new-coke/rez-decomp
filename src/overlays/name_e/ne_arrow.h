/* Name entry screen: cursor arrows. */

#ifndef REZ_NAME_E_NE_ARROW_H
#define REZ_NAME_E_NE_ARROW_H

#include "name_e.h"

void ne_arrow_init(NeArrow *w);
void ne_arrow_release(NeArrow *w);
void ne_arrow_exec(NeArrow *w);
void ne_arrow_disp(NeArrow *w);
void *ne_arrow_create(void *parent);
void ne_arrow_set_enabled(NeArrow *w, int i, int on);
void ne_arrow_light(NeArrow *w, int i);

#endif /* REZ_NAME_E_NE_ARROW_H */
