/*
 * Enemy formations: 64 slots counting assigned, appeared, shot-down and escaped enemies; when all
 * are gone, event 6 (all shot down) or 5. Pass objects fly an enemy along a model motion path.
 */

#include "types_game.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "gw_draw.h"
#include "gw_scrollbar.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_window.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "enemy_formation.h"
#include "anim.h"
#include "callback.h"
#include "debugmenu.h"
#include "math_util.h"
#include "model.h"
#include "player_status.h"
#include "stage.h"
#include "task.h"
#include "system.h"

typedef struct ScreenInfo {
    char pad_00[24];
    float left;             /* 0x18 */
    char pad_1c[8];
    float bottom;           /* 0x24 */
} ScreenInfo;

typedef struct ListLayout {
    int val;
    int min;
    float max;
    float step;
    float page;
} ListLayout;

typedef struct CtrlEnt {
    int word[10];
} CtrlEnt;

typedef struct EnemyPassDesc {
    int w[5];
} EnemyPassDesc;

extern FormSlot enemy_formation_slots[64];
extern struct TaskDesc enemy_formation_desc;
extern ScreenInfo screen_layout;
extern int enemy_formation_window;
extern int enemy_formation_list;
extern CtrlEnt enemy_pass_vardefs[14];
extern struct GwVarNode *gw_varedit_root;
extern void ulMatrixPush(void);
extern char enemy_pass_node_mtx[];
extern EnemyPassVars enemy_pass_vars;
extern float enemy_pass_shot_frames[];
extern int enemy_pass_frame_disp;
extern int enemy_pass_motion_starts[2];
extern int enemy_pass_motion_ends[2];
extern EnemyPassDesc enemy_pass_desc;
extern int enemy_pass_use_shot;

static void enemy_formation_table_clear(FormSlot *tbl);
static void enemy_formation_slot_clear(FormSlot *slot);
static void enemy_formation_slot_check(FormSlot *slot, int idx);
static void enemy_formation_window_closed(void);
static void enemy_formation_window_resize(void);
static void enemy_formation_window_draw(void);
static void enemy_formation_list_draw(int win);
static void enemy_formation_window_toggle(void);
static void enemy_pass_rotate_zxy(int dev, int *p);

/* Adds "Formation" to the Info menu. */
/* 0x00111a40 */
static void enemy_formation_init(void)
{
    enemy_formation_table_clear(enemy_formation_slots);
    menu_item_add(info_menu_name, "Formation", enemy_formation_window_toggle, 0, 0);
}

/* 0x00111a80 */
static void enemy_formation_release(void)
{
    menu_item_remove(info_menu_name, "Formation");
}

/* 0x00111a98 */
static void enemy_formation_exec(void)
{
    int i;

    for (i = 0; i < 64; i++)
        enemy_formation_slot_check(&enemy_formation_slots[i], i);
}

/* 0x00111ae8 */
static void enemy_formation_disp(void)
{
}

/* 0x00111af0 */
void *enemy_formation_create(void *parent)
{
    return task_create(parent, &enemy_formation_desc, 4);
}

/* 0x00111b18 */
static void enemy_formation_clear(void)
{
    enemy_formation_table_clear(enemy_formation_slots);
}

/* 0x00111b28 */
static void enemy_formation_table_clear(FormSlot *tbl)
{
    int i;

    i = 64;
    do {
        enemy_formation_slot_clear(tbl);
        i--;
        tbl++;
    } while (i != 0);
}

/* Last position kept. */
/* 0x00111b68 */
static void enemy_formation_slot_clear(FormSlot *slot)
{
    slot->unk_00 = 0;
    slot->assign = 0;
    slot->dispose = 0;
    slot->appear = 0;
    slot->die = 0;
    slot->away = 0;
}

/* NULL for -1 (no formation) or idx >= 64. */
/* 0x00111b88 */
static FormSlot *enemy_formation_slot_get(FormSlot *tbl, int idx)
{
    if (idx == -1 || idx >= 64)
        return 0;
    return tbl + idx;
}

/*
 * All appeared shot down: notifies the stage, player status flag 4, event 6 at the last enemy's
 * position. Else event 5. Clears the slot.
 */
