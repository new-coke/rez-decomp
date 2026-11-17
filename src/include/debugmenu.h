/* Debug menu items and the Kpro Menu popup. */

#ifndef REZ_DEBUGMENU_H
#define REZ_DEBUGMENU_H

void info_default(void);
void menu_item_set_value(char *name, char *item_name, int val);
/* cb: GwItemFn (item, window arg, data); most items ignore all three. */
void menu_item_add(char *parent, char *name, void (*cb)(), int data, int mode);
void menu_item_remove(char *name, char *path);
int kpro_menu_toggle(int root);

extern char *info_menu_name;    /* "Info": submenu of the debug info windows */

#endif /* REZ_DEBUGMENU_H */
