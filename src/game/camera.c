/*
 * Camera system: camera nodes run by the "CameraSystem" task; the last node is the active view,
 * node 1 the main camera. Also the debug free camera, debug windows and camera animation.
 */

#include "types_game.h"
#include "task.h"
#include "mathdef.h"
#include "libc.h"
#include "debug.h"
#include "debugmenu.h"
#include "system.h"

#define VSTEP(d, s, t) { \
    float va = (d).x, vb = (d).y, vc = (d).z; \
    vx = (s).x - va; vy = (s).y - vb; vz = (s).z - vc; \
    vx *= (t); vy *= (t); vz *= (t); \
    (d).x = va + vx; (d).y = vb + vy; (d).z = vc + vz; \
}

#define ROT_Z(ang, x, y) \
    do { \
        float rot_s, rot_c, rot_u = x, rot_v = y; \
        ulSinCos(ang, &rot_s, &rot_c); \
        x = rot_u * rot_c - rot_v * rot_s; \
        y = rot_u * rot_s + rot_v * rot_c; \
    } while (0)

#define ROT_X(ang, y, z) \
    do { \
        float rot_s, rot_c, rot_u = y, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        y = rot_u * rot_c - rot_v * rot_s; \
        z = rot_u * rot_s + rot_v * rot_c; \
    } while (0)

#define ROT_Y(ang, x, z) \
    do { \
        float rot_s, rot_c, rot_u = x, rot_v = z; \
        ulSinCos(ang, &rot_s, &rot_c); \
        x = rot_u * rot_c + rot_v * rot_s; \
        z = -rot_u * rot_s + rot_v * rot_c; \
    } while (0)

typedef struct CamAng {
    Vec3 eye;           /* 0x00 */
    int pitch;          /* 0x0c */
    int yaw;            /* 0x10 */
    int roll;           /* 0x14 */
} CamAng;

typedef struct { float eye[3]; float at[3]; float up[3]; float w; } CamData;
typedef struct { char pad_00[292]; CamData *cam; } CamGlobals;

typedef struct { char pad_00[48]; Pad *input; } PadHolder;
typedef struct { char pad_00[1]; unsigned char key_trig; } KeyState;

extern Camera *camera_get_current(void);
extern void camera_hooks_clear(CamHooks *hooks);
extern void camera_init_defaults(Camera *cam);
extern CamScene camera_scene;
extern CamHooks camera_default_hooks;
extern char gw_varedit_root[];
extern char camera_vardef[];
extern char camera_vardefs[];
extern int camera_vars_group;
extern float cam_near;
extern void soft_reset(const char *name);
extern void camera_node_init(CamNode *node, int id, CamHooks *hooks);
extern void camera_debug_menu_init(void);
extern int gw_varedit_add(void *a, int b, void *c);
extern void gw_varedit_add_table(void *a, int b, void *c);
extern void camera_task_create(void);
extern void camera_task_exec(void);
extern void *system1_work;
extern TaskDesc camera_system_desc;
extern void ulVec3Cross(float *dst, float *a, float *b);
extern void njUnitMatrix(int a);
extern void njSetMatrix(int a, void *m);
extern void njSetPerspective(void *p);
extern void vu0_matrix_identity(void *m);
extern void ulGxSetMatrix(int a, void *m);
extern int cam_ang;
extern void camera_build_matrices(Camera *cam, float *view_mtx, float *inv_mtx);
extern void matrix_get_or_set(void *p, void *q);
extern void vu0_matrix_mul(void *dst, void *src, void *m);
extern void light_set_dir(Vec3 *v);
extern CamNode *camera_node_head;
extern unsigned int system_flags;
extern int game_malloc(int a);
extern CamNode *heap_block_tag(int h, const char *file, int line);
extern void heap_free(void *p, int a);
extern int camera_mode;
extern int camera_view_mtx;
extern int camera_inv_mtx;
extern Camera *camera_cur;
extern float camera_view_len;
extern float vec3_length_squared(Vec3 *v);
extern int camera_main_cam;
extern float cam_far;
extern void ulGxSetDepthRange(float a, float b);
extern void anim_eval_header(float t, float *src, void *hdr, CamArgs *out);
extern float vec3_normalize(Vec3 *dst, Vec3 *src);
extern void ulSinCos(int ang, float *s, float *c);
extern int gw_get_cell_size(int *cw, int *ch);
extern void gw_win_printf(int win, int x, int y, const char *fmt, ...);
extern int camera_window;
extern float screen_layout[];
extern int gw_window_close(int w);
extern int gw_window_open(char *name, int style, int x, int y, int w, int h, int parent);
extern void gw_window_set_handlers(int w, void (*a)(void), void (*draw)(void), void (*close)(void));
extern int camera_free_node;
extern char camera_free_hooks[];
extern int camera_matrix_window;
extern int camera_inv_matrix_window;
extern void gw_window_set_close_handler(int w, void (*cb)(void));
extern void gw_item_set_value(void *menu, int on);
extern void camera_matrix_window_closed(void);
extern void camera_inv_matrix_window_closed(void);
extern void camera_free_toggle(void);
extern void camera_window_toggle(void);
extern CamView camera_free_view;
extern void pad_owner_add(int, void *);
extern PadHolder pad_system;
extern KeyState debug_key_state;
extern float camera_free_spd;
extern int debug_disp_level;
extern char str_camera_free_spd[];
extern void pad_port_merge_owned(int, void *);
extern void pad_port_clear_owned(int, void *);
extern void ulMatrixPush(void);
extern void matrix_unit_set(int);
extern void vu0_matrix_translate(int, float, float, float);
extern void vu0_matrix_rotate_y(int, int);
extern void vu0_matrix_rotate_x(int, int);
extern void vu0_matrix_rotate_z(int, int);
extern void ulMatrixApplyPoint(int, float *, float *);
extern void ulMatrixPop(void);
extern void pad_port_owner_remove(int, int);
extern void camera_debug_draw(int win);
extern void camera_get_angles(int *ang);
extern void camera_get_eye(Vec3 *out);
extern void camera_get_view_vec(Vec3 *vec);
extern void camera_load_view_matrix(int mode, void *mtx, void *fov);
extern void camera_matrix_window_toggle(void *menu);
extern CamNode *camera_node_add(unsigned int id, CamHooks *hooks, int user);
extern void camera_node_disp(void);
extern void camera_node_remove(CamNode *node);
extern void camera_update_angles();
extern void ulMatrixApplyVector(float *m, float *in, float *out);

