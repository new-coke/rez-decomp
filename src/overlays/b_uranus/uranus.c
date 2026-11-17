/*
 * Uranus boss: starts the music, course map, sequence task and part manager, and ends the stage
 * when the finish countdown runs out. Also the node pool the forms are built from, collision
 * records from the course meshes (triangles, cylinders, walls, planes) and the fog fade task.
 */

#include "types_gw.h"
#include "b_uranus.h"
#include "mathdef.h"
#include "debug.h"
#include "debugdraw.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "light.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "player_status.h"
#include "task.h"
#include "texforce.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "uranus.h"
#include "uranus_bullet.h"
#include "uranus_map.h"
#include "uranus_part.h"
#include "uranus_snake.h"
#include "../common/boss_common.h"
#include "../common/boss_demo_mes.h"
#include "../common/boss_sound.h"
#include "../common/boss_texture_anime.h"
#include "../common/boss_wire.h"
#include "../common/w_acos.h"

extern unsigned int stage_exit_flags;

extern float cmd_damage;
extern const unsigned int star_color;
extern BossStarCtl *star_ctl;
extern UranusNode *uranus_nodes;
extern int uranus_node_cnt;
extern int uranus_level;
extern int cmd_fin_frm;
extern void *venus_fog_task;
extern int venus_fog_on;
extern struct TaskDesc uranus_tc_desc;
extern struct TaskDesc uranus_desc;
extern struct TaskDesc uranus_seq_desc;
extern char str_uranus_c[];
extern int cm_fog_col[][2];
extern float cmd_fog_n;
extern float cmd_fog_f;
extern int uranus_files[3][2];
extern char uranus_vardef[];
extern GwVarDef uranus_vardefs[];
extern int uranus_vars_group;
extern void *uranus_work;
extern char str_uranus[];
extern BossModelDef uranus_model_defs[];
extern float uranus_part_radii[];
extern int uranus_end_tex_anims[6][2];
extern int uranus_wire_files[];
extern int cmd_fog_t;
extern int cmd_fog_sw;
extern int cm_deb_disp;
extern char str_uranus_link_fmt[];
extern unsigned int cm_fog_col_fin;
extern struct TaskDesc venus_fog_desc;
typedef struct UranusVtx { Vec3 pos; Vec3 nrm; } UranusVtx;
extern Vec3 uranus_col_up;

/* boss sequence task, 12 bytes */
typedef struct UranusSeqWork {
    char pad_00[8];
    UranusWork *boss;           /* 0x08 */
} UranusSeqWork;

typedef struct TaskExecView {
    char pad_00[16];
    int exec;                   /* 0x10 */
} TaskExecView;

/* 0x00ca0700 */
void *module_entry(void *parent, void *arg)
{
    texforce_disable();
    return uranus_create(parent, arg);
}

/* 0x00ca0740 */
void star_color_apply(void)
{
    if (star_ctl)
        boss_star_set_color(star_ctl, star_color);
}

/* 0x00ca0760 */
void *uranus_tc_create(void *parent)
{
    return task_create_inherit(parent, &uranus_tc_desc, 32);
}

/* 0x00ca0788 */
UranusSeqWork *uranus_seq_create(UranusWork *parent)
{
    UranusSeqWork *seq = task_create_inherit(parent, &uranus_seq_desc, 12);

    seq->boss = parent;
    return seq;
}

/* 0x00ca07c0 */
int boss_main_demo_cb(BossMainWork *w)
{
    if (w->state == 0)
        w->state = 1;
    else if (w->state == 2)
        return 1;
    return 0;
}

/* steer(cast) */
/* 0x00ca07f0 */
void uranus_init(UranusWork *w, int *arg)
{
    w->arg0 = arg[0];
    w->level = arg[1];
    w->phase = 0;
    uranus_level = w->arg0;
    venus_fog_task = 0;
    venus_fog_on = 1;
    boss_files_request((int)uranus_files[0], (int)uranus_files[1]);
    boss_start((int *)w, (int)w->parts, w->arg0, w->level);
    boss_bgm_start(w);
    cm_se_laser_create(uranus_files[2]);
    score_vars_add();
    uranus_nodes_init();
    uranus_1st_demo_create(w, (int (*)(int))boss_main_demo_cb, (int)w);
    boss_demo_mes_create(w, str_uranus);
    uranus_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, uranus_vardef);
    gw_varedit_add_table(&gw_varedit_root, uranus_vars_group, uranus_vardefs);
    uranus_work = w;
    uranus_laser_vars_add();
    uranus_bullet_vars_add();
    duos_vars_add();
}

/* 0x00ca08f8 */
void uranus_exec_start(UranusWork *w)
{
    if (w->state != 1)
        return;
    w->map = uranus_map_create(w, w->parts, 4, (Vec3 *)&w->unk_14);
    w->seq = uranus_seq_create(w);
    w->tc = uranus_tc_create(w);
    w->pm = uranus_pm_create(w);
    w->state = 2;
    boss_star_fade(w->stars, star_color, 120);
    do { TASK_OF(w)->exec = 0; } while (0); /* steer(barrier) */
    boss_adx_request();
}

