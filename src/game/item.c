/*
 * Items: pickups spawned near the player by game messages. They drift around an anchor ahead of
 * the player; shot after lock-on they give sound, score, break-up and a status report. Also the
 * floor billboard drawn through the screen corners.
 */

#include "types_game.h"
#include "types_gw.h"
#include "mathdef.h"
#include "debug.h"
#include "libc.h"
#include "gw_varedit.h"
#include "nj_draw.h"
#include "nj_view.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "item.h"
#include "approach.h"
#include "callback.h"
#include "camera.h"
#include "change_symbol.h"
#include "color.h"
#include "console_com.h"
#include "debugdraw.h"
#include "fog.h"
#include "game.h"
#include "lockon.h"
#include "map_manager.h"
#include "math_util.h"
#include "model.h"
#include "model_wire_break.h"
#include "node_draw_state.h"
#include "player.h"
#include "player_camera.h"
#include "player_status.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "task.h"
#include "system.h"
#include "fade_shape.h"
#include "game_flow_editor.h"

typedef struct { int id; int flag; int val; } ItemParam;

#define ROTZ(n) do { float rot_u = x, rot_v = y; ulSinCos(rot[2], &sc[n][0], &sc[n][1]); \
    x = rot_u * sc[n][1] - sc[n][0] * rot_v; y = rot_u * sc[n][0] + sc[n][1] * rot_v; } while (0)

#define ROTX(n) do { float rot_u = y, rot_v = z; ulSinCos(rot[0], &sc[n][0], &sc[n][1]); \
    y = rot_u * sc[n][1] - sc[n][0] * rot_v; z = rot_u * sc[n][0] + sc[n][1] * rot_v; } while (0)

#define ROTY(n) do { float rot_u = x, rot_v = z; ulSinCos(rot[1], &sc[n][0], &sc[n][1]); \
    x = rot_u * sc[n][1] + rot_v * sc[n][0]; z = -rot_u * sc[n][0] + rot_v * sc[n][1]; } while (0)

#define ROTZ_ITEM_RANDOM_OFFSET(n) do { float rot_u = x, rot_v = y; ulSinCos(ang[2], &sc[n][0], &sc[n][1]); \
    x = rot_u * sc[n][1] - sc[n][0] * rot_v; y = rot_u * sc[n][0] + sc[n][1] * rot_v; } while (0)

#define ROTX_ITEM_RANDOM_OFFSET(n) do { float rot_u = y, rot_v = z; ulSinCos(ang[0], &sc[n][0], &sc[n][1]); \
    y = rot_u * sc[n][1] - sc[n][0] * rot_v; z = rot_u * sc[n][0] + sc[n][1] * rot_v; } while (0)

#define ROTY_ITEM_RANDOM_OFFSET(n) do { float rot_u = x, rot_v = z; ulSinCos(ang[1], &sc[n][0], &sc[n][1]); \
    x = rot_u * sc[n][1] + rot_v * sc[n][0]; z = -rot_u * sc[n][0] + rot_v * sc[n][1]; } while (0)

#define RND() (RAND_FLOAT_A() * (item_base_s_range + item_base_s_range) - item_base_s_range)

typedef struct {
    void *obj;
    int timer;
    int grav_x;
    float grav_y;
    int grav_z;
    float drag, spd, rot_range;
    int col;
    int col_to;
    int rest[6];
} ItemEffect;

typedef struct {
    int kind;
    float origin[3];
    float vel[3];
    float decay;
} ItemSpawn;

#define ITEM_GLOW 0x10

extern ScreenView screen_layout;
extern ItemParam item_params[12];
extern ItemModel item_model_table[12];
extern int item_models;
extern struct GwVarNode *gw_varedit_root;
extern GwVarDef item_vardef[];
extern GwVarDef item_vardefs[];
extern int item_drop_kinds[20];
extern unsigned int game_flags;
extern float item_base_dist;
extern float item_base_forward;
extern ItemManageWork *item_manage_work;
extern int item_is_debug;
extern void ulMatrixPush(void);
extern struct TaskDesc item_manage_desc;
extern float item_base_s_range;
extern int item_textures;
extern int item_kind_effects[];
extern int item_kind_colors[];
extern float item_coll_r;
extern float item_coll_r_2;
extern char item_zero_vec[];
extern ItemModel item_effects[];
extern float item_base_a_range;

