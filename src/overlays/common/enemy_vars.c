/*
 * Shared debug-variable group of the stage objects; each enemy and effect type puts its tunables
 * under it. Created by the first user, removed when the last lets go.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "game.h"
#include "gw_varedit.h"
#include "enemy_vars.h"

extern struct GwVarNode *gw_varedit_root;

extern struct GwVarNode *enemy_vars_group;

/* how many objects hold it */
extern int enemy_vars_users;

extern char enemy_vars_group_def[];

/* 0x00b0e9d8 stg01, 0x00b19c68 stg02, 0x00b12dc0 stg03, 0x00b19638 stg04, 0x00b2cc50 stg05, 0x00af98c8 stg20, 0x00b22348 stg22, 0x00b03690 stg23 */
/* steer(cast) */
struct GwVarNode *enemy_vars_group_acquire(void)
{
    if (++enemy_vars_users == 1)
        enemy_vars_group = gw_varedit_add(&gw_varedit_root, game_varedit_get(), enemy_vars_group_def);
    return enemy_vars_group;
}

/* 0x00b0ea40 stg01, 0x00b19cd0 stg02, 0x00b12e28 stg03, 0x00b196a0 stg04, 0x00b2ccb8 stg05, 0x00af9930 stg20, 0x00b223b0 stg22, 0x00b036f8 stg23 */
void enemy_vars_group_release(void)
{
    if (--enemy_vars_users == 0)
        gw_varedit_remove(&gw_varedit_root, game_varedit_get(), (struct GwVarDef *)enemy_vars_group_def);
}
