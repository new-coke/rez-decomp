/*
 * Mars boss trees: branching trees of lock-on nodes that grow from the boss (in rings, inward or
 * by forking), fire from their leaves and break node by node; the manager spawns them in waves.
 */

#include "types_gw.h"
#include "b_mars.h"
#include "mathdef.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "model.h"
#include "node_draw_state.h"
#include "player.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "mars_tree.h"
#include "mars.h"
#include "mars_bullet.h"
#include "mars_core.h"
#include "../common/boss_common.h"

extern char mars_tree_vardef[];
extern GwVarDef mars_tree_vardefs[];
extern struct GwVarNode *mars_tree_vars_group;

struct MarsNode {       /* 156 bytes */
    short grow_cnt;
    short shot_cnt;                  /* 0x02: frames to the next shot (mars_fl_slots_state) */
    short shot_reload;
    short spread_cnt;                  /* 0x06: frames to activating the neighbours */
    short spread_reload;
    short grow_reload;
    short root;                  /* 0x0c */
    short depth;                  /* 0x0e */
    unsigned short flags;       /* 0x10: 1 lock-on registered */
    char pad_12[2];
    int used;                   /* 0x14: 0 = free (mars_tr_node_find_free) */
    int shots;                  /* 0x18 */
    int unk_1c;                 /* 0x1c: set to 0 by mars_tr_node_add, never read */
    Vec3 pos;                   /* 0x20 */
    Vec3 wpos;                  /* 0x2c: pos + ofs (flag 1) */
    Vec3 ofs;                   /* 0x38 */
    char pad_44[12];
    LockObj lock;                  /* 0x50 */
    struct MarsNode *child;     /* 0x90 */
    struct MarsNode *parent;    /* 0x94 */
    struct MarsNode *next;      /* 0x98: sibling */
};

/* 208 bytes */
struct MarsTree {
    void *pack;
    void *textures;
    MarsNode *pool;
    MarsNode *last;
    MarsNode *broken;
    MarsNode *tip;
    char pad_18[4];
    int n;                              /* 0x1c */
    int grow;
    char pad_24[4];
    int state;                          /* 0x28 */
    Vec3 tip_pos;
    Vec3 axis_y;
    Vec3 dir;
    Vec3 axis_z;
    const int *alive;
    char pad_60[4];
    int unk_64;                         /* 0x64: copy of grow, never read */
    int left;
    int base;
    int count;
    int flare_tex_a;
    int flare_tex_b;
    Vec3 *pos;
    int *rot;
    int ang[3];
    int ang2[3];
    ModelPart *mdl;
    ModelPart *unk_a0;                  /* 0xa0: the second model of the creation record, never read */
    ModelPart *break_mdl;
    int shot_mdl;
    int shot_break_mdl;
    char *boss;
    void *parent;
    int *done;
    void *unk_bc;                       /* 0xbc: from the creation record, never read */
    float ring_speed;
    float ring_turns;
    int leaves_alive;
    int leaves_set;
};

extern unsigned int col15;
extern int mars_tr_fork_every;
extern int mars_tr_depth_bits;
extern char str_mars_tree_c[];
extern struct TaskDesc mars_tr_desc;
extern float mars_tr_grow_sec;
extern float mars_tr_inward_len;
extern int cmTreeNbNum[];
extern float cmTreeShotT[];
extern float mars_tr_leaf_delay;
extern float mars_tr_leaf_delay_step;
extern int cmTreeHP[];

#define MARS_ACTIVATE(c, n) do { (c)->flags |= 4; (c)->spread_cnt = (c)->spread_reload = (n)->spread_reload; } while (0)
extern struct TaskDesc mars_tr_man_desc;
extern int mars_tr_man_dir;
extern const int mars_tr_man_mode_b;
extern float mars_tr_man_z;
extern float mars_tr_ring_ofs;
extern float mars_tr_ring_height;
extern float mars_tr_break_len;
extern float mars_tr_break_size;
extern float mars_tr_halo_size;
extern unsigned int mars_tree_cols[];
extern unsigned int mars_tr_break_col;
extern const int mars_tr_man_wait_a;
extern const int mars_tr_man_wait_b;
extern int mars_tr_man_trees;
extern const float mars_tr_man_grow_a;
extern const float mars_tr_man_grow_b;
extern const float mars_tr_man_scl_a;
extern const float mars_tr_man_scl_b;
extern float mars_tr_man_spawn_z;
extern int mars_tr_man_kind_a;
extern int mars_tr_man_kinds_b;
extern int mars_tr_man_num_a;
extern int mars_tr_man_nums_b;
extern int mars_tr_man_end_left;
extern float mars_tr_fork_ang_lo;
extern float mars_tr_fork_ang_hi;
extern float mars_tr_flare_scl;
extern float mars_tr_flare_max;
extern float mars_tr_tick_sec;

