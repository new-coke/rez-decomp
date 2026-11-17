/*
 * Venus boss player view: nine entries drive the camera through the fight, each a motion pass or
 * an orbit of the boss at a blended height, tilt and speed; the view moves on at the motion's end
 * or on venus_player_ent_next. Also debug displays of the fitted orbit and a pass motion.
 */

#include "types_gw.h"
#include "b_venus.h"
#include "mathdef.h"
#include "anim.h"
#include "camera.h"
#include "debug.h"
#include "gw_varedit.h"
#include "libc.h"
#include "math_util.h"
#include "player.h"
#include "pnb.h"
#include "task.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "venus_player.h"
#include "venus.h"
#include "../common/boss_common.h"
#include "../common/w_acos.h"

extern char venus_pl_vardef[];
extern GwVarDef venus_pl_vardefs[];
extern struct GwVarNode *venus_pl_vars_group;

/* position and three floats, 24 bytes, copied whole */
typedef struct VenusView {
    Vec3 pos;
    float tilt_x, tilt_z, ang_spd;              /* 0x0c */
} VenusView;

/* orbit view, blended from 'from' toward 'to' */
typedef struct VenusPlayerWork {
    char pad_00[68];
    VenusView from;             /* 0x44 */
    VenusView cur;              /* 0x5c */
    VenusView to;               /* 0x74 */
    int n;                      /* 0x8c: frames */
    int t;                      /* 0x90: frames left */
} VenusPlayerWork;

/* value stepped by a delta for a number of frames */
typedef struct VenusPlayerCamRamp {
    char pad_00[4];
    float v;                    /* 0x04 */
    float dv;                   /* 0x08 */
    char pad_0c[16];
    int n;                      /* 0x1c: frames left */
} VenusPlayerCamRamp;

extern struct TaskDesc venus_player_pass_desc;
extern float venus_player_view_x;
extern float venus_player_view_y;
extern float venus_player_view_z;
extern float venus_player_view_sec;
extern float venus_player_orbit_sec;
extern float venus_player_tilt_x;
extern float venus_player_tilt_z;
extern VenusPlayerWork *venus_player_work;
extern struct TaskDesc venus_player_cam_desc;

/* orbit debug task, 32 bytes: the orbit fitted to a motion */
typedef struct VenusPlayerCamWork {
    float y, ang, ang_spd, len;
    char pad_10[4];
    float y_base;                  /* 0x14: a copy of y */
    char pad_18[4];
    int frm;                    /* 0x1c: venus_player_orbit_calc's result */
} VenusPlayerCamWork;

extern char str_venus_player_pass_frm[];
extern struct TaskDesc venus_player_desc;
extern char venus_player_seq_tbl[];
extern int cmd_test_sw;
extern char str_venus_player_off_y[];
extern char str_venus_player_frm[];
extern float cmd_pass_motion_v;

/* 36 bytes */
typedef struct VenusViewEnt {
    int *blk;                   /* block of the entry's record (pnb_get_entry) */
    int frm;
    float frm_spd;                  /* 0x08 */
    float frames;               /* 0x0c: the block's count less 1 */
    int idx;                    /* 0x10 */
    int unk_14;                 /* 0x14: set to 1 by venus_player_ent_add, never read */
    int cam;                    /* 0x18 */
    int *rec;                   /* 0x1c */
    struct VenusViewEnt *next;  /* 0x20 */
} VenusViewEnt;

extern float venus_player_tilt_x_rnd;
extern float venus_player_tilt_z_rnd;
extern float venus_player_rnd_y_min;
extern float venus_player_rnd_y_max;
extern float venus_player_rnd_x_min;
extern float venus_player_rnd_x_max;
extern float venus_player_pitch_rate;
extern float venus_player_roll_rate;
extern float venus_player_roll_max;
extern float venus_player_smooth;
extern float cmd_demo_cam_st;
extern float cmd_demo_cam_fin;
extern float cmd_demo_cam_mot;
extern int cmd_pass_fade_frm;
extern float cmd_pass_loop_adj_frm;
extern const float cmd_ppass_speed_e;
extern const float cmd_ppass_speed_t;
extern float venus_player_orbit_ofs[];
extern int cmd_pass_loop_sw;
extern const int venus_player_di_ids[2];
extern int cmd_sw_seq_num;
extern int cmd_pass_disp;
extern int cmd_seq_sw;
extern int *venus_player_pass_task;
extern float cmd_sample_frm;

