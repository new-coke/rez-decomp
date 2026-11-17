/* Ending: camera and rings. */

#ifndef REZ_ENDING_END_CIRCLE_H
#define REZ_ENDING_END_CIRCLE_H

#include "types_common.h"
#include "ending_shared.h"

struct DemoCamera;
struct RingWork;

void demo_camera_node_init(void);
void demo_camera_node_update(Camera *out, struct DemoCamera *w);
void demo_camera_node_release(void);
void demo_camera_init(struct DemoCamera *w);
void demo_camera_release(struct DemoCamera *w);
void demo_camera_exec(struct DemoCamera *w);
void demo_camera_disp(void);
void *demo_camera_create(void *parent);
int demo_camera_is_done(struct DemoCamera *w);
float demo_camera_get_remaining(struct DemoCamera *w);
void end_circle_init(struct RingWork *w, RingArg *prm);
void end_circle_release(void);
void end_circle_exec(struct RingWork *w);
void end_circle_disp(struct RingWork *w);
void *end_circle_create(void *owner, RingArg *prm);
void end_circle_rotate_yxz(int *ang);

#endif /* REZ_ENDING_END_CIRCLE_H */
