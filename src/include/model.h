/* Model objects, draw walks, model files, wire data, timed textures. */

#ifndef REZ_MODEL_H
#define REZ_MODEL_H

struct DcNode;
struct MapPart;
struct DcClip;
struct DcObjectCompiled;
struct DcObjectWires;
struct DcWalkNode;
struct DcNodeView;
struct DcNodeObjView;
struct DcObjectView;
struct DcSceneNode;
struct DcObjectTreeView;
struct DcNodeView;
struct DcNodeCbView;
struct DcNodeScaleView;
struct DcObjectView;
struct DcObjectColl;
struct DcObjectColl;
struct DcObjectView;
struct DcObjectView;
struct ModelExportRec;
struct DcNodeView;
struct ModelSnapshot;
struct ModelPartBuf;
struct ModelColor4;
struct ModelPath;
struct ModelPath;
struct ModelListNode;
struct ModelTimedLoad;
struct ModelTimedLoad;
struct DcNodePosView;

void compiled_alloc_register(void);
void model_clip_far_off(void);
void model_clip_far_on(void);
struct MapPart *dc_object_set_wire(struct MapPart *obj, int wire);
struct MapPart *dc_object_init(struct MapPart *w, struct DcNode *root, struct DcClip *clip, unsigned flags);
struct MapPart *dc_object_init_cursor(struct MapPart *w, struct DcNode *root, struct DcClip *clip, int clip_b, int clip_c, unsigned flags);
void dc_object_rebuild(struct MapPart *obj);
void dc_object_free_handles(struct MapPart *obj);
void dc_object_refresh(struct MapPart *obj);
int dc_object_get_compiled(struct DcObjectCompiled *obj);
int dc_object_get_wire(struct DcObjectWires *obj);
void dc_object_release(struct MapPart *w);
void dc_object_advance_time(struct MapPart *obj);
float dc_object_next_time(struct MapPart *obj, float x);
void dc_object_set_flags(int *flags, int mask);
void dc_object_clear_flags(int *flags, int mask);
void dc_object_set_time(struct MapPart *obj, float x);
void dc_object_set_clip(struct MapPart *obj, struct DcClip *clip);
void dc_object_set_clip_cursor(struct MapPart *obj, struct DcClip *clip, int clip_b, int clip_c);
void dc_object_set_callback(struct MapPart *obj, void *cb, void *cb_arg);
void dc_object_draw(struct MapPart *obj);
void model_object_draw(struct DcObjectTreeView *obj, float time);
void model_node_run_default(struct DcNodeView *node);
void model_parts_init(int owner, struct DcObjectView *obj);
void dc_object_coll_unlock(struct DcObjectView *obj);
struct DcObjectColl *model_get_part(struct DcObjectView *obj, int idx);
int dc_object_count_flag(struct DcObjectView *obj, unsigned int mask);
int dc_object_child_count(struct DcObjectView *obj);
void model_install_allocator(void);
void model_pnb_free_handlers(void);
struct ModelListNode *model_create_timed(int src, int user);
void model_list_remove(struct ModelListNode *tex);
int model_pnb_register(void * pnb);
void model_free_all_buffers_checked(int pnb);
struct ModelTimedLoad *model_load_timed(int file, int user);
void model_unload_timed(struct ModelTimedLoad *tex);
void *model_pnb_load(int file);
void model_pnb_unload(void *pnb);
void model_tex_reload_all(void);
int model_pnb_get_wire(int pnb, int idx);
int model_get_part_cache(int pnb, int idx);
void modeltest_menu_create(void);
int *model_load_wire_data(int src);
void model_free_block(void *blk);
void *model_list_nth(void **list, int n);
void model_draw_kind_list(struct DcNodePosView *node, int kind_list);
void model_node_walk_list_anim(int list, int clip, float time, int kind_list);
void model_register_callbacks(void);

#endif /* REZ_MODEL_H */
