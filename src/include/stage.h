/* Stage number and table; "Stage" and "LogIO" tasks. */

#ifndef REZ_STAGE_H
#define REZ_STAGE_H

struct StageInfo;
struct StageWork;
struct StageWork;
struct StageDef;
struct LogIo;
struct LogIoArg;

void stage_menu_select(void *menu, void *item, int n);
void stage_debug_menu_init(void);
void stage_next_set(int stage);
int stage_get_next(void);
void stage_num_set(int n);
int stage_get_num(void);
void stage_phase_set(int phase);
int stage_get_phase(void);
char *stage_get_name(void);
int stage_get_module(void);
void stage_set(int n);
struct StageInfo *stage_info(int n);
int stage_get_analyze_total(int n);
int stage_get_map_col(int n);
int stage_get_setup_arg(int n);
void stage_init_print(void);
void stage_exit_print(void);
void stage_formation_hook(void);
void *lockon_target_create(void *parent);
void stage_unload(struct StageWork *w);
void stage_task_init(struct StageWork *w);
void stage_task_release(void *w);
void stage_task_exec(struct StageWork *w);
void stage_task_load(struct StageWork *w);
void stage_task_run(struct StageWork *w);
void stage_task_exit(struct StageWork *w);
void stage_task_disp(struct StageWork *w);
void *stage_task_create(void *parent);
int stage_get_work(void);
int stage_get_varedit(void);
int stage_get_player(void);
int stage_get_game_flow(void);
int stage_get_lockon(void);
int stage_get_child(void);
void stage_end_child(void);
int stage_get_child2(void);
int stage_get_tfctrl(void);
int stage_get_buffer(void);
struct StageWork *stage_start(struct StageDef *def);
int stage_is_not_ready(void);
void logio_task_init(struct LogIo *w, struct LogIoArg *prm);
void logio_task_release(void);
void logio_task_exec(struct LogIo *w);
void logio_task_disp(struct LogIo *w);
void *logio_create(void *parent, void *prm);

#endif /* REZ_STAGE_H */