/* steer(cast) */
/* 0x00111bb0 */
static void enemy_formation_slot_check(FormSlot *slot, int idx)
{
    FormMsg msg;
    float *pos;

    if (slot->assign > 0 && slot->assign == slot->dispose) {
        if (slot->appear == slot->die) {
            ((void (*)(int))stage_formation_hook)(idx);
            player_status_set_flags(4);
            msg.pos[0] = slot->pos[0];
            pos = slot->pos;
            msg.pos[1] = pos[1];
            msg.pos[2] = pos[2];
            msg.idx = idx;
            listener_dispatch(6, &msg);
        } else {
            msg.pos[0] = slot->pos[0];
            pos = slot->pos;
            msg.pos[1] = pos[1];
            msg.pos[2] = pos[2];
            msg.idx = idx;
            listener_dispatch(5, &msg);
        }
        enemy_formation_slot_clear(slot);
    }
}

/* 0x00111c78 */
void enemy_formation_assign(int idx, int cnt)
{
    FormSlot *slot = enemy_formation_slot_get(enemy_formation_slots, idx);

    if (slot == 0) {
        ERROR_PRINTF("Formation Assign Error %d\n", idx);
        return;
    }
    slot->assign += cnt;
}

/* 0x00111ce8 */
void enemy_formation_appear(int idx, int cnt)
{
    FormSlot *slot = enemy_formation_slot_get(enemy_formation_slots, idx);

    if (slot == 0) {
        ERROR_PRINTF("Formation Appear Error %d\n", idx);
        return;
    }
    slot->appear += cnt;
}

/* pos kept for the completion event. */
/* 0x00111d58 */
void enemy_formation_die(int idx, int cnt, float *pos)
{
    FormSlot *slot = enemy_formation_slot_get(enemy_formation_slots, idx);
    float *slot_pos;

    if (slot == 0) {
        ERROR_PRINTF("Formation Die Error %d\n", idx);
        return;
    }
    if (slot->assign > 0) {
        slot->die += cnt;
        slot->dispose += cnt;
    }
    slot->pos[0] = pos[0];
    slot_pos = slot->pos;
    slot_pos[1] = pos[1];
    slot_pos[2] = pos[2];
}

/* Bad-slot message reuses the "Appear" text. */
/* 0x00111e10 */
void enemy_formation_away(int idx, int cnt)
{
    FormSlot *slot = enemy_formation_slot_get(enemy_formation_slots, idx);

    if (slot == 0) {
        ERROR_PRINTF("Formation Appear Error %d\n", idx);
        return;
    }
    if (slot->assign > 0) {
        slot->away += cnt;
        slot->dispose += cnt;
    }
}

/* 23 x 10 cells, lower left, scrolling "List" child. */
/* 0x00111e98 */
static void enemy_formation_window_open(void)
{
    ListLayout layout;
    int cell[2];
    int win;
    int list;

    gw_get_cell_size(&cell[0], &cell[1]);
    win = gw_window_open("Formation", 95,
                       (int)(screen_layout.left + 4.0f),
                       (int)(screen_layout.bottom - (float)(cell[1] * 10) - 4.0f),
                       cell[0] * 23, cell[1] * 10, 0);
    enemy_formation_window = win;
    if (win != 0) {
        gw_window_set_handlers(win, enemy_formation_window_resize, enemy_formation_window_draw, enemy_formation_window_closed);
        list = gw_window_open("List", 0x4012c, 2, 2, 0, 0, enemy_formation_window);
        enemy_formation_list = list;
        gw_window_size_to_parent(list, 2, 2);
        gw_window_set_handlers(enemy_formation_list, 0, enemy_formation_list_draw, 0);
        layout.val = 0;
        layout.min = 0;
        layout.max = (float)(cell[1] * 10);
        layout.step = (float)cell[1];
        layout.page = (float)cell[1] * 5.0f;
        gw_window_add_scrollbar(enemy_formation_list, gw_scrollbar_create(1, &layout, 0, 0, 0, 0, enemy_formation_list));
        menu_item_set_value(info_menu_name, "Formation", 1);
    }
}

/* 0x00112048 */
static void enemy_formation_window_closed(void)
{
    enemy_formation_window = 0;
    menu_item_set_value(info_menu_name, "Formation", 0);
}

/* 0x00112068 */
static void enemy_formation_window_resize(void)
{
    gw_window_size_to_window(enemy_formation_list, enemy_formation_window, 2, 2);
}

/* 0x00112088 */
static void enemy_formation_window_draw(void)
{
}