/* 0x00ca0990 */
void uranus_exec_load(UranusWork *w)
{
    FogArg arg;

    if (boss_files_ready(uranus_files[0], uranus_files[1])) {
        do {
            w->models = model_pnb_load((char *)uranus_files[0]);
            w->textures = model_load_timed((char *)uranus_files[1], w->arg0);
        } while (0); /* steer(barrier) */
        boss_parts_build((void **)w, uranus_model_defs, w->parts, w->pool);
        parts_set_radii(w->parts, uranus_part_radii);
        TASK_OF(w)->exec = (void (*)(void *))uranus_exec_start;
        arg.range = 400.0f;
        arg.k = 0.5f;
        arg.count = 200;
        arg.color = star_color & 0xffffff;
        star_ctl = w->stars = boss_star_create(w, &arg, 0.0f);
        w->phase = 1;
    }
}

/* steer(cast) */
/* 0x00ca0a98 */
void uranus_release(UranusWork *w)
{
    float n;

    light_reset();
    light_fog_set();
    if (boss_parts_release(w->parts)) {
        model_unload_timed(w->textures);
        model_pnb_unload(w->models);
        w->textures = 0;
        w->models = 0;
    }
    BackColReq(0x80000000);
    n = fog_get_near();
    fog_set(0x80000000, 4, n, fog_get_far());
    fog_set_enable((void *)1);
    boss_end(w);
    uranus_nodes_free();
    score_vars_remove();
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)uranus_vardef);
    uranus_laser_vars_remove();
    uranus_bullet_vars_remove();
    duos_vars_remove();
}

/* 0x00ca0b68 */
void uranus_end_tex_anime_start(void *w)
{
    int *anim = uranus_end_tex_anims[(unsigned int)player_status_get_level() % 6];

    if (anim[1])
        tex_anime_start(w, anim, boss_get_arg0(), 120);
    else
        tex_anime_start(0, 0, 0, 0);
}

/* 0x00ca0c00 */
void uranus_start_finish(UranusWork *w)
{
    w->phase = 2;
    w->fin_wait = cmd_fin_frm;
}

/* 0x00ca0c18 */
void uranus_seq_exec(UranusSeqWork *seq)
{
    UranusWork *w = seq->boss;

    switch (w->phase) {
    case 2:
        if (--w->fin_wait <= 0)
            w->phase = 3;
        break;
    case 3:
        ((void (*)(void *))boss_fx_release)(w);
        ((TaskExecView *)TASK_OF(w))->exec = 0; /* steer(alias) */
        ((TaskExecView *)TASK_OF(*(int *)&w->pm))->exec = 0; /* steer(alias) */
        stage_exit_flags |= 10;
        TASK_OF(*(int *)&w->pm)->disp = 0; /* steer(alias): pm reloaded after the flags store */
        return;
    case 0:
        break;
    }
    if (debug_flags_test(4) && (kbd_state[7] & 0x44) && kbd_state[5] == 16)
        wire_export_list((void **)w, uranus_wire_files, 19);
}

/* 0x00ca0cf0 */
void uranus_get_fog(int *w)
{
    FogReq *req = (FogReq *)w;
    int level = light_mode_index();

    req->color = ((int *)cm_fog_col)[level * 2];
    req->col = ((int *)cm_fog_col)[(level << 1) | 1];
    req->near = cmd_fog_n;
    req->far = cmd_fog_f;
}

/* steer(cast) */
/* 0x00ca0d58 */
void uranus_disp(UranusWork *w)
{
    int sec;

    light_update();
    boss_texlist_bind((int *)w);
    if (cmd_fog_sw) {
        BackColReq(cm_fog_col[0][0]);
        do {
            ((void (*)(int, float, float, int))fog_set)(cm_fog_col[0][1], cmd_fog_n, cmd_fog_f, cmd_fog_t);
        } while (0); /* steer(barrier) */
        fog_set_enable((void *)1);
    }
    if (debug_flags_test(4) && cm_deb_disp) {
        debug_set_origin(10, 45);
        sec = 0;
        if (w->pm)
            sec = uranus_pm_get_sec(w->pm);
        debug_printf(str_uranus_link_fmt, uranus_node_cnt, sec);
    }
}

/* 0x00ca0e38 */
UranusWork *uranus_create(void *parent, void *arg)
{
    return task_create_inherit_arg(parent, &uranus_desc, 6220, (int)arg);
}

/* 0x00ca0e60 */
void task_finish_if(void *w)
{
    FINISH_IF(w);
}

/* 0x00ca0e80 */
int uranus_level_add(int i)
{
    return uranus_level + i;
}

/* 0x00ca0e90 */
void uranus_player_damage(void)
{
    player_status_add_damage(cmd_damage);
}

/* 0x00ca0ea0 */
void uranus_nodes_init(void)
{
    uranus_node_cnt = 0;
    uranus_nodes = heap_block_tag(game_calloc(16384), str_uranus_c, 481);
}

/* 0x00ca0ee8 */
void uranus_nodes_free(void)
{
    heap_free(uranus_nodes, 0);
}

/* 0x00ca0ef8 */
UranusNode *uranus_node_alloc(void)
{
    UranusNode *n = uranus_nodes;
    int i;

    for (i = 0; i < 512; i++, n++) {
        if (!n->used) {
            n->index = i;
            n->used = 1;
            n->data = 0;
            n->flags = 0;
            n->obj = 0;
            return n;
        }
    }
    return uranus_nodes;
}

/* 0x00ca0f48 */
UranusNode *uranus_node_new(void)
{
    UranusNode *n = uranus_node_alloc();

    uranus_node_cnt++;
    n->next = 0;
    n->child = 0;
    n->parent = 0;
    return n;
}