extern float item_move_rate;
extern float item_vel_max;
extern int item_life_time;
extern int item_locktime_o;
extern int item_locktime_s;
extern int item_locktime_p;
extern const int item_lock_names[];
extern int item_kind_scores[];
extern unsigned int stage_exit_flags;
extern int item_is_out;
extern struct TaskDesc item_desc;
extern void item_set_max(ItemWork *w, unsigned int val);
extern float item_base_spd;

/* Zero when the ray is parallel. */
/* 0x00122d48 */
void line_plane_intersect(float *out, float *plane_pos, float *nrm, float *org, float *dir)
{
    float plane_dist, org_dist, denom, t;

    plane_dist = -ulVec3Dot(plane_pos, nrm);
    org_dist = ulVec3Dot(org, nrm);
    denom = ulVec3Dot(nrm, dir);
    if (fabsf(denom) < FLT_EPSILON) {
        out[0] = 0;
        out[1] = 0;
        out[2] = 0;
    } else {
        t = (plane_dist - org_dist) / denom;
        out[0] = org[0] + dir[0] * t;
        out[1] = org[1] + dir[1] * t;
        out[2] = org[2] + dir[2] * t;
    }
}

/* UV from the hit point's x, z (times scale); projected screen position. */
/* 0x00122e40 */
void billboard_corner(float *obj, float *plane_pos, float *nrm, float *org, float *dir, float scale)
{
    float hit[3];

    line_plane_intersect(hit, plane_pos, nrm, org, dir);
    obj[3] = hit[0] * scale;
    obj[4] = hit[2] * scale;
    ulMatrixApplyPoint(ulMatrixCur, hit, hit);
    obj[2] = project_point(hit, hit);
    obj[0] = hit[0];
    obj[1] = hit[1];
}

static inline void bb_sub(float *dst, float *to, float *from)
{
    dst[0] = to[0] - from[0];
    dst[1] = to[1] - from[1];
    dst[2] = to[2] - from[2];
}

static inline void bb_scale(float *dst, float *src, float scale)
{
    dst[0] = src[0] * scale;
    dst[1] = src[1] * scale;
    dst[2] = src[2] * scale;
}

static inline void bb_add(float *dst, float *va, float *vb)
{
    dst[0] = va[0] + vb[0];
    dst[1] = va[1] + vb[1];
    dst[2] = va[2] + vb[2];
}

/* Plane height below the camera, corners where the view-corner rays meet it. No depth test. */
/* 0x00122ec0 */
void draw_billboard_quad(float height, int tex, float sc, int col, int fog, int mode)
{
    float *cam;
    float *pe;
    float *pg;
    float plane_pos[3], side[3], up[3], dir[3], center[3];
    Vtx vtx[4];
    float nrm[3];
    float ray[3];
    float dist, sx, sy;
    float ctr_x, ctr_y, ctr_z, up_x, up_y, up_z, side_x, side_y, side_z;
    float top_x, top_y, top_z, bottom_x, bottom_y, bottom_z;
    int state0, state1, state2, fog_old;
    float *pf;

    cam = camera_get_current();
    vec3_normalize(up, cam + 6);
    bb_sub(dir, cam + 3, cam);
    vec3_normalize(dir, dir);
    dist = cam_dist;
    pe = side;
    bb_scale(center, dir, dist);
    sx = screen_layout.view_x;
    sy = screen_layout.view_y;
    bb_add(center, center, cam);
    bb_scale(plane_pos, up, sy);
    ulVec3Cross(pe, up, dir);
    bb_scale(side, side, sx);
    do {
        float *p = center;

        ctr_x = p[0];
        ctr_y = p[1];
        ctr_z = p[2];
    } while (0);
    do {
        up_x = plane_pos[0];
        up_y = plane_pos[1];
        up_z = plane_pos[2];
    } while (0);
    do {
        side_x = pe[0];
        side_y = pe[1];
        side_z = pe[2];
    } while (0);
    ctr_x -= cam[0];
    ctr_y -= cam[1];
    ctr_z -= cam[2];
    plane_pos[0] = 0;
    plane_pos[1] = -height;
    plane_pos[2] = 0;
    pg = nrm;
    nrm[0] = 0;
    pg[1] = 1.0f;
    nrm[2] = 0;
    pf = ray;
    vtx[3].col = col;
    vtx[2].col = col;
    vtx[1].col = col;
    vtx[0].col = col;
    top_x = ctr_x + up_x;
    top_y = ctr_y + up_y;
    top_z = ctr_z + up_z;
    bottom_x = ctr_x - up_x;
    bottom_y = ctr_y - up_y;
    bottom_z = ctr_z - up_z;
    ray[0] = top_x + side_x;
    ray[1] = top_y + side_y;
    ray[2] = top_z + side_z;
    vec3_normalize(pf, pf);
    pf += (tex & 1) >> 1;
    fog += (fog & 1) >> 1;
    mode += (mode & 1) >> 1;
    billboard_corner(&vtx[0], plane_pos, pg, cam, pf, sc);
    ray[0] = bottom_x + side_x;
    ray[1] = bottom_y + side_y;
    ray[2] = bottom_z + side_z;
    vec3_normalize(pf, pf);
    billboard_corner(&vtx[1], plane_pos, pg, cam, pf, sc);
    ray[0] = top_x - side_x;
    ray[1] = top_y - side_y;
    ray[2] = top_z - side_z;
    vec3_normalize(pf, pf);
    billboard_corner(&vtx[2], plane_pos, pg, cam, pf, sc);
    ray[0] = bottom_x - side_x;
    ray[1] = bottom_y - side_y;
    ray[2] = bottom_z - side_z;
    vec3_normalize(pf, pf);
    billboard_corner(&vtx[3], plane_pos, pg, cam, pf, sc);
    state0 = draw_set_test_mode(0);
    state1 = draw_set_ztest_mode(0);
    state2 = draw_set_fc_mode(0x2000);
    fog_old = fog_set_enable(fog);
    ulGxSelectTexture(ulGxGetTextureList(), tex);
    nj_tex_strip_draw(vtx, 4, mode);
    fog_set_enable(fog_old);
    draw_set_fc_mode(state2);
    draw_set_ztest_mode(state1);
    draw_set_test_mode(state0);
}