/* copied into the manager and passed to each tree */
typedef struct MarsTreeArg {
    ModelPart *mdl[3];
    ModelPart *shot_mdl;
    ModelPart *shot_break_mdl;
    Vec3 *pos;
    Vec3 *dir;
    int *rot;
    int ang2[3];
    const int *alive;
    int grow;
    int frames;
    int flare_tex_a;
    int flare_tex_b;
    int frame_base;
    float ring_speed;
    float ring_turns;
    Vec3 *boss;
    void *parent;
    int done;
} MarsTreeArg;

/* 168 bytes */
typedef struct MarsTrManWork {
    void *pack;
    void *textures;
    MarsTreeArg tree;
    MarsTree **trees;
    int cur;
    int state;
    char pad_6c[4];
    int tree_cnt;                       /* 0x70 */
    int waves;
    int ended;
    int fired;
    char *boss;
    Vec3 pos;
    int ang[3];
    Vec3 dir;
} MarsTrManWork;

/* 0x00caca18 */
void mars_tree_vars_add(void)
{
    mars_tree_vars_group = gw_varedit_add(&gw_varedit_root, 0, mars_tree_vardef);
    gw_varedit_add_table(&gw_varedit_root, mars_tree_vars_group, mars_tree_vardefs);
}

/* 0x00caca68 */
void mars_tree_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)mars_tree_vardef);
}

/* 0x00caca80 */
int mars_tr_rand_ang(float lo, float hi)
{
    float deg = RAND_FLOAT_A() * 2.0f * (hi - lo) + lo;

    if (hi < deg)
        deg -= hi + hi;
    return (int)DEG_TO_ANG(deg);
}

/* 0x00cacb10 */
void mars_tr_pool_alloc(MarsTree *tree)
{
    if (tree->grow)
        tree->count = (1 << mars_tr_depth_bits) + tree->base;
    else
        tree->count = (tree->base / mars_tr_fork_every + 1 << mars_tr_depth_bits) + tree->base;
    tree->pool = heap_block_tag(game_calloc(tree->count * sizeof(MarsNode)), str_mars_tree_c, 246);
    tree->last = 0;
}

/* 0x00cacbb8 */
MarsNode *mars_tr_node_find_free(MarsTree *tree)
{
    int i;
    MarsNode *p = tree->pool;

    for (i = 0; i < tree->count; i++, p++) {
        if (p->used == 0)
            return p;
    }
    return 0;
}

/* 0x00cacbf8 */
MarsNode *mars_tr_node_add(MarsTree *tree, MarsNode *parent, int kind)
{
    MarsNode *n = mars_tr_node_find_free(tree);

    if (n == 0)
        return 0;
    if (parent == 0) {          /* only a root is recorded and marked */
        tree->last = n;
        n->depth = 1;
        do {
            n->parent = 0;
        } while (0);
        n->root = 1;
    }
    switch (kind) {
    case 2:
        parent->child = n;
        n->parent = parent;
        break;
    case 3:
        parent->next = n;
        n->parent = parent;
        break;
    }
    n->next = 0;
    n->used = 4;
    n->child = 0;
    n->flags = 0;
    n->unk_1c = 0;
    return n;
}

/* 0x00cacca8 */
MarsNode *mars_tr_find_tip(MarsTree *tree)
{
    MarsNode *tip = 0;
    MarsNode *n = tree->last;

    while (n) {
        if (n->next)
            tip = n->next;
        n = n->child;
    }
    return tip;
}

/* 0x00cacce0 */
void mars_tr_leaves_activate(MarsTree *tree, MarsNode *n, int *cnt)
{
    if (n->depth >= 2 && n->child == 0 && n->next == 0 && !(n->flags & 1)) {
        int k = mars_get_level((int)tree->boss);

        n->shot_reload = (int)(cmTreeShotT[k] * FPS);
        n->shot_cnt = (int)((mars_tr_leaf_delay + mars_tr_leaf_delay_step * (float)*cnt) * FPS);
        (*cnt)++;
        lock_init(4, &n->lock, tree, &n->wpos, 0, cmTreeHP[k], 0);
        lockon_entry(&n->lock);
        n->lock.flags &= ~LOCK_LOCKABLE;
        n->flags |= 1;
        tree->leaves_set = ++tree->leaves_alive;
        n->used = 6;
    }
    if (n->child)
        mars_tr_leaves_activate(tree, n->child, cnt);
    if (n->next)
        mars_tr_leaves_activate(tree, n->next, cnt);
}