/* 0x00ca0f88 */
UranusNode *uranus_node_add_child(UranusNode *n)
{
    UranusNode *child = uranus_node_new();
    UranusNode *last;

    if (n->child) {
        for (last = n->child; last->next; last = last->next)
            ;
        last->next = child;
        child->parent = n;
    } else {
        n->child = child;
        child->parent = n;
    }
    return child;
}

/* 0x00ca0ff8 */
UranusNode *uranus_node_add_next(UranusNode *n)
{
    UranusNode *parent = n->parent;
    UranusNode *node = uranus_node_new();

    while (n->next)
        n = n->next;
    n->next = node;
    node->parent = parent;
    return node;
}

/* 0x00ca1060 */
void uranus_node_copy_tree(UranusNode *dst, UranusNode *src)
{
    UranusNode *src_child, *src_grand, *dst_child, *dst_grand;

    if (src) {
        dst_child = uranus_node_add_child(dst);
        for (src_child = src->child; src_child; src_child = src_child->next) {
            dst_grand = uranus_node_add_child(dst_child);
            for (src_grand = src_child->child; src_grand; src_grand = src_grand->next)
                uranus_node_add_child(dst_grand);
        }
    }
}

/* 0x00ca10f0 */
void uranus_node_free(UranusNode *n)
{
    if (uranus_node_cnt > 0) {
        n->used = 0;
        uranus_node_cnt--;
    } else {
        uranus_node_cnt = 0;
    }
}

/* 0x00ca1118 */
void uranus_node_free_tree(UranusNode *n)
{
    if (n->child)
        uranus_node_free_tree(n->child);
    if (n->next)
        uranus_node_free_tree(n->next);
    uranus_node_free(n);
}

/* 0x00ca1168 */
void uranus_node_clear_flags(UranusNode *n, int mask)
{
    if (n->child)
        uranus_node_clear_flags(n->child, mask);
    if (n->next)
        uranus_node_clear_flags(n->next, mask);
    n->flags &= ~mask;
}

/* 0x00ca11c8 */
int uranus_node_count_children(UranusNode *n)
{
    UranusNode *child;
    int count = 0;

    for (child = n->child; child; child = child->next)
        count++;
    return count;
}

/* 0x00ca1200 */
int uranus_node_count_tree(UranusNode *n, int *cnt)
{
    UranusNode *child, *grand;
    int marked = 0;

    cnt[2] = 0;
    cnt[1] = 0;
    cnt[0] = 0;
    if (n) {
        if (n->obj)
            marked++;
        cnt[0]++;
        for (child = n->child; child; child = child->next) {
            if (child->obj)
                marked++;
            cnt[1]++;
            for (grand = child->child; grand; grand = grand->next) {
                if (grand->obj)
                    marked++;
                cnt[2]++;
            }
        }
    }
    return marked;
}

/* steer(cast) */
/* 0x00ca1290 */
void uranus_node_draw_debug(UranusNode *n)
{
    UranusNode *child;

    if (n) {
        ulMatrixPush();
        MTX_TRANS((Vec3 *)n->data);
        debug_draw_box(RGBA(0x80, 0, 0, 0x80), 4.0f);
        ulMatrixPop();
        for (n = n->child; n; n = n->next) {
            ulMatrixPush();
            MTX_TRANS((Vec3 *)n->data);
            ((void (*)(unsigned int, float))debug_draw_box)(RGBA(0, 0x80, 0, 0x80), 2.0f);
            ulMatrixPop();
            for (child = n->child; child; child = child->next) {
                ulMatrixPush();
                MTX_TRANS((Vec3 *)child->data);
                ((void (*)(unsigned int, float))debug_draw_box)(RGBA(0, 0, 0x80, 0x80), 1.0f);
                ulMatrixPop();
            }
        }
    }
}

/* steer(cast) */
/* 0x00ca13b0 */
void venus_fog_exec(VenusFogWork *w)
{
    BossFog fog;

    if (--w->count <= 0) {
        do { FINISH(w); } while (0);
    }
    fog_blend(&w->from, &w->to, &fog, (float)w->count / (float)w->total);
    do {
        BackColReq(fog.back);
    } while (0);
    fog_set(fog.col, cmd_fog_t, fog.near, fog.far);
    fog_set_enable((void *)1);
}

/* 0x00ca1438 */
void venus_fog_release(void)
{
    venus_fog_task = 0;
    if (!venus_fog_on)
        venus_fog_on = 1;
}

/* 0x00ca1460 */
VenusFogWork *venus_fog_create(void *parent, BossFog *to, float secs)
{
    VenusFogWork *w;

    if (!venus_fog_on)
        return 0;
    if (venus_fog_task)
        task_end_with_exit(venus_fog_task);
    w = task_create(parent, &venus_fog_desc, 44);
    venus_fog_task = w;
    w->count = w->total = (int)(secs * FPS);
    fog_get(&w->from);
    if (to) {
        w->to = *to;
    } else {
        venus_fog_on = 0;
        w->to.back = w->to.col = cm_fog_col_fin;
        w->to.near = fog_get_near();
        w->to.far = fog_get_far();
    }
    return w;
}