/* 0x00cab928 */
void venus_player_view_reset(void)
{
    Vec3 pos;

    pos.z = venus_player_view_z;
    pos.x = venus_player_view_x;
    pos.y = venus_player_view_y;
    venus_player_view_set(venus_player_work, venus_player_view_sec * FPS, &pos, venus_player_tilt_x, venus_player_tilt_z,
                           TWO_PI / (venus_player_orbit_sec * FPS));
}

/* 0x00cab9c0 */
void venus_pl_vars_add(void)
{
    venus_pl_vars_group = gw_varedit_add(&gw_varedit_root, 0, venus_pl_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_pl_vars_group, venus_pl_vardefs);
}

/* 0x00caba10 */
void venus_pl_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_pl_vardef);
}

/* 0x00caba28 */
void venus_player_motion_set(void *mot, Vec3 *pos, int *ang, Vec3 *dir, float t)
{
    Vec3 cam_pos;
    Vec3 fwd;
    int mot_ang[3];
    float sc[4];
    float zero;

    motion_point_pose(mot, t, &cam_pos, mot_ang);
    zero = 0.0f;
    ulMatrixPush();
    matrix_unit_set(0);
    venus_player_rotate_zyx(mot_ang);
    matrix_saved_pose(&fwd, mot_ang);
    ulMatrixPop();
    fwd.y = zero;
    fwd.z = 1.0f;
    fwd.x = zero;
    ROT_X(mot_ang[0], sc, fwd.y, fwd.z);
    ROT_Y(mot_ang[1], (sc + 2), fwd.x, fwd.z);
    if (pos)
        v3_copy(pos, &cam_pos);
    else
        player_set_cam_pos(&cam_pos);
    if (dir)
        v3_copy(dir, &fwd);
    else
        player_set_cam_dir(&fwd);
    if (ang)
        i3_copy(ang, mot_ang);
    else
        player_set_cam_roll(-mot_ang[2]);
}

/* 0x00cabbc8 */
void venus_player_motion_blend(char *view, Vec3 *mot_pos, Vec3 *mot_dir, int *mot_ang, float t)
{
    Vec3 pos, dir, up, up_cur, up_mot;
    int ang[3];
    int cam_ang[3];
    float zero, one;

    do {
        ang[1] = RAD_TO_ANG(atan2f(-((Vec3 *)(view + 32))->x, -((Vec3 *)(view + 32))->z));
        ang[0] = RAD_TO_ANG(atan2f(((Vec3 *)(view + 32))->y, sqrtf(((Vec3 *)(view + 32))->x * ((Vec3 *)(view + 32))->x + ((Vec3 *)(view + 32))->z * ((Vec3 *)(view + 32))->z)));
    } while (0);
    do {
        zero = 0.0f;
        one = 1.0f;
        up_cur.x = up_cur.z = zero;
        up_cur.y = one;
    } while (0);
    ang[2] = *(float *)(view + 48) * ANG_PER_RAD;
    vec_rotate_zxy(&up_cur, ang);
    do {
        ang[1] = RAD_TO_ANG(atan2f(-mot_dir->x, -mot_dir->z));
        ang[0] = RAD_TO_ANG(atan2f(mot_dir->y, sqrtf(mot_dir->x * mot_dir->x + mot_dir->z * mot_dir->z)));
    } while (0);
    ang[2] = -mot_ang[2];
    vec_rotate_zxy(&up_mot, ang);
    v3_lerp(&pos, (Vec3 *)(view + 8), mot_pos, t);
    v3_lerp(&dir, (Vec3 *)(view + 32), mot_dir, t);
    v3_lerp(&up, &up_cur, &up_mot, t);
    if (up.y < zero)
        up.y = fabsf(up.y);
    player_set_cam_pos(&pos);
    player_set_cam_dir(&dir);
    angles_from_dirs(&dir, &up, cam_ang);
    player_set_cam_roll(cam_ang[2]);
}

