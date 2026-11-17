/*
 * Area 4 enemy test task (debug flag 16): keys place balloons, marimos, hoppers, chasers and
 * perfect walls, step the map scene, clear, or dump the heap.
 */

#include "stg04.h"
#include "debug.h"
#include "heap.h"
#include "pad.h"
#include "player.h"
#include "task.h"
#include "en_check.h"
#include "en_balloon.h"
#include "en_chaser.h"
#include "en_hopper.h"
#include "en_marimo.h"
#include "stage_base.h"
#include "../common/en_hunter.h"
#include "../common/en_perfectwall_manager.h"

extern unsigned char kbd_state[];
extern struct TaskDesc en_check_desc;

extern PadSystem pad_system;

/* 0x00b15720 */
void en_check_debug_spawn_enemies(void *w)
{
    EnMarimoPlace place;
    Vec3 plpos;

    player_get_pos(&plpos);
    place.se = 17;
    place.link = -1;
    place.unk_01 = 0;
    if (kbd_state[5] == 44)
        map_menu_next();
    if (kbd_state[5] == 76)
        task_end_children(w);
    if (kbd_state[5] == 4) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y + 0.0f;
        place.scale.x = 0.0f;
        place.pos.z = plpos.z + 20.0f;
        place.scale.y = 0.0f;
        place.ang[0] = 0;
        place.scale.z = -0.5f;
        place.ang[1] = ANG_180;
        place.prm[0] = 120.0f;
        place.prm[1] = 60.0f;
        place.prm[2] = -2.0f;
        place.ang[2] = 0;
        en_balloon_create(w, &place);
    }
    if (kbd_state[5] == 5) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y + 0.0f;
        place.scale.x = 0.0f;
        place.pos.z = plpos.z - 0.0f;
        place.scale.y = 0.0f;
        place.ang[0] = 0;
        place.scale.z = -2.0f;
        place.ang[2] = 0;
        place.ang[1] = ANG_180;
        place.prm[0] = 30.0f;
        place.prm[1] = 120.0f;
        place.prm[2] = 2.0f;
        en_balloon_create(w, &place);
    }
    if (kbd_state[5] == 6) {
        place.pos.x = plpos.x - 0.0f;
        place.pos.y = plpos.y + 10.0f;
        place.pos.z = plpos.z - 10.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.scale.z = 0.0f;
        place.ang[0] = 0;
        place.ang[1] = ANG_180;
        place.prm[0] = 120.0f;
        place.prm[1] = 60.0f;
        place.prm[2] = 2.0f;
        place.ang[2] = 0;
        en_balloon_create(w, &place);
    }
    if (kbd_state[5] == 7) {
        place.pos.x = plpos.x + 0.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.ang[0] = 0;
        place.pos.y = plpos.y - 50.0f;
        place.pos.z = plpos.z - 20.0f;
        place.ang[2] = 0;
        place.scale.z = 0.0f;
        place.ang[1] = ANG_180;
        place.prm[0] = 1.0f;
        place.prm[1] = 1.0f;
        place.prm[2] = 60.0f;
        place.prm[3] = 20.0f;
        place.prm[4] = 720.0f;
        en_hunter_create(w, &place);
    }
    if (kbd_state[5] == 8) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y - 50.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.pos.z = plpos.z - 50.0f;
        place.ang[0] = 0;
        place.scale.z = 0.0f;
        place.ang[2] = 0;
        place.ang[1] = ANG_180;
        place.prm[0] = 2.0f;
        place.prm[1] = 180.0f;
        place.prm[2] = 60.0f;
        place.prm[3] = 20.0f;
        place.prm[4] = -30.0f;
        en_hunter_create(w, &place);
    }
    if (kbd_state[5] == 9) {
        place.pos.x = plpos.x + 20.0f;
        place.pos.y = plpos.y + 0.0f;
        place.scale.x = 0.0f;
        place.pos.z = plpos.z - 200.0f;
        place.scale.y = 0.0f;
        place.ang[0] = 0;
        place.ang[2] = 0;
        place.scale.z = 0.5f;
        place.ang[1] = ANG_180;
        place.prm[0] = 3.0f;
        place.prm[1] = 100.0f;
        place.prm[2] = 30.0f;
        place.prm[3] = -40.0f;
        place.prm[4] = 30.0f;
        en_hunter_create(w, &place);
    }
    if (kbd_state[5] == 10) {
        place.pos.x = plpos.x + 20.0f;
        place.pos.y = plpos.y - 0.0f;
        place.pos.z = plpos.z + 250.0f;
        place.scale.x = 0.0f;
        place.ang[0] = 0;
        place.ang[1] = 0;
        place.ang[2] = 0;
        place.scale.y = 10.0f;
        place.scale.z = 0.0f;
        place.prm[0] = 60.0f;
        place.prm[1] = 60.001f;
        place.prm[2] = 200.04f;
        en_chaser_create(w, &place);
    }
    if (kbd_state[5] == 11) {
        place.pos.x = plpos.x - 40.0f;
        place.pos.y = plpos.y - 5.0f;
        place.pos.z = plpos.z + 500.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.ang[0] = 0;
        place.ang[1] = 0;
        place.ang[2] = 0;
        place.scale.z = -10.0f;
        place.prm[0] = 60.0f;
        place.prm[1] = 60.004f;
        place.prm[2] = 200.04f;
        en_chaser_create(w, &place);
    }
    if (kbd_state[5] == 12) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y - 0.0f;
        place.pos.z = plpos.z + 500.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.ang[0] = 0;
        place.ang[1] = 0;
        place.ang[2] = 0;
        place.scale.z = 0.0f;
        place.prm[0] = 60.0f;
        place.prm[1] = 60.004f;
        place.prm[2] = 200.06f;
        en_chaser_create(w, &place);
    }
}

