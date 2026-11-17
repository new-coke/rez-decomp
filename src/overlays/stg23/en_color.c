/*
 * Trance Mission enemy colours: body and wire colour per map scene; the wire fades from near_rate
 * to far_rate between the near and far distances. Values are debug variables.
 */

#include "stg23.h"
#include "color.h"
#include "gw_varedit.h"
#include "libc.h"
#include "map_manager.h"
#include "math_util.h"
#include "player.h"
#include "en_color.h"
#include "stage_base.h"
#include "../common/enemy_vars.h"

extern ParamRec en_color_params;
extern VarDef en_color_vardefs[26];

/* en_color_params: colours by map scene */
typedef struct EnColorParams {
    char *name;
    char *label;                /* 0x04 */
    float near, far;            /* 0x08: squared distances */
    float near_rate, far_rate;  /* 0x10: alpha scales */
    unsigned int col_body[11];  /* 0x18 */
    unsigned int col_wire[11];  /* 0x44 */
} EnColorParams;

#define EN_COLOR_REC ((EnColorParams *)&en_color_params)

/* 0x00af8578 */
void en_color_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_color_params.name;
    def.fmt = en_color_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_color_vardefs;
    fld = en_color_params.v;
    for (i = 0; i < 26; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_color_vardefs);
}

/* 0x00af8628 */
void en_color_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* steer(cast) */
/* 0x00af8630 */
void en_color_body_rgba(float *rgba)
{
    EnColorParams *rec = EN_COLOR_REC;
    unsigned int col;
    float norm;

    col = rec->col_body[MIN(map_manager_get_scene_id((int)map_menu_get()) < 0
                       ? 0 : map_manager_get_scene_id((int)map_menu_get()), 10)];
    norm = 255.0f;
    rgba[0] = color_get_a(col) / norm;
    rgba[1] = (int)((unsigned int (*)(int))color_get_r)(col) / norm;
    rgba[2] = color_get_g(col) / norm;
    rgba[3] = color_get_b(col) / norm;
}

/* 0x00af8758 */
int en_color_wire_at(Vec3 *pos)
{
    EnColorParams *rec = EN_COLOR_REC;
    Vec3 pl;
    float dsq, rate;
    unsigned int col;

    player_get_pos(&pl);
    dsq = vec3_distance_squared(&pl, pos);
    col = rec->col_wire[MIN(map_manager_get_scene_id((int)map_menu_get()) < 0
                       ? 0 : map_manager_get_scene_id((int)map_menu_get()), 10)];
    if (dsq < EN_COLOR_REC->near)
        rate = EN_COLOR_REC->near_rate;
    else if (dsq < EN_COLOR_REC->far)
        rate = EN_COLOR_REC->near_rate + (EN_COLOR_REC->far_rate - EN_COLOR_REC->near_rate)
            * (dsq - EN_COLOR_REC->near) / (EN_COLOR_REC->far - EN_COLOR_REC->near);
    else
        rate = EN_COLOR_REC->far_rate;
    return (col & 0xffffff) | ((int)(color_get_a(col) * rate) + 1) >> 1 << 24;
}
