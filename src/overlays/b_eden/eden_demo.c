/*
 * Eden boss opening: cross-fades into the boss area and blends to the map's fog, waits for
 * boss_main_demo_cb, then on the bar starts the first boss sound and hands over to the fight.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "gw_varedit.h"
#include "sound_beat.h"
#include "stage.h"
#include "texforce.h"
#include "eden_demo.h"
#include "eden_map.h"
#include "eden_snake.h"
#include "../common/boss_common.h"

extern char eden_demo_vardef[];
extern GwVarDef eden_demo_vardefs[];
extern struct GwVarNode *eden_demo_vars_group;
extern struct TaskDesc eden_demo_desc;
extern float eden_demo_cross_sec;

/* the opening: camera, a cross fade and the fog change, 100 bytes */
typedef struct EdenDemoWork {
    int step;
    int time;
    int len;
    int frames;
    int fog_time;
    int fog_len;
    char pad_18[12];
    int (*cb)(int);             /* 0x24: the step waits until it returns nonzero */
    int cb_arg;
    BossCameraWork *cam;
    void *parent;
    char pad_34[16];
    BossFog fog_to;             /* 0x44 */
    BossFog fog_from;
} EdenDemoWork;

/* 0x00cae670 */
void eden_demo_vars_add(void)
{
    eden_demo_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_demo_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_demo_vars_group, eden_demo_vardefs);
}

/* 0x00cae6c0 */
void eden_demo_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_demo_vardef);
}

/* steer(cast) */
/* 0x00cae6d8 */
void eden_demo_exec(int *w)
{
    EdenDemoWork *demo = (EdenDemoWork *)w;
    BossFog fog;

    texforce_disable();
    switch (demo->step) {
    case 0:
        w[11] = (int)boss_camera_create(w, 1.0f, 0, 0, 0, 88, 1, 1, 1);
        boss_camera_follow(demo->cam, 60);
        demo->step++;
    case 1:
        demo->step++;
        break;
    case 2:
        fog_get(&demo->fog_from);
        eden_map_get_fog((int *)&demo->fog_to);
        demo->fog_time = demo->fog_len = 60;
        demo->len = demo->time = (int)(eden_demo_cross_sec * FPS);
        cm_sc_cross_create(demo, 254, eden_demo_cross_sec - 0.1f, eden_demo_cross_sec);
        demo->step++;
    case 3:
        if (--demo->time <= 0)
            demo->step++;
        else if (demo->time == 2)
            stage_end_child();
        break;
    case 4:
        if (!demo->cb || demo->cb(demo->cb_arg)) {
            demo->time = 120;
            demo->step++;
        }
        break;
    case 5:
        if (!beat_grid_hit(0))
            break;
        if (beat_clock_get_haku() & 3)
            break;
        boss_pl_di_create(demo, 1, 0);
        boss_vol_play(0, 0);
        demo->step++;
    case 6:
        if (--demo->time <= 0)
            demo->step++;
        break;
    case 7:
        if (beat_grid_hit(0))
            do {
                FINISH(demo);
            } while (0);
        break;
    }
    if (demo->fog_time) {
        demo->fog_time--;
        fog_blend(&demo->fog_from, &demo->fog_to, &fog, (float)demo->fog_time / (float)demo->fog_len);
        fog_apply(&fog, 4);
    }
    if (demo->step >= 3)
        demo->frames++;
}

/* 0x00cae930 */
int *eden_demo_create(void *parent, int arg, int cb_arg)
{
    EdenDemoWork *w = task_create(parent, &eden_demo_desc, 100);

    w->cb = (int (*)(int))arg;
    w->cb_arg = cb_arg;
    w->parent = parent;
    return (int *)w;
}
