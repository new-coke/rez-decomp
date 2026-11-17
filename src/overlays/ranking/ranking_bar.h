/* Score ranking screen: rows and paging camera. */

#ifndef REZ_RANKING_RANKING_BAR_H
#define REZ_RANKING_RANKING_BAR_H

#include "ranking.h"
#include "types_common.h"

int bar_node_cb(unsigned int *flags, int node_idx, Bar *w);
void bar_init(Bar *w, int rank, int table, Vec3 *pos, int mode);
void bar_release(Bar *w);
void bar_exec(Bar *w);
void bar_disp(Bar *w);
Bar *bar_create(Box *parent, int rank, int table, Vec3 *pos, int mode);
void bar_play(Bar *w, int mode);
void demo_camera_node_init(void);
void demo_camera_node_update(Camera *out, PageCam *w);
void demo_camera_node_release(void);
void demo_camera_init(PageCam *w);
void demo_camera_release(PageCam *w);
void demo_camera_exec(PageCam *w);
void demo_camera_disp(void);
PageCam *demo_camera_create(void *parent);
int demo_camera_get_page(PageCam *w);
void demo_camera_request_page(PageCam *w, int up);

#endif /* REZ_RANKING_RANKING_BAR_H */
