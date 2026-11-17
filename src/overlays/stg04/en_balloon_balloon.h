/* Balloon break-up, Area 4. */

#ifndef REZ_STG04_EN_BALLOON_BALLOON_H
#define REZ_STG04_EN_BALLOON_BALLOON_H

#include "stg04.h"
#include "types_common.h"
#include "types_overlays.h"

void en_balloon_balloon_vars_register(void);
void en_balloon_balloon_vars_unregister(void);
int en_balloon_balloon_count_edges(int *lst);
void en_balloon_balloon_init_edges(BalloonBurstWork *w, int cache, Vec3 *pos, void *src);
void en_balloon_balloon_free(BalloonBurstWork *w);
void en_balloon_balloon_exec(BalloonBurstWork *w);
void en_balloon_balloon_disp(BalloonBurstWork *w);
void en_balloon_balloon_init(BalloonBurstWork *w, Vec3 *pos, void *src, unsigned int color);
void en_balloon_balloon_release(BalloonBurstWork *w);
BalloonBurstWork *en_balloon_balloon_create(void *parent, Vec3 *pos, void *src, unsigned int color);
int balloon_ctrlbar_eye_outside(BalloonCtrlbarWork *w);
void balloon_ctrlbar_state_fall(BalloonCtrlbarWork *w);
void balloon_ctrlbar_exec(BalloonCtrlbarWork *w);
void balloon_ctrlbar_disp(BalloonCtrlbarWork *w);
void balloon_ctrlbar_init(BalloonCtrlbarWork *w, Vec3 *pos, int *ang);
void balloon_ctrlbar_release(BalloonCtrlbarWork *w);
BalloonCtrlbarWork *balloon_ctrlbar_create(void *parent, Vec3 *pos, int *ang);
Shatter *en_balloon_balloon_spawn_debris(void *obj, int piece);
void en_balloon_balloon_nodes_collect(void);
void en_balloon_balloon_rotate_yxz(int *ang);

#endif /* REZ_STG04_EN_BALLOON_BALLOON_H */
