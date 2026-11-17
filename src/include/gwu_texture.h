/* Debug GUI texture viewer (desktop menu). */

#ifndef REZ_GWU_TEXTURE_H
#define REZ_GWU_TEXTURE_H
#include "types_gw.h"

struct GwTexWin;
struct GwWin;
struct GwBtn;

void gw_texview_install(void);

extern float gw_texview_zoom_table[];
extern GwTexWin gw_texview_state;

#endif /* REZ_GWU_TEXTURE_H */