/* 0x00104cc0 */
void camera_set_fov(int fov)
{
    camera_get_current()->fov = fov;
}

/* 0x00104ce8 */
void camera_hooks_clear(CamHooks *hooks)
{
    hooks->name = 0;
    hooks->init = 0;
    hooks->update = 0;
    hooks->release = 0;
}

/* Eye at origin looking down -z, y up, fov 13653 (75 degrees). */
/* 0x00104d00 */
void camera_init_defaults(Camera *cam)
{
    Vec3 *look_at = &cam->center;
    Vec3 *up = &cam->up;

    cam->eye.x = 0;
    cam->eye.y = 0;
    cam->eye.z = 0;
    look_at->x = 0;
    look_at->z = -1.0f;
    look_at->y = 0;
    up->x = 0;
    up->y = 1.0f;
    up->z = 0;
    cam->fov = 13653;
}

/* hooks copied (or none). */
/* 0x00104d48 */
void camera_node_init(CamNode *node, int id, CamHooks *hooks)
{
    node->id = id;
    matrix_unit_set(node->m1);
    matrix_unit_set(node->m2);
    if (hooks)
        node->hooks = *hooks;
    else
        camera_hooks_clear(&node->hooks);
    camera_init_defaults(&node->cam);
    node->next = 0;
}

/* 0x00104dc8 */
void camera_system_init(void)
{
    CamScene *sc = &camera_scene;

    soft_reset("Camera");
    sc->head = &sc->root;
    camera_node_init(sc->head, 0, &camera_default_hooks);
    sc->inv_mtx = sc->view_mtx = sc->sel = 0;
    camera_debug_menu_init();
    camera_vars_group = gw_varedit_add(gw_varedit_root, 0, camera_vardef);
    gw_varedit_add_table(gw_varedit_root, camera_vars_group, camera_vardefs);
    camera_task_create();
    cam_near = 1.0f;
}

/* Draws nothing. */
/* 0x00104e78 */
void camera_node_disp(void)
{
}

/* 0x00104e80 */
void camera_task_init(void)
{
}

/* 0x00104e88 */
void camera_system_exec(void)
{
    camera_task_exec();
}

/* Debug flag 1: draws every node after the root except the active one. */
/* 0x00104e90 */
void camera_task_disp(void)
{
    CamNode *node;

    if (debug_flags_test(1)) {
    CamScene *sc = &camera_scene;
    node = sc->head;
    node = node->next;
    while (node) {
        if (sc->sel != &node->cam)
            ((void (*)(Camera *))camera_node_disp)(&node->cam); /* steer(cast) */
        node = node->next;
    }
}
}

/* Sets task flag 8. */
/* 0x00104ef8 */
void camera_task_create(void)
{
    void *w = task_create(system1_work, &camera_system_desc, 0);

    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
}