/* 0x00cabee0 */
void venus_player_orbit(char *view, int apply)
{
    int ang[2];
    Vec3 up, side, fwd;
    float sc[12];
    float tau, t, pitch, roll, lim;
    int n;
    Vec3 *live = (Vec3 *)(view + 68), *cur = (Vec3 *)(view + 92), *to = (Vec3 *)(view + 116);
    Vec3 *live_tilt, *cur_tilt, *to_tilt;

    tau = TWO_PI;
    *(float *)(view + 64) += live[1].z;
    if (*(float *)(view + 64) > tau)
        *(float *)(view + 64) -= tau;
    if (*(float *)(view + 64) < -TWO_PI)
        *(float *)(view + 64) += tau;
    if (*(int *)(view + 140)) {
        n = --*(int *)(view + 140);
        t = (float)n / (float)*(int *)(view + 144);
        ulSinCosf(t * PI, &sc[0], &sc[1]);
        t = sc[1] * -0.5f + 0.5f;
        live->x = cur->x * t + to->x * (1.0f - t);
        live->y = cur->y * t + to->y * (1.0f - t);
        live->z = cur->z * t + to->z * (1.0f - t);
        live_tilt = (Vec3 *)(view + 80);
        cur_tilt = (Vec3 *)(view + 104);
        to_tilt = (Vec3 *)(view + 128);
        live_tilt->x = cur_tilt->x * t + to_tilt->x * (1.0f - t);
        live_tilt->y = cur_tilt->y * t + to_tilt->y * (1.0f - t);
        live_tilt->z = cur_tilt->z * t + to_tilt->z * (1.0f - t);
    }
    v3_copy((Vec3 *)(view + 20), (Vec3 *)(view + 8));
    v3_copy((Vec3 *)(view + 8), (Vec3 *)(view + 68));
    ROT_Y((int)(*(float *)(view + 64) * ANG_PER_RAD), (sc + 2), ((Vec3 *)(view + 8))->x,
          ((Vec3 *)(view + 8))->z);
    /* tilt pair of the blended view (live[1]) */
    ROT_Z((int)(live[1].y * ANG_PER_RAD), (sc + 4), ((Vec3 *)(view + 8))->x,
          ((Vec3 *)(view + 8))->y);
    ROT_X((int)(live[1].x * ANG_PER_RAD), (sc + 6), ((Vec3 *)(view + 8))->y,
          ((Vec3 *)(view + 8))->z);
    if (apply)
        player_set_cam_pos((Vec3 *)(view + 8));
    fwd.x = ((Vec3 *)(view + 8))->x;
    fwd.z = ((Vec3 *)(view + 8))->z;
    up.y = 1.0f;
    fwd.y = up.x = up.z = 0.0f;
    vec3_normalize(&fwd, &fwd);
    ulVec3Cross(&side, &fwd, &up);
    do {
        ang[1] = atan2f(-side.x, -side.z) * ANG_TURN / tau;
        ang[0] = atan2f(side.y, sqrtf(side.x * side.x + side.z * side.z)) * ANG_TURN / tau;
    } while (0);
    side.y = 0.0f;
    side.x = 0.0f;
    side.z = 1.0f;
    pitch = (*(float *)(view + 12) - *(float *)(view + 24)) * venus_player_pitch_rate;
    *(float *)(view + 44) = *(float *)(view + 44) * venus_player_smooth + pitch * (1.0f - venus_player_smooth);
    ROT_X((int)DEG_TO_ANG(*(float *)(view + 44)), (sc + 8), side.y, side.z);
    ROT_Y(ang[1], (sc + 10), side.x, side.z);
    if (apply)
        player_set_cam_dir(&side);
    v3_copy((Vec3 *)(view + 32), &side);
    if (live[1].z != 0.0f) {
        roll = venus_player_roll_rate * live[1].z / tau;
        lim = venus_player_roll_max * RAD_PER_DEG;
        if (roll > lim)
            roll = lim;
        else if (roll < -lim)
            roll = -lim;
    } else
        roll = 0.0f;
    *(float *)(view + 48) = *(float *)(view + 48) * venus_player_smooth + roll * (1.0f - venus_player_smooth);
    if (apply)
        player_set_cam_roll(*(float *)(view + 48) * ANG_PER_RAD);
}

/* 0x00cac478 */
void venus_player_view_set(VenusPlayerWork *w, int frm, Vec3 *pos, float tilt_x, float tilt_z, float ang_spd)
{
    VenusView *to = &w->to;

    v3_copy(&to->pos, pos);
    to->tilt_x = tilt_x;
    to->tilt_z = tilt_z;
    to->ang_spd = ang_spd;
    if (!frm) {
        w->from = w->to;
        w->cur = w->to;
        return;
    }
    w->cur = w->from;
    w->t = frm;
    w->n = frm;
}

