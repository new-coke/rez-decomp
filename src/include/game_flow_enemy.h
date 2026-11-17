/* Flow enemy events, FlowEdit enemy page. */

#ifndef REZ_GAME_FLOW_ENEMY_H
#define REZ_GAME_FLOW_ENEMY_H

struct GeRec;
struct FlowEnemyEventNode;
struct FlowEnemyEvent;
struct GameFlowEnemyFlowEnemyEvent;
struct FlowEnemyEvent;

void EnemyTaskInit(void *parent, int *data);
void EnemyTaskExit(int *data);
void flow_enemy_tbl_call_fn4_builtin(int *w);
void flow_enemy_tbl_call_fn4_regist(int *w);
void flow_enemy_tbl_call_fn8_regist(int *data);
void flow_enemy_kill_all(void);
int flow_enemy_type_check(int data, int idx, int rec, int *type);
void flow_enemy_events_free_array(int *data);
void flow_enemy_events_free_list(int *data);
struct GeRec *flow_enemy_events_copy_array(int *data, struct GeRec *dst, int count);
struct GeRec *flow_enemy_events_set_array(int *data, struct GeRec *src, int count);
char *flow_enemy_events_set_array_v0(int *obj, char *src, int n);
char *flow_enemy_events_set_array_v1(int *obj, char *src, int n);
void flow_enemy_events_array_to_list(int *obj);
void flow_enemy_events_list_to_array(int *obj);
struct FlowEnemyEventNode **flow_enemy_event_list_slot(char *data, int idx);
struct FlowEnemyEventNode *flow_enemy_event_list_nth(char *data, int idx);
int flow_enemy_tbl_get(int *data, int idx);
int flow_enemy_event_list_append(int *obj);
void flow_enemy_event_list_remove(char *obj, int n);
void flow_enemy_apply_camera(float *pos, float *dir, int *angles);
void flow_enemy_event_call(int *data, int idx);
void flow_enemy_edit_colors_reset(void);
void flow_edit_window_clamp(void *win, int width, int height);
void flow_enemy_edit_key_help(void);
void flow_enemy_edit_disp(int *win);
void flow_enemy_place_at_camera(struct FlowEnemyEvent *ev);
void flow_enemy_move_camera(struct GameFlowEnemyFlowEnemyEvent *ev);
void flow_editor_pad_type(int *data, unsigned char *type);
void flow_editor_pad_flag(int data, unsigned char *p);
void flow_editor_pad_short(int data, short *p);
void flow_editor_pad_pos(int data, float *p);
void flow_editor_pad_angle(int data, struct FlowEnemyEvent *ev);
void flow_editor_pad_vector(int data, float *p);
void flow_editor_pad_adjust_float(void *data, float *val);
void flow_editor_param_cursor_set(unsigned char *ev, unsigned int idx);
int flow_editor_param_exec(void *data, int idx);

#endif /* REZ_GAME_FLOW_ENEMY_H */
