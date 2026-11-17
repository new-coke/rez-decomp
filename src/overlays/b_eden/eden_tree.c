/*
 * Eden boss tree attack: a branching beam from the claw that splits at random angles, up to 16
 * branches; once split enough the tips turn on the player and fire NB bullets.
 */

#include "types_gw.h"
#include "b_eden.h"
#include "mathdef.h"
#include "gw_varedit.h"
#include "libc.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "eden_tree.h"
#include "eden_bullet.h"
#include "eden_core.h"
#include "eden_snake.h"
#include "../common/boss_common.h"

extern char eden_tree_vardef[];
extern GwVarDef eden_tree_vardefs[];
extern struct GwVarNode *eden_tree_vars_group;
extern struct TaskDesc eden_tree_desc;
extern float cmd_tr_gr_t;

/* a growing branch, 56 bytes */
typedef struct EdenAim {
    int used;                   /* 1 growing, 2 holding, 3 fading */
    int at_player;
    int t0, t1;
    Vec3 pos;
    Vec3 dir;
    int ang[3];
    float rate;
} EdenAim;

/* 944 bytes */
typedef struct EdenTreeWork {
    struct PnbHead *pack;
    void *tex_list;
    ModelPart **parts;
    void *parent;
    Vec3 pos;
    Vec3 dir;
    EdenAim aims[16];
    int count;
    int se_played;              /* only the first tree of a volley plays the SE */
} EdenTreeWork;

extern float cmd_tr_len;
extern float cmd_tr_fr_t;
extern float cmd_tr_fin_t;
extern float eden_tree_fade[4];
extern float cmd_eda_scl_z;
extern float cmd_eda_scl_xy;
extern float cmd_tr_div_deg_s, cmd_tr_div_deg_l;
extern int eden_tree_aim_modes[];

/* 0x00cc00c8 */
void eden_tree_vars_add(void)
{
    eden_tree_vars_group = gw_varedit_add(&gw_varedit_root, 0, eden_tree_vardef);
    gw_varedit_add_table(&gw_varedit_root, eden_tree_vars_group, eden_tree_vardefs);
}

/* 0x00cc0118 */
void eden_tree_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)eden_tree_vardef);
}

/* 0x00cc0130 */
EdenAim *eden_tree_aim_alloc(EdenTreeWork *w)
{
    EdenAim *slot = w->aims;
    int i;

    w->count++;
    for (i = 0; i < 16; i++, slot++)
        if (!slot->used)
            return slot;
    return w->aims;
}

/* 0x00cc0170 */
void eden_tree_aim_spread(EdenTreeWork *w, EdenAim *src)
{
    int ang[2];
    float sc[4];
    EdenAim *aim;
    int mode;

    do {
        float v = RAND_FLOAT_A() * ((cmd_tr_div_deg_s - cmd_tr_div_deg_l) - (cmd_tr_div_deg_l - cmd_tr_div_deg_s)) + (cmd_tr_div_deg_l - cmd_tr_div_deg_s);
        float u;
        if (0.0f < v)
            u = v + cmd_tr_div_deg_s;
        else
            u = v - cmd_tr_div_deg_s;
        ang[0] = (int)DEG_TO_ANG(u);
    } while (0);
    do {
        float v = RAND_FLOAT_A() * ((cmd_tr_div_deg_s - cmd_tr_div_deg_l) - (cmd_tr_div_deg_l - cmd_tr_div_deg_s)) + (cmd_tr_div_deg_l - cmd_tr_div_deg_s);
        float u;
        if (0.0f < v)
            u = v + cmd_tr_div_deg_s;
        else
            u = v - cmd_tr_div_deg_s;
        ang[1] = (int)DEG_TO_ANG(u);
    } while (0);
    do {
        aim = eden_tree_aim_alloc(w);
        mode = eden_tree_aim_modes[w->count];
        if (w->count >= 12)
            aim->at_player = 1;
        aim->used = 1;
        aim->t0 = aim->t1 = (int)(cmd_tr_gr_t * FPS);
        vec3_normalize(&aim->dir, &src->dir);
        v3_scale(&aim->dir, cmd_tr_len);
        v3_sum(&aim->pos, &src->pos, &aim->dir);
        ROT_X(ang[0], sc, aim->dir.y, aim->dir.z);
        ROT_Y(ang[1], (sc + 2), aim->dir.x, aim->dir.z);
        vec3_normalize(&aim->dir, &aim->dir);
        do {
            aim->ang[1] = (int)RAD_TO_ANG(atan2f(-aim->dir.x, -aim->dir.z));
            aim->ang[0] = (int)RAD_TO_ANG(atan2f(aim->dir.y, sqrtf(aim->dir.x * aim->dir.x + aim->dir.z * aim->dir.z)));
        } while (0);
        ang[0] = -ang[0];
        ang[1] = -ang[1];
    } while (mode == 2);
}