/* 0x00cace58 */
void mars_tr_fire_walk_b(MarsTree *tree, MarsNode *n)
{
    MarsNode *sub;

    if (n->shot_cnt != 0 && (n->flags & 4)) {
        if (!(n->flags & 8)) {
            n->flags |= 8;
            n->lock.flags |= LOCK_LOCKABLE;
        }
        if (--n->shot_cnt <= 0) {
            int k = mars_get_level((int)tree->parent);

            n->shots++;
            n->shot_cnt = n->shot_reload;
            if (n->shots >= cmTreeNbNum[k])
                n->flags |= 0x10;
            mars_nb_fire_ring(tree->parent, &n->wpos, tree->shot_mdl, tree->shot_break_mdl, 1);
        }
    }
    if (n->spread_cnt != 0 && --n->spread_cnt <= 0) {
        n->spread_cnt = n->spread_reload;
        if (n->depth >= 2) {
            sub = n->child;
            if (sub && !(sub->flags & 4))
                MARS_ACTIVATE(sub, n);
            sub = n->next;
            if (sub && !(sub->flags & 4))
                MARS_ACTIVATE(sub, n);
        } else if (n->child) {
            sub = n->child;
            if ((tree->tip == n || sub->depth == 1) && !(sub->flags & 4))
                MARS_ACTIVATE(sub, n);
        } else {
            sub = n->next;
            if (sub && !(sub->flags & 4))
                MARS_ACTIVATE(sub, n);
        }
    }
    if (n->child)
        mars_tr_fire_walk_b(tree, n->child);
    if (n->next)
        mars_tr_fire_walk_b(tree, n->next);
}

/* 0x00cad028 */
void mars_tr_fire_walk(MarsTree *tree, MarsNode *n)
{
    MarsNode *sub;

    if (n->shot_cnt != 0 && (n->flags & 4)) {
        if (!(n->flags & 8)) {
            n->flags |= 8;
            n->lock.flags |= LOCK_LOCKABLE;
        }
        if (--n->shot_cnt <= 0) {
            int k = mars_get_level((int)tree->parent);

            n->shots++;
            n->shot_cnt = n->shot_reload;
            if (n->shots >= cmTreeNbNum[k])
                n->flags |= 0x10;
            mars_nb_fire_ring(tree->parent, &n->wpos, tree->shot_mdl, tree->shot_break_mdl, 1);
        }
    }
    if (n->spread_cnt != 0 && --n->spread_cnt <= 0) {
        n->spread_cnt = n->spread_reload;
        sub = n->child;
        if (sub && !(sub->flags & 4)) {
            sub->flags |= 4;
            sub->spread_cnt = sub->spread_reload = n->spread_reload;
        }
        sub = n->next;
        if (sub && !(sub->flags & 4)) {
            sub->flags |= 4;
            sub->spread_cnt = sub->spread_reload = n->spread_reload;
        }
    }
    if (n->child)
        mars_tr_fire_walk(tree, n->child);
    if (n->next)
        mars_tr_fire_walk(tree, n->next);
}

/* 0x00cad198 */
void mars_tr_locks_remove(MarsNode *n)
{
        if (n->flags & 1) {
            lock_remove_entry(4, &n->lock);
            n->flags &= ~1;
        }
        if (n->child)
            mars_tr_locks_remove(n->child);
    if (n->next)
        mars_tr_locks_remove(n->next);
}

/* 0x00cad200 */
void mars_tr_node_shatter_all(MarsTree *tree, MarsNode *n)
{
    int ang[3];
    Vec3 sc;
    Vec3 halo_pos;

    if (n->parent)
        n->parent->child = 0;
    for (; n; n = n->child) {
        ang[2] = 0;
        sc.x = sc.y = mars_tr_break_size;
        sc.z = vec3_length(&n->ofs) * mars_tr_break_len;
        do {
            ang[1] = RAD_TO_ANG(atan2f(-n->ofs.x, -n->ofs.z));
            ang[0] = RAD_TO_ANG(atan2f(n->ofs.y, sqrtf(n->ofs.x * n->ofs.x + n->ofs.z * n->ofs.z)));
        } while (0);
        if (mars_debris_bounded()) {
            CmWireBreakWork *brk = cm_wire_break_scaled(tree->parent, 13, tree->break_mdl, &n->pos, 0, ang, &sc, mars_tr_break_col);

            cm_wire_break_set_cb(brk, (void (*)(void *, Vec3 *, Vec3 *))mars_bounds_reflect);
        } else {
            cm_wire_break_scaled(tree->parent, 5, tree->break_mdl, &n->pos, 0, ang, &sc, mars_tr_break_col);
        }
        scroll_relative(&n->pos, &halo_pos);
        cm_circle_create(tree->parent, &halo_pos, mars_tree_cols[2], mars_tr_halo_size, 0.8f, 0.9f);
    }
}