/* 0x00ca1568 */
void uranus_col_tri_bound(Vec3 *p0, Vec3 *p1, Vec3 *p2, Vec3 *ctr, float *rad, float *rad_sq)
{
    Vec3 ofs;
    float rmax = 0.0f;
    float len;
    float sx = p2->x + (p0->x + p1->x);
    float sy = p2->y + (p0->y + p1->y);
    float sz = p2->z + (p0->z + p1->z);

    ctr->x = sx * 0.33333334f;
    ctr->y = sy * 0.33333334f;
    ctr->z = sz * 0.33333334f;
    ofs.x = ctr->x - p0->x;
    ofs.y = ctr->y - p0->y;
    ofs.z = ctr->z - p0->z;
    len = vec3_length(&ofs);
    if (rmax < len)
        rmax = len;
    ofs.x = ctr->x - p1->x;
    ofs.y = ctr->y - p1->y;
    ofs.z = ctr->z - p1->z;
    len = vec3_length(&ofs);
    if (rmax < len)
        rmax = len;
    ofs.x = ctr->x - p2->x;
    ofs.y = ctr->y - p2->y;
    ofs.z = ctr->z - p2->z;
    len = vec3_length(&ofs);
    if (rmax < len)
        rmax = len;
    *rad = rmax;
    *rad_sq = rmax * rmax;
}

/* 0x00ca1700 */
int uranus_col_strip_build(short *p, UranusVtx *v, int stride, char *rec)
{
    int count = 0;
    int n, len;

    n = *p++;
    while (n) {
        len = *p++;
        if (len < 0)
            len = -len;
        for (len -= 2; len; len--) {
            if (rec) {
                uranus_col_cyl_init(rec, &v[p[0]].pos, &v[p[stride]].pos, &v[p[stride * 2]].pos, &((Vec3 *)v)[p[0] * 2 + 1]);
                rec += 64;
            }
            p += stride;
            count++;
        }
        n--;
        p += stride * 2;
    }
    return count;
}

/* 0x00ca1820 */
int uranus_col_poly_build_cyl(short *p, UranusVtx *v, int mode, char *rec)
{
    int count = 0;
    unsigned short head = *p++;
    int n = (short)head;
    int pad = (n >> 14) & 3;

    n &= 0x3fff;

    while (n) {
        switch (mode) {
        case 3:
            if (rec) {
                uranus_col_cyl_init(rec, &v[p[0]].pos, &v[p[1]].pos, &v[p[2]].pos, &((Vec3 *)v)[p[0] * 2 + 1]);
                rec += 64;
            }
            count++;
            p += (unsigned)(pad + 3);
            break;
        case 4:
            if (rec) {
                uranus_col_cyl_init(rec, &v[p[0]].pos, &v[p[1]].pos, &v[p[2]].pos, &((Vec3 *)v)[p[0] * 2 + 1]);
                rec += 64;
                uranus_col_cyl_init(rec, &v[p[0]].pos, &v[p[2]].pos, &v[p[3]].pos, &((Vec3 *)v)[p[0] * 2 + 1]);
                rec += 64;
            }
            count += 2;
            p += (unsigned)(pad + 4);
            break;
        }
        n--;
    }
    return count;
}

/* 0x00ca19b8 */
int uranus_col_poly_build_wall(short *p, UranusVtx *v, int mode, char *rec)
{
    int count = 0;
    unsigned short head = *p++;
    int n = (short)head;
    int pad = (n >> 14) & 3;

    n &= 0x3fff;

    while (n) {
        switch (mode) {
        case 3:
            if (rec) {
                uranus_col_wall_init(rec, &v[p[0]].pos, &v[p[1]].pos, &v[p[2]].pos, &((Vec3 *)v)[p[0] * 2 + 1]);
                rec += 64;
            }
            count++;
            p += (unsigned)(pad + 3);
            break;
        case 4:
            if (rec) {
                uranus_col_wall_init(rec, &v[p[0]].pos, &v[p[1]].pos, &v[p[2]].pos, &((Vec3 *)v)[p[0] * 2 + 1]);
                rec += 64;
            }
            count++;
            p += (unsigned)(pad + 4);
            break;
        }
        n--;
    }
    return count;
}

/* 0x00ca1b18 */
int uranus_col_mesh_build(void *mesh, char *rec, int *idx, int mode)
{
    short *p = (short *)((int *)mesh)[1];
    UranusVtx *v = (UranusVtx *)(((int *)mesh)[0] + 8);
    int total = 0;
    int op, len, n;

    while ((op = *p++) != 255) {
        op &= 0xff;
        if (op < 8)
            continue;
        if (op < 16) {
            p++;
            continue;
        }
        switch (op) {
        case 65:
            len = *p++;
            n = uranus_col_strip_build(p, v, 3, rec);
            if (rec)
                rec += n * 64;
            total += n;
            p += len;
            break;
        case 64:
            len = *p++;
            n = uranus_col_strip_build(p, v, 1, rec);
            if (rec)
                rec += n * 64;
            total += n;
            p += len;
            break;
        case 56:
            len = *p++;
            if (mode == 1)
                n = uranus_col_poly_build_cyl(p, v, 3, rec);
            else
                n = uranus_col_poly_build_wall(p, v, 3, rec);
            if (rec)
                rec += n * 64;
            total += n;
            p += len;
            break;
        case 57:
            len = *p++;
            if (mode == 1)
                n = uranus_col_poly_build_cyl(p, v, 4, rec);
            else
                n = uranus_col_poly_build_wall(p, v, 4, rec);
            if (rec)
                rec += n * 64;
            total += n;
            p += len;
            break;
        default:
            if (op < 64) {
                len = *p++;
                p += len;
            }
            break;
        }
    }
    if (idx)
        *idx += total;
    return total;
}