/* 12 item models (wire forms where flagged) and the item variables. */
/* 0x00123298 */
void item_params_register(void)
{
    ItemModel *model = item_model_table;
    ItemParam *p = item_params;
    int i;
    int *ent;
    int id;

    for (i = 12; i != 0; i--) {
        ent = pnb_get_entry(item_models, p->id);
        dc_object_init(model, *ent, 0, p->val);
        if (p->flag) {
            id = model_pnb_get_wire(item_models, p->id);
            dc_object_set_wire(model, id);
        }
        model++;
        p++;
    }
    model_pnb_get_wire(item_models, 13);
    model_pnb_get_wire(item_models, 16);
    id = gw_varedit_add(&gw_varedit_root, 0, item_vardef);
    gw_varedit_add_table(&gw_varedit_root, id, item_vardefs);
    change_symbol_params_register();
}

/* Event 5, formation not all shot down: its item still counts. */
/* 0x00123380 */
void item_count_listener(FormMsg *msg)
{
    unsigned int k = msg->idx;
    int drop;

    if ((int)k >= 0) {
        if (k < 20) {
            drop = item_drop_kinds[k];
            if (drop == 1) {
                if (!(game_flags & 0x40))
                    return;
            }
            if (drop >= 0)
                item_count();
        }
    }
}

/* Event 6, formation all shot down: drops its item. */
/* 0x001233e8 */
void item_spawn_listener(FormMsg *msg)
{
    unsigned int k = msg->idx;
    Spawn spawn;

    if ((int)k >= 0) {
      if (k < 20) {
        spawn.kind = item_drop_kinds[k];
        if (spawn.kind >= 0) {
            spawn.x = msg->pos[0];
            spawn.y = msg->pos[1];
            spawn.z = msg->pos[2];
            spawn.scale = 0.9f;
            spawn.vel_x = 0;
            spawn.vel_y = 0;
            spawn.vel_z = 0;
            item_create(&spawn);
        }
      }
    }
}

/* 0x00123468 */
void item_got_listener(void)
{
    item_count_got();
}