/* 0x00cad418 */
MarsNode *mars_tr_node_break(MarsTree *tree, MarsNode *n, int flag)
{
    int ang[3];
    Vec3 sc;
    Vec3 halo_pos;
    MarsNode *cut;

    for (;;) {
        ang[2] = 0;
        sc.x = sc.y = mars_tr_break_size;
        sc.z = vec3_length(&n->ofs) * mars_tr_break_len;
        do {
            ang[1] = RAD_TO_ANG(atan2f(-n->ofs.x, -n->ofs.z));
            ang[0] = RAD_TO_ANG(atan2f(n->ofs.y, sqrtf(n->ofs.x * n->ofs.x + n->ofs.z * n->ofs.z)));
        } while (0);
        if (mars_debris_bounded()) {
            CmWireBreakWork *brk = cm_wire_break_scaled(tree->parent, 13, tree->break_mdl, &n->pos, 0, ang, &sc, mars_tr_break_col);

            cm_wire_break_set_cb(brk, (void (*)(void *, Vec3 *, Vec3 *))mars_bounds_reflect);
        } else {
            cm_wire_break_scaled(tree->parent, 5, tree->break_mdl, &n->pos, 0, ang, &sc, mars_tr_break_col);
        }
        if (flag) {
            scroll_relative(&n->pos, &halo_pos);
            cm_circle_create(tree->parent, &halo_pos, mars_tree_cols[2], mars_tr_halo_size, 0.8f, 0.9f);
        }
        n->used = 0;
        if (n->flags & 1) {
            lock_remove_entry(4, &n->lock);
            n->flags &= ~1;
        }
        cut = n;
        n = n->parent;
        cut->child = 0;
        cut->next = 0;
        cut->parent = 0;
        if (n == 0) {
            tree->state += 1;
        } else {
            if (n->next == 0)
                continue;
            if (cut == n->next) {
                n->next = 0;
                n->flags |= 2;
            } else {
                n->child = n->next;
                n->flags |= 2;
                n->next = 0;
            }
        }
        return n;
    }
}

/* 0x00cad6a0 */
void mars_tr_hit_walk(MarsTree *tree, MarsNode *n, int *out)
{
    if (n->flags & 1) {
        *out = 1;
        if (n->flags & 0x10) {
            boss_se_play(17, (int)&n->lock, 0);
            lock_remove_entry(4, &n->lock);
            n->flags &= ~1;
            n = mars_tr_node_break(tree, n, 0);
            if (n == 0)
                return;
        } else if (n->lock.flags & LOCK_HIT) {
            if (n->lock.life <= 0) {
                boss_se_play(17, (int)&n->lock, 0);
                lock_remove_entry(4, &n->lock);
                lock_score(4, &n->lock);
                n->flags &= ~1;
                tree->leaves_alive -= 1;
                n = mars_tr_node_break(tree, n, 1);
                if (n == 0)
                    return;
            } else {
                boss_se_play(16, (int)&n->lock, 0);
            }
        }
    }
    if (n->child)
        mars_tr_hit_walk(tree, n->child, out);
    if (n->next)
        mars_tr_hit_walk(tree, n->next, out);
}

/* 0x00cad7f8 */
int mars_tr_grow_ring(MarsTree *tree, MarsNode *n, int mode)
{
    MarsNode *added = mars_tr_node_add(tree, n, 2);
    Vec3 target;
    float sc[2];
    float k;
    int v;

    if (added == 0)
        return 0;
    v3_sum(&added->pos, &n->pos, &n->ofs);
    added->depth = n->depth;
    k = 1.0f - (float)tree->left / (float)tree->base;
    target.z = tree->tip_pos.z - mars_tr_ring_ofs + tree->axis_z.z * k * (tree->ring_speed * (float)tree->base);
    target.y = mars_curve_y((struct MarsWork *)tree->boss, target.z - tree->pos->z) * mars_tr_ring_height;
    target.x = 0.0f;
    ROT_Z((int)DEG_TO_ANG(tree->ring_turns * 360.0f * k) + tree->ang2[2], sc, target.x, target.y);
    target.y += tree->tip_pos.y;
    added->used = 7;
    v = (int)(mars_tr_grow_sec * FPS);
    added->grow_cnt = added->grow_reload = v;
    v3_sub(&added->ofs, &target, &added->pos);
    return 1;
}

/* 0x00cad9f8 */
int mars_tr_grow_inward(MarsTree *tree, MarsNode *n, int mode)
{
    MarsNode *added = mars_tr_node_add(tree, n, 3);
    Vec3 target;
    int v;

    if (added == 0)
        return 0;
    added->depth = n->depth + 1;
    v3_sum(&added->pos, &n->pos, &n->ofs);
    target.z = added->pos.z;
    target.y = tree->tip_pos.y;
    target.x = 0.0f;
    v3_sub(&added->ofs, &target, &added->pos);
    vec3_normalize(&added->ofs, &added->ofs);
    v3_scale(&added->ofs, mars_tr_inward_len);
    added->used = 4;
    v = (int)(mars_tr_grow_sec * FPS);
    added->grow_cnt = added->grow_reload = v;
    return 1;
}

