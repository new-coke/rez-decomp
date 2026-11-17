/* Area choose screen: the player model. */

#ifndef REZ_AREACHO_AC_PLAYER_H
#define REZ_AREACHO_AC_PLAYER_H

#include "areacho.h"

void ac_player_init(AcPlayer *w);
void ac_player_release(void);
void ac_player_exec(AcPlayer *w);
void ac_player_disp(AcPlayer *w);
void *ac_player_create(void *parent);
void ac_player_play(AcPlayer *w);
void ac_player_rotate_yxz(int *ang);

#endif /* REZ_AREACHO_AC_PLAYER_H */