/* item_base_dist along player + camera angles, plus item_base_forward along motion while moving. */
/* 0x00123470 */
void item_scatter_pos(float *out)
{
    float cam[3];
    int ang[3];
    float dir[3];
    int rot[3];
    float t[3];
    float sc[3][2];
    float x, y, z;
    int tiny;

    player_get_pos(cam);
    player_get_rot(ang);
    player_get_accel(dir);
    tiny = 0;
    if (fabsf(dir[0]) < 0.01f && fabsf(dir[1]) < 0.01f && fabsf(dir[2]) < 0.01f)
        tiny = 1;
    if (!tiny)
        vec3_normalize(dir, dir);
    x = 0.0f;
    y = 0.0f;
    z = -1.0f;
    player_camera_get_angles(rot);
    rot[0] = rot[0] + ang[0];
    rot[1] = rot[1] + ang[1];
    rot[2] = rot[2] + ang[2];
    ROTZ(0);
    ROTX(1);
    ROTY(2);
    t[0] = x * item_base_dist;
    t[1] = y * item_base_dist;
    t[2] = z * item_base_dist;
    cam[0] += t[0];
    cam[1] += t[1];
    cam[2] += t[2];
    dir[0] = dir[0] * item_base_forward;
    dir[1] = dir[1] * item_base_forward;
    dir[2] = dir[2] * item_base_forward;
    cam[0] += dir[0];
    cam[1] += dir[1];
    cam[2] += dir[2];
    out[0] = cam[0];
    out[1] = cam[1];
    out[2] = cam[2];
}

/* 0x00123678 */
void item_task_init(ItemManageWork *w)
{
    item_manage_work = w;
    w->count_listener = listener_add(5, item_count_listener);
    w->spawn_listener = listener_add(6, item_spawn_listener);
    w->got_listener = listener_add(10, item_got_listener);
}

/* 0x001236e0 */
void item_task_release(ItemManageWork *w)
{
    listener_remove(5, w->count_listener);
    listener_remove(6, w->spawn_listener);
    listener_remove(10, w->got_listener);
    item_manage_work = 0;
}

/* Debug key 68 ends every item. */
/* 0x00123730 */
void item_task_exec(ItemManageWork *w)
{
    if (kbd_key == 68)
        task_end_children(w);
    item_scatter_pos(w->pos);
}

/* Debug: box at the anchor and five sample spots. */
/* 0x00123770 */
void item_task_disp(ItemManageWork *w)
{
    float pos[3];
    int n;

    if (item_is_debug) {
        ulMatrixPush();
        do {
            vu0_matrix_translate(ulMatrixCur, w->pos[0], w->pos[1], w->pos[2]);
        } while (0);
        debug_draw_box(RGBA(0x80, 0x00, 0x00, 0x80), 30.0f);
        n = 5;
        ulMatrixPop();
        do {
            do {
                item_random_offset(pos);
            } while (0);
            ulMatrixPush();
            n--;
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
            debug_draw_box(RGBA(0x80, 0x80, 0x00, 0x80), 1.0f);
            ulMatrixPop();
        } while (n);
    }
}

/* 0x00123830 */
int item_task_create(void *parent)
{
    return task_create(parent, &item_manage_desc, sizeof(ItemManageWork));
}

/* Up to item_base_s_range on the camera's right and up axes; none in game types 0 and 6. */
/* 0x00123858 */
void item_random_offset(float *out)
{
    ItemManageWork *obj = item_manage_work;
    float ofs[3];
    int ang[3];
    float sc[6][2];
    float scale, x, y, z, rnd;
    int type;

    if (!obj || (TASK_OF(obj)->flags & TASK_F_END)) {
        out[0] = 0;
        out[1] = 0;
        out[2] = 0;
        return;
    }
    type = game_type_get();
    if (type == 0 || type == 6)
        scale = 0.0f;
    else
        scale = 1.0f;
    player_camera_get_angles(ang);
    x = 1.0f;
    y = 0.0f;
    z = 0.0f;
    ROTZ_ITEM_RANDOM_OFFSET(0);
    ROTX_ITEM_RANDOM_OFFSET(1);
    ROTY_ITEM_RANDOM_OFFSET(2);
    rnd = RND();
    rnd *= scale;
    ofs[0] = x * rnd;
    ofs[1] = y * rnd;
    ofs[2] = z * rnd;
    z = 0.0f;
    y = 1.0f;
    x = 0.0f;
    ROTZ_ITEM_RANDOM_OFFSET(3);
    ROTX_ITEM_RANDOM_OFFSET(4);
    ROTY_ITEM_RANDOM_OFFSET(5);
    rnd = RND();
    rnd *= scale;
    ofs[0] = ofs[0] + x * rnd;
    ofs[1] = ofs[1] + y * rnd;
    ofs[2] = ofs[2] + z * rnd;
    out[0] = obj->pos[0] + ofs[0];
    out[1] = obj->pos[1] + ofs[1];
    out[2] = obj->pos[2] + ofs[2];
}