/* 0x00cadb48 */
int mars_tr_grow_fork(MarsTree *tree, MarsNode *n, int mode)
{
    MarsNode *added;
    float sc[4][2];
    int ang_x, ang_y;
    int v;

    added = mars_tr_node_add(tree, n, 3);
    if (added == 0)
        return 0;
    added->depth = n->depth + 1;
    v3_sum(&added->pos, &n->pos, &n->ofs);
    v3_copy(&added->ofs, &n->ofs);
    ang_x = mars_tr_rand_ang(mars_tr_fork_ang_lo, mars_tr_fork_ang_hi);
    ang_y = mars_tr_rand_ang(mars_tr_fork_ang_lo, mars_tr_fork_ang_hi);
    ROT_X(ang_x, sc[0], added->ofs.y, added->ofs.z);
    ROT_Y(ang_y, sc[1], added->ofs.x, added->ofs.z);
    {
        int k = (int)(mars_tr_grow_sec * FPS);
        added->used = 4;
        added->grow_cnt = added->grow_reload = k;
    }
    added = mars_tr_node_add(tree, n, 2);
    if (added == 0)
        return 0;
    added->depth = n->depth + 1;
    v3_sum(&added->pos, &n->pos, &n->ofs);
    v3_copy(&added->ofs, &n->ofs);
    ROT_X(-ang_x, sc[2], added->ofs.y, added->ofs.z);
    ROT_Y(-ang_y, sc[3], added->ofs.x, added->ofs.z);
    v = (int)(mars_tr_grow_sec * FPS);
    added->used = 4;
    added->grow_cnt = added->grow_reload = v;
    return 1;
}

/* 0x00cade50 */
void mars_tr_move_all(MarsTree *tree, MarsNode *n, Vec3 *delta)
{
    v3_add(&n->pos, delta);
    if (n->flags & 1)
        v3_sum(&n->wpos, &n->pos, &n->ofs);
    if (n->child)
        mars_tr_move_all(tree, n->child, delta);
    if (n->next)
        mars_tr_move_all(tree, n->next, delta);
}

/* 0x00cadf20 */
void mars_tr_move_rotate(MarsTree *tree, MarsNode *n, Vec3 *delta, int ang)
{
    Vec3 rel;
    float sc[2][2];
    Vec3 *ctr;

    v3_add(&n->pos, delta);
    ctr = tree->pos;
    v3_sub(&rel, &n->pos, ctr);
    ROT_Z(ang, sc[0], rel.x, rel.y);
    ROT_Z(ang, sc[1], n->ofs.x, n->ofs.y);
    v3_sum(&n->pos, &rel, tree->pos);
    if (n->flags & 1)
        v3_sum(&n->wpos, &n->pos, &n->ofs);
    if (n->child)
        mars_tr_move_rotate(tree, n->child, delta, ang);
    if (n->next)
        mars_tr_move_rotate(tree, n->next, delta, ang);
}

/* 0x00cae108 */
int mars_tr_grow_walk(MarsTree *tree, MarsNode *n, int *st)
{
    int stop = -1;

    if (*st == stop)
        return 1;
    if (n->used == 7) {
        if (*st == 0)
            *st = 1;
        if (--n->grow_cnt <= 0) {
            n->used = 5;
            if (tree->left) {
                int left = --tree->left;

                if (tree->grow) {
                    if (left <= 0 || mars_tr_grow_ring(tree, n, -1) == 0)
                        goto fail;
                } else {
                    if (left <= 0)
                        goto fail;
                    if (left % mars_tr_fork_every == 0) {
                        if (mars_tr_grow_ring(tree, n, -1) == 0)
                            goto fail;
                        mars_tr_grow_inward(tree, n, -1);
                        goto next;
                    }
                    if (mars_tr_grow_ring(tree, n, -1) == 0) {
                    fail:
                        *st = stop;
                        tree->broken = n;
                        mars_tr_grow_inward(tree, n, -1);
                    }
                }
            }
        }
    } else if (n->used == 4) {
        if (--n->grow_cnt <= 0) {
            n->used = 5;
            if (n->depth < mars_tr_depth_bits)
                mars_tr_grow_fork(tree, n, -1);
            else if (*st == 0)
                *st = -2;
        }
    }
next:
    if (n->child)
        mars_tr_grow_walk(tree, n->child, st);
    if (n->next)
        mars_tr_grow_walk(tree, n->next, st);
    return *st == 0;
}

