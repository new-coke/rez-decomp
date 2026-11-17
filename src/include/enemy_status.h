/* Enemy slots, events, totals, simulation ticks. */

#ifndef REZ_ENEMY_STATUS_H
#define REZ_ENEMY_STATUS_H

struct FxEntry;
struct EnemyStatus;

void enemy_status_vars_register(void);
int enemy_status_set_flags(int mask);
int enemy_status_clear_flags(int mask);
int enemy_status_ent_get_id(unsigned idx);
int enemy_status_get_cur(void);
void enemy_status_event(unsigned idx, unsigned op, float *pos);
int enemy_status_ent_alloc(int id, int handle, int owner);
int enemy_status_ent_create(unsigned char *p, int owner);
void enemy_status_score_popup(int slot, struct Vec3 *pos, int score, int shots);
void enemy_status_ent_set_pos(unsigned idx, float *pos, int val);
void enemy_status_set_action_time(float v);
float enemy_status_get_action_time(void);
int enemy_status_get_ticks(void);
int enemy_status_get_gm_appear(void);
int enemy_status_get_gm_die(void);
int enemy_status_get_stg_appear(void);
int enemy_status_get_stg_die(void);
void EnemyStatusInitGame(void);
void EnemyStatusInitStage(void);
int enemy_status_man_create(void *parent);
int enemy_status_man2_create(void *parent);

#endif /* REZ_ENEMY_STATUS_H */