/* 0x00ca1d58 */
void uranus_col_tree_build_r(YNode *n, char *recs, int *idx, int *depth, int arg)
{
    if (n->mesh) {
        if (recs) {
            if (*depth > 1)
                uranus_col_mesh_build(n->mesh, recs + *idx * 64, idx, 1);
            else
                uranus_col_mesh_build(n->mesh, recs + *idx * 64, idx, 48);
        } else if (*depth > 1)
            uranus_col_mesh_build(n->mesh, 0, idx, 1);
        else
            uranus_col_mesh_build(n->mesh, 0, idx, 48);
    }
    if (n->child) {
        (*depth)++;
        uranus_col_tree_build_r(n->child, recs, idx, depth, arg);
        (*depth)--;
    }
    if (n->next)
        uranus_col_tree_build_r(n->next, recs, idx, depth, arg);
}

/* 0x00ca1e60 */
int uranus_col_tree_count(YNode *n)
{
    int depth = 0;
    int idx = 0;

    uranus_col_tree_build_r(n, 0, &idx, &depth, 1);
    return idx;
}

/* 0x00ca1e98 */
void uranus_col_tree_build(YNode *n, char *recs)
{
    int depth = 0;
    int idx = 0;

    uranus_col_tree_build_r(n, recs, &idx, &depth, 1);
}

/* 0x00ca1ec8 */
int uranus_col_find_near(char **out, char *recs, Vec3 *pos, int n, float rad)
{
    int cnt = 0;
    Vec3 ofs;

    for (; n; recs += 64, n--) {
        v3_sub(&ofs, (Vec3 *)(recs + 4), pos);
        if (vec3_length(&ofs) < rad) {
            *out++ = recs;
            cnt++;
        }
    }
    return cnt;
}

/* 0x00ca1fa0 */
void uranus_col_debug_draw(int (*recs)[16], int n)
{
    Vec3 pts[4];
    Vec3 seg[2];
    float *rec;

    draw_set_blend(1, 1);
    for (; n; recs++, n--) {
        rec = (float *)*recs;
        switch ((*recs)[0]) {
        case 1: {
            float nrm_len;

            lines_begin(0.5f, 0, 0);
            v3_copy(&pts[0], (Vec3 *)(*recs)[10]);
            v3_copy(&pts[3], (Vec3 *)(*recs)[10]);
            v3_copy(&pts[1], (Vec3 *)(*recs)[11]);
            v3_copy(&pts[2], (Vec3 *)(*recs)[12]);
            v3_copy(&seg[0], (Vec3 *)(rec + 1));
            nrm_len = rec[13] / 10.0f;
            v3_mul(&seg[1], (Vec3 *)(rec + 4), nrm_len);
            v3_addto(&seg[1], &seg[0]);
            lines_add_strip(pts, 4);
            lines_add_pairs(seg, 2);
            lines_end();
            break;
        }
        case 32:
            lines_begin(0.5f, RGBA(0xff, 0, 0, 0xff), 0);
            goto quad;
        case 16:
            lines_begin(0.5f, RGBA(0, 0, 0xff, 0xff), 0);
        quad:
            v3_copy(&pts[0], (Vec3 *)(*recs)[10]);
            v3_copy(&pts[3], (Vec3 *)(*recs)[10]);
            v3_copy(&pts[1], (Vec3 *)(*recs)[11]);
            v3_copy(&pts[2], (Vec3 *)(*recs)[12]);
            v3_copy(&seg[0], (Vec3 *)(rec + 1));
            v3_copy(&seg[1], (Vec3 *)(rec + 4));
            v3_addto(&seg[1], &seg[0]);
            lines_add_strip(pts, 4);
            lines_add_pairs(seg, 2);
            lines_end();
            break;
        case 4: {
            float k, z;
            Vec3 ab[2];
            Vec3 ofs;

            lines_begin(0.5f, RGBA(0x40, 0, 0xff, 0xff), 0);
            z = 0.0f;
            k = uranus_col_cyl_y_ends(rec, &ab[0].x, &ab[1].x);
            ofs.x = k; ofs.y = ofs.z = z;
            v3_sum(&seg[0], &ab[0], &ofs);
            ofs.x = -k; ofs.y = ofs.z = z;
            v3_sum(&seg[1], &ab[0], &ofs);
            lines_add_pairs(seg, 2);
            ofs.x = k; ofs.y = ofs.z = z;
            v3_sum(&seg[0], &ab[1], &ofs);
            ofs.x = -k; ofs.y = ofs.z = z;
            v3_sum(&seg[1], &ab[1], &ofs);
            lines_add_pairs(seg, 2);
            lines_add_pairs(ab, 2);
            lines_end();
            break;
        }
        case 8: {
            float k, z;
            Vec3 ab[2];
            Vec3 ofs;

            lines_begin(0.5f, RGBA(0x40, 0xff, 0, 0xff), 0);
            z = 0.0f;
            k = uranus_col_cyl_x_ends(rec, &ab[0].x, &ab[1].x);
            ofs.y = k; ofs.x = ofs.z = z;
            v3_sum(&seg[0], &ab[0], &ofs);
            ofs.y = -k; ofs.x = ofs.z = z;
            v3_sum(&seg[1], &ab[0], &ofs);
            lines_add_pairs(seg, 2);
            ofs.y = k; ofs.x = ofs.z = z;
            v3_sum(&seg[0], &ab[1], &ofs);
            ofs.y = -k; ofs.x = ofs.z = z;
            v3_sum(&seg[1], &ab[1], &ofs);
            lines_add_pairs(seg, 2);
            lines_add_pairs(ab, 2);
            lines_end();
            break;
        }
        }
    }
}