/* Per active slot: assigned, disposed, appeared, shot-down, escaped. */
/* 0x00112090 */
static void enemy_formation_list_draw(int win)
{
    int dim[4];
    FormSlot *slot;
    FormSlot *tbl = enemy_formation_slots;
    int i;
    int rows;
    int y;
    int obj;
    int n;
    int lim;

    gw_window_get_size(win, &dim[0], &dim[1]);
    slot = tbl;
    rows = 1;
    gw_get_cell_size(&dim[2], &dim[3]);
    gw_win_printf(win, 2, 2, "id|ass dis ap d_ o_");
    y = dim[3] + 2;
    for (i = 0; i < 64; i++, slot++) {
        if (slot->assign > 0) {
            gw_win_printf(win, 2, y, "%2d|%3d %3d %2d %2d %2d", i, slot->assign, slot->dispose, slot->appear, slot->die, slot->away);
            rows++;
            y += dim[3];
        }
    }
    obj = gw_window_get_vscrollbar(win);
    lim = gw_scrollbar_get_track(obj);
    n = (int)((float)dim[1] / (float)(rows * dim[3]) * (float)lim);
    if (lim < n)
        n = lim;
    if (!(n > 3))
        n = 4;
    gw_scrollbar_set_pos(obj, n);
    gw_scrollbar_set_max(obj, (float)((rows - 1) * dim[3]));
}

/* 0x00112208 */
static void enemy_formation_window_toggle(void)
{
    if (enemy_formation_window != 0)
        gw_window_close(enemy_formation_window);
    else
        enemy_formation_window_open();
}

/* Group entry, then the 14 enemy_pass_vardefs pointed into enemy_pass_vars. */
/* 0x00112228 */
static void enemy_pass_vars_register(int name)
{
    int ctrl[10];
    int *p;
    unsigned i;
    CtrlEnt *ent;

    memset(ctrl, 0, 40);
    p = ((int *)&enemy_pass_vars); /* steer(cast) */
    ctrl[2] = p[0];
    ctrl[3] = p[1];
    name = gw_varedit_add(&gw_varedit_root, name, ctrl);
    for (ent = enemy_pass_vardefs, i = 0; i < 14; i++, ent++) {
        ent->word[4] = (int)&p[i + 2];
    }
    gw_varedit_add_table(&gw_varedit_root, name, enemy_pass_vardefs);
}

/* Yaw and pitch from the forward axis, then roll from the up axis. */
/* 0x001122d8 */
void enemy_pass_get_matrix_angles(int *ang)
{
    float in[2][3] = { { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } };
    float out[2][3];
    float up[3];
    int yaw, pitch, roll, rx;

    matrix_apply_vectors(in[0], out[0], 2);
    yaw = (int)RAD_TO_ANG(atan2f(out[0][0], out[0][2]));
    pitch = (int)RAD_TO_ANG(atan2f(out[0][1], sqrtf(out[0][0] * out[0][0] + out[0][2] * out[0][2])));
    rx = -pitch;
    ulMatrixPush();
    matrix_unit_set(0);
    vu0_matrix_rotate_x(ulMatrixCur, pitch);
    vu0_matrix_rotate_y(ulMatrixCur, -yaw);
    ulMatrixApplyVector(ulMatrixCur, out[1], up);
    roll = (int)RAD_TO_ANG(atan2f(-up[0], up[1]));
    ulMatrixPop();
    ang[0] = rx;
    ang[1] = yaw;
    ang[2] = roll;
}

/* Node flag 0x10 is the flight point: its world position and rotation drive the pass object. */
/* 0x00112480 */
void enemy_pass_draw_tree(EnemyPassWork *node, int *child)
{
    unsigned flags;

    do {
        flags = child[0];
        ulMatrixPush();
        anim_apply_node(child);
        if (flags & 0x10) {
            ulMatrixGetTranslation(ulMatrixCur, node->pos);
            ulMatrixPush();
            vu0_matrix_mul(ulMatrixCur, ulMatrixCur, enemy_pass_node_mtx);
            enemy_pass_get_matrix_angles(node->ang);
            ulMatrixPop();
        } else {
            enemy_pass_draw_tree(node, (int *)child[11]);
        }
        ulMatrixPop();
        child = (int *)child[12];
    } while (child != 0);
}

/* 0x00112558 */
static void enemy_pass_draw(EnemyPassWork *w)
{
    ulMatrixPush();
    ulMatrixLoad(w->mtx);
    anim_start(w->model.clip, w->model.frame);
    enemy_pass_draw_tree(w, w->model.root);
    ulMatrixPop();
}

