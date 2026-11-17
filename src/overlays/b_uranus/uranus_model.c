/*
 * Uranus boss model form: the cubes take the node and vertex positions of an animated model. Each
 * motion picks the next from the state table: an attack towards the side with more cubes, the
 * core motion once the core can be exposed, and after enough motions a form switch request.
 */

#include "types_gw.h"
#include "b_uranus.h"
#include "mathdef.h"
#include "anim.h"
#include "camera_demo.h"
#include "debug.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "math_util.h"
#include "pad_vib.h"
#include "player.h"
#include "pnb.h"
#include "task.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "uranus_model.h"
#include "uranus.h"
#include "uranus_map.h"
#include "uranus_part.h"
#include "uranus_snake.h"

extern char uranus_model_vardef[];
extern GwVarDef uranus_model_vardefs[];
extern struct GwVarNode *uranus_model_vars_group;
extern int uranus_model_ranges[][6];
extern int uranus_model_range_r[6];
extern int uranus_model_range_c[6];
extern int uranus_model_deb_sw;
extern char *uranus_model_motion_names[];
extern char str_uranus_model_fmt[];
extern float uranus_model_ground_ofs;
extern int uranus_model_root_node;
extern int uranus_model_switch_cnt;
extern int uranus_model_mirror_sw;

enum {
    MOT_CORE = 1,               /* leads into exposing the core */
    MOT_DOWN = 4,
    MOT_ATTACK_L,
    MOT_ATTACK_R,
    MOT_ATTACK_C,
    MOT_ENTER                   /* drawn without the player offset */
};

/* motion table entry, 12 bytes */
typedef struct UranusModelMot {
    void *mot;
    float len;
    int *def;                   /* its uranus_model_motions row: kind, pnb entry, next kinds (-1 = none) */
} UranusModelMot;

/* 176 bytes */
typedef struct UranusModelWork {
    void *models;
    char pad_04[4];
    YNode *model;               /* 0x08 */
    float t;
    char pad_10[4];
    float spd;                  /* 0x14 */
    char pad_18[4];
    Vec3 ofs;                   /* 0x1c */
    Vec3 pl_pos;
    float ground_y;
    char pad_38[4];
    UranusPartWork *pm;         /* 0x3c */
    struct UranusMapWork *map;
    UranusNode *nodes;
    UranusModelMot *mots;
    Vec3 root_a;
    Vec3 root_b;
    Vec3 root;
    Vec3 *node_pos;
    Vec3 *vtx_pos;
    int cnt[3];                 /* root, nodes and vertices, from uranus_node_count_tree */
    int cur;
    int next;
    int nmots;
    int intro;                  /* segments to wait before the entry motion */
    int done;                   /* motions since the last switch request */
    int sw_after;
    int forced;
    int to_core;
    int fires;
    int shook;
    int cued;
} UranusModelWork;

extern float uranus_model_spd_scale;
extern float uranus_model_ofs_y;
extern float uranus_model_ofs_z;
extern float uranus_model_blend_frm;
extern struct TaskDesc uranus_model_desc;
extern char str_uranus_model_c[];
extern int uranus_model_motions[][5];
extern int uranus_model_motions2[][5];
extern int uranus_model_sw_cnt0;
extern int uranus_model_sw_cnt1;
extern int cmd_change_dmg;
extern int uranus_model_down_frm;
extern float uranus_model_shake_pow;
extern float uranus_model_shake_rate;

/* 0x00ca9d70 */
void uranus_model_vars_add(void)
{
    uranus_model_vars_group = gw_varedit_add(&gw_varedit_root, 0, uranus_model_vardef);
    gw_varedit_add_table(&gw_varedit_root, uranus_model_vars_group, uranus_model_vardefs);
}

/* 0x00ca9dc0 */
void uranus_model_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)uranus_model_vardef);
}

/* 0x00ca9dd8 */
void uranus_model_nodes_build_r(void *node, UranusNode *dst, int *ptr_a, int *ptr_b)
{
    YNode *n = node;
    short *mesh;
    UranusNode *child;
    int k;

    if (n->mesh) {
        mesh = (short *)n->mesh->data;
        child = uranus_node_add_child(dst);
        child->data = *ptr_a;
        *ptr_a += 12;
        for (k = mesh[3]; k; k--) {
            uranus_node_add_child(child)->data = *ptr_b;
            *ptr_b += 12;
        }
    }
    if (n->child)
        uranus_model_nodes_build_r(n->child, dst, ptr_a, ptr_b);
    if (n->next)
        uranus_model_nodes_build_r(n->next, dst, ptr_a, ptr_b);
}

