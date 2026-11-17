/* Title screen: attract demo and title picture. */

#ifndef REZ_TITLE_TITLE_DM_MODEL_H
#define REZ_TITLE_TITLE_DM_MODEL_H

#include "title_shared.h"
#include "types_common.h"

struct TitleDemo;
struct DemoModel;

void bg_init(BgWork *w, int sound);
void bg_release(void);
void bg_exec(BgWork *w);
void bg_disp(BgWork *w);
void *bg_create(int sound);
void bg_draw_image(int color);
int bg_is_opaque(BgWork *w);
void title_demo_init(struct TitleDemo *w);
void title_demo_release(struct TitleDemo *w);
void title_demo_exec_play(struct TitleDemo *w);
void title_demo_exec(struct TitleDemo *w);
void title_demo_disp(void);
void *title_demo_create(void *parent);
void demo_camera_node_init(void);
void demo_camera_node_update(Camera *out, CameraWork *w);
void demo_camera_node_release(void);
void demo_camera_init(CameraWork *w, int screen);
void demo_camera_release(CameraWork *w);
void demo_camera_exec(CameraWork *w);
void demo_camera_disp(void);
void *demo_camera_create(void *parent, int screen);
float demo_camera_get_remaining(CameraWork *w);
void title_demo_preload_lights(struct TitleDemo *w);
void demo_model_init(struct DemoModel *w, int screen);
void demo_model_release(struct DemoModel *w);
void demo_model_exec(struct DemoModel *w);
void demo_model_disp(struct DemoModel *w);
void *demo_model_create(void *parent, int screen);
float demo_model_get_remaining(struct DemoModel *w);

#endif /* REZ_TITLE_TITLE_DM_MODEL_H */
