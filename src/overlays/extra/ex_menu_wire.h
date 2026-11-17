/* Beyond menu: page frame lines. */

#ifndef REZ_EXTRA_EX_MENU_WIRE_H
#define REZ_EXTRA_EX_MENU_WIRE_H

#include "extra_shared.h"

void ex_menu_wire_init(ExMenuWire *w, int id);
void ex_menu_wire_release(void);
void ex_menu_wire_exec(ExMenuWire *w);
void ex_menu_wire_disp(ExMenuWire *w);
void *ex_menu_wire_create(void *parent, int id);
void ex_menu_wire_close(ExMenuWire *w);

#endif /* REZ_EXTRA_EX_MENU_WIRE_H */