/* 0x00ca9eb8 */
UranusNode *uranus_model_nodes_build(void *model, int data, int ptr_a, int ptr_b)
{
    int ptrs[2];
    UranusNode *n = uranus_node_new();
    UranusNode *child = uranus_node_add_child(n);

    child->data = data;
    ptrs[0] = ptr_a;
    ptrs[1] = ptr_b;
    uranus_model_nodes_build_r(model, child, &ptrs[0], &ptrs[1]);
    return n;
}

/* 0x00ca9f38 */
int uranus_model_find_motion(UranusModelWork *w, int id)
{
    UranusModelMot *ent;
    int i;

    for (i = 0, ent = w->mots; i < w->nmots; i++, ent++)
        if (*ent->def == id)
            return i;
    for (i = 0, ent = w->mots; i < w->nmots; i++, ent++)
        if (*ent->def == MOT_ATTACK_L)
            return i;
    return 1;
}

/* 0x00ca9fb0 */
void uranus_model_get_motions(UranusModelWork *w, UranusModelMot **cur, UranusModelMot **next)
{
    if (cur)
        *cur = w->mots + w->cur;
    if (next)
        *next = w->mots + w->next;
}

/* 0x00ca9ff8 */
int uranus_model_count_live(UranusModelWork *w, int first, int cnt)
{
    int count = 0;
    UranusNode *n = w->nodes->child->child;
    UranusNode *child;
    int i;

    for (i = first; n && i; i--)
        n = n->next;
    for (i = cnt; n && i; i--) {
        child = n->child;
        if (n->obj && uranus_pm_obj_get_hp((UranusObj *)n->obj))
            count++;
        for (; child; child = child->next)
            if (child->obj && uranus_pm_obj_get_hp((UranusObj *)child->obj))
                count++;
        n = n->next;
    }
    return count;
}

/* 0x00caa0f0 */
int uranus_model_pick_attack(UranusModelWork *w)
{
    int left = uranus_model_count_live(w, uranus_model_ranges[0][3], uranus_model_ranges[0][4]);
    int right = uranus_model_count_live(w, uranus_model_range_r[3], uranus_model_range_r[4]);

    if (!left && !right) {
        if (uranus_model_count_live(w, uranus_model_range_c[3], uranus_model_range_c[4]))
            return MOT_ATTACK_C;
        return MOT_CORE;
    }
    if (left >= right)
        return MOT_ATTACK_L;
    return MOT_ATTACK_R;
}

/* 0x00caa190 */
void uranus_model_pick_next(UranusModelWork *w)
{
    UranusModelMot *ent = &w->mots[w->cur];
    int n = 0;
    int i, attack, id;

    for (i = 0; i < 3; i++)
        if (ent->def[2 + i] != -1)
            n++;
    n = n * RAND_FLOAT_A();
    {
        int *list = ent->def;

        list += n;
        n = list[2];
    }
    for (w->next = 0; i < w->nmots; w->next++)
        if (w->mots[w->next].def[0] == n)
            break;
    id = w->mots[w->next].def[0];
    if (id == MOT_ATTACK_L) {
        if (uranus_pm_can_core(w->pm))
            goto one;
        attack = uranus_model_pick_attack(w);
        w->next = uranus_model_find_motion(w, attack);
        if (attack == MOT_CORE)
            w->to_core = attack;
    } else if (id == MOT_ATTACK_C) {
        if (uranus_pm_can_core(w->pm)) {
        one:
            w->next = uranus_model_find_motion(w, MOT_CORE);
            w->to_core = 1;
        } else if (!uranus_model_count_live(w, uranus_model_range_c[3], uranus_model_range_c[4]))
            w->next = uranus_model_find_motion(w, 0);
    }
}

/* 0x00caa380 */
void uranus_model_attack_cue(UranusModelWork *w)
{
    int event;

    if (w->cued)
        return;
    switch (*w->mots[w->cur].def) {
    case MOT_ATTACK_L:
    case MOT_ATTACK_R:
        peri_act_play_number(1);
        event = 20;
        break;
    case MOT_ATTACK_C:
        peri_act_play_number(1);
        event = 22;
        break;
    case MOT_DOWN:
    default:
        return;
    }
    boss_se_play(event, 0, (int)&w->root);
    w->cued = 1;
}

