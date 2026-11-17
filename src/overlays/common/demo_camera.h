/* Demo camera of the area select screens. */

#ifndef REZ_COMMON_DEMO_CAMERA_H
#define REZ_COMMON_DEMO_CAMERA_H

#include "types_common.h"

struct DemoCamera;

void demo_camera_node_init(void);
void demo_camera_node_update(Camera *out, struct DemoCamera *w);
void demo_camera_node_release(void);
void demo_camera_init(struct DemoCamera *w);
void demo_camera_release(struct DemoCamera *w);
void demo_camera_exec(struct DemoCamera *w);
void demo_camera_disp(void);
void *demo_camera_create(void *parent);
void demo_camera_play(struct DemoCamera *w);
int demo_camera_is_done(struct DemoCamera *w);
float demo_camera_get_remaining(struct DemoCamera *w);

#endif /* REZ_COMMON_DEMO_CAMERA_H */