/* 0x00b15d08 */
void en_check_debug_spawn_marimo(void *w)
{
    EnMarimoPlace place;
    Vec3 plpos;

    player_get_pos(&plpos);
    place.se = 17;
    place.link = -1;
    place.unk_01 = 0;
    if (kbd_state[5] == 13) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y - 0.0f;
        place.pos.z = plpos.z - 100.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.scale.z = 1.0f;
        place.ang[0] = 0;
        place.ang[2] = 0;
        place.ang[1] = ANG_180;
        place.prm[0] = 120.004f;
        place.prm[1] = 0.0f;
        place.prm[2] = 0.0f;
        place.prm[3] = 0.5f;
        en_marimo_create(w, &place);
    }
    if (kbd_state[5] == 14) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y + 30.0f;
        place.pos.z = plpos.z + 100.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.scale.z = -200.0f;
        place.ang[0] = 0;
        place.ang[2] = 0;
        place.ang[1] = ANG_180;
        place.prm[0] = 60.008f;
        place.prm[1] = 0.0f;
        place.prm[2] = 0.0f;
        place.prm[3] = 0.5f;
        en_marimo_create(w, &place);
    }
    if (kbd_state[5] == 15) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y + 100.0f;
        place.pos.z = plpos.z + 0.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.scale.z = -200.0f;
        place.ang[0] = 0;
        place.ang[1] = ANG_180;
        place.ang[2] = 0;
        place.prm[0] = 30.02f;
        place.prm[1] = 0.0f;
        place.prm[2] = 0.0f;
        place.prm[3] = 0.5f;
        en_marimo_create(w, &place);
    }
    if (kbd_state[5] == 16) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y + 20.0f;
        place.pos.z = plpos.z - 40.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.scale.z = 0.0f;
        place.ang[0] = 0;
        place.ang[2] = 0;
        place.ang[1] = ANG_180;
        place.prm[0] = 1.0f;
        en_hopper_create(w, (EnHopperPlace *)&place);
    }
    if (kbd_state[5] == 17) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y + 100.0f;
        place.pos.z = plpos.z - 200.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.scale.z = 0.0f;
        place.ang[0] = 0;
        place.ang[2] = 0;
        place.ang[1] = ANG_180;
        place.prm[0] = 1.0f;
        en_hopper_create(w, (EnHopperPlace *)&place);
    }
    if (kbd_state[5] == 18) {
        place.pos.x = plpos.x + 0.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.pos.y = plpos.y + 20.0f;
        place.scale.z = 0.0f;
        place.ang[0] = 0;
        place.pos.z = plpos.z - 200.0f;
        place.ang[2] = 0;
        place.ang[1] = ANG_180;
        place.prm[0] = 1.0f;
        en_hopper_create(w, (EnHopperPlace *)&place);
    }
    if (kbd_state[5] == 19) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y + 0.0f;
        place.pos.z = plpos.z - 200.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.scale.z = 0.0f;
        place.ang[0] = -5461;
        place.ang[1] = ANG_180;
        place.ang[2] = 0;
        place.prm[0] = 300.0f;
        place.prm[1] = 20.0f;
        place.prm[2] = 60.0f;
        place.prm[3] = 40.0f;
        place.prm[4] = 40.0f;
        en_perfectwall_manager_create(w, &place);
    }
    if (kbd_state[5] == 20) {
        place.pos.x = plpos.x + 0.0f;
        place.scale.x = 0.0f;
        place.pos.y = plpos.y + 0.0f;
        place.pos.z = plpos.z - 200.0f;
        place.scale.y = 0.0f;
        place.scale.z = 0.0f;
        i3_set(place.ang, 5461, ANG_180, 8192);
        place.prm[0] = 260.0f;
        place.prm[3] = 40.0f;
        place.prm[1] = 20.0f;
        place.prm[2] = 60.0f;
        place.prm[4] = 40.0f;
        en_perfectwall_manager_create(w, &place);
    }
    if (kbd_state[5] == 21) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y + 0.0f;
        place.pos.z = plpos.z - 200.0f;
        i3_set(place.ang, -ANG_90, ANG_180, 8192);
        place.prm[0] = 400.0f;
        place.prm[2] = -60.0f;
        place.scale.x = 0.0f;
        place.scale.y = 0.0f;
        place.scale.z = 0.0f;
        place.prm[3] = 0.0f;
        place.prm[1] = 40.0f;
        place.prm[4] = 40.0f;
        en_perfectwall_manager_create(w, &place);
    }
}