/* 0x00cae2c8 */
void mars_tr_draw_walk(MarsTree *tree, int node_addr)
{
    MarsNode *n = (MarsNode *)node_addr;
    int ang[2];
    Vec3 pt;
    float k;
    int list;

    k = 1.0f - (float)n->grow_cnt / (float)n->grow_reload;
    ulMatrixPush();
    do {
        ang[1] = RAD_TO_ANG(atan2f(-n->ofs.x, -n->ofs.z));
        ang[0] = RAD_TO_ANG(atan2f(n->ofs.y, sqrtf(n->ofs.x * n->ofs.x + n->ofs.z * n->ofs.z)));
    } while (0);
    MTX_TRANS(&n->pos);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_scale(ulMatrixCur, mars_tr_break_size, mars_tr_break_size, vec3_length(&n->ofs) * k * mars_tr_break_len);
    dc_object_draw(tree->mdl);
    ulMatrixPop();
    if (n->flags & 4) {
        list = RenderSetCurrentDisplayList(6);
        if (n->flags & 1) {
            v3_sum(&pt, &n->pos, &n->ofs);
            k = 1.0f - (float)n->shot_cnt / (float)n->shot_reload;
            k *= mars_tr_flare_scl;
            if (k <= mars_tr_flare_max)
                k = mars_tr_flare_max;
            draw_set_blend(4, 1);
            flare_draw(tree->flare_tex_a, mars_tree_cols[0], &pt, 0, k);
        }
        k = 1.0f - (float)n->spread_cnt / (float)n->spread_reload;
        {
            Vec3 *ofs = &n->ofs, *pos = &n->pos;

            pt.x = ofs->x * k + pos->x;
            pt.y = ofs->y * k + pos->y;
            pt.z = ofs->z * k + pos->z;
        }
        draw_set_blend(4, 1);
        flare_draw(tree->flare_tex_b, mars_tree_cols[1], &pt, 0, mars_tr_flare_max);
        RenderSetCurrentDisplayList(list);
    }
    if (n->child)
        mars_tr_draw_walk(tree, (int)n->child);
    if (n->next)
        mars_tr_draw_walk(tree, (int)n->next);
}

/* steer(alias): stores outside the struct, so the global loads stay after them */
/* 0x00cae630 */
void mars_tr_init(char *w, int *arg)
{
    MarsNode *root;
    float sc[3][2];
    MarsTreeArg *tree_arg = (MarsTreeArg *)arg;
    int v;
    int *rot;
    Vec3 *pos;

    *(int *)(w + 104) = *(int *)(w + 108) = tree_arg->frames;
    mars_tr_pool_alloc((MarsTree *)w);
    rot = tree_arg->rot;
    *(ModelPart **)(w + 156) = tree_arg->mdl[0];
    *(ModelPart **)(w + 160) = tree_arg->mdl[1];
    *(ModelPart **)(w + 164) = tree_arg->mdl[2];
    *(ModelPart **)(w + 168) = tree_arg->shot_mdl;
    *(Vec3 **)(w + 176) = tree_arg->boss;
    pos = tree_arg->pos;
    *(ModelPart **)(w + 172) = tree_arg->shot_break_mdl;
    *(Vec3 **)(w + 124) = pos;
    *(int **)(w + 128) = rot;
    i3_copy((int *)(w + 132), rot);
    i3_copy((int *)(w + 144), tree_arg->ang2);
    *(int *)(w + 100) = tree_arg->grow;
    *(const int **)(w + 92) = tree_arg->alive;
    *(void **)(w + 180) = tree_arg->parent;
    *(float *)(w + 192) = tree_arg->ring_speed;
    *(int *)(w + 116) = tree_arg->flare_tex_a;
    *(float *)(w + 196) = tree_arg->ring_turns;
    *(int *)(w + 120) = tree_arg->flare_tex_b;
    v3_copy((Vec3 *)(w + 68), tree_arg->dir);
    *(int *)(w + 32) = tree_arg->grow;
    *(void **)(w + 188) = (void *)tree_arg->frame_base;
    *(int *)(w + 184) = tree_arg->done;
    v3_sum((Vec3 *)(w + 44), pos, tree_arg->dir);
    root = mars_tr_node_add((MarsTree *)w, 0, 1);
    *(float *)(w + 56) = *(float *)(w + 60) = 0.0f;
    *(float *)(w + 64) = 1.0f;
    *(float *)(w + 80) = *(float *)(w + 84) = 0.0f;
    *(float *)(w + 88) = 1.0f;
    ROT_X(**(int **)(w + 128) + ANG_180, sc[0], ((Vec3 *)(w + 68))->y, ((Vec3 *)(w + 68))->z);
    ROT_X(**(int **)(w + 128) + ANG_180, sc[1], ((Vec3 *)(w + 56))->y, ((Vec3 *)(w + 56))->z);
    ROT_X(**(int **)(w + 128) + ANG_180, sc[2], ((Vec3 *)(w + 80))->y, ((Vec3 *)(w + 80))->z);
    v3_copy(&root->pos, (Vec3 *)(w + 44));
    root->used = 7;
    v = (int)(mars_tr_grow_sec * FPS);
    root->grow_cnt = root->grow_reload = v;
    v3_mul(&root->ofs, (Vec3 *)(w + 80), mars_tr_ring_ofs);
    *(int *)(w + 40) = 10;
}