/* 0x00caa428 */
void uranus_model_motion_end(UranusModelWork *w)
{
    int id;

    w->cued = 0;
    uranus_pm_clear_hits(w->pm);
    if (w->intro) {
        w->next = w->cur;
        return;
    }
    if (w->to_core) {
        w->cur = w->next;
        w->next = uranus_model_find_motion(w, 2);
        if (*w->mots[w->cur].def != 2)
            return;
        uranus_pm_to_core(w->pm);
        if (uranus_model_switch_cnt < ++w->done) {
            uranus_pm_fire(w->pm, (w->fires += 1) + 2);
            w->done = 0;
        }
        return;
    }
    w->cur = w->next;
    uranus_model_pick_next(w);
    w->shook = 0;
    if (uranus_pm_sw_is_ready(w->pm, (int)w))
        uranus_model_attack_cue(w);
    id = *w->mots[w->cur].def;
    if (id != 0 && id != 2)
        return;
    if (!uranus_pm_sw_is_live(w->pm, (int)w))
        return;
    if (w->sw_after < ++w->done) {
        uranus_pm_sw_request(w->pm, (int)w, 10);
        w->done = 0;
    }
}

/* 0x00caa590 */
void uranus_model_pose_r(YNode *n, float **root, float **node_pos, float **vtx_pos, int *cnt)
{
    short *mesh;
    float *vtx;
    int k;

    ulMatrixPush();
    anim_apply_node((struct AnimNode *)n);
    if (*cnt == uranus_model_root_node)
        ulMatrixGetTranslation((float *)ulMatrixCur, *root);
    (*cnt)++;
    if (n->mesh) {
        mesh = (short *)n->mesh->data;
        ulMatrixGetTranslation((float *)ulMatrixCur, *node_pos);
        *node_pos += 3;
        vtx = (float *)n->mesh->data + 2;
        for (k = mesh[3]; k; k--) {
            ulMatrixApplyPoint((float *)ulMatrixCur, vtx, *vtx_pos);
            vtx += 6;
            *vtx_pos += 3;
        }
    }
    if (n->child)
        uranus_model_pose_r(n->child, root, node_pos, vtx_pos, cnt);
    ulMatrixPop();
    if (n->next)
        uranus_model_pose_r(n->next, root, node_pos, vtx_pos, cnt);
}

/* 0x00caa6d8 */
void uranus_model_root_pos_r(YNode *n, float *root)
{
    ulMatrixPush();
    anim_apply_node((struct AnimNode *)n);
    if (root) {
        ulMatrixGetTranslation((float *)ulMatrixCur, root);
        root = 0;
    }
    if (n->child)
        uranus_model_root_pos_r(n->child, root);
    ulMatrixPop();
    if (n->next)
        uranus_model_root_pos_r(n->next, root);
}

/* 0x00caa758 */
void uranus_model_ground_y(struct UranusMapWork *map, Vec3 *pos, Vec3 *ofs, YNode *n, void *mot, float *out, float t)
{
    float root[8];
    float ray[8];

    ulMatrixPush();
    matrix_unit_set(0);
    do {
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    } while (0);
    vu0_matrix_translate(ulMatrixCur, ofs->x, ofs->y, ofs->z);
    anim_start(mot, t);
    uranus_model_root_pos_r(n, root);
    ulMatrixPop();
    ray[0] = root[0];
    ray[2] = root[2];
    ray[1] = -1000.0f;
    if (uranus_map_col_check(map, (Vec3 *)ray, root, &root[4], 0.0f, 1072))
        *out = root[1] + uranus_model_ground_ofs;
}