/* 0x00123b18 */
void item_count(void)
{
    if (item_manage_work && !(TASK_OF(item_manage_work)->flags & TASK_F_END))
        player_status_count_item();
}

/* 0x00123b48 */
void item_count_got(void)
{
    if (item_manage_work && !(TASK_OF(item_manage_work)->flags & TASK_F_END))
        player_status_count_item_got();
}

/* 0x00123b78 */
void item_set_max(ItemWork *w, unsigned int val)
{
    if (w->cur < val)
        w->max = val;
}

/* 0x00123b90 */
void item_pickup_notify(ItemWork *w)
{
}

/* steer(cast) */
/* 0x00123b98 */
void item_draw_model(ItemWork *w)
{
    float *pos = w->pos;
    float col[4];
    int saved;
    int idx;

    if (view_sphere_visible(pos, 10.0f) != 0) {
        saved = fog_set_enable(0);
        ulGxSetTextureList(item_textures);
        draw_set_blend(1, 1);
        draw_set_color(w->color);
        do {
            nj_cnk_set_override_flags(8192);
        } while (0);
        ulMatrixPush();
        ((void (*)(unsigned int, int))draw_set_strip_flags)(0xffffffff, 2048);
        col[1] = 1.0f;
        col[2] = 1.0f;
        col[3] = 1.0f;
        col[0] = 1.0f - w->lock_used;
        draw_set_override_color(col);
        do {
            vu0_matrix_translate(ulMatrixCur, w->pos[0], pos[1], pos[2]);
        } while (0);
        item_set_camera(w->rot);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        idx = item_kind_effects[w->kind];
        draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_SET);
        dc_object_draw(&item_model_table[idx]);
        draw_set_override_flags(0);
        if (w->flags & ITEM_GLOW) {
            ulGxSetAlphaBlend(4, 5);
            dc_object_draw(&item_model_table[idx + 2]);
        }
        dc_object_draw(&item_model_table[idx + 1]);
        ulMatrixPop();
        fog_set_enable(saved);
        nj_cnk_set_override_flags(0);
    }
}

/* 0x00123d18 */
void item_update_scale(ItemWork *w)
{
    int left = w->lock.life;

    w->lock_left = left;
    w->lock_used = 1.0f - (float)left / (float)w->lock_time;
}

/* 0x00123d50 */
void item_update_speed(ItemWork *w)
{
    w->color = color_blend(w->color, item_kind_colors[w->kind], 0.1f);
}

/* r is the squared radius. */
/* 0x00123da0 */
void item_push_away(float *center, float *pos, float r)
{
    float diff[3];
    float len;
    float rate;

    diff[0] = center[0] - pos[0];
    diff[1] = center[1] - pos[1];
    diff[2] = center[2] - pos[2];
    len = vec3_length_squared(diff);
    if (len <= r) {
        rate = r / len;
        rate = sqrtf(rate);
        rate = 1.0f - rate;
        diff[0] = diff[0] * rate;
        diff[1] = diff[1] * rate;
        diff[2] = diff[2] * rate;
        pos[0] = pos[0] + diff[0];
        pos[1] = pos[1] + diff[1];
        pos[2] = pos[2] + diff[2];
    }
}

/* Out of each earlier item's sphere (item_coll_r), then the player's (item_coll_r_2). */
/* 0x00123ea0 */
void item_push_chain(ItemWork *w)
{
    ItemWork *item;
    ItemWork *next;
    Task *link;

    link = TASK_OF(item_manage_work)->child;
    item = link ? TASK_WORK(link) : 0;
    if (item && item != w) {
        do {
            item_push_away(item->org.pos, w->org.pos, item_coll_r);
            {
                Task *next_link = TASK_OF(item)->next;
                next = next_link ? TASK_WORK(next_link) : 0;
            }
            item = next;
        } while (next && next != w);
    }
    item_push_away(item_zero_vec, w->org.pos, item_coll_r_2);
}