/* 0x00cac548 */
void venus_player_view_set_spd(VenusPlayerWork *w, int frm, float ang_spd)
{
    VenusView *to = &w->to;

    venus_player_view_set(w, frm, &to->pos, to->tilt_x, to->tilt_z, ang_spd);
}

/* steer(cast) */
/* 0x00cac560 */
void venus_player_seq(char *w)
{
    Vec3 view_pos;
    Vec3 pos, dir;
    int ang[3];
    float orb_y, orb_ang, orb_spd, orb_len, orb_spd2;
    float ang_spd;
    VenusViewEnt *ent = *(VenusViewEnt **)(w + 172);
    BossPlDiWork *cmd;
    int *mot;
    int fin_frm;
    float blend;

    if (ent->cam && (TASK_OF(ent->cam)->flags & TASK_F_END))
        ent->cam = 0;
    if (ent->rec[0] != -1 && !ent->cam) {
        if (ent->next->rec[14] == 1)
            fin_frm = 0;
        else
            fin_frm = cmd_demo_cam_fin * FPS;
        ent->cam = (int)boss_camera_create(w, cmd_pass_motion_v, ent->rec[0], ent->rec[1], (Vec3 *)(ent->rec + 2), 220, cmd_demo_cam_st * FPS, fin_frm, cmd_demo_cam_mot * FPS);
        if (ent->next->rec[8] == -1) {
            if (ent->rec[14] == 2) {
                int *cam_blk;

                cam_blk = ((int *)pnb_get_entry(*(void **)w, ent->rec[0]));
                mot = ((int *)pnb_get_entry(*(void **)w, ent->rec[1]));
                *(int *)(w + 152) = venus_player_orbit_calc((void *)*cam_blk, (void *)*mot, venus_player_orbit_ofs, &orb_y, &orb_ang, &orb_spd, &orb_len);
                *(int *)(w + 156) = cmd_ppass_speed_t * FPS;
                *(float *)(w + 160) = TWO_PI / (cmd_ppass_speed_e * FPS);
                mot = ((int *)pnb_get_entry(*(void **)w, ent->rec[8]));
                venus_player_orbit_calc(0, (void *)*mot, venus_player_orbit_ofs + 3, &orb_y, &orb_ang, &orb_spd2, &orb_len);
                orb_spd2 = orb_spd;
                ang_spd = orb_spd2;
                orb_ang += ang_spd * venus_player_orbit_ofs[3];
            } else {
                int *cam_blk;

                cam_blk = ((int *)pnb_get_entry(*(void **)w, ent->rec[0]));
                mot = ((int *)pnb_get_entry(*(void **)w, ent->rec[1]));
                *(int *)(w + 148) = venus_player_orbit_calc((void *)*cam_blk, (void *)*mot, venus_player_orbit_ofs, &orb_y, &orb_ang, &orb_spd2, &orb_len);
                ang_spd = orb_spd2;
            }
            ((union { int i; float f; } *)&view_pos.x)->i = 0;
            view_pos.z = orb_len;
            view_pos.y = orb_y;
            {
                float x = view_pos.x;

                *(float *)(w + 64) = orb_ang;
                venus_player_view_set((VenusPlayerWork *)w, 0, &view_pos, x, x, ang_spd);
            }
        }
        if (ent->rec[11])
            boss_cam_ang_create((BossCameraWork *)ent->cam, ent->rec[11], ent->rec[12], ent->rec[9], ent->rec[10]);
        boss_camera_set_player_vec((BossCameraWork *)ent->cam, ent->rec + 5);
        if (ent->rec[13] & 1)
            venus_seq_step(*(int *)(w + 164));
        if (ent->rec[15]) {
            cmd = boss_pl_di_create(w, ent->rec[15], 0);
            if (ent->rec[16]) {
                *(int *)(w + 1328) = *((int *)pnb_get_entry(*(void **)w, venus_player_di_ids[0]));
                *(int *)(w + 1332) = *((int *)pnb_get_entry(*(void **)w, venus_player_di_ids[1]));
                boss_pl_di_set(cmd, (int *)(w + 1328));
            }
        }
    }
    if (ent->blk) {
        /* past its end the entry loops, holds or moves on */
        *(float *)&ent->frm += ent->frm_spd;
        if (ent->frames < *(float *)&ent->frm) {
            if (ent->rec[14] == 1) {
                *(float *)&ent->frm -= ent->frames + cmd_pass_loop_adj_frm;
            } else if (ent->rec[14] == 3) {
                if (ent->next)
                    *(VenusViewEnt **)(w + 172) = ent->next;
                *(float *)&ent->frm = ent->frames;
            } else if (cmd_pass_loop_sw) {
                *(float *)&ent->frm = 0.0f;
            } else {
                *(VenusViewEnt **)(w + 172) = ent->next;
            }
        }
    }
    if (ent->blk) {
        if (ent->rec[14] == 2 && ent->frames - *(float *)&ent->frm < (float)cmd_pass_fade_frm) {
            blend = 1.0f - (ent->frames - *(float *)&ent->frm) / (float)cmd_pass_fade_frm;
            venus_player_orbit(w, 1);
            venus_player_motion_set(ent->blk, &pos, ang, &dir, *(float *)&ent->frm);
            venus_player_motion_blend(w, &pos, &dir, ang, blend);
        } else {
            venus_player_motion_set(ent->blk, 0, 0, 0, *(float *)&ent->frm);
            venus_player_orbit(w, 0);
        }
    } else {
        venus_player_orbit(w, 1);
    }
    if (*(int *)(w + 148) && --*(int *)(w + 148) <= 0) {
        venus_player_view_rand(w, 0);
        venus_seq_step(*(int *)(w + 164));
    }
    if (*(int *)(w + 152) && --*(int *)(w + 152) <= 0)
        venus_player_view_set_spd((VenusPlayerWork *)w, *(int *)(w + 156), *(float *)(w + 160));
}