/* 0x00caa878 */
void uranus_model_blend_pose(UranusModelWork *w, UranusModelMot *mot_a, UranusModelMot *mot_b, float t)
{
    float *outs[3];
    int cnt;
    float t_a, t_b;
    UranusNode *node, *tip;
    Vec3 *pa, *pa_next, *pb, *pb_next;

    ulMatrixPush();
    matrix_unit_set(0);
    outs[1] = &w->node_pos->x;
    outs[2] = &w->vtx_pos->x;
    cnt = 0;
    outs[0] = &w->root_a.x;
    t_a = MIN(mot_a->len, w->t);
    ulMatrixPush();
    if (*mot_a->def != MOT_ENTER)
        MTX_TRANS(&w->pl_pos);
    MTX_TRANS(&w->ofs);
    if (uranus_model_mirror_sw)
        vu0_matrix_scale(ulMatrixCur, -1.0f, 1.0f, 1.0f);
    anim_start(mot_a->mot, t_a);
    uranus_model_pose_r(w->model, &outs[0], &outs[1], &outs[2], &cnt);
    ulMatrixPop();
    t_b = mot_a->len - t_a;
    cnt = 0;
    outs[0] = &w->root_b.x;
    outs[1] = &(w->node_pos + w->cnt[1])->x;
    outs[2] = &(w->vtx_pos + w->cnt[2])->x;
    if (t_b < 0.0f)
        t_b = 0.0f;
    ulMatrixPush();
    if (*mot_b->def != MOT_ENTER)
        MTX_TRANS(&w->pl_pos);
    MTX_TRANS(&w->ofs);
    if (uranus_model_mirror_sw)
        vu0_matrix_scale(ulMatrixCur, -1.0f, 1.0f, 1.0f);
    anim_start(mot_b->mot, t_b);
    uranus_model_pose_r(w->model, &outs[0], &outs[1], &outs[2], &cnt);
    ulMatrixPop();
    node = w->nodes->child;
    v3_lerp((Vec3 *)node->data, &w->root_a, &w->root_b, t);
    v3_copy(&w->root, (Vec3 *)node->data);
    pa = w->node_pos;
    pb = w->vtx_pos;
    pb_next = pb + w->cnt[2];
    pa_next = pa + w->cnt[1];
    for (node = node->child; node; node = node->next) {
        v3_lerp((Vec3 *)node->data, pa, pa_next, t);
        pa++;
        pa_next++;
        for (tip = node->child; tip; tip = tip->next) {
            v3_lerp((Vec3 *)tip->data, pb, pb_next, t);
            pb++;
            pb_next++;
        }
    }
    ulMatrixPop();
}

/* 0x00caac70 */
void uranus_model_pose(UranusModelWork *w, UranusModelMot *ent)
{
    float *cursor[3];
    int cnt;
    float t;
    UranusNode *node, *tip;
    Vec3 *node_pos, *vtx_pos;

    ulMatrixPush();
    matrix_unit_set(0);
    if (*ent->def != MOT_ENTER)
        MTX_TRANS(&w->pl_pos);
    MTX_TRANS(&w->ofs);
    cnt = 0;
    cursor[1] = &w->node_pos->x;
    cursor[2] = &w->vtx_pos->x;
    cursor[0] = &w->root_a.x;
    t = MIN(ent->len, w->t);
    if (uranus_model_mirror_sw)
        vu0_matrix_scale(ulMatrixCur, -1.0f, 1.0f, 1.0f);
    anim_start(ent->mot, t);
    uranus_model_pose_r(w->model, &cursor[0], &cursor[1], &cursor[2], &cnt);
    node = w->nodes->child;
    v3_copy((Vec3 *)node->data, &w->root_a);
    v3_copy(&w->root, (Vec3 *)node->data);
    vtx_pos = w->vtx_pos;
    node_pos = w->node_pos;
    for (node = node->child; node; node = node->next) {
        v3_copy((Vec3 *)node->data, node_pos);
        node_pos++;
        for (tip = node->child; tip; tip = tip->next) {
            v3_copy((Vec3 *)tip->data, vtx_pos);
            vtx_pos++;
        }
    }
    ulMatrixPop();
}

/* 0x00caae58 */
void uranus_model_set_parts_on(UranusModelWork *w, int on, int first, int cnt)
{
    UranusNode *n = w->nodes->child->child;
    UranusNode *child;
    int i;

    if (on) {
        for (i = first; n && i; i--)
            n = n->next;
        for (i = cnt; n && i; i--) {
            child = n->child;
            if (n->obj)
                uranus_pm_obj_set_glow((UranusObj *)n->obj, 1);
            for (; child; child = child->next)
                if (child->obj)
                    uranus_pm_obj_set_glow((UranusObj *)child->obj, 1);
            n = n->next;
        }
    } else {
        for (; n; n = n->next) {
            child = n->child;
            if (n->obj)
                uranus_pm_obj_set_glow((UranusObj *)n->obj, 0);
            for (; child; child = child->next)
                if (child->obj)
                    uranus_pm_obj_set_glow((UranusObj *)child->obj, 0);
        }
    }
}