static inline void v3_negate(Vec3 *v)
{
    v->x = -v->x;
    v->y = -v->y;
    v->z = -v->z;
}

/* 0x00ca2428 */
void uranus_col_tri_init(int *rec, Vec3 *p0, Vec3 *p1, Vec3 *p2, Vec3 *ref)
{
    Vec3 e1, e2;
    Vec3 *n = (Vec3 *)(rec + 4);
    float *recf = (float *)rec;

    rec[0] = 1;
    v3_sub(&e1, p1, p0);
    v3_sub(&e2, p2, p0);
    rec[10] = (int)p0;
    rec[11] = (int)p1;
    rec[12] = (int)p2;
    vec3_normalize(&e1, &e1);
    vec3_normalize(&e2, &e2);
    ulVec3Cross(n, &e1, &e2);
    vec3_normalize(n, n);
    if (ulVec3Dot(n, ref) < 0.0f)
        v3_negate(n);
    uranus_col_tri_bound(p0, p1, p2, (Vec3 *)(rec + 1), recf + 13, recf + 14);
    recf[7] = -(recf[1] * recf[4] + recf[2] * recf[5] + recf[3] * recf[6]);
}

/* 0x00ca25c0 */
int uranus_col_tri_hit(Vec3 *ray, int va, int vb, int vc, int *nrm, float dist)
{
    Vec3 hit, ea, eb, ec, ca, cb, cc;
    Vec3 *dir = ray + 1;
    Vec3 *n = (Vec3 *)nrm;
    float k, denom;

    k = ulVec3Dot(ray, n) + dist;
    denom = ulVec3Dot(dir, n);
    if (0.0f < denom)
        return 0;
    k = -k / denom;
    hit.x = dir->x * k + ray->x;
    hit.y = dir->y * k + ray->y;
    hit.z = dir->z * k + ray->z;
    v3_sub(&ea, (Vec3 *)va, &hit);
    v3_sub(&eb, (Vec3 *)vb, &hit);
    v3_sub(&ec, (Vec3 *)vc, &hit);
    ulVec3Cross(&ca, &ea, &eb);
    ulVec3Cross(&cb, &eb, &ec);
    ulVec3Cross(&cc, &ec, &ea);
    if (ulVec3Dot(&ca, &cb) < 0.0f)
        return 0;
    if (ulVec3Dot(&cb, &cc) < 0.0f)
        return 0;
    v3_copy(ray, &hit);
    vec_reflect_unit(dir, n, dir);
    return 1;
}

/* 0x00ca27e0 */
int uranus_col_tri_hit_uv(float *ray, Vec3 *nrm, float plane_d, float ofs_x, float ofs_y, float curve_k, float u_div, float v_k)
{
    Vec3 hit;
    float k, n, u, v, bary;
    Vec3 *dv = (Vec3 *)(ray + 3);

    k = ulVec3Dot((struct Vec3 *)ray, nrm) + plane_d;
    n = ulVec3Dot(dv, nrm);
    if (0.0f < n)
        return 0;
    k = -k / n;
    hit.x = dv->x * k + ray[0];
    hit.y = dv->y * k + ray[1];
    hit.z = dv->z * k + ray[2];
    u = hit.x - ofs_x;
    v = (hit.y - ofs_y) - curve_k / u;
    if (v < 0.0f)
        return 0;
    bary = u / u_div - v_k * v;
    if (bary < 0.0f)
        return 0;
    if (1.0f < v + bary)
        return 0;
    ray[0] = hit.x;
    ray[1] = hit.y;
    ray[2] = hit.z;
    vec_reflect_unit(dv, nrm, dv);
    return 1;
}

/* 0x00ca2960 */
int uranus_col_check(int *rec, int pos, int near, float rad)
{
    Vec3 *pt = (Vec3 *)pos;

    if (!rec[0])
        return 0;
    if (near && ((float *)rec)[14] <= vec3_distance_squared(pt, (struct Vec3 *)(rec + 1)))
        return 0;
    switch (rec[0]) {
    case 1:
        if (uranus_col_tri_hit(pt, rec[10], rec[11], rec[12], rec + 4, ((float *)rec)[7]))
            return 1;
        break;
    case 16:
    case 32:
        if (uranus_col_wall_hit((float *)rec, (float *)pt, rad))
            return 48;
        break;
    case 4:
        if (uranus_col_cyl_y_hit(rec, (float *)pt, rad))
            return 4;
        break;
    case 8:
        if (uranus_col_cyl_x_hit(rec, (float *)pt, rad))
            return 8;
        break;
    }
    return 0;
}

/* 0x00ca2a80 */
void uranus_col_merge_cyls(int (*recs)[16], int n)
{
    int i, j;

    int *rec;

    for (i = 0; i < n; i++) {
        rec = recs[i];
        if (rec[0] == 4)
            for (j = i + 1; j < n; j++)
                if (recs[j][0] == 4)
                    uranus_col_cyl_merge(recs[j], rec);
    }
}