/* Wire-break of the kind's effect model, falling slowly, fading over 120 frames. */
/* 0x00123f60 */
void item_spawn_effect(ItemWork *w)
{
    ItemEffect eff;
    int col = w->color;
    int inv_mtx;
    Task *parent;

    eff.obj = &item_effects[item_kind_effects[w->kind]];
    eff.spd = 0.16666667f;
    eff.drag = 0.999f;
    eff.rot_range = 0.3f;
    eff.col = col;
    eff.grav_y = -0.0055555557f;
    eff.grav_x = 0;
    eff.grav_z = 0;
    eff.timer = 120;
    eff.col_to = col & 0xffffff;
    ulMatrixPush();
    inv_mtx = camera_get_inv_mtx();
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, inv_mtx);
    do {
        float *p = w->pos;

        vu0_matrix_translate(ulMatrixCur, p[0], p[1], p[2]);
    } while (0);
    item_set_camera(w->rot);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    parent = TASK_OF(w)->parent;
    wire_break_task_create(parent ? TASK_WORK(parent) : 0, &eff);
    ulMatrixPop();
}

/* Start and target (random spot near the anchor) relative to the player. */
/* 0x00124088 */
void item_set_origin(ItemOrigin *w, float *src)
{
    float player_pos[3];
    float *dst = w->target;

    w->pos[0] = src[0];
    w->pos[1] = src[1];
    w->pos[2] = src[2];
    item_random_offset(dst);
    player_get_pos(player_pos);
    w->pos[0] -= player_pos[0];
    w->pos[1] -= player_pos[1];
    w->pos[2] -= player_pos[2];
    w->target[0] -= player_pos[0];
    w->state = 0;
    dst[1] -= player_pos[1];
    dst[2] -= player_pos[2];
}

/* 0: waits for item_base_a_range; 1: moves against player motion; 2: sits on the player. */
/* 0x00124138 */
void item_approach_exec(ItemOrigin *w, float speed)
{
    float move[3];
    int tiny;
    float *p;

    switch ((unsigned int)w->state) {
    case 0:
        if (vec3_distance_squared((Vec3 *)w->pos, (Vec3 *)w->target) <= item_base_a_range * item_base_a_range)
            w->state = 1;
        break;
    case 1:
        player_get_frame_accel(move);
        tiny = 0;
        if (fabsf(move[0]) < 0.01f && fabsf(move[1]) < 0.01f && fabsf(move[2]) < 0.01f)
            tiny = 1;
        if (!tiny) {
            vec3_normalize(move, move);
            move[0] = move[0] * speed;
            move[1] = move[1] * speed;
            move[2] = move[2] * speed;
        }
        p = w->target;
        w->target[0] = w->target[0] - move[0];
        p[1] = p[1] - move[1];
        p[2] = p[2] - move[2];
        break;
    case 2:
        p = w->target;
        *(int *)&w->target[0] = 0; /* steer(type) */
        ((int *)p)[1] = 0;
        ((int *)p)[2] = 0;
        break;
    }
}

/* 4: with the player; others but 3 ease to the target (item_move_rate, item_vel_max). */
/* 0x00124298 */
void item_resolve_pos(ItemOrigin *obj, float *out, float *add)
{
    float vec[3];
    float step[3];

    if (obj->state != 3) {
        if (obj->state == 4) {
            player_get_frame_accel(vec);
            obj->pos[0] = obj->pos[0] + vec[0];
            obj->pos[1] = obj->pos[1] + vec[1];
            obj->pos[2] = obj->pos[2] + vec[2];
        } else {
            vec3_sub_scale(item_move_rate, (Vec3 *)obj->target, (Vec3 *)obj->pos, (Vec3 *)step);
            vec3_clamp_length(item_vel_max, step, step);
            obj->pos[0] = obj->pos[0] + step[0];
            obj->pos[1] = obj->pos[1] + step[1];
            obj->pos[2] = obj->pos[2] + step[2];
        }
    }
    if (add) {
        obj->pos[0] = obj->pos[0] + add[0];
        obj->pos[1] = obj->pos[1] + add[1];
        obj->pos[2] = obj->pos[2] + add[2];
    }
    if (out) {
        player_get_pos(vec);
        out[0] = obj->pos[0] + vec[0];
        out[1] = obj->pos[1] + vec[1];
        out[2] = obj->pos[2] + vec[2];
    }
}

/* 0x001243e0 */
void item_set_state(ItemOrigin *obj, int state)
{
    obj->state = state;
}