/* 0x00caaf98 */
void uranus_model_attack_step(UranusModelWork *w)
{
    UranusModelMot *ent;

    uranus_model_get_motions(w, &ent, 0);
    if (!uranus_pm_sw_is_ready(w->pm, (int)w))
        goto off;
    switch (ent->def[0]) {
    case MOT_ATTACK_L:
        uranus_model_attack_cue(w);
        if (uranus_pm_get_hits(w->pm) >= cmd_change_dmg && (int)w->t >= uranus_model_ranges[0][5]
            && uranus_pm_sw_force(w->pm, (int)w, 1))
            w->forced = 1;
        if ((float)uranus_model_ranges[0][0] < w->t && w->t < (float)uranus_model_ranges[0][1])
            uranus_model_set_parts_on(w, 1, uranus_model_ranges[0][3], uranus_model_ranges[0][4]);
        else
            uranus_model_set_parts_on(w, 0, 0, 0);
        if ((int)w->t < uranus_model_ranges[0][2] || w->shook)
            return;
        w->shook = 1;
        oscilate_create(uranus_model_shake_pow, uranus_model_shake_rate);
        boss_se_play(19, 0, (int)&w->root);
        break;
    case MOT_ATTACK_R:
        uranus_model_attack_cue(w);
        if (uranus_pm_get_hits(w->pm) >= cmd_change_dmg && (int)w->t >= uranus_model_range_r[5]
            && uranus_pm_sw_force(w->pm, (int)w, 1))
            w->forced = 1;
        if ((float)uranus_model_range_r[0] < w->t && w->t < (float)uranus_model_range_r[1])
            uranus_model_set_parts_on(w, 1, uranus_model_range_r[3], uranus_model_range_r[4]);
        else
            uranus_model_set_parts_on(w, 0, 0, 0);
        if ((int)w->t < uranus_model_range_r[2] || w->shook)
            return;
        w->shook = 1;
        oscilate_create(uranus_model_shake_pow, uranus_model_shake_rate);
        boss_se_play(19, 0, (int)&w->root);
        break;
    case MOT_ATTACK_C:
        uranus_model_attack_cue(w);
        if (uranus_pm_get_hits(w->pm) >= cmd_change_dmg && (int)w->t >= uranus_model_range_c[5]
            && uranus_pm_sw_force(w->pm, (int)w, 1))
            w->forced = 1;
        if ((float)uranus_model_range_c[0] < w->t && w->t < (float)uranus_model_range_c[1])
            uranus_model_set_parts_on(w, 1, uranus_model_range_c[3], uranus_model_range_c[4]);
        else
            uranus_model_set_parts_on(w, 0, 0, 0);
        if ((int)w->t < uranus_model_range_c[2] || w->shook)
            return;
        w->shook = 1;
        oscilate_create(uranus_model_shake_pow, uranus_model_shake_rate);
        boss_se_play(21, 0, (int)&w->root);
        break;
    case MOT_DOWN:
        if ((int)w->t >= uranus_model_down_frm && !w->shook) {
            w->shook = 1;
            oscilate_create(uranus_model_shake_pow, uranus_model_shake_rate);
            boss_se_play(23, 0, (int)&w->root);
        }
        uranus_model_set_parts_on(w, 0, 0, 0);
        break;
    default:
        uranus_model_set_parts_on(w, 0, 0, 0);
        break;
    }
    return;
off:
    uranus_model_set_parts_on(w, 0, 0, 0);
}

/* 0x00cab420 */
void uranus_model_exec(UranusModelWork *w)
{
    UranusModelMot *ent;
    UranusModelMot *cur, *next;
    int n;
    float left;

    w->ofs.y = uranus_model_ofs_y;
    w->ofs.z = uranus_model_ofs_z;
    w->ofs.x = 0.0f;
    if (w->forced)
        w->spd = 0.0f;
    else
        w->spd = uranus_map_get_spd(w->map) * uranus_model_spd_scale;
    if (w->intro && uranus_map_is_seg_end(w->map) && --w->intro <= 0) {
        if (uranus_map_get_seg_id(w->map, 0) == 26) {
            w->cur = uranus_model_find_motion(w, MOT_ENTER);
            w->next = uranus_model_find_motion(w, 2);
            w->t = 0.0f;
        } else if (uranus_map_get_seg_id(w->map, 1) == 26)
            w->intro++;
    }
    player_get_frame_pos(&w->pl_pos);
    uranus_model_get_motions(w, &ent, 0);
    uranus_model_ground_y(w->map, &w->pl_pos, &w->ofs, w->model, ent->mot, &w->ground_y, w->t);
    w->ofs.y += w->ground_y;
    for (n = enemy_status_get_ticks(); n; n--) {
        w->t += w->spd;
        uranus_model_get_motions(w, &cur, &next);
        if (cur->len < w->t) {
            w->t -= cur->len;
            uranus_model_motion_end(w);
            uranus_model_get_motions(w, &cur, &next);
        }
        left = cur->len - w->t;
        if (left <= uranus_model_blend_frm)
            uranus_model_blend_pose(w, cur, next, left / uranus_model_blend_frm);
        else
            uranus_model_pose(w, cur);
    }
    uranus_model_attack_step(w);
}

