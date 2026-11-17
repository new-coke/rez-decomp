/*
 * Player camera: "Camera" task following the player along the rail. Turns toward the view angles
 * (dead zone, follow rate), height/distance from view pitch, settings eased toward a target set
 * (stage presets, game events); fed to the camera system as the "Player Camera" node.
 */

#include "task.h"
#include "types_game.h"
#include "types_gw.h"
#include "mathdef.h"
#include "libc.h"
#include "gw_varedit.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "player_camera.h"
#include "approach.h"
#include "camera.h"
#include "config.h"
#include "debug.h"
#include "game.h"
#include "lockon.h"
#include "math_util.h"
#include "player.h"
#include "system.h"

typedef struct PlayerCam {
    float ofs_y;
    float ofs_z;
    char pad_08[12];
    Vec3 pos;
    IVec3 ang;
    Vec3 eye;
    IVec3 aim;
    IVec3 aim_tgt;
    IVec3 rot;
    char pad_5c[8];
} PlayerCam;

typedef struct { int x, y, z; } PlayerCamAng;

/* Rotations by -ang about Y, X, Z. */
#define ROT_Y(ang, x, z)                                                                     \
    do {                                                                                     \
        float rot_s, rot_c, rot_u = x, rot_v = z;                                            \
        ulSinCos(-(ang), &rot_s, &rot_c);                                                \
        x = rot_u * rot_c + rot_v * rot_s;                                                   \
        z = -rot_u * rot_s + rot_v * rot_c;                                                  \
    } while (0)

#define ROT_X(ang, y, z)                                                                     \
    do {                                                                                     \
        float rot_s, rot_c, rot_u = y, rot_v = z;                                            \
        ulSinCos(-(ang), &rot_s, &rot_c);                                                \
        y = rot_u * rot_c - rot_v * rot_s;                                                   \
        z = rot_u * rot_s + rot_v * rot_c;                                                   \
    } while (0)

#define ROT_Z(ang, x, y)                                                                     \
    do {                                                                                     \
        float rot_s, rot_c, rot_u = x, rot_v = y;                                            \
        ulSinCos(-(ang), &rot_s, &rot_c);                                                \
        x = rot_u * rot_c - rot_v * rot_s;                                                   \
        y = rot_u * rot_s + rot_v * rot_c;                                                   \
    } while (0)

/* steer(alias): the code that matched reads PlayerCam through these member types; one view
 * shared by those sites (src/game/player_camera.c). */
typedef struct PlayerCamAngZView {
    char pad_00[20];
    Vec3 pos;
    int ang_x;
    int ang_y;
    int ang_z;
    Vec3 eye;
    int aim_x;
    int aim_y;
    int aim_z;
} PlayerCamAngZView;

#define PUT_XFORM(v, r)                                                                      \
    do {                                                                                     \
        float *pv = (v);                                                                     \
        vu0_matrix_translate(ulMatrixCur, pv[0], pv[1], pv[2]);                                       \
    } while (0);                                                                             \
    vu0_matrix_rotate_y(ulMatrixCur, (r)[1]);                                                        \
    vu0_matrix_rotate_x(ulMatrixCur, (r)[0]);                                                        \
    vu0_matrix_rotate_z(ulMatrixCur, (r)[2])

typedef struct PlayerCamOfsView { float x, y; char pad_08[48]; int ang; } PlayerCamOfsView;
typedef struct PlayerCameraData { float x0, x1, y0, y1; } PlayerCameraData;

typedef struct PlayerCameraDataMarginView {
    char pad_00[28];
    float cam_ang_rate_x, cam_ang_rate_y, cam_rate_x, cam_rate_y;
    int cam_margin_dx, cam_margin_dy;
} PlayerCameraDataMarginView;

/* steer(alias): the code that matched reads PlayerCam through these member types; one view
 * shared by those sites (src/game/player_camera.c). */
typedef struct PlayerCamAimView {
    char pad_00[56];
    int aim[2];
    char pad_40[4];
    int aim_tgt[2];
    char pad_4c[16];
    Vec3 fwd;
} PlayerCamAimView;

#define SX16(a) (((s_ = (a) & 0x8000)) ? ((a) | ~0xffff) : ((a) & 0xffff))

#define DEADZONE(c, dd, lim, k) {                                       \
    PlayerCameraDataMarginView *z_ = &(*(PlayerCameraDataMarginView *)&player_camera_data);                                            \
                                                                        \
    if ((dd) >= z_->lim)                                                \
        (c) = (c) + (int)((float)((dd) - z_->lim) * z_->k);             \
    else if (!(-z_->lim < (dd)))                                        \
        (c) = (c) + (int)((float)((dd) + z_->lim) * z_->k);             \
}