static inline void vsub3(float *dst, float *to, float *from)
{
    dst[0] = to[0] - from[0];
    dst[1] = to[1] - from[1];
    dst[2] = to[2] - from[2];
}

/* inv_mtx: camera-to-world. */
/* 0x00104f30 */
void camera_build_matrices(Camera *cam, float *view_mtx, float *inv_mtx)
{
    float trans[4];
    float back[4];
    float up[4];
    float cr[4];
    float one = 1.0f;
    float x, y, z;

    vsub3(back, (float *)&cam->eye, (float *)&cam->center);
    vec3_normalize(back, back);
    vec3_normalize(up, &cam->up);
    ulVec3Cross(cr, up, back);
    x = cr[0];
    y = cr[1];
    z = cr[2];
    inv_mtx[0] = x;
    inv_mtx[1] = y;
    inv_mtx[2] = z;
    x = up[0];
    y = up[1];
    z = up[2];
    inv_mtx[4] = x;
    inv_mtx[5] = y;
    inv_mtx[6] = z;
    x = back[0];
    y = back[1];
    z = back[2];
    inv_mtx[8] = x;
    inv_mtx[9] = y;
    inv_mtx[10] = z;
    inv_mtx[3] = 0;
    inv_mtx[7] = 0;
    inv_mtx[11] = 0;
    x = cam->eye.x;
    y = cam->eye.y;
    z = cam->eye.z;
    inv_mtx[12] = x;
    inv_mtx[13] = y;
    inv_mtx[14] = z;
    inv_mtx[15] = one;
    x = cr[0];
    y = up[0];
    z = back[0];
    view_mtx[0] = x;
    view_mtx[1] = y;
    view_mtx[2] = z;
    x = cr[1];
    y = up[1];
    z = back[1];
    view_mtx[4] = x;
    view_mtx[5] = y;
    view_mtx[6] = z;
    x = cr[2];
    y = up[2];
    z = back[2];
    view_mtx[8] = x;
    view_mtx[9] = y;
    view_mtx[10] = z;
    view_mtx[3] = 0;
    view_mtx[7] = 0;
    view_mtx[11] = 0;
    ulMatrixApplyVector(view_mtx, cam, trans);
    x = trans[0];
    y = trans[1];
    z = trans[2];
    view_mtx[12] = -x;
    view_mtx[13] = -y;
    view_mtx[14] = -z;
    view_mtx[15] = one;
}

/* Also resets the ulGx model matrix. */
/* 0x001050e8 */
void camera_load_view_matrix(int mode, void *mtx, void *fov)
{
    float buf[16];

    njUnitMatrix(0);
    njSetMatrix(0, mtx);
    njSetPerspective(fov);
    vu0_matrix_identity(buf);
    ulGxSetMatrix(1, buf);
}

/*
 * Skips id 4. Last node -> active, node 1 -> main; main view relative to active kept in the scene
 * matrix. Points view parameters and light direction along the active camera.
 */
/* 0x00105148 */
void camera_update(void)
{
    CamScene *sc = &camera_scene;
    CamNode *node;
    Camera *cam = 0;
    float *view_mtx = 0;
    float *inv_mtx = 0;
    int id = 0;
    float *main_mtx = 0;
    Camera *main_cam = 0;
    Vec3 *ce;
    Vec3 dir;

    for (node = sc->head; node != 0; node = node->next) {
        if (node->id == 4)
            continue;
        cam = &node->cam;
        view_mtx = node->m1;
        inv_mtx = node->m2;
        id = node->id;
        camera_build_matrices(cam, view_mtx, inv_mtx);
        if (id == 1) {
            main_cam = cam;
            main_mtx = view_mtx;
        }
    }
    sc->mode = id;
    sc->sel = cam;
    sc->view_mtx = (int)view_mtx;
    sc->inv_mtx = (int)inv_mtx;
    if (cam != 0) {
        cam_ang = cam->fov;
        camera_load_view_matrix(0, view_mtx, cam->fov);
        sc->main_cam = (int)main_cam;
        sc->main_view_mtx = (int)main_mtx;
        if (main_mtx != 0) {
            matrix_get_or_set(main_mtx, sc);
            vu0_matrix_mul(sc, sc, inv_mtx);
        }
        ce = &cam->center;
        dir.x = ce->x - cam->eye.x;
        dir.y = ce->y - cam->eye.y;
        dir.z = ce->z - cam->eye.z;
        vec3_normalize(&dir, &dir);
        set_view_params(cam, &dir, cam->fov);
        light_set_dir(&dir);
    }
}

/*
 * Node 3 always updates; others pause while system flag bits 0-1 are set. A hook may remove its
 * own node, so the walk resumes from the previous one.
 */
