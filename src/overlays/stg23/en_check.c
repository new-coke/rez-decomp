/*
 * Trance Mission debug helper (debug flag 16): keys skip to the next map, end its spawns, or
 * spawn one of three test dom glyph formations ahead of the player.
 */

#include "stg23.h"
#include "debug.h"
#include "player.h"
#include "task.h"
#include "en_check.h"
#include "en_dom_generator.h"
#include "stage_base.h"

extern unsigned char kbd_state[];
extern struct TaskDesc en_check_desc;

/* EnPlacement with its header bytes */
typedef struct EnCheckArg {
    unsigned char kind;
    char unk_01;                /* 0x01: set to 0 by every spawner, never read */
    short fx_arg;
    Vec3 pos;                   /* 0x04 */
    int ang[3];                 /* 0x10 */
    Vec3 dir;                   /* 0x1c */
    float scene;                /* 0x28 */
    float time;                 /* 0x2c */
    float param2, param3, param4, param5;   /* 0x30 */
} EnCheckArg;

/* 0x00af82b8 */
void en_check_exec(void *w)
{
    EnCheckArg place;
    Vec3 plpos;

    if (debug_flags_test(16)) {
        player_get_pos(&plpos);
        place.kind = 16;
        place.unk_01 = 0;
        place.fx_arg = -1;
        if (kbd_state[5] == ',')
            map_menu_next();
        if (kbd_state[5] == 'L')
            task_end_children(w);
        if (kbd_state[5] == 4) {
            place.pos.x = plpos.x + 0.0f;
            place.pos.y = plpos.y - 0.0f;
            place.pos.z = plpos.z - 100.0f;
            place.dir.z = -20.0f;
            place.scene = 33.0f;
            place.time = 10.3f;
            place.param3 = 0.2f;
            place.param4 = -1.0f;
            place.dir.x = 0.0f;
            place.dir.y = 0.0f;
            place.ang[0] = 0;
            place.ang[1] = 0;
            place.ang[2] = 0;
            place.param2 = 0.0f;
            place.param5 = 0.0f;
            en_dom_generator_create(w, (struct EnPlacement *)&place);
        }
        if (kbd_state[5] == 5) {
            place.pos.x = plpos.x + 0.0f;
            place.pos.y = plpos.y - 0.0f;
            place.pos.z = plpos.z - 100.0f;
            place.dir.z = -20.0f;
            place.ang[2] = ANG_90;
            place.scene = 34.0f;
            place.time = 10.3f;
            place.param2 = -0.5f;
            place.param3 = 0.5f;
            place.param4 = -1.0f;
            place.dir.x = 0.0f;
            place.dir.y = 0.0f;
            place.ang[0] = 0;
            place.ang[1] = 0;
            place.param5 = 0.0f;
            en_dom_generator_create(w, (struct EnPlacement *)&place);
        }
        if (kbd_state[5] == 6) {
            place.pos.x = plpos.x + 0.0f;
            place.pos.y = plpos.y - 0.0f;
            place.pos.z = plpos.z - 100.0f;
            place.dir.z = -20.0f;
            place.ang[2] = ANG_90;
            place.scene = 35.0f;
            place.time = 10.3f;
            place.dir.x = 0.0f;
            place.dir.y = 0.0f;
            place.ang[0] = 0;
            place.ang[1] = 0;
            place.param2 = 0.0f;
            place.param3 = 0.0f;
            place.param4 = -1.0f;
            place.param5 = 100.013f;
            en_dom_generator_create(w, (struct EnPlacement *)&place);
        }
    }
}

/* 0x00af8550 */
void *en_check_create(void *parent)
{
    return task_create(parent, &en_check_desc, 4);
}
