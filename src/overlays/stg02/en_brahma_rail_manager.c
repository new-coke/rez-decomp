/*
 * Area 2 path recorder: the player's last 200 frames of movement and angles, for Brahma's rails.
 * Kills every enemy once the boss map scene starts.
 */

#include "stg02.h"
#include "callback.h"
#include "game_flow_enemy.h"
#include "player.h"
#include "en_brahma_rail_manager.h"
#include "stage_base.h"

extern int en_brahma_rail_manager_work;
extern struct TaskDesc en_brahma_rail_manager_desc;

#ifndef VERSION_US
#endif

typedef struct EnBrahmaRailManagerWork {
#ifdef VERSION_US
    int listener;               /* kill-all listener handle */
#endif
    int cur;
    Vec3 pos[200];              /* 0x004 */
    int ang[200][3];            /* 0x964 */
} EnBrahmaRailManagerWork;

#ifdef VERSION_US
/* Event 24 listener: kills every enemy while the map scene is 21 (boss). */

/* 0x00aff400 */
void en_brahma_rail_manager_kill_all(int msg)
{
    if (map_get_current() == 21)
        flow_enemy_kill_all();
}

/* 0x00aff4a0 */
void en_brahma_rail_manager_init(EnBrahmaRailManagerWork *w)
{
    en_brahma_rail_manager_work = (int)w;
    w->listener = (int)listener_add(24, en_brahma_rail_manager_kill_all);
}

/* 0x00aff4e0 */
void en_brahma_rail_manager_release(EnBrahmaRailManagerWork *w)
{
    listener_remove(24, (void *)w->listener);
    en_brahma_rail_manager_work = 0;
}
#else
/* 0x00aff4a0 */
void en_brahma_rail_manager_init(int w)
{
    en_brahma_rail_manager_work = w;
}

/* 0x00aff4e0 */
void en_brahma_rail_manager_release(void)
{
    en_brahma_rail_manager_work = 0;
}
#endif

/* 0x00aff430 */
void en_brahma_rail_manager_exec(EnBrahmaRailManagerWork *w)
{
    player_get_accel(&w->pos[w->cur]);
    player_get_rot(w->ang[w->cur]);
    if (++w->cur == 200)
        w->cur = 0;
}

/* 0x00aff508 */
void *en_brahma_rail_manager_create(void *parent)
{
#ifdef VERSION_US
    return task_create(parent, &en_brahma_rail_manager_desc, 4808);
#else
    return task_create(parent, en_brahma_rail_manager_desc, 4804);
#endif
}

/* 0x00aff530 */
void en_brahma_rail_manager_get_past(Vec3 *out, int *ang, int n)
{
    Vec3 sum;
    int idx, i;

    sum.x = 0;
    sum.y = 0;
    sum.z = 0;
    idx = ((EnBrahmaRailManagerWork *)en_brahma_rail_manager_work)->cur - 1;
    for (i = n - 1; i >= 0; i--) {
        if (--idx < 0)
            idx += 200;
        v3_add(&sum, &((EnBrahmaRailManagerWork *)en_brahma_rail_manager_work)->pos[idx]);
    }
    player_get_pos(out);
    v3_sub(out, out, &sum);
    i3_copy(ang, ((EnBrahmaRailManagerWork *)en_brahma_rail_manager_work)->ang[idx]);
}
