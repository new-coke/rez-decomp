/* Scene sequence of a stage area. */

#ifndef REZ_MAP_MANAGER_H
#define REZ_MAP_MANAGER_H

struct CamAngWork;
struct MapManager;
struct MapPair;
struct MapModelInfo;
struct MapManagerView;
struct MapManagerDesc;
struct MapManagerView;
struct MapManagerParentCbView;
struct MapObj;
struct MapScene;

int map_manager_task_create_mode2(void *parent, int scenes, int nscene, int use_wires, int parent_cb);
int map_manager_task_create_select(void *parent, int *tables, int ntables, int nscene, int use_wires, int parent_cb);
void map_manager_start_scene(struct MapManager *w, int idx);
void map_manager_set_next_scene(struct MapManager *w, int idx);
void map_manager_request_scene(struct MapManager *w, int idx);
int map_manager_get_scene(struct MapManager *w);
int map_manager_swap_start_cb(struct MapManager *w, int cb);
int map_manager_swap_change_cb(struct MapManager *w, int cb);
void map_manager_set_work(struct MapManager *w, int work);
int map_manager_get_work(struct MapManager *w);
struct MapObj *map_manager_get_map(struct MapManager *w);
struct MapScene *map_manager_get_scenes(struct MapManager *w);
void map_manager_set_sub_task(struct MapManager *w, void *task);
int map_manager_scene_flag(struct MapManager *w);
int map_manager_get_scene_id(struct MapManager *w);
void map_obj_set_params(struct MapObj *w, int first, int second);
void vec3_clamp_length(float max, float *in, float *out);
int fourcc_equal(signed char *fcc_a, signed char *fcc_b);

#endif /* REZ_MAP_MANAGER_H */