/* 0x00ca2b50 */
float uranus_col_cyl_y_ends(float *rec, float *top, float *bottom)
{
    top[0] = bottom[0] = rec[1];
    top[2] = bottom[2] = rec[3];
    top[1] = rec[6];
    bottom[1] = rec[7];
    return rec[4];
}

/* 0x00ca2b80 */
float uranus_col_cyl_x_ends(float *rec, float *end0, float *end1)
{
    end0[1] = end1[1] = rec[2];
    end0[2] = end1[2] = rec[3];
    end0[0] = rec[6];
    end1[0] = rec[7];
    return rec[4];
}

/* 0x00ca2bb0 */
void uranus_col_cyl_y_init(char *rec, Vec3 *va, Vec3 *vb, Vec3 *vc, Vec3 *n)
{
    int *words = (int *)rec;
    float lo, hi, rad;
    float ctr[2];

    uranus_col_tri_bound(va, vb, vc, (Vec3 *)(words + 1), (float *)(words + 13), (float *)(words + 14));
    words[0] = 4;
    words[10] = (int)va;
    words[11] = (int)vb;
    words[12] = (int)vc;
    lo = hi = va->y;
    if (vb->y < lo)
        lo = vb->y;
    if (vc->y < lo)
        lo = vc->y;
    if (hi < vb->y)
        hi = vb->y;
    if (hi < vc->y)
        hi = vc->y;
    rad = circle3_2d_degenerate(&ctr[0], &ctr[1], va->x, va->z, vb->x, vb->z, vc->x, vc->z);
    ((float *)words)[4] = rad;
    if (rad < 0.0f) {
        words[0] = 0;
        return;
    }
    ((float *)words)[5] = rad * rad;
    ((float *)words)[1] = ctr[0];
    ((float *)words)[3] = ctr[1];
    ((float *)words)[2] = (lo + hi) * 0.5f;
    ((float *)words)[6] = lo;
    ((float *)words)[7] = hi;
}

/* 0x00ca2cf8 */
void uranus_col_cyl_x_init(char *rec, Vec3 *va, Vec3 *vb, Vec3 *vc, Vec3 *n)
{
    int *words = (int *)rec;
    float lo, hi, rad;
    float ctr[2];

    uranus_col_tri_bound(va, vb, vc, (Vec3 *)(words + 1), (float *)(words + 13), (float *)(words + 14));
    words[0] = 8;
    words[10] = (int)va;
    words[11] = (int)vb;
    words[12] = (int)vc;
    lo = hi = va->x;
    if (vb->x < lo)
        lo = vb->x;
    if (vc->x < lo)
        lo = vc->x;
    if (hi < vb->x)
        hi = vb->x;
    if (hi < vc->x)
        hi = vc->x;
    rad = circle3_2d_degenerate(&ctr[0], &ctr[1], va->y, va->z, vb->y, vb->z, vc->y, vc->z);
    ((float *)words)[4] = rad;
    if (rad < 0.0f) {
        words[0] = 0;
        return;
    }
    ((float *)words)[5] = rad * rad;
    ((float *)words)[2] = ctr[0];
    ((float *)words)[3] = ctr[1];
    ((float *)words)[1] = (lo + hi) * 0.5f;
    ((float *)words)[6] = lo;
    ((float *)words)[7] = hi;
}

/* 0x00ca2e40 */
void uranus_col_cyl_init(char *rec, Vec3 *va, Vec3 *vb, Vec3 *vc, Vec3 *n)
{
    float mx, my, span_x, span_y;

    mx = fabsf(va->x - vb->x);
    span_x = fabsf(vb->x - vc->x);
    if (mx < span_x)
        mx = span_x;
    span_x = fabsf(va->x - vc->x);
    if (mx < span_x)
        mx = span_x;
    my = fabsf(va->y - vb->y);
    span_y = fabsf(vb->y - vc->y);
    if (my < span_y)
        my = span_y;
    span_y = fabsf(va->y - vc->y);
    if (my < span_y)
        my = span_y;
    if (my < mx)
        uranus_col_cyl_x_init(rec, va, vb, vc, n);
    else
        uranus_col_cyl_y_init(rec, va, vb, vc, n);
}

/* 0x00ca2ee8 */
int uranus_col_cyl_y_hit(int *rec, float *p, float rad)
{
    float *cyl = (float *)rec;
    float px, py, pz, cx, cz, dx, dz, k;

    px = *p++;
    py = *p++;
    pz = *p;
    if (py + rad < cyl[6])
        return 0;
    if (cyl[7] < py - rad)
        return 0;
    cx = cyl[1];
    cz = cyl[3];
    dx = cx - px;
    dz = cz - pz;
    k = dx * dx + dz * dz;
    k = rad / sqrtf(k);
    dx *= k;
    dz *= k;
    pz += dz;
    px += dx;
    if (cyl[5] < (px - cx) * (px - cx) + (pz - cz) * (pz - cz))
        return 0;
    k = (cyl[4] + rad) / rad;
    dz *= -k;
    dx *= -k;
    pz = cz + dz;
    px = cx + dx;
    *p-- = pz;
    *p-- = py;
    *p = px;
    p += 5;
    *p-- = dz;
    p[-1] = dx;
    *p = 0.0f;
    return 1;
}