/* steer(alias): the code that matched reads PlayerCameraData through these member types; one view
 * shared by those sites (src/game/player_camera.c). */
typedef struct PlayerCameraDataView {
    float cam_under_height;
    float cam_top_height;
    float cam_under_dist;
    float cam_top_dist;
    char p_ang[12];
    float cam_ang_rate_x;
    float cam_ang_rate_y;
    float cam_rate_x;
    float cam_rate_y;
    int cam_margin_dx;
    int cam_margin_dy;
    int eye_limit_y;
    int eye_limit_x_top;
    int eye_limit_x_und;
} PlayerCameraDataView;

#define ROT_Y_PLAYER_CAMERA_SET_CURSOR(ang, x, z)                                                                     \
    do {                                                                                     \
        float rot_s, rot_c, rot_u = x, rot_v = z;                                            \
        ulSinCos(ang, &rot_s, &rot_c);                                                   \
        x = rot_u * rot_c + rot_v * rot_s;                                                   \
        z = -rot_u * rot_s + rot_v * rot_c;                                                  \
    } while (0)

#define ROT_X_PLAYER_CAMERA_SET_CURSOR(ang, y, z)                                                                     \
    do {                                                                                     \
        float rot_s, rot_c, rot_u = y, rot_v = z;                                            \
        ulSinCos(ang, &rot_s, &rot_c);                                                   \
        y = rot_u * rot_c - rot_v * rot_s;                                                   \
        z = rot_u * rot_s + rot_v * rot_c;                                                   \
    } while (0)

typedef struct I3ZeroPlayerCameraData { char b[64]; } I3ZeroPlayerCameraData;

typedef struct PlayerCamAngXView {
    char pad_00[20];
    PlayerCamVec pos;
    float ang_x, ang_y;
    char pad_28[4];
    PlayerCamVec eye;
    int aim_x;
    float aim_y, aim_z;
    char pad_44[48];
    int prev;
    int listener;
} PlayerCamAngXView;

/* steer(alias): the code that matched reads PlayerCameraData through these member types; one view
 * shared by those sites (src/game/player_camera.c). */
typedef struct PlayerCameraDataRotView {
    char pad_00[16];
    int rot[3];
} PlayerCameraDataRotView;

extern char gw_varedit_root[];
extern GwVarDef player_camera_vardef[];
extern GwVarDef player_camera_vardefs[];
extern int ulMatrixPush(void);
extern float cam_data_comp_rate;

extern void project_xyz(float, float, float, float *);

extern PlayerCamAngXView *player_camera_work;
extern I3ZeroPlayerCameraData player_camera_data;
extern char player_camera_hooks[];
extern struct TaskDesc player_camera_desc;
extern char player_camera_presets[][64];
extern int player_camera_target[];

static void player_camera_task_exec(PlayerCamWork *w);
static void *player_camera_task_create(void *parent);
static void player_camera_rotate_yxz(int *ang);

/* 0x001545a8 */
void player_camera_vars_register(void)
{
    int grp = gw_varedit_add(gw_varedit_root, 0, player_camera_vardef);

    gw_varedit_add_table(gw_varedit_root, grp, player_camera_vardefs);
}

/* 0x001545f0 */
int player_camera_start(void * parent)
{
    return player_camera_task_create(parent);
}

/* Null allowed. */
/* 0x00154610 */
void player_task_end_child(void *work)
{
    if (work) {
        do { TASK_OF(work)->flags |= TASK_F_END; } while (0);
    }
}

/* 0x00154630 */
void player_camera_copy(PlayerCam *src, PlayerCam *dst)
{
    dst->ofs_y = src->ofs_y;
    dst->ofs_z = src->ofs_z;
    c13_v3copy(&dst->pos, &src->pos);
    c13_i3copy(&dst->ang, &src->ang);
    c13_v3copy(&dst->eye, &src->eye);
    c13_i3copy(&dst->aim, &src->aim);
    c13_i3copy(&dst->aim_tgt, &src->aim_tgt);
}

/* Aim and its target. */
/* 0x001546e8 */
void player_camera_set_aim(PlayerCam *w, IVec3 *ang)
{
    c13_i3copy(&w->aim, ang);
    c13_i3copy(&w->aim_tgt, ang);
}

/*
 * Camera mode 1: own eye and aim; else view angles from the current camera, w->pos relative to
 * its eye. ang gets the inverse view angles.
 */