/* 0x001052c8 */
void camera_task_exec(void)
{
    CamNode *cur;
    CamNode *prev;
    Camera *cam;

    cur = camera_node_head;
    prev = cur;
    while (cur) {
        cam = &cur->cam;
        if (cur->hooks.update) {
            if (cur->id == 3) {
                cur->hooks.update(cam, cur->user);
            } else {
                if ((system_flags & (SYS_PAUSE_KBD | SYS_PAUSED)) == 0)
                    cur->hooks.update(cam, cur->user);
                cur = prev->next;
            }
        }
        if (cur) {
            prev = cur;
            cur = cur->next;
        }
    }
}

/* List sorted by id; inserted after nodes of the same id. */
/* 0x00105370 */
CamNode *camera_node_add(unsigned int id, CamHooks *hooks, int user)
{
    CamScene *sc = &camera_scene;
    CamNode *node;
    CamNode *cur;
    CamNode **pp;

    node = heap_block_tag(game_malloc(208), "CameraParam", -1);
    camera_node_init(node, id, hooks);
    node->user = user;
    if (hooks->init)
        hooks->init(&node->cam, user);
    pp = &sc->head;
    cur = *pp;
    while (cur) {
    if (id < (unsigned int)cur->id) {
        node->next = cur;
        break;
    }
    pp = &(*pp)->next;
    cur = *pp;
}
*pp = node;
    return node;
}

/* Active camera removed -> root becomes active. */
/* 0x00105448 */
void camera_node_remove(CamNode *node)
{
    CamScene *sc = &camera_scene;
    CamNode *prev;
    CamNode *nx;
    CamNode **pp;
    Camera *cam;
    void (*rel)(Camera *c, int user);

    pp = &sc->head;
    if (*pp) {
        if (*pp == node) {
            rel = node->hooks.release;
            do { nx = node->next; } while (0);
            cam = &node->cam;
            *pp = nx;
        } else {
            rel = node->hooks.release;
            cam = &node->cam;
            for (;;) {
                prev = *pp;
                nx = prev->next;
                pp = &prev->next;
                if (nx == 0)
                    break;
                if (nx == node) {
                    prev->next = node->next;
                    break;
                }
            }
        }
    } else {
        rel = node->hooks.release;
        cam = &node->cam;
    }
    if (rel)
        rel(cam, node->user);
    if (sc->sel == cam)
        sc->sel = &sc->root.cam;
    heap_free(node, 0);
}

/* 0x00105508 */
int camera_get_mode(void)
{
    return camera_mode;
}

/* 0x00105518 */
int camera_get_view_mtx(void)
{
    return camera_view_mtx;
}

/* 0x00105528 */
int camera_get_inv_mtx(void)
{
    return camera_inv_mtx;
}

/* 0 while the main camera is the active one. */
/* 0x00105538 */
int camera_get_main_view_mtx(void)
{
    if (camera_scene.mode == 1)
        return 0;
    return camera_scene.main_view_mtx;
}

/* 0 while the main camera is the active one. */
/* 0x00105560 */
CamScene *camera_scene_if_active(void)
{
    if (camera_scene.mode == 1)
        return 0;
    return &camera_scene;
}

/* 0x00105578 */
void camera_get_eye(Vec3 *out)
{
    Camera *cam = camera_cur;

    out->x = cam->eye.x;
    out->y = cam->eye.y;
    out->z = cam->eye.z;
}

/* centre - eye; squared length kept in camera_view_len. Near-zero reported and replaced by +z. */
/* 0x001055a0 */
void camera_get_view_vec(Vec3 *vec)
{
    Camera *cam = camera_cur;
    Vec3 *ce = &cam->center;
    float len;

    vec->x = cam->center.x - cam->eye.x;
    vec->y = ce->y - cam->eye.y;
    vec->z = ce->z - cam->eye.z;
    len = vec3_length_squared(vec);
    camera_view_len = len;
    if (len <= 0.5f) {
        printf("vec_cam is ZERO? % 8.3f\n", len);
        printf("center % 8.3f, % 8.3f, % 8.3f\n", cam->center.x, cam->center.y, cam->center.z);
        printf("eye    % 8.3f, % 8.3f, % 8.3f\n", cam->eye.x, cam->eye.y, cam->eye.z);
        printf("vec    % 8.3f, % 8.3f, % 8.3f\n", vec->x, vec->y, vec->z);
        vec->y = 0;
        vec->z = 1.0f;
        vec->x = 0;
    }
}

static inline void vsub(float *dst, float *to, float *from)
{
    dst[0] = to[0] - from[0];
    dst[1] = to[1] - from[1];
    dst[2] = to[2] - from[2];
}