static inline void fo_copy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static inline void fo_sub(float *dst, float *to, float *from)
{
    dst[0] = to[0] - from[0];
    dst[1] = to[1] - from[1];
    dst[2] = to[2] - from[2];
}

/* At enemy_pass_shot_frames[pass_no]: shot flag bit 1 this frame, bit 0 for good. */
/* 0x001125a0 */
static void enemy_pass_exec(EnemyPassWork *obj)
{
    float prev[3];
    EnemyPassVars *cfg = &enemy_pass_vars;
    int flags;

    obj->model.speed = cfg->spd_pass * FRAME_SEC;
    fo_copy(prev, obj->pos);
    dc_object_advance_time(&obj->model);
    enemy_pass_draw(obj);
    fo_sub(obj->move, obj->pos, prev);
    flags = obj->shot;
    obj->shot = flags & ~2;
    if (!(flags & 1)) {
        if (enemy_pass_shot_frames[cfg->pass_no] <= obj->model.frame)
            obj->shot = (flags & ~2) | 3;
    }
}

/* While enemy_pass_frame_disp: current and last frame. */
/* 0x00112698 */
static void enemy_pass_disp(EnemyPassWork *p)
{
    if (enemy_pass_frame_disp) {
        debug_set_origin(10, 12);
        debug_printf("frame:%f/%f", (double)p->model.frame, (double)p->model.frames);
    }
}

/* Frame range from prm, or the pass_no table entry while force_pass. */
/* 0x00112718 */
static void enemy_pass_init(EnemyPassWork *obj, int *prm)
{
    float *mtx = obj->mtx;
    EnemyPassVars *cfg = &enemy_pass_vars;
    float *src;
    float *dst;

    matrix_unit_set(mtx);
    vu0_matrix_translate(mtx, cfg->move_x, cfg->move_y, cfg->move_z);
    enemy_pass_rotate_zxy(mtx, (int *)prm[2]);
    vu0_matrix_scale(mtx, cfg->scl_x, cfg->scl_y, cfg->scl_z);
    if (cfg->force_pass) {
        dc_object_init(&obj->model, obj->data + enemy_pass_motion_starts[cfg->pass_no], obj->data + enemy_pass_motion_ends[cfg->pass_no], 0);
    } else {
        dc_object_init(&obj->model, obj->data + prm[0], obj->data + prm[1], 0);
    }
    obj->model.frame = 0.0f;
    obj->model.speed = enemy_pass_vars.spd_pass * FRAME_SEC;
    enemy_pass_draw(obj);
    src = obj->pos;
    dst = obj->move;
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* 0x00112860 */
static void enemy_pass_release(EnemyPassWork *obj)
{
    dc_object_release(&obj->model);
}

/* From a copy of enemy_pass_desc. */
/* 0x00112868 */
static void *enemy_pass_create(void *parent, int prm)
{
    EnemyPassDesc desc;

    desc = enemy_pass_desc;
    return task_create_inherit_arg(parent, &desc, sizeof(EnemyPassWork), prm);
}

/* Only while enemy_pass_use_shot. */
/* 0x001128c0 */
static int enemy_pass_shot_test(EnemyPassWork *p)
{
    if (enemy_pass_use_shot)
        return p->shot & 2;
    return 0;
}

/* Over the last frame. */
/* 0x001128e8 */
static void enemy_pass_get_move(EnemyPassWork *src, float *dst)
{
    float *move = src->move;

    dst[0] = move[0];
    dst[1] = move[1];
    dst[2] = move[2];
}

/* 0x00112908 */
static void enemy_pass_get_ang(EnemyPassWork *src, int *dst)
{
    int *ang = src->ang;

    dst[0] = ang[0];
    dst[1] = ang[1];
    dst[2] = ang[2];
}

/* 0x00112928 */
static int enemy_pass_is_end(EnemyPassWork *p)
{
    return p->model.frame == p->model.frames;
}

/* p = (x, y, z), applied Z, X, Y. */
/* 0x00112950 */
static void enemy_pass_rotate_zxy(int dev, int *p)
{
    vu0_matrix_rotate_z(dev, p[2]);
    vu0_matrix_rotate_x(dev, p[0]);
    vu0_matrix_rotate_y(dev, p[1]);
}
