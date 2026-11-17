/* Flow function events, FlowEdit page, count stop/skip and BPM controls. */

#ifndef REZ_GAME_FLOW_FUNC_H
#define REZ_GAME_FLOW_FUNC_H

#include "types_game.h"

struct FlowFuncObj;
struct GameFlowFuncFlowFuncEvNode;

FlowFuncEv *flow_func_events_set_array(struct FlowFuncObj *data, FlowFuncEv *src, int n);
void flow_func_event_call(struct FlowFuncObj *data, int idx);
int flow_func_event_list_append(struct FlowFuncObj *data);
void flow_func_event_list_remove(struct FlowFuncObj *data, int idx);
struct GameFlowFuncFlowFuncEvNode *flow_func_event_list_nth(char *data, int n);
void flow_func_edit_colors_reset(void);
void flow_func_events_array_to_list(struct FlowFuncObj *data);
void flow_func_events_list_to_array(struct FlowFuncObj *data);
void flow_func_edit_float_pad(void *data, float *val);
void flow_func_edit_param_cursor_set(int *ev, unsigned int idx);
int flow_func_edit_param_input(void *data, int *ev);
int flow_func_edit_update(void *data, int idx);
void flow_func_edit_disp(struct FlowFuncObj *win);
void flow_ctrl_stop_init(FlowCtrl *w, FlowCtrl *src);
void flow_ctrl_release(void);
void flow_ctrl_stop_exec(FlowCtrl *w);
void flow_ctrl_skip_init(FlowCtrl *w, FlowCtrl *src);
void flow_ctrl_skip_exec(FlowCtrl *w);
void *flow_ctrl_request(void *owner, FlowCtrl *ctrl);
void flow_ctrl_request_stop(void *owner, float *p);
void flow_ctrl_request_skip(void *owner, float *p);
void flow_func_bpm_change(void *owner, float *p);

#endif /* REZ_GAME_FLOW_FUNC_H */
