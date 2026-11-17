/* Tutorial stage support. */

#ifndef REZ_STG20_STAGE_BASE_H
#define REZ_STG20_STAGE_BASE_H

struct Stg20ButtonWork;
struct ExplainWork;

void stg20_button_init(void);
void stg20_button_release(void);
void stg20_button_exec(struct Stg20ButtonWork *btn);
int stg20_button_disp(struct Stg20ButtonWork *btn);
void *stg20_button_create(void *parent);
void button_command(void *w, float *args);
void listeners_add(void);
void listeners_remove(void);
void listener_symbol_effect(void);
void listener_next_map(void);
void listener_symbol_end(void);
void listener_item_spawn(float *ev);
void explain_init(void **w);
void explain_release(void **w);
void explain_exec(int *w);
void explain_disp(struct ExplainWork *ex);
void *explain_create(void *parent);
void *word0_or_null(void *ptr);
void explain_set_message(int *w, int msg);
void explain_command(void *w, float *args);
void start_map(void *menu);
void start_map_shape(void *menu);
void map_init(int *w);
void map_release(void);
void map_exec(void);
void *map_create(void *w);
void map_menu_create(void *parent);
void map_menu_next(void);
void map_menu_set_page(int page);
void map_menu_set_value(int scene);
int map_get_current(void);
void *map_menu_get(void);
int *map_get_work(void);

#endif /* REZ_STG20_STAGE_BASE_H */