/* 0x00caca70 */
void venus_player_exec(char *w)
{
    VenusViewEnt *ent;
    unsigned int i;
    int eye_ang[3];
    char *mng;

    venus_player_seq(w);
    if (debug_flags_test(4)) {
        if (cmd_pass_disp) {
            if (!venus_player_pass_task) {
                do {
                    ent = *(VenusViewEnt **)(w + 172);
                    venus_player_pass_task = venus_player_pass_create(w, (int)ent->blk, (int)&ent->frm);
                } while (0);
            } else {
                FINISH(venus_player_pass_task);
                *(int *)&venus_player_pass_task = 0;
            }
            cmd_pass_disp = 0;
        }
        if (kbd_state[7] & 0x44) {
            switch (kbd_state[5]) {
            case 21:
                venus_player_view_rand(w, 0);
                break;
            case 6:
                if (!venus_player_pass_task) {
                    ent = *(VenusViewEnt **)(w + 172);
                    venus_player_pass_task = venus_player_pass_create(w, (int)ent->blk, (int)&ent->frm);
                } else {
                    FINISH(venus_player_pass_task);
                    *(int *)&venus_player_pass_task = 0;
                }
                break;
            case 22:
                goto reset;
            }
        }
    }
    if (!cmd_seq_sw)
        return;
reset:
    cmd_seq_sw = 0;
    {
        VenusViewEnt *v = *(VenusViewEnt **)(w + 172);

        if (v->cam)
            FINISH((int *)v->cam);
    }
    *(int *)(w + 172) = 0;
    venus_battle_init(*(char **)(w + 164));
    for (i = cmd_sw_seq_num; i < 9; i++)
        venus_player_ent_add(w, venus_player_seq_tbl + i * 68, i);
    if (cmd_sw_seq_num) {
        mng = *(char **)(w + 164);
        *(int *)(mng + 6268) = cmd_sw_seq_num;
        venus_seq_step((int)mng);
        *(int *)(mng + 6272) = 0;
    }
    eye_ang[0] = 0;
    eye_ang[1] = 0;
    eye_ang[2] = 0;
    player_set_eye(eye_ang);
}

/* steer(cast) */
/* 0x00cacc58 */
void venus_player_ent_add(void *work, void *rec_in, int i)
{
    int *w = work;
    int *rec = rec_in;
    VenusViewEnt *ent, *last;
    int n = 0;

    if (!w[43]) {
        w[43] = (int)(w + 44);
        ent = (VenusViewEnt *)w[43];
    } else {
        for (ent = (VenusViewEnt *)w[43]; ent; ent = ent->next) {
            last = ent;
            n++;
        }
        ent = (VenusViewEnt *)(w + 44) + n;
        last->next = ent;
    }
    ent->idx = n;
    ent->rec = rec;
    ent->next = 0;
    if (rec[8] != -1) {
        ent->blk = (int *)*((int *)pnb_get_entry((void *)w[0], rec[8]));
        ent->frames = (float)(unsigned int)ent->blk[1] - 1.0f;
        if (i == 5)
            ent->frames -= 2.0f;
    } else {
        ent->blk = 0;
        ent->frames = 0.0f;
    }
    ent->unk_14 = 1;
    ent->frm_spd = cmd_pass_motion_v;
    ent->frm = 0;
    ent->cam = 0;
}

