/* Option menu camera. */

#ifndef REZ_TITLE_TITLE_OPTION_CAMERA_H
#define REZ_TITLE_TITLE_OPTION_CAMERA_H

#include "types_common.h"
#include "title_shared.h"

void option_camera_node_init(void);
void option_camera_node_update(Camera *out, CameraWork *w);
void option_camera_node_release(void);
void option_camera_init(CameraWork *w);
void option_camera_release(CameraWork *w);
void option_camera_exec(CameraWork *w);
void option_camera_disp(void);
void *option_camera_create(void *parent);
void option_camera_play(CameraWork *w);

#endif /* REZ_TITLE_TITLE_OPTION_CAMERA_H */
