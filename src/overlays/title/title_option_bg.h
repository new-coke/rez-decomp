/* Option menu background. */

#ifndef REZ_TITLE_TITLE_OPTION_BG_H
#define REZ_TITLE_TITLE_OPTION_BG_H

#include "title_shared.h"
#include "types_common.h"

void option_bg_init(OptionBgWork *w);
void option_bg_release(OptionBgWork *w);
void option_bg_exec(void);
void option_bg_disp(OptionBgWork *w);
void *option_bg_create(OptionWork *w);
int option_bg_node_cb(unsigned int *flags, int node_no, OptionBgWork *w);
void option_bg_get_anchor(OptionBgWork *w, Vec3 *out);
void option_bg_set_icons(OptionBgWork *w, int icon12, int icon14);

#endif /* REZ_TITLE_TITLE_OPTION_BG_H */