/* 0x00cab648 */
void uranus_model_release(UranusModelWork *w)
{
    uranus_node_free_tree(w->nodes);
    heap_free(w->mots, 0);
    heap_free(w->node_pos, 0);
    heap_free(w->vtx_pos, 0);
}

/* 0x00cab690 */
void uranus_model_disp(UranusModelWork *w)
{
    UranusModelMot *cur, *next;

    if (debug_flags_test(4) && uranus_model_deb_sw) {
        uranus_model_get_motions(w, &cur, &next);
        debug_set_origin(10, 43);
        debug_printf(str_uranus_model_fmt, uranus_model_motion_names[*cur->def],
                     uranus_model_motion_names[*next->def], (int)w->t, (int)cur->len);
    }
}

/* 0x00cab740 */
void uranus_model_restart_cb(UranusModelWork *w)
{
    Vec3 to_pl, drift, pl;

    if (w->intro)
        return;
    player_get_pos(&pl);
    uranus_map_get_drift(&drift);
    v3_sum(&to_pl, uranus_pm_get_obj0_pos(w->pm), &drift);
    v3_sub(&to_pl, &to_pl, &pl);
    if (to_pl.z > 0.0f)
        w->next = w->cur = uranus_model_find_motion(w, 0);
    else
        w->next = w->cur = uranus_model_find_motion(w, 2);
    w->cued = 0;
    w->t = 0.0f;
    w->forced = 0;
    uranus_model_attack_cue(w);
}

/* steer(cast) */
/* 0x00cab820 */
UranusModelWork *uranus_model_create(void *parent, UranusPartWork *pm, Vec3 **pts, UranusNode **out, YNode *model, int mode, int slot)
{
    UranusModelWork *w = task_create_inherit(parent, &uranus_model_desc, 176);
    int (*tbl)[5];
    UranusModelMot *ent;
    int i;

    w->pm = pm;
    w->map = uranus_pm_get_map(pm);
    w->model = model;
    w->nodes = uranus_model_nodes_build(model, (int)pts[0], (int)pts[1], (int)pts[2]);
    if (out)
        *out = w->nodes->child;
    if (!mode) {
        tbl = uranus_model_motions;
        w->nmots = 8;
        w->intro = 3;
    } else {
        w->nmots = 7;
        tbl = uranus_model_motions2;
    }
    uranus_node_count_tree(w->nodes->child, w->cnt);
    w->mots = heap_block_tag(game_calloc(w->nmots * sizeof(UranusModelMot)), str_uranus_model_c, 819);
    ent = w->mots;
    w->node_pos = heap_block_tag(game_calloc(w->cnt[1] * 24), str_uranus_model_c, 820);
    w->vtx_pos = heap_block_tag(game_calloc(w->cnt[2] * 24), str_uranus_model_c, 821);
    for (i = 0; i < w->nmots; i++, tbl++, ent++) {
        ent->def = *tbl;
        ent->mot = *(void **)pnb_get_entry(w->models, (*tbl)[1]);
        ent->len = (((unsigned int *)ent->mot)[1] - 1);
    }
    uranus_pm_set_cb(w->pm, (int)w, (void (*)(int))uranus_model_restart_cb, slot);
    if (!mode) {
        w->cur = uranus_model_find_motion(w, 2);
        w->sw_after = uranus_model_sw_cnt0;
        w->next = w->cur;
    } else {
        w->cur = 0;
        uranus_model_pick_next(w);
        w->sw_after = uranus_model_sw_cnt1;
    }
    return w;
}