/* 0x00cc0518 */
void eden_tree_aim_player(EdenTreeWork *w, EdenAim *src)
{
    Vec3 spawn_pos, pl, dir;

    v3_mul(&pl, &src->dir, cmd_tr_len);
    v3_sum(&spawn_pos, &src->pos, &pl);
    player_get_pos(&pl);
    v3_sub(&pl, &pl, &spawn_pos);
    vec_up_for_dir(&pl, &dir, (int)DEG_TO_ANG(RAND_FLOAT_A() * 360.0f - 180.0f));
    eden_bullet_nb_create(w->parent, &spawn_pos, &dir, (int)w->parts);
    if (!w->se_played) {
        w->se_played = 1;
        boss_se_play(17, 0, eden_core_shine_get_pos(0));
    }
}

/* 0x00cc0680 */
int eden_tree_aims_step(EdenTreeWork *w)
{
    EdenAim *aim = w->aims;
    int n = 0;
    int i;

    for (i = 0; i < 16; i++, aim++) {
        switch (aim->used) {
        case 1:
            n++;
            aim->t0--;
            aim->rate = 1.0f - (float)aim->t0 / (float)aim->t1;
            if (!aim->t0) {
                aim->used = 2;
                aim->t0 = aim->t1 = (int)(cmd_tr_fr_t * FPS);
                if (aim->at_player)
                    eden_tree_aim_player(w, aim);
                else
                    eden_tree_aim_spread(w, aim);
            }
            break;
        case 2:
            n++;
            if (--aim->t0 <= 0) {
                aim->t0 = aim->t1 = (int)(cmd_tr_fin_t * FPS);
                aim->used = 3;
            }
            break;
        case 3:
            n++;
            if (--aim->t0 <= 0)
                aim->used = 0;
            break;
        }
    }
    return n;
}

/* 0x00cc0820 */
void eden_tree_exec(EdenTreeWork *w)
{
    if (!eden_tree_aims_step(w))
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00cc0868 */
void eden_tree_disp(EdenTreeWork *w)
{
    int old;
    int i;
    EdenAim *aim;
    float k;

    ulGxSetTextureList(w->tex_list);
    old = draw_set_override_flags(0);
    for (i = 0, aim = w->aims; i < 16; i++, aim++) {
        if (!aim->used)
            continue;
        if (aim->used == 3) {
            draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            draw_push_override_color();
            draw_push_strip_flags();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
            eden_tree_fade[0] = (float)aim->t0 / (float)aim->t1 + -1.0f;
            draw_set_override_color(eden_tree_fade);
            ulMatrixPush();
            MTX_TRANS(&aim->pos);
            eden_tree_rotate_yxz(aim->ang);
            k = aim->rate;
            vu0_matrix_scale(ulMatrixCur, k * cmd_eda_scl_xy, k * cmd_eda_scl_xy, k * cmd_eda_scl_z);
            dc_object_draw(w->parts[16]);
            ulMatrixPop();
            draw_pop_override_color();
            draw_pop_strip_flags();
        } else {
            draw_set_override_flags(0);
            ulMatrixPush();
            MTX_TRANS(&aim->pos);
            eden_tree_rotate_yxz(aim->ang);
            k = aim->rate;
            vu0_matrix_scale(ulMatrixCur, k * cmd_eda_scl_xy, k * cmd_eda_scl_xy, k * cmd_eda_scl_z);
            dc_object_draw(w->parts[16]);
            ulMatrixPop();
        }
    }
    draw_set_override_flags(old);
}

/* 0x00cc0a58 */
void eden_tree_release(Earth1stDemoWork *w)
{
}

/* 0x00cc0a60 */
void eden_tree_aim_start(EdenTreeWork *w)
{
    EdenAim *aim = eden_tree_aim_alloc(w);

    v3_copy(&aim->pos, &w->pos);
    v3_copy(&aim->dir, &w->dir);
    do {
        aim->ang[1] = (int)RAD_TO_ANG(atan2f(-aim->dir.x, -aim->dir.z));
        aim->ang[0] = (int)RAD_TO_ANG(atan2f(aim->dir.y, sqrtf(aim->dir.x * aim->dir.x + aim->dir.z * aim->dir.z)));
    } while (0);
    aim->used = 1;
    aim->t0 = aim->t1 = (int)(cmd_tr_gr_t * FPS);
}

/* 0x00cc0ba8 */
int *eden_tree_create(void *parent, Vec3 *pos, Vec3 *dir, int arg, int kind)
{
    EdenTreeWork *w = task_create_inherit(parent, &eden_tree_desc, 944);

    w->parent = parent;
    v3_copy(&w->pos, pos);
    w->parts = (ModelPart **)arg;
    w->se_played = kind;
    vec3_normalize(&w->dir, dir);
    eden_tree_aim_start(w);
    return (int *)w;
}

/* 0x00cc0c58 */
void eden_tree_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
