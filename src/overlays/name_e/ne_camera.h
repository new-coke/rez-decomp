/* Name entry screen: camera. */

#ifndef REZ_NAME_E_NE_CAMERA_H
#define REZ_NAME_E_NE_CAMERA_H

#include "types_common.h"
#include "types_overlays.h"

struct NeCam;

void demo_camera_node_init(void);
void demo_camera_node_update(Camera *out, Cam *w);
void demo_camera_node_release(void);
void demo_camera_init(struct NeCam *w, int screen);
void demo_camera_release(struct NeCam *w);
void demo_camera_exec(struct NeCam *w);
void demo_camera_disp(void);
void *demo_camera_create(void *parent, int screen);

#endif /* REZ_NAME_E_NE_CAMERA_H */
