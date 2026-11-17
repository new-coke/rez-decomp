/* Virtues walls, Area 5. */

#ifndef REZ_STG05_VIRTUES_WALL_H
#define REZ_STG05_VIRTUES_WALL_H

#include "stg05.h"

struct VirtuesWallArg;

void virtues_wall_vars_register(void);
void virtues_wall_vars_unregister(void);
int virtues_wall_get_init_arg(void);
int virtues_wall_vars_init(int *prm);
int virtues_wall_vars_release(void);
void virtues_wall_models_load(void);
void virtues_wall_models_free(void);
void virtues_wall_init(void *obj, VirtuesWallWork *w, int place, struct VirtuesWallArg *prm);
void virtues_wall_release(void *obj, VirtuesWallWork *w);
void virtues_wall_state_wait(void);
void virtues_wall_state_touch(void *obj, VirtuesWallWork *w);
void virtues_wall_state_end(void);
void virtues_wall_update(void *obj, VirtuesWallWork *w);
void virtues_wall_exec_appear(void *obj, VirtuesWallWork *w);
void virtues_wall_exec_normal(void *obj, VirtuesWallWork *w);
void virtues_wall_exec_leave(void *obj, VirtuesWallWork *w);
void virtues_wall_exec_destroyed(void *obj, VirtuesWallWork *w);
void virtues_wall_pre_exec(void);
void virtues_wall_exec(void *obj, VirtuesWallWork *w);
void virtues_wall_post_exec(void *obj, VirtuesWallWork *w);
void virtues_wall_disp_reflect(void *obj, VirtuesWallWork *w);
void virtues_wall_disp_body(void *obj, VirtuesWallWork *w);
void virtues_wall_disp(void *obj, VirtuesWallWork *w);
void *virtues_wall_create(EnVirtuesBaseWork *w, int first, int i, float y, float height);
void virtues_wall_destroy(void *obj);
void virtues_wall_set_height(void *obj, int on, float y);
void virtues_wall_rotate_yxz(int *ang);

#endif /* REZ_STG05_VIRTUES_WALL_H */