/* 0x00cae950 */
void mars_tr_exec(MarsTree *w)
{
    Vec3 move;
    int st, leaf_cnt, grow_st;
    int *src_ang = w->rot;
    int n, reload;
    MarsNode *node;

    int da;

    v3_sub(&move, &w->tip_pos, &w->dir);
    da = src_ang[2] - w->ang[2];
    i3_copy(w->ang, src_ang);
    v3_sub(&move, w->pos, &move);
    mars_tr_move_rotate(w, w->last, &move, da);
    v3_sum(&w->tip_pos, w->pos, &w->dir);
    n = enemy_status_get_ticks();
    switch (w->state) {
    case 14:
        mars_tr_node_break(w, w->broken, 1);
        goto kill;
    case 12:
        for (; n; n--) {
            if (w->grow)
                mars_tr_fire_walk(w, w->last);
            else
                mars_tr_fire_walk_b(w, w->last);
        }
        st = 0;
        if (w->grow)
            mars_tr_hit_walk(w, w->last, &st);
        else
            mars_tr_hit_walk(w, w->tip, &st);
        if (st == 0) {
            w->state = 11;
            node = mars_tr_find_tip(w);
            if (node && node->parent && node->parent->child)
                mars_tr_node_shatter_all(w, node->parent->child);
        }
        break;
    case 11:
        node = w->last;
        if (node) {
            reload = (int)(mars_tr_tick_sec * FPS);
            leaf_cnt = 0;
            node->flags |= 4;
            node->spread_cnt = node->spread_reload = reload;
            if (w->grow) {
                mars_tr_leaves_activate(w, node, &leaf_cnt);
                if (leaf_cnt) {
                    mars_tr_set_done((struct MarsTrManWork *)w->done);
                    w->state = 12;
                } else {
                    w->state = 14;
                }
            } else {
                node = mars_tr_find_tip(w);
                w->tip = node;
                if (node == 0) {
                    w->state = 14;
                    return;
                }
                mars_tr_leaves_activate(w, node, &leaf_cnt);
                if (leaf_cnt) {
                    mars_tr_set_done((struct MarsTrManWork *)w->done);
                    w->state = 12;
                } else {
                    w->state = 14;
                }
            }
        } else {
            w->state = 14;
        }
        break;
    case 10:
        for (; n; n--) {
            grow_st = 0;
            mars_tr_grow_walk(w, w->last, &grow_st);
            if (grow_st == -2)
                w->state = 13;
        }
        break;
    case 13:
        break;
    }
    if (w->last == 0)
        FINISH_IF(w);
    if (*w->alive == -1)
        goto kill;
    if (*w->alive)
        return;
kill:
    FINISH_IF(w);
}

/* 0x00caecb8 */
void mars_tr_disp(MarsTree *w)
{
    int old;
    int save;

    ulGxSetTextureList(w->textures);
    old = RenderSetCurrentDisplayList(6);
    ulGxSetTextureList(w->textures);
    RenderSetCurrentDisplayList(old);
    save = draw_set_override_flags(0);
    draw_push_strip_flags();
    draw_push_override_color();
    draw_set_color(col15);
    mars_tr_draw_walk(w, (int)w->last);
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(save);
}

/* 0x00caed48 */
void mars_tr_release(MarsTree *tree)
{
    mars_tr_locks_remove(tree->last);
    heap_free(tree->pool, 0);
    tree->last = 0;
    tree->pool = 0;
    tree->n = 0;
}

/* 0x00caed88 */
int mars_tr_take_state13(MarsTree *w)
{
    if (w->state == 13) {
        w->state = 11;
        return 1;
    }
    return 0;
}

/* 0x00caedb0 */
int mars_tr_leaf_count(MarsTree *w)
{
    if (w->leaves_set)
        return w->leaves_alive;
    return 16;
}

/* 0x00caedd0 */
void *mars_tr_create(void *parent, void *arg)
{
    return task_create_inherit_arg(parent, &mars_tr_desc, 208, arg);
}

/* 0x00caedf8 */
void mars_tr_set_done(MarsTrManWork *w)
{
    w->fired = 1;
}

