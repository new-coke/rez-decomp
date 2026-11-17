/* Thrones armour coming off. */

#ifndef REZ_COMMON_EN_THRONES_ARMOR_DIE_H
#define REZ_COMMON_EN_THRONES_ARMOR_DIE_H

#include "types_overlays.h"

struct EnThronesArmorDieWork;
struct EnThronesArmorDie2Work;

void en_thrones_armor_die_vars_register(void);
void en_thrones_armor_die_vars_unregister(void);
void en_thrones_armor_die_init(struct EnThronesArmorDieWork *w, EnThronesArmorDieArg *prm);
void en_thrones_armor_die_release(struct EnThronesArmorDieWork *w);
void en_thrones_armor_die_move_piece(void *w, EnThronesArmorPiece *p, float drag, float spin_drag);
void en_thrones_armor_die_draw_piece(struct EnThronesArmorDieWork *work, EnThronesArmorPiece *piece);
void en_thrones_armor_die_fall(struct EnThronesArmorDieWork *w);
void en_thrones_armor_die_draw_pieces(struct EnThronesArmorDieWork *w);
void en_thrones_armor_die_exec(struct EnThronesArmorDieWork *w);
void en_thrones_armor_die_disp(struct EnThronesArmorDieWork *w);
void *en_thrones_armor_die_create(void *parent, void *prm);
void en_thrones_armor_die2_init(struct EnThronesArmorDie2Work *w, EnThronesArmorDieArg *prm);
void en_thrones_armor_die2_release(struct EnThronesArmorDie2Work *w);
void en_thrones_armor_die2_exec(struct EnThronesArmorDie2Work *w);
void en_thrones_armor_die2_disp(struct EnThronesArmorDie2Work *w);
void *en_thrones_armor_die2_create(void *parent, void *prm);
void en_thrones_armor_die_rotate_yxz(int *ang);

#endif /* REZ_COMMON_EN_THRONES_ARMOR_DIE_H */
