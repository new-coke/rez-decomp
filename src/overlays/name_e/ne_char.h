/* Name entry screen: an entered letter. */

#ifndef REZ_NAME_E_NE_CHAR_H
#define REZ_NAME_E_NE_CHAR_H

#include "name_e.h"

void ne_char_init(NeChar *w, NeCharArg *prm);
void ne_char_release(NeChar *w);
void ne_char_exec_dismiss(NeChar *w);
void ne_char_exec(NeChar *w);
void ne_char_disp(NeChar *w);
void *ne_char_create(void *parent, NeCharArg *prm);
void ne_char_dismiss(NeChar *w);
void ne_char_rotate_yxz(int *ang);

#endif /* REZ_NAME_E_NE_CHAR_H */