/* ang[0] pitch, ang[1] yaw. */
/* 0x00105720 */
void camera_get_angles(int *ang)
{
    Camera *cam;
    float dir[3];

    cam = camera_cur;
    vsub(dir, &cam->center.x, &cam->eye.x);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-dir[0], -dir[2]));
        ang[0] = (int)RAD_TO_ANG(atan2f(dir[1], sqrtf(dir[0] * dir[0] + dir[2] * dir[2])));
    } while (0);
}

/* 13653 without a camera. */
/* 0x00105820 */
int camera_get_fov(void)
{
    Camera *cam = camera_cur;

    if (cam)
        return cam->fov;
    return 13653;
}

/* Null outputs skipped. */
/* 0x00105840 */
void camera_get_params(void *eye, void *vec, void *ang)
{
    if (eye)
        camera_get_eye(eye);
    if (vec)
        camera_get_view_vec(vec);
    if (ang)
        camera_get_angles(ang);
}

/* 0x001058a0 */
Camera *camera_get_current(void)
{
    return camera_cur;
}

/* 0x001058b0 */
int camera_get_main_cam(void)
{
    return camera_main_cam;
}

/* Keeps the view direction. */
/* 0x001058c0 */
void camera_move_eye(Vec3 *eye)
{
    Camera *cam = camera_cur;
    Vec3 *ce = &cam->center;
    Vec3 dir;

    dir.x = ce->x - cam->eye.x;
    dir.y = ce->y - cam->eye.y;
    dir.z = ce->z - cam->eye.z;
    cam->eye.x = eye->x;
    cam->eye.y = eye->y;
    cam->eye.z = eye->z;
    ce->x = eye->x + dir.x;
    ce->y = eye->y + dir.y;
    ce->z = eye->z + dir.z;
    camera_update_angles(cam, 0);
}

/* centre = eye + ofs. */
/* 0x00105968 */
void camera_look_offset(Vec3 *ofs)
{
    Camera *cam = camera_cur;
    Vec3 *look_at = &cam->center;

    look_at->x = ofs->x + cam->eye.x;
    look_at->y = ofs->y + cam->eye.y;
    look_at->z = ofs->z + cam->eye.z;
    camera_update_angles(cam, 0);
}

/* 0x001059b8 */
void camera_set_fov_direct(int fov)
{
    camera_cur->fov = fov;
}

/* Active camera (camera_scene.sel). */
/* 0x001059c8 */
void camera_set_lookat(Vec3 *eye, Vec3 *center, Vec3 *up)
{
    Camera *cam;
    Vec3 *ce;
    Vec3 *cam_up;
    cam = camera_scene.sel;
    cam->eye.x = eye->x;
    cam->eye.y = eye->y;
    cam->eye.z = eye->z;
    ce = &cam->center;
    ce->x = center->x;
    ce->y = center->y;
    ce->z = center->z;
    cam_up = &cam->up;
    cam_up->x = up->x;
    cam_up->y = up->y;
    cam_up->z = up->z;
    camera_update_angles(cam, 0);
}

/* Near stays 1. */
/* 0x00105a38 */
void camera_set_far(float x)
{
    cam_far = x;
    ulGxSetDepthRange(1.0f, x);
}

/* 0x00105a58 */
float camera_get_far(void)
{
    return cam_far;
}

/* t clamped to 0..1. mask bit 0: eye, centre, up (re-orthogonalized); bit 1: fov. */
/* 0x00105a68 */
void camera_lerp(float t, Camera *cam, Camera *target, int mask)
{
    float tt;
    float vx, vy, vz;
    Vec3 dir;
    Vec3 r;
    Vec3 u;

    tt = (((t >= 0.0f) ? t : 0.0f) < 1.0f) ? ((t >= 0.0f) ? t : 0.0f) : 1.0f;
    if (mask & 1) {
        VSTEP(cam->eye, target->eye, tt);
        VSTEP(cam->center, target->center, tt);
        vsub((float *)&dir, (float *)&cam->center, (float *)&cam->eye);
        vec3_normalize(&dir, &dir);
        VSTEP(cam->up, target->up, tt);
        vec3_normalize(&u, &cam->up);
        ulVec3Cross(&r, &u, &dir);
        ulVec3Cross(&cam->up, &dir, &r);
    }
    if (mask & 2)
        cam->fov = (int)((float)cam->fov + (float)(target->fov - cam->fov) * tt);
}

static inline void cam_vadd(float *dst, float *pos, float *ofs)
{
    dst[0] = pos[0] + ofs[0];
    dst[1] = pos[1] + ofs[1];
    dst[2] = pos[2] + ofs[2];
}