/* 0x00cacd80 */
void venus_player_disp(void)
{
    Vec3 eye, pl;
    float dx, dz;
    float *ent;

    if (cmd_test_sw) {
        camera_get_eye(&eye);
        player_get_pos(&pl);
        debug_set_origin(20, 22);
        dx = pl.x - eye.x;
        dz = pl.z - eye.z;
        debug_printf(str_venus_player_off_y, pl.y - eye.y, sqrtf(dx * dx + dz * dz));
        ent = *(float **)((char *)venus_player_work + 172);
        debug_set_origin(20, 23);
        debug_printf(str_venus_player_frm, (int)ent[1], (int)ent[3]);
    }
}

/* 0x00cace90 */
void venus_player_release(void *w)
{
    player_swap_fullrange(1);
    player_demo_motion_end();
}

/* 0x00caceb0 */
int venus_player_create(char *parent, int x)
{
    VenusPlayerWork *w = task_create_inherit(parent, &venus_player_desc, 1392);
    Vec3 pos;
    unsigned int i;

    *(void **)&venus_player_work = w;
    ((void **)w)[41] = parent;
    for (i = 0; i < 9U; i++)
        venus_player_ent_add(w, venus_player_seq_tbl + i * 68, i);
    pos.z = venus_player_view_z;
    pos.x = venus_player_view_x;
    pos.y = venus_player_view_y;
    venus_player_view_set(w, 0, &pos, venus_player_tilt_x, venus_player_tilt_z,
                           TWO_PI / (venus_player_orbit_sec * FPS));
    return (int)w;
}

/* steer(cast) */
/* 0x00cacfa8 */
int venus_player_ent_next(int *w)
{
    int *ent = (int *)((int *)w[43])[8];
    int *rec = (int *)ent[7];

    w[43] = (int)ent;
    if (rec[1] != -1)
        return (int)((float)((unsigned int *)*((int *)pnb_get_entry((void *)w[0], rec[1])))[1] - 1.0f);
    return 0;
}

/* steer(cast) */
/* 0x00cad038 */
int venus_player_orbit_calc(void *cam_mot, void *mot, float *ofs, float *out_y, float *out_ang, float *out_spd, float *out_len)
{
    Vec3 samples[3];
    Vec3 cam_dir;
    CamArgs args;
    Vec3 up, cr;
    int roll, fov;
    float frm, avg_y, avg_len, spd, ang, z;

    frm = (float)((unsigned int *)mot)[1] - 1.0f;
    if (cam_mot) {
        args.d = &cam_dir;
        args.roll = (short *)&roll;
        args.fov = (short *)&fov;
        roll = 0;
        fov = 0;
        args.c = (Camera *)&samples[2];
        anim_eval_header(frm, cam_mot, mot, &args);
        frm -= cmd_sample_frm;
        args.c = (Camera *)&samples[1];
        anim_eval_header(frm, cam_mot, mot, &args);
        frm -= cmd_sample_frm;
        args.c = (Camera *)&samples[0];
        anim_eval_header(frm, cam_mot, mot, &args);
    } else {
        motion_point_pose(mot, frm, &samples[2], 0);
        frm -= cmd_sample_frm;
        motion_point_pose(mot, frm, &samples[1], 0);
        motion_point_pose(mot, frm - cmd_sample_frm, &samples[0], 0);
    }
    z = 0.0f;
    avg_y = (samples[0].y + samples[1].y + samples[2].y) / 3.0f;
    samples[2].y = z;
    samples[1].y = z;
    samples[0].y = z;
    avg_len = vec3_length(&samples[0]);
    avg_len += vec3_length(&samples[1]);
    avg_len += vec3_length(&samples[2]);
    avg_len /= 3.0f;
    avg_len += ofs[2];
    avg_y += ofs[1];
    vec3_normalize(&samples[0], &samples[0]);
    vec3_normalize(&samples[2], &samples[2]);
    spd = acos(ulVec3Dot(&samples[0], &samples[2])) * 0.5 / cmd_sample_frm;
    up.x = z;
    up.z = 1.0f;
    up.y = avg_y;
    ang = acos(ulVec3Dot(&up, &samples[2]));
    ulVec3Cross(&cr, &up, &samples[2]);
    if (cr.y < z)
        ang = -ang;
    ang += spd * ((float)((unsigned int *)mot)[1] - 1.0f);
    *out_y = avg_y;
    *out_ang = ang;
    *out_spd = spd;
    *out_len = avg_len;
    return (float)((unsigned int *)mot)[1] - 1.0f;
}

