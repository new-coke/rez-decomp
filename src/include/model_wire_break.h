/* Wire-break effect: segment sets from a model. */

#ifndef REZ_MODEL_WIRE_BREAK_H
#define REZ_MODEL_WIRE_BREAK_H

struct WireBreakWork;
struct WireBreakArg;
struct WireBreakWork;
struct WireBreakWork;
struct WireBreakWork;
struct WireBreakWork;
struct WireBreakTaskCreate2WireBreakWork;
struct WireBreakData;
struct WireBreakData;
struct WireBreakWork;
struct WireBreakData;
struct WireBreakLink;
struct DcNodeRotView;
struct DcSceneNode;
struct WireBreakDataView;
struct WireBreakWorkView;

struct WireBreakWork *wire_break_task_create(int parent, int src);
void wire_break_step_default(struct WireBreakData *w, float *force, float damp);
void wire_break_draw(void * obj, int col);
void wire_break_set_globals(int cb, int cb_arg);
void model_node_draw_tree2_anim(void *tree, int clip, float time);

#endif /* REZ_MODEL_WIRE_BREAK_H */
