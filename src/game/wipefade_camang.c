/*
 * Camera angle zoom transition ("WFCamAng"): widens the fov while keeping the player's screen
 * size, and straightens the view angles. A wipe_start transition type.
 */

#include "tvset.h"
#include "mathdef.h"
#include "libc.h"
#include "ul_math.h"
#include "ul_vu0.h"
#include "wipefade_camang.h"
#include "camera.h"
#include "player.h"
#include "task.h"

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

extern int wfcamang_desc;

static inline void wf_iset(int *dst, int x, int y, int z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

static inline void wf_vsub(float *dst, float *to, float *from)
{
    dst[0] = to[0] - from[0];
    dst[1] = to[1] - from[1];
    dst[2] = to[2] - from[2];
}

static inline void wf_vsubi(float *dst, float *src)
{
    dst[0] -= src[0];
    dst[1] -= src[1];
    dst[2] -= src[2];
}

static inline void wf_vaddi(float *dst, float *src)
{
    dst[0] += src[0];
    dst[1] += src[1];
    dst[2] += src[2];
}

/*
 * Fov toward 0x6aaa by count / frames; eye and target moved so the player's next position keeps
 * its size. Eye angles scaled down (state 1), cleared (2), half turn (3, 4).
 */
/* steer(cast) */
/* 0x001740a0 */
static void wfcamang_camera_exec(WfCam *cam, WfCamAng *w)
{
    Vec3 rel;
    Vec3 dir;
    int ang[3];
    int buf[4];
    float rate, ta, tb, dist;

    rate = (float)w->count / (float)w->frames;
    *cam = *((WfCam *(*)(void))camera_get_main_cam)();
    tb = ulTan(cam->fov / 2);
    cam->fov = (0x6aaa - cam->fov) * rate + cam->fov;
    ta = ulTan(cam->fov / 2);
    wf_vsub(&dir.x, cam->target, cam->pos);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    player_get_next_pos(&rel, buf);
    wf_vsubi(&rel.x, cam->target);
    ROT_Y(ang[1], rel.x, rel.z);
    ROT_X(ang[0], rel.y, rel.z);
    dist = fabsf(rel.z) * (1.0f - tb / ta);
    vec3_normalize(&dir, &dir);
    dir.x *= dist;
    dir.y *= dist;
    dir.z *= dist;
    wf_vaddi(cam->pos, &dir.x);
    wf_vaddi(cam->target, &dir.x);
    player_get_eye(ang);
    switch (w->state) {
    case 3:
    case 4:
        wf_iset(ang, 0, ANG_180, 0);
        break;
    case 1:
        ang[0] = ang[0] * rate;
        ang[1] = ang[1] * rate;
        ang[2] = ang[2] * rate;
        break;
    case 2:
        ang[0] = 0;
        ang[1] = 0;
        ang[2] = 0;
        break;
    default:
        printf("Illigal switch wipefade_camang.c : CameraExec()\n");
        break;
    }
    player_set_eye_sync(ang);
}

/* 0x001744b0 */
static void wfcamang_init(WfCamAng *w)
{
    w->state = 0;
    w->count = 0;
}

/* Unless idle or done. */
/* 0x001744c0 */
static void wfcamang_release(WfCamAng *w)
{
    if (w->state != 0 && w->state != 4)
        camera_node_remove(w->handle);
}

/* State 1 counts up, then holds (2); state 3 counts down, unhooks the camera, done (4). */
/* 0x001744e8 */
static void wfcamang_exec(WfCamAng *w)
{
    switch (w->state) {
    case 1:
        if (++w->count >= w->frames)
            w->state = 2;
        break;
    case 3:
        if (--w->count == 0) {
            camera_node_remove(w->handle);
            w->state = 4;
        }
        break;
    default:
        printf("Illigal switch wipefade_camang.c : Exec()\n");
        break;
    }
}

/* Draws nothing. */
/* 0x00174578 */
static void wfcamang_disp(WfCamAng *w)
{
}

/* Wipe type 2. */
/* 0x00174580 */
static WfCamAng *wfcamang_create(void *parent)
{
    return task_create_inherit(parent, &wfcamang_desc, 24);
}

/* State 1; hooks wfcamang_camera_exec into the camera. */
/* 0x001745a8 */
static void wfcamang_start(WfCamAng *w, int n)
{
    WfCamDesc desc = { "WFCamAng", 0, wfcamang_camera_exec, 0 };

    w->frames = n;
    w->count = 0;
    w->state = 1;
    w->handle = camera_node_add(2, &desc, w);
}

/* State 2. */
/* 0x00174618 */
static int wfcamang_is_covered(WfCamAng *w)
{
    return w->state == 2;
}

/* State 3. */
/* 0x00174628 */
static void wfcamang_finish(WfCamAng *w, int n)
{
    w->frames = n;
    w->state = 3;
    w->count = n;
}

/* Done (4) or idle (0). */
/* 0x00174640 */
static int wfcamang_is_done(WfCamAng *w)
{
    int done = 0;

    if (w->state == 4 || w->state == 0)
        done = 1;
    return done;
}