/* 0x00cad3d0 */
void venus_player_view_rand(void *work, int n)
{
    VenusPlayerWork *w = work;
    Vec3 pos;
    float tilt_x, tilt_z;

    pos.z = w->from.pos.z;
    pos.x = RAND_FLOAT_A() * (venus_player_rnd_x_max - venus_player_rnd_x_min) + venus_player_rnd_x_min;
    pos.y = RAND_FLOAT_A() * (venus_player_rnd_y_max - venus_player_rnd_y_min) + venus_player_rnd_y_min;
    tilt_x = RAND_FLOAT_A() * venus_player_tilt_x_rnd;
    tilt_z = RAND_FLOAT_A() * venus_player_tilt_z_rnd;
    venus_player_view_set(w, venus_player_view_sec * FPS, &pos, tilt_x, tilt_z, TWO_PI / (venus_player_orbit_sec * FPS));
}

/* 0x00cad520 */
void venus_player_cam_exec(VenusPlayerCamRamp *w)
{
    w->v += w->dv;
    if (--w->n <= 0)
        FINISH_IF(w);
}

/* steer(cast) */
/* 0x00cad560 */
void venus_player_cam_disp(float *w)
{
    Vec3 v;
    float sc[2];

    circle_draw_flat((Vec3 *)(w + 4), 0xffffffff, w[3]);
    ulMatrixPush();
    v.x = 0.0f;
    v.z = w[3];
    v.y = w[0];
    ROT_Y((int)(w[1] * ANG_PER_RAD), sc, v.x, v.z);
    vu0_matrix_translate(ulMatrixCur, v.x, v.y, v.z);
    axes_draw(20.0f);
    ulMatrixPop();
}

/* 0x00cad640 */
VenusPlayerCamWork *venus_player_cam_create(void *parent, void *cam_mot, void *mot, float *ofs)
{
    VenusPlayerCamWork *w = task_create_inherit(parent, &venus_player_cam_desc, 32);

    w->frm = venus_player_orbit_calc(cam_mot, mot, ofs, &w->y, &w->ang, &w->ang_spd, &w->len);
    w->y_base = w->y;
    w->ang += *ofs * w->ang_spd;
    unit_circle_build();
    return w;
}

/* 0x00cad6e0 */
void venus_player_pass_exec(int *w)
{
    Vec3 pos;
    int ang[3];

    motion_point_pose((void *)w[0], *(float *)w[1], &pos, ang);
    v3_sub((Vec3 *)(w + 5), &pos, (Vec3 *)(w + 2));
    v3_copy((Vec3 *)(w + 2), &pos);
}

/* 0x00cad768 */
void venus_player_pass_disp(int *w)
{
    Vec3 pos;
    int ang[3];
    int frame;

    motion_path_draw((unsigned int *)w[0]);
    motion_point_pose((void *)w[0], *(float *)w[1], &pos, ang);
    ulMatrixPush();
    MTX_TRANS(&pos);
    venus_player_rotate_yxz(ang);
    axes_draw(20.0f);
    ulMatrixPop();
    debug_set_origin(20, 22);
    frame = *(float *)w[1];
    debug_printf(str_venus_player_pass_frm, frame, vec3_length((Vec3 *)(w + 5)));
}

/* 0x00cad838 */
int *venus_player_pass_create(void *parent, int mot, int frm_p)
{
    int *w;

    if (!mot)
        return 0;
    w = task_create_inherit(parent, &venus_player_pass_desc, 32);
    w[0] = mot;
    w[1] = frm_p;
    return w;
}

/* 0x00cad890 */
void venus_player_rotate_zyx(int *ang)
{
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
}

/* 0x00cad8e0 */
void venus_player_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
