/* Stage event list: load, play, seek; FlowEdit editor. */
#ifndef REZ_GAME_FLOW_EDITOR_H
#define REZ_GAME_FLOW_EDITOR_H

struct FlowEvent;
struct FlowEventNode;
struct GameFlowData;
struct FlowEditWork;
struct FlowEditWork;
struct FlowEventSaveWork;
struct GameFlowData;

extern unsigned char kbd_key;
extern unsigned char kbd_key2;

char *GameFlowInit(void *parent, int val, void *enemy_tbl, void *func_tbl);
void GameFlowExit(char *flow);
void game_flow_regist_init(char *flow);
void game_flow_regist_release(char *flow);
void GameFlowLoadFile(void);
void GameFlowClear(void);
int game_flow_is_finished(void);
void GameFlowStart(void);
void GameFlowStop(void);
void game_flow_queue_back(void);
void game_flow_queue_forward(void);
void *game_flow_get_data(void);
void *game_flow_get_current(void);
int game_flow_get_task_parent(void);
void game_flow_seek_to_time(void);
void game_flow_editor_toggle(int idx);

#endif /* REZ_GAME_FLOW_EDITOR_H */
