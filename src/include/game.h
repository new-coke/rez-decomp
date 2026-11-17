/* Game task: start/exit, game/flow/pass timers and speeds, demo mode. */
#ifndef REZ_GAME_H
#define REZ_GAME_H

struct GameWork;

extern int game_lang;
extern int game_menu_color;

void game_core_init(void);
void game_type_set(int type);
int game_type_get(void);
void game_varedit_register(void);
int game_varedit_get(void);
void game_count_spd_default(void);
void game_count_spd_changed(void);
void game_flow_count_link_changed(void);
void game_flow_count_rate_changed(void);
void game_flow_count_spd_changed(void);
void game_times_clear(float *times);
void game_count_speed_reset(float *times);
int game_get_sound_task(void);
void game_demo_start(int obj);
void game_demo_end(int w);
void *game_task_request(void *parent);
void GameExit(unsigned int code);
void *game_stage_start(int *prm);
void game_debug_pause(struct GameWork *game);
void game_debug_resume(struct GameWork *game);
void game_debug_keys(struct GameWork *obj);
void game_state_normal(void);
void game_demo_check_end(void);
void GameStart(struct GameWork *w);
void GameEnd(struct GameWork *w);
void game_task_exec(struct GameWork *w);
void game_task_disp(void);
void *game_task_create(void *parent);
void game_time_set(float v);
float game_time_get(void);
float game_prev_time_get(void);
float game_total_time_get(void);
void GameCountStart(void);
void GameCountStop(void);
void game_flow_time_reset(void);
void game_flow_time_set(float v);
float game_flow_time_get(void);
void GameFlowCountStart(void);
void GameFlowCountStop(void);
void game_pass_time_reset(void);
void game_pass_time_set(float v);
float game_pass_time_get(void);
void game_pass_count_spd_set(float v);
float game_pass_count_spd_get(void);
void GamePassCountStart(void);
void GamePassCountStop(void);
void game_times_reset(void);
int game_info_draw_flags(void *w, int x, int y, int dy, int mask);
void game_info_window_draw(void *w);
void game_info_window_toggle(void);
void game_info_menu_register(void);

#endif /* REZ_GAME_H */