/* Kinds 0..3; a higher kind ends the task. */
/* 0x001243e8 */
void item_init(ItemWork *w, ItemSpawn *spawn)
{
    int kind = spawn->kind;
    int lock_time;
    LockObj *lock;
    float *src;
    float *dst;

    if (kind >= 4) {
        ERROR_PRINTF("Item Kind Over : %d\n", kind);
        TASK_END(w);
        return;
    }
    w->kind = kind;
    src = spawn->vel;
    dst = w->vel;
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    w->decay = spawn->decay;
    item_set_origin(&w->org, spawn->origin);
    w->scale = 1.0f;
    w->life = item_life_time;
    w->color = RGBA(0xff, 0xff, 0xff, 0x80);
    w->rot_spd_y = 728;
    w->flags = ITEM_GLOW;
    w->rot_spd_z = 0;
    w->approach = 1;
    switch (w->kind) {
    case 0:
        lock_time = item_locktime_o;
        break;
    case 1:
        lock_time = item_locktime_s;
        break;
    case 2:
    default:
        lock_time = item_locktime_p;
        break;
    }
    lock = &w->lock;
    w->lock_time = lock_time;
    lockon_obj_init(lock, w, (Vec3 *)w->pos, 0, item_lock_names[w->kind], -1, lock_time, 1, 16);
    lockon_entry(lock);
    item_count();
}

/* 0x00124550 */
void item_release(ItemWork *w)
{
    if (w->con)
        console_destroy_follow(w->con);
    lockon_remove(&w->lock);
}

/* Life ran out: keeps drifting and spinning, ends after one second. */
/* 0x00124588 */
void item_fade_exec(ItemWork *w)
{
    float *vel = w->vel;
    float damp;
    float t;

    item_resolve_pos(&w->org, w->pos, w->vel);
    damp = w->decay;
    vel[0] = vel[0] * damp;
    w->rot[0] = w->rot[0] + w->rot_spd_y;
    vel[1] = vel[1] * damp;
    vel[2] = vel[2] * damp;
    item_push_chain(w);
    t = w->scale - FRAME_SEC;
    w->scale = t;
    if (t <= 0.0f) {
        if (w)
            TASK_OF(w)->flags |= TASK_F_END;
    }
}

/* Spins faster; when life is out, shrinks and ends unless a console follows it. */
/* 0x00124640 */
void item_fall_exec(ItemWork *w)
{
    float *vel = w->vel;
    float damp;
    float t;
    int life;

    item_resolve_pos(&w->org, w->pos, w->vel);
    damp = w->decay;
    vel[0] = vel[0] * damp;
    w->rot[1] = w->rot[1] + w->rot_spd_y;
    w->rot[2] = w->rot[2] + w->rot_spd_z;
    w->rot_spd_z = w->rot_spd_z + 18;
    vel[1] = vel[1] * damp;
    vel[2] = vel[2] * damp;
    item_push_chain(w);
    item_update_speed(w);
    life = w->life;
    if (life <= 0) {
        t = w->scale - 0.05f;
        w->scale = t;
        if (t <= 0.0f) {
            w->scale = 0.0f;
            if (!w->con)
                TASK_END(w);
        }
    } else {
        w->life = life - 1;
    }
    item_pickup_notify(w);
}

/* Lock time used up: pickup sound, break-up, status display, score, message 10. */
/* 0x00124740 */
void item_pickup_exec(ItemWork *w)
{
    float *vel = w->vel;
    float damp;
    int spd;
    unsigned int flags;
    float pos[3];
    int arg[1];

    item_resolve_pos(&w->org, w->pos, w->vel);
    damp = w->decay;
    vel[0] = vel[0] * damp;
    w->rot[1] = w->rot[1] + w->rot_spd_y;
    w->rot[2] = w->rot[2] + w->rot_spd_z;
    vel[1] = vel[1] * damp;
    vel[2] = vel[2] * damp;
    item_push_chain(w);
    w->rot_spd_y = (int)((float)w->rot_spd_y * 0.99f);
    item_update_speed(w);
    spd = w->rot_spd_y + 54;
    w->rot_spd_y = spd;
    if (spd >= 1456)
        w->rot_spd_y = 1456;
    item_update_scale(w);
    if (w->lock_left <= 0) {
        w->life = 12;
        item_set_max(w, 4);
        SoundRhythmSe_StgCommon(w->kind + 1);
        flags = w->flags;
        if (flags & 1)
            w->flags = flags & ~1;
        item_spawn_effect(w);
        w->flags &= ~ITEM_GLOW;
        item_set_state(&w->org, 3);
        lockon_obj_get_screen_pos((float *)&w->lock, pos);
        switch (w->kind) {
        case 0:
            ps_sub_add_item_spawn(pos[0], pos[1], 0, 1);
            break;
        case 1:
            break;
        case 2:
            ps_sub_add_item_spawn(pos[0], pos[1], 2, 1);
            break;
        case 3:
            ps_sub_add_item_spawn(pos[0], pos[1], 2, 3);
            break;
        }
        player_status_add_score(item_kind_scores[w->kind]);
        arg[0] = w->kind;
        listener_dispatch(10, arg);
        TASK_OF(w)->exec = item_fall_exec;
    }
    item_pickup_notify(w);
}