/* 0x00ca3070 */
int uranus_col_cyl_x_hit(int *rec, float *p, float rad)
{
    float *cyl = (float *)rec;
    float px, py, pz, cy, cz, dy, dz, k;

    px = *p++;
    py = *p++;
    pz = *p;
    if (px + rad < cyl[6])
        return 0;
    if (cyl[7] < px - rad)
        return 0;
    cy = cyl[2];
    cz = cyl[3];
    dy = cy - py;
    dz = cz - pz;
    k = dy * dy + dz * dz;
    k = rad / sqrtf(k);
    dy *= k;
    dz *= k;
    pz += dz;
    py += dy;
    if (cyl[5] < (py - cy) * (py - cy) + (pz - cz) * (pz - cz))
        return 0;
    k = (cyl[4] + rad) / rad;
    dz *= -k;
    dy *= -k;
    pz = cz + dz;
    py = cy + dy;
    *p-- = pz;
    *p-- = py;
    *p = px;
    p += 5;
    *p-- = dz;
    *p = dy;
    p[-1] = 0.0f;
    return 1;
}

/* 0x00ca31f8 */
int uranus_col_cyl_merge(int *src, int *dst)
{
    float *srcf = (float *)src;
    float *dstf = (float *)dst;
    float dx, dz, dist, rad, k;

    if (srcf[7] < dstf[6])
        return 0;
    if (srcf[6] > dstf[7])
        return 0;
    dx = srcf[1] - dstf[1];
    dz = srcf[3] - dstf[3];
    dist = dx * dx + dz * dz;
    if (!(fabsf(dist) < FLT_EPSILON))
        dist = sqrtf(dist);
    if (dstf[4] + srcf[4] < dist)
        return 0;
    if (dist + srcf[4] <= dstf[4]) {
        if (srcf[7] > dstf[7])
            dstf[7] = srcf[7];
    } else {
        rad = dist + srcf[4] + dstf[4];
        if (!(fabsf(dist) < FLT_EPSILON)) {
            dx /= dist;
            dz /= dist;
        }
        k = dstf[4];
        dstf[5] = rad * rad;
        dstf[4] = rad;
        k = rad * 0.5f - k;
        dstf[1] += k * dx;
        dstf[3] += k * dz;
        if (dstf[7] < srcf[7])
            dstf[7] = srcf[7];
    }
    if (srcf[6] < dstf[6])
        dstf[6] = srcf[6];
    src[0] = 0;
    return 1;
}

/* 0x00ca33c0 */
void uranus_col_plane_init(int *rec, Vec3 *va, Vec3 *vb, Vec3 *vc, Vec3 *ref)
{
    Vec3 e1, e2, nrm;
    Vec3 *pn = (Vec3 *)(rec + 4);

    v3_sub(&e1, vb, va);
    v3_sub(&e2, vc, va);
    ulVec3Cross(&nrm, &e1, &e2);
    vec3_normalize(pn, &nrm);
    if (ulVec3Dot(pn, ref) < 0.0f)
        v3_negate(pn);
    ((float *)rec)[7] = -(((float *)rec)[4] * va->x + ((float *)rec)[5] * va->y + ((float *)rec)[6] * va->z);
}

/* 0x00ca3500 */
void uranus_col_wall_init(char *rec, Vec3 *va, Vec3 *vb, Vec3 *vc, Vec3 *nrm)
{
    int *words = (int *)rec;
    float lo, hi;
    Vec3 n;

    words[0] = 48;
    words[10] = (int)va;
    words[11] = (int)vb;
    words[12] = (int)vc;
    lo = hi = va->z;
    if (vb->z < lo)
        lo = vb->z;
    if (hi < vb->z)
        hi = vb->z;
    if (vc->z < lo)
        lo = vc->z;
    if (hi < vc->z)
        hi = vc->z;
    ((float *)words)[8] = lo;
    ((float *)words)[9] = hi;
    uranus_col_plane_init(words, va, vb, vc, nrm);
    uranus_col_tri_bound(va, vb, vc, (Vec3 *)(words + 1), (float *)(words + 13), (float *)(words + 14));
    n.x = ((float *)words)[4];
    n.y = ((float *)words)[5];
    n.z = ((float *)words)[6];
    if (acos(ulVec3Dot(&n, &uranus_col_up)) < 0.5235987f)
        words[0] = 16;
    else
        words[0] = 32;
}

/* 0x00ca3638 */
int uranus_col_plane_hit(float *nrm, float *p, float rad)
{
    float nx, ny, nz, x, y, z, dist;

    nx = *nrm++;
    x = *p++;
    ny = *nrm++;
    y = *p++;
    nz = *nrm;
    z = *p;
    x -= nx * rad;
    y -= ny * rad;
    z -= nz * rad;
    dist = nx * x + ny * y + nz * z + nrm[1];
    if (dist >= 0.0f)
        return 0;
    {
        float t = dist - rad;
        *--p = y - ny * t;
        *--p = x - nx * t;
    }
    p += 5;
    *p-- = nz;
    *p = ny;
    p[-1] = nx;
    return 1;
}

/* 0x00ca36f0 */
int uranus_col_wall_hit(float *rec, float *pos, float rad)
{
    if (pos[2] < rec[8] || rec[9] < pos[2])
        return 0;
    return uranus_col_plane_hit(rec + 4, pos, rad);
}
