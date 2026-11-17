/* Virtues effects, Area 5. */

#ifndef REZ_STG05_EN_VTS_CMD_DMG_EFF_H
#define REZ_STG05_EN_VTS_CMD_DMG_EFF_H

#include "types_common.h"
#include "stg05.h"
#include "types_overlays.h"

struct EnVtsCmdDmgEffWork;
struct EnVtsCmdDmgEffArg;
struct EnVirtuesLaserWork;
struct EnVirtuesLaserKind;

void en_vts_cmd_dmg_eff_vars_register(void);
void en_vts_cmd_dmg_eff_vars_unregister(void);
void en_vts_cmd_dmg_eff_init(struct EnVtsCmdDmgEffWork *w, struct EnVtsCmdDmgEffArg *prm);
void en_vts_cmd_dmg_eff_release(struct EnVtsCmdDmgEffWork *w);
void en_vts_cmd_dmg_eff_exec(struct EnVtsCmdDmgEffWork *w);
void en_vts_cmd_dmg_eff_disp(struct EnVtsCmdDmgEffWork *w);
void *en_vts_cmd_dmg_eff_create(void *parent, void *prm);
void en_virtues_laser_vars_register(void);
void en_virtues_laser_vars_unregister(void);
void en_virtues_laser_calc_hexagon(struct EnVirtuesLaserWork *w);
void en_virtues_laser_place_on_path(Vec3 *pos, int *ang, EnVirtuesBaseArg *prm);
void en_virtues_laser_place(struct EnVirtuesLaserWork *w);
void en_virtues_laser_init(struct EnVirtuesLaserWork *w, struct EnVirtuesLaserKind *k);
void en_virtues_laser_release(struct EnVirtuesLaserWork *w);
void en_virtues_laser_exec(struct EnVirtuesLaserWork *w);
void en_virtues_laser_disp(struct EnVirtuesLaserWork *w);
void *en_virtues_laser_create(void *w, EnVirtuesBaseArg *prm);
void en_virtues_laser_detach(EnVirtuesLaserTarget *w);
void en_virtues_laser_draw_beam(LineStyle *lens, Vec3 *pts, int n, int lit);

#endif /* REZ_STG05_EN_VTS_CMD_DMG_EFF_H */
