/* Debug variable editor window. */

#ifndef REZ_GW_VAREDIT_WIN_H
#define REZ_GW_VAREDIT_WIN_H

#include "types_gw.h"

struct GwVarWin;
struct GwVarNode;
struct GwVar;
struct GwWin;

int gw_varedit_win_is_open(void);
int gw_varedit_win_set(int on);
void gw_varedit_win_notify(struct GwVar *it);
struct GwWin *gw_varedit_open(int root, char *title);
void gw_varedit_set_close_callback(struct GwWin *win, GwFn cb);

extern char *gw_varedit_win_kind_names[9];

#endif /* REZ_GW_VAREDIT_WIN_H */