static inline void cam_vset(float *dst, float x, float y, float z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

/* Fov 0 -> 10922 (60 degrees). Centre one unit along the view; up = +y by roll, pitch, yaw. */
/* 0x00105c78 */
void camera_eval_motion(Camera *cam, float *src, void *hdr, float t)
{
    Vec3 dir;
    CamArgs args;
    short rf[2];
    int ang[2];
    int roll;
    float x, y, z;

    rf[0] = 0;
    args.c = cam;
    args.d = &dir;
    args.roll = &rf[0];
    args.fov = &rf[1];
    anim_eval_header(t, src, hdr, &args);
    roll = rf[0];
    if (rf[1])
        cam->fov = rf[1];
    else
        cam->fov = 10922;
    vec3_normalize(&dir, &dir);
    cam_vadd(&cam->center.x, &cam->eye.x, &dir.x);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    do {
        x = 0.0f;
    } while (0);
    y = 1.0f;
    z = 0.0f;
    ROT_Z(roll, x, y);
    ROT_X(ang[0], y, z);
    ROT_Y(ang[1], x, z);
    cam_vset(&cam->up.x, x, y, z);
}

/* Fov in degrees, hook name, eye, centre, up of every node. */
/* 0x00105e78 */
void camera_debug_draw(int win)
{
    int cw;
    int ch;
    int y = 0;
    CamNode *node;
    Camera *cam;

    gw_get_cell_size(&cw, &ch);
    for (node = camera_node_head; node != 0; node = node->next) {
        cam = &node->cam;
        gw_win_printf(win, 4, y, "Angle  % 8.3f  [%s]", (float)cam->fov * DEG_PER_ANG,
                     node->hooks.name);
        y += ch;
        gw_win_printf(win, 4, y, "Eye    % 8.1f/% 8.1f/% 8.1f", cam->eye.x, cam->eye.y, cam->eye.z);
        y += ch;
        gw_win_printf(win, 4, y, "Center % 8.1f/% 8.1f/% 8.1f", cam->center.x, cam->center.y,
                     cam->center.z);
        y += ch;
        gw_win_printf(win, 4, y, "Up     % 8.1f/% 8.1f/% 8.1f", cam->up.x, cam->up.y, cam->up.z);
        y += ch;
    }
}

/* 0x00106038 */
void camera_window_toggle(void)
{
    int cw;
    int ch;
    int win;

    if (camera_window) {
        gw_window_close(camera_window);
        camera_window = 0;
        menu_item_set_value(info_menu_name, "Camera", 0);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    win = gw_window_open("Camera", 95, (int)(screen_layout[6] + 4.0f), (int)(screen_layout[8] + 34.0f),
                     cw * 37, ch * 12, 0);
    camera_window = win;
    if (win != 0) {
        gw_window_set_handlers(win, 0, camera_debug_draw, 0);
        menu_item_set_value(info_menu_name, "Camera", 1);
    }
}

/* Free camera: node id 3. */
/* 0x00106148 */
void camera_free_toggle(void)
{
    if (camera_free_node) {
        camera_node_remove(camera_free_node);
        camera_free_node = 0;
        menu_item_set_value(0, "FreeCamera", 0);
    } else {
        camera_free_node = camera_node_add(3, camera_free_hooks, 0);
        menu_item_set_value(0, "FreeCamera", 1);
    }
}

/* 0x001061c8 */
void camera_matrix_window_closed(void)
{
    camera_matrix_window = 0;
}

/* 0x001061d8 */
void camera_inv_matrix_window_closed(void)
{
    camera_inv_matrix_window = 0;
}

/* "Cam Matrix" and "Cam InverseMatrix" windows. */
/* 0x001061e8 */
void camera_matrix_window_toggle(void *menu)
{
    CamScene *sc = &camera_scene;

    if (camera_matrix_window != 0 || camera_inv_matrix_window != 0) {
        gw_window_close(camera_matrix_window);
        gw_window_close(camera_inv_matrix_window);
        gw_item_set_value(menu, 0);
        return;
    }
    if (sc->view_mtx)
        camera_matrix_window = matrix_window_create("Cam Matrix", (int)(screen_layout[7] - 300.0f),
                                  (int)(screen_layout[8] + 23.0f), &sc->view_mtx);
    if (sc->inv_mtx)
        camera_inv_matrix_window = matrix_window_create("Cam InverseMatrix", (int)(screen_layout[7] - 300.0f),
                                  (int)(screen_layout[8] + 100.0f), &sc->inv_mtx);
    gw_window_set_close_handler(camera_matrix_window, camera_matrix_window_closed);
    gw_window_set_close_handler(camera_inv_matrix_window, camera_inv_matrix_window_closed);
    if (camera_matrix_window == 0 && camera_inv_matrix_window == 0)
        return;
    gw_item_set_value(menu, 1);
}

/* 0x00106390 */
void camera_debug_menu_init(void)
{
    menu_item_add(0, "FreeCamera", camera_free_toggle, 0, 0);
    menu_item_add(info_menu_name, "CameraMatrix", camera_matrix_window_toggle, 0, 0);
    menu_item_add(info_menu_name, "Camera", camera_window_toggle, 0, 0);
}

static inline void vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* camera_free_view: eye, yaw, pitch, and roll of the up vector along the view direction. */
/* 0x00106410 */
void camera_update_angles(void)
{
    Camera *cam = camera_cur;
    CamAng *view = (CamAng *)&camera_free_view;
    float dir[3];
    float sc[4];
    Vec3 *up;
    float ux;
    float uy;
    float uz;

    vsub(dir, &cam->center.x, &cam->eye.x);
    vec3_normalize((Vec3 *)dir, (Vec3 *)dir);
    vcopy(&view->eye.x, &cam->eye.x);
    do { } while (0);
    view->yaw = (int)RAD_TO_ANG(atan2f(-dir[0], -dir[2]));
    view->pitch = (int)RAD_TO_ANG(atan2f(dir[1], sqrtf(dir[0] * dir[0] + dir[2] * dir[2])));
    do {
        up = &cam->up;
    } while (0);
    uz = up->z;
    uy = up->y;
    do {
        ux = cam->up.x;
    } while (0);
    do {
        float old_x = ux, old_z = uz;

        ulSinCos(-view->yaw, &sc[0], &sc[1]);
        ux = old_x * sc[1] + old_z * sc[0];
        uz = -old_x * sc[0] + old_z * sc[1];
    } while (0);
    do {
        float old_y = uy, old_z = uz;

        ulSinCos(-view->pitch, &sc[2], &sc[3]);
        uy = old_y * sc[3] - old_z * sc[2];
        uz = old_y * sc[2] + old_z * sc[3];
    } while (0);
    view->roll = (int)RAD_TO_ANG(atan2f(-ux, uy));
}

static inline void cd_vsub(float *dst, float *to, float *from)
{
    dst[0] = to[0] - from[0];
    dst[1] = to[1] - from[1];
    dst[2] = to[2] - from[2];
}

static inline void cd_vcopy(float *dst, float *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

/* Copies the scene camera, angles as camera_update_angles; takes over pad 0. */
/* 0x001065f8 */
void camera_free_start(CamData *out)
{
    CamView *vw = &camera_free_view;
    CamData *cam;
    float dir[3];
    float two_pi, scale;
    float x, y, z;

    *out = *(*(CamGlobals *)&camera_scene).cam; /* steer(cast) */
    cam = (*(CamGlobals *)&camera_scene).cam; /* steer(cast) */
    cd_vsub(dir, cam->at, cam->eye);
    vec3_normalize(dir, dir);
    cd_vcopy(vw->pos, cam->eye);
    do { } while (0);
    {
        float nx = -dir[0], nz = -dir[2];

        scale = ANG_TURN;
        two_pi = TWO_PI;
        vw->yaw = (int)(atan2f(nx, nz) * scale / two_pi);
    }
    vw->pitch = (int)(atan2f(dir[1], sqrtf(dir[0] * dir[0] + dir[2] * dir[2])) * scale / two_pi);
    do {
        pad_owner_add(0, out);
    } while (0);
    do {
        float *up = cam->up;

        x = up[0];
        y = up[1];
        z = up[2];
    } while (0);
    ROT_Y(-vw->yaw, x, z);
    ROT_X(-vw->pitch, y, z);
    vw->roll = (int)(atan2f(-x, y) * scale / two_pi);
}

static inline void cff_vzero(float *p)
{
    p[0] = 0.0f;
    p[1] = 0.0f;
    p[2] = 0.0f;
}

static inline void cff_izero(int *p)
{
    p[0] = 0;
    p[1] = 0;
    p[2] = 0;
}

/*
 * Pad 0: second button set moves 30x and turns 4x faster, stick rolls; both yaw buttons cycle
 * speed 0.1/1/10/100. Button 0x100 holds a fixed preset view; debug key 74 resets.
 */
/* 0x00106850 */
void camera_free_fly(float *out)
{
    CamView *view = &camera_free_view;
    float mv[3];
    float vz[3];
    float *pv;
    float sc[2];
    int ang_step;
    float step;

    if (pad_system.input->raw.on & 0x100) {
        out = (float *)camera_cur;
        out[1] = 93.16926f;
        out[0] = 50.082077f;
        out[2] = 258.44754f;
        out[3] = 49.285046f;
        out[4] = 93.16926f;
        out[5] = 257.91467f;
        out[6] = 0.39938328f;
        out[7] = 0.90991515f;
        out[8] = -0.11201448f;
        ((int *)out)[9] = 0x3555;
        ((void (*)(int, int))camera_update_angles)(0, 0); /* steer(cast) */
        return;
    }
    if (debug_key_state.key_trig == 74) {
        cff_vzero(view->pos);
        cff_izero(&view->pitch);
    }
    mv[0] = 0.0f;
    mv[1] = 0.0f;
    mv[2] = 0.0f;
    ang_step = 91;
    pad_port_merge_owned(0, out);
    step = FRAME_SEC;
    if ((pad_system.input->raw.trig & 0x30000) != 0 && (pad_system.input->raw.on & 0x30000) == 0x30000) {
        if (camera_free_spd == 0.1f)
            camera_free_spd = 1.0f;
        else if (camera_free_spd == 1.0f)
            camera_free_spd = 10.0f;
        else if (camera_free_spd == 10.0f)
            camera_free_spd = 100.0f;
        else if (camera_free_spd == 100.0f)
            camera_free_spd = 0.1f;
    }
    if (debug_disp_level == 0) {
        debug_set_origin(20, 1);
        debug_printf(str_camera_free_spd, camera_free_spd);
    }
    step *= camera_free_spd;
    if (pad_system.input->raw.on & 0x10)
        mv[2] -= step;
    if (pad_system.input->raw.on & 0x20)
        mv[2] += step;
    if (pad_system.input->raw.on & 0x40)
        mv[0] -= step;
    if (pad_system.input->raw.on & 0x80)
        mv[0] += step;
    if (pad_system.input->raw.on & 0x400)
        mv[1] += step;
    if (pad_system.input->raw.on & 4)
        mv[1] -= step;
    if (pad_system.input->raw.on & 0x20000)
        view->yaw += 91;
    if (pad_system.input->raw.on & 0x10000)
        view->yaw -= 91;
    if (pad_system.input->raw.on & 0x200)
        view->pitch += 91;
    if (pad_system.input->raw.on & 2)
        view->pitch -= 91;
    if (pad_system.input->raw.stick[0] < -64)
        view->roll -= 91;
    if (pad_system.input->raw.stick[0] > 64)
        view->roll += 91;
    if (pad_system.input->raw.on2 & 0x10)
        mv[2] -= step * 30.0f;
    if (pad_system.input->raw.on2 & 0x20)
        mv[2] += step * 30.0f;
    if (pad_system.input->raw.on2 & 0x40)
        mv[0] -= step * 30.0f;
    if (pad_system.input->raw.on2 & 0x80)
        mv[0] += step * 30.0f;
    if (pad_system.input->raw.on2 & 0x400)
        mv[1] += step * 30.0f;
    if (pad_system.input->raw.on2 & 4)
        mv[1] -= step * 30.0f;
    if (pad_system.input->raw.on2 & 0x20000)
        view->yaw += ang_step * 4;
    if (pad_system.input->raw.on2 & 0x10000)
        view->yaw -= ang_step * 4;
    if (pad_system.input->raw.on2 & 0x200)
        view->pitch += ang_step * 4;
    if (pad_system.input->raw.on2 & 2)
        view->pitch -= ang_step * 4;
    pad_port_clear_owned(0, out);
    ulSinCos(view->yaw, &sc[0], &sc[1]);
    {
        float x = sc[1] * mv[0] + sc[0] * mv[2];
        float z = -sc[0] * mv[0] + sc[1] * mv[2];

        view->pos[0] += x;
        view->pos[2] += z;
        view->pos[1] += mv[1];
    }
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        vu0_matrix_translate(ulMatrixCur, view->pos[0], view->pos[1], view->pos[2]);
        vu0_matrix_rotate_y(ulMatrixCur, view->yaw);
        vu0_matrix_rotate_x(ulMatrixCur, view->pitch);
    } while (0);
        vu0_matrix_rotate_z(ulMatrixCur, view->roll);
    pv = vz;
    vz[0] = 0.0f;
    pv[1] = 1.0f;
    vz[2] = 0.0f;
    ulMatrixApplyVector(ulMatrixCur, vz, out + 6);
    vz[0] = 0.0f;
    vz[1] = 0.0f;
    pv[2] = -1.0f;
    ulMatrixApplyPoint(ulMatrixCur, vz, out + 3);
    ulMatrixPop();
    out[0] = view->pos[0];
    out[1] = view->pos[1];
    out[2] = view->pos[2];
}

/* Gives pad 0 back. */
/* 0x00106e80 */
void camera_free_end(int cam)
{
    pad_port_owner_remove(0, cam);
}
