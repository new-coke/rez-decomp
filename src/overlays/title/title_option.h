/* Option menu from the title. */

#ifndef REZ_TITLE_TITLE_OPTION_H
#define REZ_TITLE_TITLE_OPTION_H

#include "title_shared.h"

int option_box_create(void *menu, int item);
void option_box_close(void *box, int item);
void option_init(OptionWork *w, void *caller);
void option_release(OptionWork *w);
void option_open(OptionWork *w);
void option_navigate(OptionWork *w);
void option_autosave(OptionWork *w);
void option_ranking_data(OptionWork *w);
void option_load_data(OptionWork *w);
void option_exec(OptionWork *w);
void option_disp(OptionWork *w);
void *option_create(void *caller);
void option_select_ranking_data(OptionWork *w);
void option_select_load_data(OptionWork *w);
void option_select_default(void);
void option_select_return(OptionWork *w);
int option_get_stereo(void);
void option_set_stereo(int v);

#endif /* REZ_TITLE_TITLE_OPTION_H */