/* 0x00154728 */
static void player_camera_view_transform(PlayerCam *w, float *out, int *ang)
{
    int cam_ang[3];
    Vec3 dir;
    float x, y, z;
    Camera *cam;

    if (camera_get_mode() == 1) {
        x = -w->eye.x;
        y = -w->eye.y;
        z = -w->eye.z;
        ROT_Y(w->aim.y, x, z);
        ROT_X(w->aim.x, y, z);
        ROT_Z(w->aim.z, x, y);
        out[0] = x;
        out[1] = y;
        out[2] = z;
        ang[0] = -w->aim.x;
        ang[1] = -w->aim.y;
        ang[2] = -w->aim.z;
    } else {
        cam = camera_get_current();
        do {
            Vec3 *pd = &dir;
            Vec3 *center = &cam->center;

            dir.x = center->x - cam->eye.x;
            dir.y = center->y - cam->eye.y;
            dir.z = center->z - cam->eye.z;
            vec3_normalize(pd, pd);
        } while (0);
        cam_ang[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        cam_ang[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
        do {
            Vec3 *up = &cam->up;

            x = up->x;
            y = up->y;
            z = up->z;
        } while (0);
        ROT_Y(cam_ang[1], x, z);
        ROT_X(cam_ang[0], y, z);
        cam_ang[2] = (int)RAD_TO_ANG(atan2f(-x, y));
        x = w->pos.x - cam->eye.x;
        y = w->pos.y - cam->eye.y;
        z = w->pos.z - cam->eye.z;
        ROT_Y(cam_ang[1], x, z);
        ROT_X(cam_ang[0], y, z);
        ROT_Z(cam_ang[2], x, y);
        out[0] = x;
        out[1] = y;
        out[2] = z;
        ang[0] = -cam_ang[0];
        ang[1] = -cam_ang[1];
        ang[2] = -cam_ang[2];
    }
}

/* 0x00154b10 */
static void player_camera_node_init(void)
{
}

/* At w->pos turned by w's angles, eye offset turned by the aim; look point one unit ahead. */
/* 0x00154b18 */
static void player_camera_node_update(Camera *cam, PlayerCamAngZView *w)
{
    float up[3];
    float fw[3];
    float *dir = fw;

    up[0] = 0;
    up[1] = 1.0f;
    up[2] = 0;
    fw[0] = 0;
    fw[1] = 0;
    dir[2] = -1.0f;
    ulMatrixPush();
    matrix_unit_set(0);
    PUT_XFORM(&w->pos.x, &w->ang_x);
    PUT_XFORM(&w->eye.x, &w->aim_x);
    ulMatrixGetTranslation(ulMatrixCur, cam);
    ulMatrixApplyVector(ulMatrixCur, up, &cam->up);
    ulMatrixApplyPoint(ulMatrixCur, dir, &cam->center.x);
    ulMatrixPop();
}

/* 0x00154c40 */
static void player_camera_node_release(void)
{
}

/* Halfway toward the under..top settings over a view pitch of -80..+80 degrees. */
/* 0x00154c48 */
static void player_camera_follow_ofs(PlayerCamOfsView *w)
{
    PlayerCameraData *data = &player_camera_data;
    int ang = w->ang;
    float t;
    float tx;
    float ty;

    if (ang & 0x8000)
        t = (float)((int)(ang | 0xffff0000) + 14563) / 29127.0f;
    else
        t = (float)((ang & 0xffff) + 14563) / 29127.0f;
    tx = data->x0 + (data->x1 - data->x0) * t;
    ty = data->y0 + (data->y1 - data->y0) * t;
    w->x = w->x + (tx - w->x) * 0.5f;
    w->y = w->y + (ty - w->y) * 0.5f;
}

/*
 * Per axis, cam_rate of the difference beyond cam_margin. Pitch target scaled by cam_ang_rate_x,
 * yaw by cam_ang_rate_y while player_eye_ang_limit.
 */
/* 0x00154cf8 */
static void player_camera_follow_aim(PlayerCamAimView *w)
{
    int *cur = w->aim;
    int *tgt = w->aim_tgt;
    int yaw;
    int diff;
    int s_;

    debug_set_origin(0, 30);
    if (player_eye_ang_limit)
        yaw = (int)((float)tgt[1] * (*(PlayerCameraDataMarginView *)&player_camera_data).cam_ang_rate_y); /* steer(cast) */
    else
        yaw = tgt[1];
    do { /* steer(barrier) */
        diff = SX16(yaw - cur[1]);
        DEADZONE(cur[1], diff, cam_margin_dy, cam_rate_y);
        diff = SX16((int)((float)tgt[0] * (*(PlayerCameraDataMarginView *)&player_camera_data).cam_ang_rate_x) - cur[0]); /* steer(cast) */
        DEADZONE(cur[0], diff, cam_margin_dx, cam_rate_x);
        do { } while (0);
    } while (0);
}

/*
 * Heights, distances, turn by cam_data_comp_rate; rates, margins, view limits at once (limits
 * also to player_view_ang_mid/hi/lo).
 */
/* 0x00154e60 */
static void player_camera_data_blend(PlayerCameraDataView *dst, PlayerCameraDataView *src)
{
    dst->cam_under_height = float_lerp(src->cam_under_height, dst->cam_under_height, cam_data_comp_rate);
    dst->cam_top_height = float_lerp(src->cam_top_height, dst->cam_top_height, cam_data_comp_rate);
    dst->cam_under_dist = float_lerp(src->cam_under_dist, dst->cam_under_dist, cam_data_comp_rate);
    dst->cam_top_dist = float_lerp(src->cam_top_dist, dst->cam_top_dist, cam_data_comp_rate);
    ang3_approach((char *)src + 16, (char *)dst + 16, cam_data_comp_rate);
    dst->cam_margin_dx = src->cam_margin_dx;
    dst->cam_ang_rate_x = src->cam_ang_rate_x;
    dst->cam_margin_dy = src->cam_margin_dy;
    dst->cam_ang_rate_y = src->cam_ang_rate_y;
    player_view_ang_mid = dst->eye_limit_y = src->eye_limit_y;
    dst->cam_rate_x = src->cam_rate_x;
    player_view_ang_hi = dst->eye_limit_x_top = src->eye_limit_x_top;
    dst->cam_rate_y = src->cam_rate_y;
    player_view_ang_lo = dst->eye_limit_x_und = src->eye_limit_x_und;
}

/* Lock-on cursor where the player looks: view angles minus camera aim, projected per axis. */
/* 0x00154f58 */
static void player_camera_set_cursor(PlayerCamAngZView *w)
{
    float pos[4];
    float rot[4];
    int eye_ang[4];
    float out[4];
    float x, y, z;
    float u;
    int pitch, yaw;

    player_get_state_vectors(pos, 0, rot, eye_ang);
    pitch = eye_ang[0] - w->aim_x;
    yaw = eye_ang[1] - w->aim_y;
    x = 0.0f;
    y = 0.0f;
    z = 1.0f;
    ROT_X_PLAYER_CAMERA_SET_CURSOR(pitch, y, z);
    z *= 1.3f;
    project_xyz(x, y, z, out);
    u = out[1];
    x = 0.0f;
    y = 0.0f;
    z = 1.0f;
    ROT_Y_PLAYER_CAMERA_SET_CURSOR(yaw, x, z);
    z *= 1.3f;
    project_xyz(x, y, z, out);
    lockon_set_cursor_pos(out[0], u);
}

/* Keeps the previous camera; preset settings, settled filters, one update, "Player Camera" node. */
/* 0x00155060 */
static void player_camera_task_init(PlayerCamAngXView *w)
{
    PlayerCamAngXView *prev = player_camera_work;
    int i;

    cam_data_comp_rate = 0.05f;
    {
        PlayerCamVec *pos = &w->pos;
        PlayerCamVec *eye = &w->eye;
        w->prev = (int)prev;
        player_camera_work = w;
        pos->x = 0;
        pos->y = 0;
        pos->z = 0;
        eye->x = 0;
        w->ang_x = 0;
        w->ang_y = 0;
        w->aim_y = 0;
        w->aim_z = 0;
        eye->y = 0;
        w->aim_x = -910;
        eye->z = 0;
    }
    player_camera_data = *((I3ZeroPlayerCameraData * (*)(void))player_camera_get_preset)(); /* steer(cast) */
    (*(I3ZeroPlayerCameraData *)&player_camera_target) = player_camera_data; /* steer(cast) */
    i = 4;
    do {
        player_camera_follow_aim(w);
        i--;
        player_camera_follow_ofs(w);
    } while (i);
    player_camera_task_exec(w);
    w->listener = camera_node_add(1, player_camera_hooks, w);
}

/* 0x00155248 */
static void player_camera_task_release(int *work)
{
    camera_node_remove(work[30]);
    player_camera_work = work[29];
}

/*
 * Adds the settings' turn per frame (keeping its forward vector), filters, eases settings to the
 * target, eye from aim and offsets, lock-on cursor.
 */
/* 0x00155280 */
static void player_camera_task_exec(PlayerCamWork *w)
{
    float origin[3];
    float x, y, z;
    PlayerCameraDataRotView *rec;

    player_get_eye(w->aim_tgt);
    player_get_next_pos(w->pos, w->ang);
    rec = &player_camera_data;
    w->rot[0] = rec->rot[0];
    w->rot[1] = rec->rot[1];
    w->rot[2] = rec->rot[2];
    x = 0.0f;
    y = 0.0f;
    z = -1.0f;
    do {
        float s, c, old_x = x, old_y = y;

        ulSinCos(w->rot[2], &s, &c);
        x = old_x * c - old_y * s;
        y = old_x * s + old_y * c;
    } while (0);
    do {
        float s, c, old_y = y, old_z = z;

        ulSinCos(w->rot[0], &s, &c);
        y = old_y * c - old_z * s;
        z = old_y * s + old_z * c;
    } while (0);
    do {
        float s, c, old_x = x, old_z = z;

        ulSinCos(w->rot[1], &s, &c);
        x = old_x * c + old_z * s;
        z = -old_x * s + old_z * c;
    } while (0);
    {
        float *fwd = w->fwd;

        fwd[0] = x;
        fwd[1] = y;
        fwd[2] = z;
    }
    w->ang[0] += w->rot[0];
    w->ang[1] += w->rot[1];
    w->ang[2] += w->rot[2];
    player_camera_follow_aim(w);
    player_camera_follow_ofs(w);
    player_camera_data_blend(rec, &player_camera_target);
    ulMatrixPush();
    matrix_unit_set(0);
    player_camera_rotate_yxz(w->aim);
    vu0_matrix_translate(ulMatrixCur, 0.0f, w->ofs_y, w->ofs_z);
    origin[0] = 0.0f;
    origin[1] = 0.0f;
    origin[2] = 0.0f;
    ulMatrixApplyPoint(ulMatrixCur, origin, w->eye);
    ulMatrixPop();
    player_camera_set_cursor(w);
}

/* 0x00155448 */
static void player_camera_task_disp(void)
{
}

/* 0x00155450 */
static void *player_camera_task_create(void *parent)
{
    return task_create(parent, &player_camera_desc, 124);
}

/* 0x00155478 */
static void *player_camera_get_target(void)
{
    return player_camera_target;
}

/* Game type 5: the options' preset, else preset 0. */
/* 0x00155488 */
void *player_camera_get_preset(void)
{
    int i;

    if (game_type_get() == 5)
        i = ConfigGetSelect(8);
    else
        i = 0;
    return player_camera_presets[i];
}

/* Target to ease toward. */
/* 0x001554d0 */
void player_camera_set_target(I3ZeroPlayerCameraData *src)
{
    (*(I3ZeroPlayerCameraData *)&player_camera_target) = *src; /* steer(cast) */
}

/* At once. */
/* 0x00155560 */
void player_camera_set_data(I3ZeroPlayerCameraData *src)
{
    (*(I3ZeroPlayerCameraData *)&player_camera_target) = *src; /* steer(cast) */
    player_camera_data = *src;
}

/* Zero with no player camera. */
/* 0x00155678 */
void player_camera_get_angles(IVec3 *out)
{
    PlayerCam *cam = player_camera_work;

    if (cam && !(TASK_OF(cam)->flags & TASK_F_END)) {
        c13_i3copy(out, &cam->rot);
    } else {
        out->x = 0;
        out->y = 0;
        out->z = 0;
    }
}

/* Forward vector of this frame's turn; zero with no player camera. */
/* 0x001556c8 */
void player_camera_get_fwd(Vec3 *out)
{
    PlayerCamAimView *cam = player_camera_work;

    if (cam && !(TASK_OF(cam)->flags & TASK_F_END)) {
        c13_v3copy(out, &cam->fwd);
    } else {
        out->x = 0;
        out->y = 0;
        out->z = 0;
    }
}

/* Target turn per frame from three angles in degrees. */
/* 0x00155718 */
static void player_camera_angle_center_cb(int item, float *vals)
{
    float scale = ANG_PER_DEG;
    int ang_x = vals[1] * scale;
    int ang_y = vals[2] * scale;
    int ang_z = vals[3] * scale;

    player_camera_target[4] = ang_x;
    player_camera_target[6] = ang_z;
    player_camera_target[5] = ang_y;
}

/* Easing rate of the settings. */
/* 0x00155760 */
static void player_camera_angle_rate_cb(int item, float *vals)
{
    cam_data_comp_rate = vals[1];
}

/* 0x00155770 */
static void player_camera_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}

static inline void i3_zero(IVec3 *vec)
{
    vec->x = 0;
    vec->z = 0;
    vec->y = 0;
}
