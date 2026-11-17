/* Debug GUI 'Peripheral Info' window. */

#ifndef REZ_GWU_PERIINFO_H
#define REZ_GWU_PERIINFO_H
#include "types_gw.h"

struct GwBtn;
struct GwWin;

struct GwWin *gw_periinfo_window_create(int x, int y, struct GwWin *parent);

extern int gw_periinfo_ports[];        /* pad port ids */
extern char *gw_periinfo_port_names[];
extern char *gw_periinfo_type_names[];      /* controller names */

#endif /* REZ_GWU_PERIINFO_H */