/* 0x00caee08 */
void mars_tr_man_spawn(MarsTrManWork *w)
{
    int arr[32];
    int k = 0;
    int i;
    int trees;
    void *boss;

    boss_se_play(19, 0, (int)&w->pos);
    i3_set(w->ang, 0, 0, 0);
    v3_set(&w->pos, 0.0f, 0.0f, 0.0f);
    trees = mars_tr_man_trees;
    boss = w->boss;
    w->tree.dir = &w->dir;
    w->tree.pos = &w->pos;
    w->tree.rot = w->ang;
    w->tree.ang2[2] = w->tree.ang2[0] = 0;
    w->tree.ang2[1] = 0;
    w->tree.ang2[0] = 0;
    w->tree.boss = (Vec3 *)boss;
    w->tree.parent = boss;
    w->tree.frames = trees;
    w->tree.done = (int)w;
    if (w->tree.grow == 0) {
        w->tree.frame_base = mars_tr_man_wait_a;
        w->tree.ring_speed = mars_tr_man_grow_b;
        w->tree.ring_turns = mars_tr_man_scl_b;
        w->tree_cnt = 1;
        if (!w->trees)
            w->trees = heap_block_tag(game_calloc(4), str_mars_tree_c, 1016);
    } else {
        w->tree.frame_base = mars_tr_man_wait_b;
        w->tree.ring_speed = mars_tr_man_grow_a;
        w->tree.ring_turns = mars_tr_man_scl_a;
        if (w->waves == mars_tr_man_mode_b) {
            *(int *)((char *)w + 112) = mars_tr_man_kind_a; /* steer(alias): tree_cnt, stored outside the struct */
            k = mars_tr_man_num_a;
        } else {
            *(int *)((char *)w + 112) = mars_tr_man_kinds_b; /* steer(alias) */
            k = mars_tr_man_nums_b;
        }
        if (!w->trees)
            w->trees = heap_block_tag(game_calloc(w->tree_cnt * 4), str_mars_tree_c, 1029);
    }
    for (i = 0; i < w->tree_cnt; i++)
        arr[i] = k * i + w->tree.frame_base;
    shuffle_ints(arr + 1, w->tree_cnt - 1);
    for (i = 0; i < w->tree_cnt; i++) {
        w->tree.frames = arr[i];
        w->tree.ang2[2] += (int)DEG_TO_ANG(360.0f / (float)w->tree_cnt);
        w->trees[i] = mars_tr_create(w, &w->tree);
    }
    w->dir.z = -mars_tr_man_spawn_z;
    w->waves -= 1;
}

/* 0x00caf0a0 */
void mars_tr_man_exec(MarsTrManWork *w)
{
    Vec3 pl_vel;
    Vec3 saved;
    MarsTree **list;
    int i, n;
    int cnt, sum, v;

    player_get_frame_vel(&pl_vel);
    v3_add(&w->pos, &pl_vel);
    switch (w->state) {
    case 0:
        n = w->tree_cnt;
        for (i = 0, cnt = 0; i < n; i++) {
            MarsTree *tree = w->trees[i];

            if (tree && tree->state == 13)
                cnt++;
        }
        if (n == cnt)
            w->state = 1;
        break;
    case 1:
        for (cnt = 0, i = 0; i < w->tree_cnt; i++) {
            cnt = (cnt + 1) % w->tree_cnt;
            if (w->trees[cnt] && mars_tr_take_state13(w->trees[cnt])) {
                w->cur = cnt;
                w->state = 2;
                break;
            }
        }
        break;
    case 2:
        {
            MarsTree *tree = w->trees[w->cur];

            if (tree == 0 || (TASK_OF(tree)->flags & TASK_F_END)) {
                w->state = 1;
                w->fired = 0;
            }
        }
        break;
    }
    sum = 0;
    cnt = 0;
    for (i = 0; i < w->tree_cnt; i++) {
        list = w->trees;
        if (list[i] == 0)
            continue;
        if (TASK_OF(list[i])->flags & TASK_F_END) {
            list[i] = 0;
            continue;
        }
        cnt++;
        v = mars_tr_leaf_count(list[i]);
        {
            int alive = v > 0;

            sum += alive * v;
        }
    }
    if (w->waves == 0 && sum <= mars_tr_man_end_left && w->ended == 0) {
        mars_request_next((struct MarsWork *)w->boss);
        w->ended = 1;
    }
    if (cnt == 0) {
        if (w->waves) {
            v3_copy(&saved, &w->pos);
            mars_tr_man_spawn(w);
            v3_copy(&w->pos, &saved);
            w->state = 0;
        } else {
            FINISH_IF(w);
        }
    }
    if (beat_grid_hit(0) && w->fired)
        boss_se_play(18, 0, (int)&w->pos);
}

/* 0x00caf380 */
void mars_tr_man_release(MarsTrManWork *w)
{
    heap_free(w->trees, 0);
}

/* 0x00caf390 */
void *mars_tr_man_create(void *parent, int *arg)
{
    MarsTrManWork *w = task_create_inherit(parent, &mars_tr_man_desc, 168);
    Vec3 pl_vel;

    w->boss = parent;
    w->tree = *(MarsTreeArg *)arg;
    if (w->tree.grow < 0)
        w->tree.grow = mars_tr_man_dir;
    {
        int i = mars_tr_man_mode_b;
        float dist_z = -mars_tr_man_z;
        w->waves = i;
        w->dir.z = dist_z;
    }
    mars_tr_man_spawn(w);
    player_get_frame_vel(&pl_vel);
    mars_pose_cb(w->boss, &w->pos, w->ang);
    v3_sub(&w->pos, &w->pos, &pl_vel);
    return w;
}
