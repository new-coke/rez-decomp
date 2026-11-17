/* Debug GUI menus. */

#ifndef REZ_GW_MENU_H
#define REZ_GW_MENU_H

#include "types_gw.h"

struct GwItem;
struct GwItemDef;
struct GwPop;
struct GwWin;

void gw_menu_set_all_values(struct GwItem *list, int val);
struct GwItem *gw_menu_get_nth(struct GwItem *list, int n);
struct GwItem *gw_menu_find_path(struct GwItem *list, char *path);
void gw_menu_free_all(struct GwItem *list);
struct GwItem *gw_menu_remove_path(struct GwItem *list, char *path);
struct GwItem *gw_menu_append(struct GwItem *list, struct GwItem *item);
struct GwItem *gw_menu_build(struct GwItem *list, struct GwItemDef *def);
struct GwItem *gw_item_set_submenu(struct GwItem *item, struct GwItem *sub);
int gw_item_is_checked(struct GwItem *item);
void gw_item_set_visible(struct GwItem *item, int show);
int gw_item_set_value(struct GwItem *item, int val);
int gw_item_get_value(struct GwItem *item);
struct GwItem *gw_item_get_submenu(struct GwItem *item);
struct GwItem *gw_menu_create_submenu(char *name, struct GwItem *sub);
struct GwItem *gw_item_alloc(char *name, short type, short flags, GwItemFn cb, int data);
void gw_menu_update_input(void);
int gw_menu_close(struct GwPop *menu);
struct GwPop *gw_menu_popup_create(struct GwItem *list, char *name, int x, int y, struct GwPop *parent);

#endif /* REZ_GW_MENU_H */
