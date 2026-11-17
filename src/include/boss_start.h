/* Loads and runs a boss overlay. */

#ifndef REZ_BOSS_START_H
#define REZ_BOSS_START_H

struct BossStartWork;
struct Pair;
struct BossStartWorkView;

void boss_start_stage_init(void);
void boss_start_stage_release(void);
int boss_start_alive(void);
void *boss_start_create(void *prm);
void boss_start_kill(void);

#endif /* REZ_BOSS_START_H */