/* 0x00b16270 */
void en_check_exec(void *w)
{
    EnMarimoPlace place;
    Vec3 plpos;

    if (!debug_flags_test(16))
        return;
    player_get_pos(&plpos);
    place.se = 17;
    place.link = -1;
    place.unk_01 = 0;
    if (kbd_state[5] == 44)
        map_menu_next();
    if (kbd_state[5] == 76)
        task_end_children(w);
    if (pad_system.pad[0]->menu.trig & PAD_Y) {
        place.pos.x = plpos.x + 0.0f;
        place.pos.y = plpos.y + 0.0f;
        place.pos.z = plpos.z + 20.0f;
        place.scale.x = 0.0f;
        place.ang[0] = 0;
        place.ang[2] = 0;
        place.scale.y = 0.0f;
        place.scale.z = -0.5f;
        place.ang[1] = ANG_180;
        place.prm[0] = 120.0f;
        place.prm[1] = 60.0f;
        place.prm[2] = -2.0f;
        en_balloon_create(w, &place);
    }
    if (pad_system.pad[0]->menu.trig & PAD_X)
        heap_map_dump(1, 0);
    en_check_debug_spawn_enemies(w);
    en_check_debug_spawn_marimo(w);
}

/* 0x00b163c0 */
void *en_check_create(void *parent)
{
    return task_create(parent, &en_check_desc, 4);
}