static inline void vadd3(float *dst, float *va, float *vb)
{
    dst[0] = va[0] + vb[0];
    dst[1] = va[1] + vb[1];
    dst[2] = va[2] + vb[2];
}

/* While locked, life holds; shot: item_pickup_exec; life out: item_fade_exec. */
/* 0x00124958 */
void item_main_exec(ItemWork *w)
{
    float *vel;
    float damp;
    int life;
    unsigned int flags;

    if (w->approach)
        item_approach_exec(&w->org, item_base_spd);
    vel = w->vel;
    item_resolve_pos(&w->org, w->pos, w->vel);
    damp = w->decay;
    vel[0] = vel[0] * damp;
    w->rot[1] = w->rot[1] + w->rot_spd_y;
    w->rot[2] = w->rot[2] + w->rot_spd_z;
    vel[1] = vel[1] * damp;
    vel[2] = vel[2] * damp;
    item_push_chain(w);
    life = w->life - 1;
    w->life = life;
    if (life > 0) {
        w->rot_spd_y = (int)((float)w->rot_spd_y * 0.99f);
        item_update_speed(w);
        flags = w->lock.flags;
        if (flags & 0x4000) {
            item_set_max(w, 1);
            item_set_state(&w->org, 2);
            flags = w->lock.flags;
            w->life = w->life + 1;
        }
        if (flags & 0x8000) {
            item_set_max(w, 2);
            TASK_OF(w)->exec = item_pickup_exec;
        }
        item_pickup_notify(w);
    } else {
        item_spawn_effect(w);
        w->flags &= ~ITEM_GLOW;
        item_set_state(&w->org, 3);
        TASK_OF(w)->exec = item_fade_exec;
        w->scale = 1.0f;
        w->lock.flags &= ~LOCK_LOCKABLE;
    }
}

/* Debug: boxes at position and target. */
/* 0x00124af0 */
void item_disp(ItemWork *w)
{
    float pos[3];

    item_draw_model(w);
    if (item_is_debug) {
        player_get_pos(pos);
        vadd3(pos, pos, w->org.pos);
        ulMatrixPush();
        do {
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
        } while (0);
        debug_draw_box(RGBA(0x00, 0x80, 0x00, 0x80), item_base_a_range);
        ulMatrixPop();
        player_get_pos(pos);
        vadd3(pos, pos, w->org.target);
        ulMatrixPush();
        do {
            vu0_matrix_translate(ulMatrixCur, pos[0], pos[1], pos[2]);
        } while (0);
        debug_draw_box(RGBA(0x00, 0x00, 0x80, 0x80), item_base_a_range);
        ulMatrixPop();
    }
}

/* 0x00124c10 */
void *item_create(Spawn *spawn)
{
    if (!(stage_exit_flags & 2)) {
        if (spawn->kind == 1) {
            if (!(game_flags & 0x40))
                return 0;
        }
        if (item_is_out) {
            DEBUG_PRINTF("Item In! [%d]\n", spawn->kind);
            return task_create_arg(item_manage_work, &item_desc, sizeof(ItemWork), spawn);
        }
    }
    return 0;
}

/* 0x00124ca8 */
void item_state_if_alive(ItemWork *w, int state)
{
    if (w && !(TASK_OF(w)->flags & TASK_F_END))
        item_set_state(&w->org, state);
}

/* 0x00124cd0 */
void item_set_level(ItemWork *w, int level)
{
    if (w && !(TASK_OF(w)->flags & TASK_F_END))
        w->life = level > 1 ? level : 2;
}

/* 0x00124d00 */
void item_set_camera(int *rot)
{
    vu0_matrix_rotate_y(ulMatrixCur, rot[1]);
    vu0_matrix_rotate_x(ulMatrixCur, rot[0]);
    vu0_matrix_rotate_z(ulMatrixCur, rot[2]);
}
