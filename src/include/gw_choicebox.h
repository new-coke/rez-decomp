/* Debug GUI drop-down choice box. */

#ifndef REZ_GW_CHOICEBOX_H
#define REZ_GW_CHOICEBOX_H

struct GwChoiceMenu;
struct GwWin;
struct GwListNode;
struct GwChoice;

void gw_choicebox_set_sel(struct GwChoiceMenu *box, int i);
void gw_choicebox_set_callback(struct GwChoice *box, int cb);
void gw_choicebox_add_item(struct GwChoice *box, char *str);
struct GwChoice *gw_choicebox_create(char *name, int x, int y, int width, int height, void *parent);

#endif /* REZ_GW_CHOICEBOX_H */
