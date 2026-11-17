/*
 * Mars boss core and flower: the core (movement, life, hit flash, debris, trail), the opening
 * demo, and the flower of 16 lock-on slots that rise, aim, fire and fall when shot. Also the
 * background map task, score lock table and sound and music helpers.
 */

#include "types_gw.h"
#include "b_mars.h"
#include "mathdef.h"
#include "color.h"
#include "console.h"
#include "enemy_status.h"
#include "fog.h"
#include "game.h"
#include "gw_varedit.h"
#include "item.h"
#include "libc.h"
#include "lockon.h"
#include "map.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player.h"
#include "pnb.h"
#include "render.h"
#include "rez_wire_compile.h"
#include "sound_bank.h"
#include "sound_beat.h"
#include "stage.h"
#include "task.h"
#include "texforce.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "mars_core.h"
#include "mars.h"
#include "mars_bullet.h"
#include "mars_color.h"
#include "../common/boss_common.h"
#include "../common/boss_sound.h"
#include "../common/boss_wire.h"

typedef struct MarsCNode {     /* 56 bytes */
    int unk_00;                 /* 0x00: set to 15 by mars_c_nodes_init, never read */
    int unk_04;                 /* 0x04: set to 0 by mars_c_nodes_init, never read */
    int unk_08[9];              /* 0x08: cleared by mars_c_nodes_init, never read */
    struct MarsCNode *prev;     /* 0x2c */
    int unk_30;                 /* 0x30: set to 0 by mars_c_nodes_init, never read */
    char pad_34[4];
} MarsCNode;

/* 468 bytes */
typedef struct MarsCoreWork {
    void *pack;
    void *textures;
    Vec3 pos;
    Vec3 *home;
    char pad_18[12];
    int ang[3];                         /* 0x24 */
    int *boss_ang;
    int disp_ang[3];
    ModelPart *body;
    ModelPart *shell;
    ModelPart *dust_part;
    void *parts;
    char *unk_50;                       /* 0x50: copied from the creation record, never read */
    float ofs_z;
    float unk_58;                       /* 0x58: set to 75 with ofs_z, never read */
    float dock_t;
    float dock_ph;
    int timer;
    int move_frames;
    int motion_delay;
    int hit_cnt;
    LockObj lock;
    int life;
    int state;
    int motion_step;
    int side;
    int back;
    int hit_flags;
    int exposed_frm;
    int debris_cnt;
    int debris_step;
    int trail_on;
    int flip_start;
    char *boss;
    char pad_e4[60];
    MarsCNode *node_tail;               /* 0x120 */
    char pad_124[4];
    MarsCNode nodes[2];                 /* 0x128 */
    int *mot_appear;
    int *mot_a;
    int *mot_b;
    int appear_frm;
    int appear_end;
    float flash[4];
    float glow;
    float roll_v;
    float roll;
    float roll_to;
    int roll_base;
    CmObjectBlurWork *trail;
} MarsCoreWork;

typedef struct MarsCoreInitView {
    char pad_00[20];
    void *home;                         /* 0x14 */
    char pad_18[24];
    int boss_ang;                       /* 0x30 */
    char pad_34[12];
    char *body;                         /* 0x40 */
    char *shell;
    int dust_part;
    int parts;
    char pad_50[144];
    int boss;                           /* 0xe0 */
} MarsCoreInitView;

typedef enum MarsSt { MARS_ST_FALL = -1, MARS_ST_IDLE, MARS_ST_RISE, MARS_ST_RISE2 } MarsSt;

/* 576 bytes */
typedef struct MarsFlSlot {
    int on;
    int stop;                           /* nonzero ends the slot's shot spawner */
    int mot_st;
    int firing;
    int life;
    int mode;
    int depth;
    char pad_1c[4];
    Vec3 pos;                           /* 0x20 */
    LockObj lock;
    float sway_v;
    float sway;
    float tilt_v;
    float tilt;
    float tilt_to;
    int sway_ang;
    int ang[3];
    ModelPart part;
    int wire_clip;
    float mtx[16];
    Vec3 aim_scl[19];
    int unk_214;                        /* 0x214: cleared when the slot starts firing, never read */
    int timer;
    int timer_len;
    int break_timer;
    int break_cnt;
    int fired;
    float tip_frm;
    float tip_len;
    char pad_234[12];
} MarsFlSlot;

/* 9344 bytes */
typedef struct MarsFlowerWork {
    void *pack;
    void *textures;
    ModelPart *slot_part;
    ModelPart *tip_part;
    int unk_10;                         /* 0x10: copied from the boss part list, never read */
    int shot_mdl;
    int shot_break_mdl;
    ModelPart *hub;
    ModelPart *hub_tinted;
    ModelPart *pair_part;
    ModelPart *wire_part;
    ModelPart *fire_part;
    ModelPart *depth_part;
    ModelPart *break_parts;
    char pad_38[8];
    MarsFlSlot slot[16];                /* 0x40 */
    int retract;
    int at_timer;
    int attack_on;
    int start_wait;
    int *mask;
    int pattern_idx;
    int burst_left;
    int burst_num;
    int last_slot;
    int attack_st;
    int spawned;
    char *boss;
    float ofs_z;
    char pad_2474[12];
} MarsFlowerWork;

/*
 * Slot i seen from 64 bytes before it, as a 576-byte stride from the flower work: the matched
 * slot loops index this way.
 */
typedef struct MarsFlSlotView {
    char pad_00[64];
    struct {
        int on;                         /* 0x40 */
        int stop;
        int mot_st;
        int firing;
        int life;
        int mode;
        int depth;
        char pad_1c[4];
        Vec3 pos;
        LockObj lock;
        float sway_v;
        char pad_70[400];
    } s;
} MarsFlSlotView;

typedef struct MarsFlowerCreateView {
    char pad_00[8];
    int cfg[12];                        /* 0x08: values of the creation record */
    char pad_38[9228];
    enum { MARS_TIME_ZERO } at_timer;   /* 0x2444 */
    char pad_2448[8];
    int *mask;                          /* 0x2450 */
    char pad_2454[20];
    int spawned;                        /* 0x2468 */
    void *boss;
    float ofs_z;
    char pad_2474[12];
} MarsFlowerCreateView;

extern int modeltest_compile_obj;
extern BossSe se_table;
extern char mars_demo_vardef[];
extern GwVarDef mars_demo_vardefs[];
extern struct GwVarNode *mars_demo_vars_group;
extern char mars_score_vardef[];
extern BossVolDef vol_table[];
extern int adx_files[2][2];
extern struct TaskDesc mars_c_desc;
extern char mars_core_vardef[];
extern int mars_c_life_lo;
extern int mars_c_life_hi;
extern float CMD_CORE_LIFE_X2T;

/* first five words of a TaskDesc */
typedef struct MarsTaskDesc {
    void (*init)(void *w);
    void (*exec)(void *w);      /* 0x04 */
    void (*disp)(void *w);      /* 0x08 */
    void (*release)(void *w);   /* 0x0c */
    char *name;                 /* 0x10 */
} MarsTaskDesc;

extern const MarsTaskDesc mars_1st_demo_desc;
extern float CMD_FOG_FADE_T;
extern BossLockDef mars_score_lock_defs[];
extern int mars_score_vars_group;
extern GwVarDef mars_score_vardefs[];
extern int pcm_files[][2];
extern char mars_flower_vardef[];
extern int bgm_seq_table[];
extern float mars_map_pass_spd[2];
extern int mars_fl_slot_frames[];
extern struct TaskDesc mars_map_desc;
extern int mars_map_model_def[];
extern int mars_map_part_value;
extern int cmFlowerHP[];
extern struct TaskDesc mars_fl_desc;
extern float CMD_FLOWER_AT_ST;
extern float mars_fl_decay_init;

extern struct GwVarNode *mars_flower_vars_group;

extern GwVarDef mars_flower_vardefs[];
extern int CMD_CORE_LIFE;
extern GwVarDef mars_core_vardefs[];
extern int mars_core_vars_group;
extern float mars_fl_fire_sec;
extern float mars_fl_aim_sec;
extern float mars_fl_fire_fade;
extern float mars_fl_sway_damp;
extern float mars_fl_sway_k;
extern float mars_fl_tilt_damp;
extern float mars_fl_tilt_k;
extern float mars_fl_tilt_range;
extern float mars_fl_tilt_sec;
extern float CMD_FIRST_WAIT;
extern float mars_fl_hit_fade;
extern float mars_fl_break_sec;
extern int mars_fl_fallen_end;
extern int *mars_fl_patterns[];
extern float CMD_FLOWER_AT_T;
extern float mars_fl_aim_amp;
extern float mars_fl_aim_turns;
extern float mars_fl_aim_spd;
extern float mars_fl_ofs_z;
extern unsigned int mars_fl_halo_col;
extern unsigned int cmFWcol;
extern float mars_fl_halo_size;
extern float mars_fl_halo_fade;
extern float mars_fl_halo_sec;
extern float mars_c_blend_frac_a;
extern float mars_c_blend_frac_b;
extern float mars_c_glow_scl;
extern int cmCoreShellColor[2];
extern float mars_fl_ref_x;
extern float mars_fl_ref_y;
extern float mars_fl_ref_z;
extern char mars_1st_demo_files[][8];
extern float CMD_STJ_WAIT;
extern float CMD_CROSS_ST;
extern float CMD_CROSS_T;
extern float CMD_DEMO_CAM_WAIT;
extern int mars_1st_demo_light_cols[];
extern const float CMD_EAT_MOT_V;
extern const float CMD_FL_MOT_V;
extern float mars_c_glow_decay;
extern int mars_c_debris_num;
extern unsigned int cmDustCol;
extern int mars_c_debris_kind;
extern BossCfKey mars_c_flash_keys[];
extern int mars_c_hit_step;
extern float mars_c_flash_scl;
extern int mars_c_debris_min;
extern unsigned int cmFlWireCol1[2];
extern float mars_fl_scl;
extern float CMD_CORE_MOVE_T;
extern const float mars_c_wobble_spd;
extern float mars_c_dock_accel;
extern float mars_c_dock_rate;
extern float mars_c_spring_damp;
extern float mars_c_spring_k;

const MarsTaskDesc mars_1st_demo_desc = {
    0, (void (*)(void *))mars_1st_demo_exec, 0, (void (*)(void *))mars_1st_demo_release, "Mars_1stDemo",
};

/* 0x00ca8850 */
MarsCNode *mars_c_nodes_init(MarsCNode *p, int n)
{
    MarsCNode *prev = 0;

    for (; n != 0; n--) {
        p->unk_00 = 15;
        p->unk_04 = 0;
        memset(p->unk_08, 0, sizeof(p->unk_08));
        p->prev = prev;
        p->unk_30 = 0;
        prev = p;
        p++;
    }
    return p - 1;
}

/* steer(cast) */
/* 0x00ca88d0 */
void mars_c_init(MarsCoreWork *w, MarsCoreArg *arg)
{
    MarsCoreInitView *iv = (MarsCoreInitView *)w; /* steer(alias) */

    iv->body = arg->body;
    iv->home = arg->home;
    w->state = 0;
    iv->shell = arg->shell;
    w->hit_flags = 0;
    w->motion_step = 0;
    w->side = 0;
    iv->boss_ang = (int)arg->boss_ang;
    w->unk_50 = arg->unk_08;
    iv->parts = (int)arg->parts;
    iv->dust_part = (int)arg->dust_part;
    w->unk_58 = 75.0f;
    w->ofs_z = -75.0f;
    iv->boss = (int)arg->boss;
    w->node_tail = mars_c_nodes_init(w->nodes, 2);
    w->mot_appear = *(int **)pnb_get_entry(w->pack, 78);
    w->mot_a = *(int **)pnb_get_entry(w->pack, 8);
    w->mot_b = *(int **)pnb_get_entry(w->pack, 9);
    w->appear_end = w->mot_appear[1] - 1;
    lock_init(0, &w->lock, w, &w->pos, 0, CMD_CORE_LIFE, 0);
    lockon_entry(&w->lock);
    w->life = CMD_CORE_LIFE;
    v3_set((Vec3 *)(*(WireObj **)&w->body->root)->pos, 0.0f, 0.0f, 0.0f); /* steer(alias) */
    mars_core_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, mars_core_vardef);
    gw_varedit_add_table(&gw_varedit_root, mars_core_vars_group, mars_core_vardefs);
}

/* 0x00ca8a50 */
void mars_c_to_world(MarsCoreWork *w, float *in, float *out)
{
    Vec3 pos;
    int ang[3];

    ulMatrixPush();
    mars_pose_cb(w->boss, &pos, ang);
    matrix_unit_set(0);
    MTX_TRANSF(pos.x, pos.y, pos.z);
    mars_c_rotate_yxz(ang);
    mars_get_ref_pose(w->boss, &pos, ang);
    vu0_matrix_translate(ulMatrixCur, pos.x, pos.y, pos.z);
    mars_c_rotate_yxz(ang);
    ulMatrixApplyPoint((float *)ulMatrixCur, in, out);
    ulMatrixPop();
}

/* 0x00ca8b20 */
void mars_c_motion_step(MarsCoreWork *w)
{
    float k;
    float sc[2];

    k = 1.0f - (float)w->timer / (float)w->move_frames;
    if (w->back == 0) {
        motion_point_pose(w->mot_a, (float)(((unsigned int *)w->mot_a)[1] - 1) * k, &w->pos, w->ang);
        w->roll_base = w->ang[2];
        vec_rotate_zxy(&w->pos, w->boss_ang);
        w->ang[1] += ANG_180;
    } else {
        motion_point_pose(w->mot_b, (float)(((unsigned int *)w->mot_b)[1] - 1) * k, &w->pos, w->ang);
        w->roll_base = w->ang[2];
        vec_rotate_zxy(&w->pos, w->boss_ang);
        ROT_X(ANG_180, sc, w->pos.y, w->pos.z);
    }
    v3_sum(&w->pos, w->home, &w->pos);
    switch (w->motion_step) {
    case 0:
        if (mars_c_blend_frac_a < k) {
            mars_motion_start(w->boss, 0, 1);
            if (w->back == 0)
                boss_se_play(3, (int)&w->lock, (int)&w->pos);
            w->motion_step += 1;
        }
        break;
    case 1:
        if (mars_c_blend_frac_b < k) {
            mars_motion_start(w->boss, 0, 2);
            w->motion_step += 1;
        }
        break;
    }
}

/* 0x00ca8d78 */
void mars_c_move(MarsCoreWork *w)
{
    Vec3 v;
    Vec3 pl_vel;
    ModelPart *part;

    player_get_frame_vel(&pl_vel);
    spring_to(&w->roll_v, &w->roll, w->roll_to, mars_c_spring_damp, mars_c_spring_k);
    switch (*(unsigned int *)&w->state) {
    case 7:
        w->life = CMD_CORE_LIFE;
        w->hit_flags = 0;
        w->motion_step = 0;
        w->side = 0;
        w->back = 0;
        w->appear_frm = 0;
        w->exposed_frm = 0;
        w->unk_58 = 75.0f;
        w->ofs_z = -75.0f;
        w->state = 1;
        w->lock.life = w->life;
        console2_swap_boss_on(0);
        *(int *)&w->glow = 0;
        *(int *)&w->body->frame = 0; /* steer(type) */
        w->shell->frame = 0;
        w->debris_cnt = 0;
        w->debris_step = 0;
        w->trail_on = 0;
        w->flip_start = 0;
        if (w->trail) {
            FINISH(w->trail);
            *(int *)&w->trail = 0; /* steer(type) */
        }
        break;
    case 8:
        w->appear_frm = 0;
        w->state = 9;
    case 9:
        if (++w->appear_frm >= w->appear_end) {
            boss_gage_create(0);
            w->state = 1;
            w->appear_frm = w->appear_end;
            mars_motion_start(w->boss, 0, 2);
        } else if ((float)w->appear_frm == 120.0f) {
            mars_motion_start(w->boss, 0, 1);
        }
        tree_node_pose((WireObj *)w->body->root, w->mot_appear, &w->pos, w->ang, 0, 1, (float)w->appear_frm);
        mars_c_to_world(w, &w->pos.x, &w->pos.x);
        break;
    case 6:
        w->back = 1;
        w->side ^= 1;
    case 3:
        w->move_frames = (int)(CMD_CORE_MOVE_T * FPS);
        mars_motion_start(w->boss, 0, 1);
        w->timer = 1;
        w->state = 4;
    case 4:
        v.x = v.y = 0.0f;
        v.z = w->ofs_z;
        vec_rotate_zxy(&v, w->boss_ang);
        v3_copy(&w->pos, w->home);
        v3_add(&w->pos, &v);
        do {
            w->ang[1] = RAD_TO_ANG(atan2f(-v.x, -v.z));
            w->ang[0] = RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
        w->ang[1] += ANG_180;
        if (--w->timer <= 0) {
            w->hit_flags |= 1;
            w->motion_delay = 120;
            w->timer = w->move_frames;
            w->state = 5;
            w->motion_step = 0;
        }
        break;
    case 5:
        mars_c_motion_step(w);
        if (--w->timer <= 0) {
            if (w->back) {
                w->back = 0;
                w->state = 2;
            } else {
                w->side ^= 1;
                w->ang[1] += ANG_180;
                w->state = 1;
                w->flip_start = 1;
            }
            w->roll_base = 0;
            mars_motion_start(w->boss, 0, -1);
        }
        if (w->motion_delay) {
            if (--w->motion_delay <= 0) {
                w->motion_delay = 0;
                mars_motion_start(w->boss, 0, 2);
            }
        }
        break;
    case 2:
        v.x = v.y = 0.0f;
        v.z = -75.0f;
        ulMatrixPush();
        matrix_unit_set(0);
        mars_c_rotate_yxz(w->boss_ang);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&v, (float *)&v);
        ulMatrixPop();
        v3_copy(&w->pos, w->home);
        v3_add(&w->pos, &v);
        v3_sub(&v, w->home, &w->pos);
        do {
            w->ang[1] = RAD_TO_ANG(atan2f(-v.x, -v.z));
            w->ang[0] = RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
        if (w->trail_on) {
            dc_object_advance_time(w->body);
            dc_object_advance_time(w->shell);
        }
        break;
    case 1:
        w->motion_step = 0;
        w->hit_cnt = 0;
        if (w->side) {
            w->ofs_z = 75.0f;
            w->dock_t = w->ofs_z / (float)(w->move_frames / 2);
        } else {
            w->ofs_z = -75.0f;
            w->dock_t = -w->ofs_z / (float)(w->move_frames / 2);
        }
        v.x = v.y = 0.0f;
        if (w->flip_start) {
            v.z = 75.0f;
            w->flip_start = 0;
        } else {
            v.z = -75.0f;
        }
        mars_c_to_world(w, (float *)&v, &w->pos.x);
        v3_sub(&v, &w->pos, w->home);
        do {
            w->ang[1] = RAD_TO_ANG(atan2f(-v.x, -v.z));
            w->ang[0] = RAD_TO_ANG(atan2f(v.y, sqrtf(v.x * v.x + v.z * v.z)));
        } while (0);
        w->timer = w->move_frames;
        break;
    case 0:
    case 10:
        w->dock_t += mars_c_dock_rate;
        if (1.0f <= w->dock_t)
            w->dock_t = 1.0f;
        w->dock_ph += mars_c_dock_accel * (1.0f - w->dock_t);
        wobble_step_slow(&v, mars_c_wobble_spd, &w->ofs_z);
        v3_sum(&w->pos, &v, &w->pos);
        break;
    }
    w->ang[2] = w->roll_base + (int)DEG_TO_ANG(w->roll);
}

/* 0x00ca9558 */
int mars_c_random_kind(void)
{
    int pick = (int)(RAND_FLOAT_A() * 4.0f);

    switch (pick) {
    case 0:
        return 0;
    case 1:
        return 2;
    }
    return 1;
}

/* 0x00ca95d0 */
void mars_c_exec(MarsCoreWork *w)
{
    Vec3 pl_vel;
    Spawn spawn;
    int n, k, old, i;

    player_get_frame_vel(&pl_vel);
    n = enemy_status_get_ticks();
    v3_add(&w->pos, &pl_vel);
    for (; n != 0; n--) {
        mars_c_move(w);
        if (w->state == 2 && (float)w->exposed_frm < CMD_CORE_LIFE_X2T * FPS)
            w->exposed_frm += 1;
    }
    w->glow *= mars_c_glow_decay;
    k = w->state;
    if (k == 0 || k == 10)
        w->lock.flags &= ~LOCK_LOCKABLE;
    else if (k == 1 && !((MarsWork *)w->boss)->core_open)
        w->lock.flags &= ~LOCK_LOCKABLE;
    else
        w->lock.flags |= LOCK_LOCKABLE;
    switch (k) {
    case 10:
        break;
    default:
        if (w->life < mars_c_life_hi) {
            w->life = mars_c_life_hi;
            w->lock.life = *(short *)&mars_c_life_hi;
        }
        break;
    case 2:
        if (w->life <= 0) {
            int delay;

            boss_se_play(2, (int)&w->lock, (int)&w->pos);
            w->life = 0;
            delay = 10;
            /* never taken ((x & 1) >> 1 is 0) */
            if (((int)w & 1) >> 1)
                w->motion_step = 1;
            w->state = delay;
            w->lock.flags &= ~LOCK_LOCKABLE;
            console2_swap_boss_on(0);
            w->dock_t = w->dock_ph = 0.0f;
            if (w->trail) {
                cm_object_blur_set_fade(w->trail, 2.0f);
                w->trail = 0;
            }
        }
        break;
    }
    if (w->lock.flags & LOCK_HIT) {
        old = w->life;
        switch (w->state) {
        case 1:
            boss_se_play(0, (int)&w->lock, (int)&w->pos);
            for (i = 0; i < mars_c_debris_num; i++)
                cm_wire_break_set_cb(cm_wire_break_simple(w->boss, 13, w->dust_part, &w->pos, 0, w->ang, cmDustCol), (void (*)(void *, Vec3 *, Vec3 *))mars_bounds_reflect);
            break;
        case 5:
            boss_se_play(1, (int)&w->lock, (int)&w->pos);
            if (w->debris_cnt < 21) {
                for (i = 0; i < mars_c_debris_num; i++)
                    cm_wire_break_set_cb(cm_wire_break_simple(w->boss, 13, w->dust_part, &w->pos, 0, w->ang, cmDustCol), (void (*)(void *, Vec3 *, Vec3 *))mars_bounds_reflect);
                k = mars_c_debris_kind;
                if (++w->debris_step >= k) {
                    w->debris_step = 0;
                    w->debris_cnt += 1;
                }
            }
            break;
        default:
            boss_se_play(0, (int)&w->lock, (int)&w->pos);
            break;
        }
        cm_cfchange_create(w, (BossCfKey *)w->flash, mars_c_flash_keys, 4, 1.0f, 0.5f);
        w->life = w->lock.life;
        if (mars_is_early_phase((int *)w->boss)) {
            if (w->state == 1) {
                if (old - w->life >= mars_c_hit_step) {
                    spawn.kind = mars_c_random_kind();
                    v3_copy((Vec3 *)&spawn.x, &w->pos);
                    ((Vec3 *)&spawn.vel_x)->x = 0.0f;
                    ((Vec3 *)&spawn.vel_x)->y = 0.0f;
                    ((Vec3 *)&spawn.vel_x)->z = 0.0f;
                    spawn.scale = mars_c_flash_scl;
                    item_create(&spawn);
                }
            } else if (w->state == 5) {
                if (w->hit_cnt < mars_c_debris_min)
                    w->hit_cnt += 1;
            }
        }
        if (!(w->hit_flags & 1) && w->state != 1) {
            do {
                w->hit_flags |= 1;
            } while (0);
            if (w->state != 2)
                mars_vcc_start(((MarsWork *)w->boss)->vcc, &w->pos, 0);
        }
    }
    angles_turn_y(w->ang, ANG_180, w->disp_ang);
}

/* steer(cast) */
/* 0x00ca9aa0 */
void mars_c_disp(MarsCoreWork *w)
{
    int save;
    float c[4];
    float sc[2];
    void *fog;
    float glow;
    float s;
    int list;

    ulGxSetTextureList(w->textures);
    if (w->state == 0)
        return;
    if (!mars_wait_done((int)w->boss))
        return;
    save = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    c[0] = mars_fog_progress(w->boss, 14);
    c[1] = w->flash[1];
    c[2] = w->flash[2];
    c[3] = w->flash[3];
    draw_set_override_color(c);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    switch (w->state) {
    case 10:
        mars_c_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        ulSinCosf(w->dock_ph, &sc[0], &sc[1]);
        s = sc[0] * 0.5f + 0.5f;
        w->body->frame = w->body->frames * s;
        w->shell->frame = w->shell->frames * s;
        break;
    case 1:
    case 8:
    case 9:
        mars_c_rotate_yxz(w->ang);
        break;
    default:
        mars_c_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        if (w->trail_on) {
            glow = w->glow * mars_c_glow_scl;
            if (1.0f < glow) {
                list = RenderSetCurrentDisplayList(6);
                ulGxSetTextureList(w->textures);
                draw_set_blend(4, 1);
                flare_draw_here(4, COL_WHITE, glow);
                RenderSetCurrentDisplayList(list);
            }
        }
        break;
    }
    fog = fog_set_enable(0);
    dc_object_draw(w->body);
    draw_set_blend(4, 7);
    part_draw_beat_frame(w->shell, w->shell->frame, cmCoreShellColor[0], cmCoreShellColor[1], 2.0f);
    fog_set_enable(fog);
    ulMatrixPop();
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(save);
    console2_set_boss_rate((float)w->life / (float)CMD_CORE_LIFE);
}

/* 0x00ca9d28 */
void mars_c_release(MarsCoreWork *w)
{
    lock_remove_entry(0, &w->lock);
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)mars_core_vardef);
}

/* 0x00ca9d60 */
MarsCoreWork *mars_c_create(void *parent, MarsCoreArg *arg)
{
    return task_create_inherit_arg(parent, &mars_c_desc, 468, arg);
}

/* 0x00ca9d88 */
int mars_c_get_state(MarsCoreWork *w)
{
    return w->state;
}

/* 0x00ca9d90 */
void mars_c_set_state(MarsCoreWork *w, int state)
{
    w->state = state;
}

/* 0x00ca9d98 */
void mars_c_trail_start(MarsCoreWork *w, int mode)
{
    if (mode == -1) {
        w->lock.life = 0;
        w->life = 0;
    } else {
        w->trail_on = 1;
        if (!w->trail) {
            ModelPart *part;

            w->trail = cm_object_blur_create(w, &w->pos, w->disp_ang, w->body, 4, 2, 0.0f, 0.0f);
            part = w->body;
            cm_object_blur_set_anim(w->trail, (unsigned int *)part->clip, part->frame, part->speed);
            cm_object_blur_set_fog((int *)w->trail, 0);
        }
    }
}

/* 0x00ca9e38 */
int mars_c_life_level(MarsCoreWork *w)
{
    int n = w->life;

    if (n <= 0)
        return -1;
    if (n <= mars_c_life_lo)
        return 2;
    if (mars_c_life_hi >= n) {
        if (CMD_CORE_LIFE_X2T * FPS <= (float)w->exposed_frm)
            return 2;
        return 1;
    }
    return 0;
}

/* 0x00ca9eb0 */
void *mars_c_get_pos(MarsCoreWork *w)
{
    return &w->pos;
}

/* 0x00ca9eb8 */
void mars_c_get_pose(MarsCoreWork *w, void **pos_out, void **ang_out)
{
    *ang_out = w->ang;
    *pos_out = &w->pos;
}

/* 0x00ca9ed0 */
void mars_c_pulse(MarsCoreWork *w)
{
    w->glow = 1.0f;
    w->roll_to += 120.0f;
}

/* 0x00ca9ef8 */
void mars_c_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}

/* 0x00ca9f48 */
void mars_demo_vars_add(void)
{
    mars_demo_vars_group = gw_varedit_add(&gw_varedit_root, 0, mars_demo_vardef);
    gw_varedit_add_table(&gw_varedit_root, mars_demo_vars_group, mars_demo_vardefs);
}

/* 0x00ca9f98 */
void mars_demo_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)mars_demo_vardef);
}

/* mars_1st_demo_desc, 72 bytes: the intro, from loading the music to the camera's swing-in */
typedef struct Mars1stDemoWork {
    int step;
    int timer;                  /* 0x04 */
    int timer_max;              /* 0x08 */
    int fog_t;                  /* 0x0c: fog fade, frames left */
    int fog_t_max;              /* 0x10 */
    int (*cb)(void *);          /* 0x14: holds the intro until it returns nonzero */
    void *arg;                  /* 0x18: the boss main task */
    BossCameraWork *cam;        /* 0x1c */
    int *load_a;                /* 0x20 */
    int load_b;                 /* 0x24 */
    BossFog fog_to;             /* 0x28 */
    BossFog fog_from;           /* 0x38 */
} Mars1stDemoWork;

/* steer(cast) */
/* 0x00ca9fb0 */
void mars_1st_demo_exec(Mars1stDemoWork *w)
{
    Vec3 v;
    BossFog fog;
    int *mpb;
    CmLightChangeWork *light;

    texforce_disable();
    switch (w->step) {
    case 0:
        v.z = -10.0f;
        v.x = v.y = 0.0f;
        w->cam = boss_camera_create(w->arg, 1.0f, 0, 0, &v, 89, 120, 150, 240);
        boss_cam_ang_create(w->cam, 0, 13653, 0, 90);
        w->load_a = (int *)sound_mpb_load_create_bpms(mars_1st_demo_files[0], 1);
        w->load_b = sound_mpb_load_create_bpms(mars_1st_demo_files[1], 2);
        w->step++;
    case 1:
        mpb = w->load_a;
        if (mpb && (TASK_OF(mpb)->flags & TASK_F_END)) {
            w->load_a = 0;
            mpb = 0;
        }
        if (w->load_b && (TASK_OF(w->load_b)->flags & TASK_F_END))
            w->load_b = 0;
        if (mpb == 0 && w->load_b == 0)
            w->step++;
        break;
    case 2:
        w->timer_max = w->timer = (int)(CMD_STJ_WAIT * FPS);
        cm_sc_cross_create(w, 255, CMD_CROSS_ST, CMD_CROSS_T);
        boss_pl_di_create(w, 0, 0);
        light = cm_light_change_create(0.0f, 2.0f);
        if (light)
            cm_light_change_set_target(light, mars_1st_demo_light_cols);
        w->step++;
    case 3:
        if (--w->timer <= 0)
            w->step++;
        else if (w->timer == 1)
            stage_end_child();
        break;
    case 4:
        if (w->cb && w->cb(w->arg) == 0)
            break;
        if (!light_mode_is_fog())
            peri_act_beat_tick();
        w->timer = (int)(CMD_DEMO_CAM_WAIT * FPS);
        w->step++;
    case 5:
        if (w->timer) {
            w->timer--;
            break;
        }
        if (beat_grid_hit(0)) {
            int *cam_mot, *cam_path;

            boss_vol_play(0, 0);
            if (light_mode_is_fog())
                peri_act_beat_tick();
            cam_mot = pnb_get_entry(*(void **)w->arg, 76);
            cam_path = pnb_get_entry(*(void **)w->arg, 77);
            boss_camera_set_motion(w->cam, *cam_mot, (unsigned int *)*cam_path);
            boss_camera_add_frame(w->cam, -18.0f);
            w->step++;
        }
        break;
    case 6:
        if (w->cam && !(TASK_OF(w->cam)->flags & TASK_F_END))
            break;
        w->step++;
    case 7:
        if (beat_grid_hit(0)) {
            boss_vol_play(1, 0);
            do {
                FINISH(w);
            } while (0);
        }
        break;
    }
    if (w->step >= 2 && w->fog_t != 0) {
        w->fog_t--;
        fog_blend(&w->fog_from, &w->fog_to, &fog, (float)w->fog_t / (float)w->fog_t_max);
        fog_apply(&fog, 4);
    }
}

/* 0x00caa350 */
void mars_1st_demo_release(Mars1stDemoWork *w)
{
}

/* 0x00caa358 */
int *mars_1st_demo_create(void *parent, int cb, int arg)
{
    Mars1stDemoWork *demo = task_create(parent, (void *)&mars_1st_demo_desc, 72);

    demo->cb = (int (*)(void *))cb;
    demo->arg = (void *)arg;
    fog_get(&demo->fog_from);
    mars_fog_params((int *)&demo->fog_to);
    demo->fog_t = demo->fog_t_max = (int)(CMD_FOG_FADE_T * FPS);
    return (int *)demo;
}

/* 0x00caa3e0 */
void tree_hide_depth_r(WireObj *obj, int *depth, int lim)
{
        if (obj->model) {
            if (*depth >= lim)
                obj->flags |= 8;
            else
                obj->flags &= ~8;
        }
        if (obj->child) {
            (*depth)++;
            tree_hide_depth_r(obj->child, depth, lim);
            (*depth)--;
        }
    if (obj->sibling)
        tree_hide_depth_r(obj->sibling, depth, lim);
}

/* 0x00caa480 */
void tree_hide_depth(WireObj *obj, int lim)
{
    int n = 0;

    tree_hide_depth_r(obj, &n, lim);
}

/* 0x00caa4a8 */
int mars_fl_slots_busy(MarsFlowerWork *w)
{
    int n = 0;
    int i;

    for (i = 0; i < 16; i++) {
        MarsFlSlot *slot = &w->slot[i];

        if (slot->on)
            n++;
    }
    return n;
}

/* 0x00caa4e0 */
void mars_fl_locks_set(MarsFlowerWork *w, int on)
{
    MarsFlSlotView *slots = (MarsFlSlotView *)w;
    int i;

    if (on) {
        for (i = 0; i < 16; i++) {
            if ((*w->mask >> i) & 1) {
                int hp = cmFlowerHP[mars_get_level((int)w->boss)];
                lock_init(1, (LockObj *)((char *)&slots[i] + 108), slots, (Vec3 *)((char *)&slots[i] + 96), 0, hp, 1);
                *(int *)((char *)&((MarsFlSlotView *)w)[i] + 80) = hp;
                lockon_entry((LockObj *)((char *)&((MarsFlSlotView *)w)[i] + 108));
                ((LockObj *)((char *)&((MarsFlSlotView *)w)[i] + 108))->flags &= ~LOCK_LOCKABLE;
                ((MarsFlSlotView *)w)[i].s.on = 1;
                *(int *)((char *)&((MarsFlSlotView *)w)[i] + 88) = 19;
                w->spawned++;
            } else {
                slots[i].s.on = 0;
                *(int *)((char *)&slots[i] + 88) = 0;
            }
        }
    } else {
        for (i = 0; i < 16; i++) {
            if (slots[i].s.on) {
                lock_remove_entry(1, (LockObj *)((char *)&slots[i] + 108));
                slots[i].s.on = 0;
            }
        }
    }
}

/* 0x00caa640 */
void mars_fl_slots_copy_part(MarsFlowerWork *w)
{
    int i;

    for (i = 0; i < 16; i++)
        w->slot[i].part = *w->slot_part;
}

/* steer(cast) */
/* 0x00caa758 */
void mars_fl_slot_motion(void *wp, int i, int mode, int arg)
{
    MarsFlowerWork *w = wp;
    ModelPart *part = 0;
    MarsFlSlot *slot;
    unsigned int *mot;

    if (i >= 0) {
        if (mode == 5 && w->slot[i].mode != 4)
            return;
        slot = &w->slot[i];
        slot->mot_st = arg;
        slot->mode = mode;
        part = &((MarsFlowerWork *)((int)w + (((int)w & 1) >> 1)))->slot[i].part; /* steer(zero) */
    }
    switch (mode) {
    case -5:
        for (i = 0; i < 16; i++)
            mars_fl_slot_motion(w, i, 6, 1);
        break;
    case -4:
        for (i = 0; i < 16; i++) {
            w->slot[i].part.clip = w->slot_part->clip;
            w->slot[i].wire_clip = w->wire_part->clip;
            w->slot[i].part.frame = 128.0f;
            w->slot[i].mot_st = MARS_ST_IDLE;
        }
        break;
    case -3:
        for (i = 0; i < 16; i++)
            mars_fl_slot_motion(w, i, 3, -1);
        break;
    case -2:
        for (i = 0; i < 16; i++)
            mars_fl_slot_motion(w, i, 2, 1);
        break;
    case -1:
        for (i = 0; i < 16; i++)
            mars_fl_slot_motion(w, i, 1, 1);
        break;
    case 0:
        break;
    case 6:
        w->slot[i].wire_clip = w->wire_part->clip;
        part->clip = w->slot_part->clip;
        part->frame = 128.0f;
        part->frames = w->slot_part->frames;
        part->speed = CMD_FL_MOT_V;
        break;
    case 1:
        w->slot[i].wire_clip = w->wire_part->clip;
        part->clip = w->slot_part->clip;
        part->frame = 0.0f;
        part->frames = 60.0f;
        part->speed = CMD_FL_MOT_V;
        break;
    case 2:
        w->slot[i].wire_clip = w->wire_part->clip;
        part->clip = w->slot_part->clip;
        part->frame = 60.0f;
        part->frames = 128.0f;
        part->speed = CMD_EAT_MOT_V;
        break;
    case 3:
        w->slot[i].wire_clip = w->wire_part->clip;
        part->clip = w->slot_part->clip;
        part->frame = w->slot_part->frames;
        part->frames = w->slot_part->frames * 0.5f;
        part->speed = CMD_FL_MOT_V;
        break;
    case 4: {
        unsigned int *mot;

        w->slot[i].wire_clip = *((int *)pnb_get_entry(w->pack, 2));
        mot = (unsigned int *)*((int *)pnb_get_entry(w->pack, 2));
        *(unsigned int **)&part->clip = mot; /* steer(alias) */
        part->frame = 0.0f;
        part->frames = ((float)mot[1] - 1.0f) * 0.5f;
        part->speed = CMD_FL_MOT_V;
        break;
    }
    case 5:
        w->slot[i].wire_clip = *((int *)pnb_get_entry(w->pack, 2));
        mot = (unsigned int *)*((int *)pnb_get_entry(w->pack, 2));
        *(unsigned int **)&part->clip = mot; /* steer(alias) */
        part->frame = ((float)mot[1] - 1.0f) * 0.5f;
        part->frames = (float)(*(unsigned int **)&part->clip)[1] - 1.0f;
        part->speed = CMD_FL_MOT_V;
        break;
    }
}

/* 0x00caab88 */
void mars_fl_slots_fade(MarsFlowerWork *w)
{
    int i;
    MarsFlSlot *slot = w->slot;

    for (i = 0; i < 16; i++, slot++) {
        switch (slot->mot_st) {
        case 2:
            slot->part.frame += slot->part.speed;
            if (slot->part.frames <= slot->part.frame) {
                slot->part.frame = slot->part.frames;
                slot->mot_st = 0;
            }
            break;
        case 1:
            slot->part.frame += slot->part.speed;
            if (slot->part.frames <= slot->part.frame) {
                slot->part.frame = slot->part.frames;
                slot->mot_st = 0;
            }
            break;
        case -1:
            slot->part.frame -= slot->part.speed;
            if (slot->part.frame <= slot->part.frames) {
                slot->part.frame = slot->part.frames;
                slot->mot_st = 0;
            }
            break;
        }
    }
}

/* 0x00caac38 */
void mars_fl_slots_sway(MarsFlowerWork *w)
{
    int i;
    MarsFlSlot *slot;

    for (i = 0, slot = w->slot; i < 16; i++, slot++) {
        spring_step(&slot->sway_v, &slot->sway, mars_fl_sway_damp, mars_fl_sway_k);
        spring_to(&slot->tilt_v, &slot->tilt, slot->tilt_to, mars_fl_tilt_damp, mars_fl_tilt_k);
        if (w->attack_on) {
            if ((int)(RAND_FLOAT_A() * mars_fl_tilt_sec * FPS) == 0)
                slot->tilt_to = RAND_FLOAT_A() * (mars_fl_tilt_range + mars_fl_tilt_range) - mars_fl_tilt_range;
        }
        slot->sway_ang = (int)DEG_TO_ANG((slot->sway + slot->tilt) * -180.0f);
    }
}

/* 0x00caadd0 */
void mars_fl_slot_aim(MarsFlSlot *slot)
{
    float t, k, ph;
    float s, c;
    float v;
    float step;
    int i;
    float *px, *py, *pz;

    t = 1.0f - (float)(slot->timer % slot->timer_len) / (float)slot->timer_len;
    ph = 0.0f;
    step = 0.05263158f;
    if (slot->timer <= 0) {
        if (mars_fl_aim_sec * FPS < (float)-slot->timer)
            k = ph;
        else
            k = (float)slot->timer / (mars_fl_aim_sec * FPS) + 1.0f;
    } else {
        k = 1.0f;
    }
    px = &slot->aim_scl[0].x;
    py = &slot->aim_scl[0].y;
    pz = &slot->aim_scl[0].z;
    for (i = 0; i < 19; i++) {
        ulSinCosf(mars_fl_aim_turns * TWO_PI * ph + mars_fl_aim_spd * t, &s, &c);
        v = k * mars_fl_aim_amp * (s * 0.5f + 0.5f);
        if (v <= 1.0f) {
            v3_set(&slot->aim_scl[i], 0.0f, 0.0f, 0.0f);
        } else {
            pz[i * 3] = v;
            py[i * 3] = v;
            px[i * 3] = v;
        }
        ph += step;
    }
}

/* 0x00caafd0 */
void mars_fl_fire(MarsFlowerWork *w)
{
    int i;
    MarsFlSlot *slot;

    for (i = 0, slot = w->slot; i < 16; i++, slot++) {
        if (slot->firing) {
            if (--slot->timer <= (int)(mars_fl_fire_sec * FPS)) {
                slot->lock.flags |= LOCK_LOCKABLE;
                if (slot->tip_frm < slot->tip_len) {
                    slot->tip_frm += 0.25f;
                    if (slot->tip_len <= slot->tip_frm)
                        slot->tip_frm = slot->tip_len;
                }
            }
            if (slot->timer == 0) {
                MarsFlSlot *cur = &w->slot[i];

                boss_se_play(5, (int)&cur->lock, 0);
                mars_mbm_create(w, (int)&cur->pos, w->shot_mdl, w->shot_break_mdl, (int)&cur->stop);
                slot->fired = 1;
            } else if (slot->timer < 0) {
                if (slot->timer % (int)(mars_fl_aim_sec * FPS) == 0)
                    slot->sway_v += mars_fl_fire_fade;
            }
            mars_fl_slot_aim(slot);
        }
    }
}

/* steer(cast) */
/* 0x00cab158 */
void mars_fl_pose(MarsFlowerWork *w)
{
    Vec3 fl_pos;
    int ang[3];
    Vec3 pos;
    Vec3 ofs;
    int ref_ang[3];
    int rot[3];
    MarsFlSlot *slot;
    int n;
    int i, j;

    for (n = enemy_status_get_ticks(); n != 0; n--) {
        mars_fl_slots_fade(w);
        mars_fl_slots_sway(w);
    }
    ulMatrixPush();
    matrix_unit_set(0);
    mars_pose_cb(w->boss, &pos, ang);
    MTX_TRANS(&pos);
    mars_fl_rotate_yxz(ang);
    slot = w->slot;
    if (slot->mode == 6) {
        motion_point_pose((void *)*(int *)pnb_get_entry(w->pack, 3), slot->part.frame, &ofs, ref_ang);
        mars_set_ref_pose(w->boss, &ofs, ref_ang);
    } else {
        mars_set_ref_pose(w->boss, 0, 0);
    }
    for (i = 0; i < 16; i++, slot++) {
        j = i * 2048;
        tree_node_matrix((WireObj *)slot->part.root, (void *)slot->part.clip, slot->mtx, 10, slot->part.frame);
        ulMatrixGetTranslation(slot->mtx, (float *)&fl_pos);
        matrix_to_angles(slot->mtx, slot->ang);
        ofs.x = mars_fl_ref_x;
        ofs.y = mars_fl_ref_y;
        ofs.z = mars_fl_ref_z;
        vec_rotate_zxy(&ofs, slot->ang);
        rot[0] = -8192;
        rot[1] = ANG_90;
        rot[2] = -ANG_180;
        vec_rotate_zxy(&ofs, rot);
        v3_add(&fl_pos, &ofs);
        ulMatrixPush();
        vu0_matrix_rotate_z(ulMatrixCur, j + j);
        vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, -mars_fl_ofs_z);
        vu0_matrix_rotate_x(ulMatrixCur, slot->sway_ang);
        vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, mars_fl_ofs_z);
        ulMatrixApplyPoint((float *)ulMatrixCur, (float *)&fl_pos, (float *)&slot->pos);
        ulMatrixPop();
    }
    ulMatrixPop();
}

/* 0x00cab408 */
int mars_fl_slots_state(MarsFlowerWork *w)
{
    int flags = 0;
    int i;

    for (i = 0; i < 16; i++) {
        if ((&w->slot[0])[i].on) { /* steer(cast): indexed from a plain pointer */
            flags |= 1;
            if ((&w->slot[0])[i].firing)
                flags |= 2;
        }
    }
    return flags;
}

/* 0x00cab448 */
int mars_fl_hit_check(MarsFlowerWork *fl)
{
    char *w = (char *)fl;
    int n = 0;
    int i;

    for (i = 0; i < 16; i++) {
        int byte_ofs = i * 576;
        char *base = w + byte_ofs;

        if (*(int *)(base + 64)) {
            LockObj *lock = (LockObj *)(base + 108);

            n++;
            if (lock->flags & LOCK_HIT) {
                int life = lock->life;
                int old = *(int *)(base + 80);
                int dmg;

                *(int *)(base + 80) = life;
                dmg = old - life;
                if (life <= 0) {
                    char *lp;

                    if (*(int *)(base + 616)) {
                        char *slot = w + (byte_ofs + 64);

                        boss_se_play(26, (int)(slot + 44), 0);
                        *(int *)(base + 616) = 0;
                    }
                    lp = w + (byte_ofs + 64);
                    lp += 44;
                    boss_se_play(9, (int)lp, 0);
                    n--;
                    lock_score(1, (LockObj *)lp);
                    *(int *)(base + 608) = (int)(mars_fl_break_sec * FPS);
                    lock_remove_entry(1, (LockObj *)lp);
                    *(int *)(base + 64) = 0;
                    *fl->mask &= ~(1 << i);
                    *(int *)(base + 76) = 0;
                    mars_fl_slot_motion(w, i, 5, 2);
                    *(float *)(base + 172) += mars_fl_hit_fade * (float)dmg;
                    *(int *)(base + 68) = 1;
                } else {
                    boss_se_play(8, (int)(w + 64 + byte_ofs) + 44, 0);
                    *(float *)(base + 172) += mars_fl_hit_fade * (float)dmg;
                }
            } else if (*(int *)(base + 68) != 2) {
                *(int *)(base + 68) = 0;
            }
        }
    }
    return n;
}

/* 0x00cab608 */
void mars_fl_slot_start(MarsFlowerWork *w, MarsFlSlot *slot)
{
    int level = mars_get_level((int)w->boss);
    int n = mars_fl_slots_busy(w);
    int j;

    if (n >= 10)
        j = 0;
    else if (n >= 4)
        j = 1;
    else
        j = 2;
    slot->firing = 1;
    slot->unk_214 = 0;
    slot->timer = slot->timer_len = mars_fl_slot_frames[level * 3 + j];
    boss_se_play(7, (int)&slot->lock, 0);
    slot->tip_frm = 0.0f;
    slot->tip_len = w->tip_part->frames;
}

/* 0x00cab6c8 */
int mars_fl_slot_try(MarsFlowerWork *w, int start)
{
    int i, j;

    j = start + 16 / w->burst_num;
    for (i = 0; i < 16; i++) {
        MarsFlSlot *slot;

        j = (j + 1) % 16;
        slot = &w->slot[j];
        if (*(long *)&slot->mot_st == 0 && slot->on) { /* steer(cast): one load tests mot_st and firing */
            mars_fl_slot_start(w, slot);
            mars_fl_slot_motion(w, j, 4, 2);
            w->last_slot = j;
            if (--w->burst_left < 1)
                return 0;
            return 1;
        }
    }
    return 0;
}

/* 0x00cab7b0 */
void mars_fl_attack_step(MarsFlowerWork *w)
{
    switch (w->attack_st) {
    case 0:
        if (--w->at_timer <= 0)
            w->attack_st = 1;
        break;
    case 1:
        if (!(mars_fl_slots_state(w) & 2)) {
            if (w->spawned - mars_fl_slots_busy(w) >= mars_fl_fallen_end) {
                mars_request_next((int *)w->boss);
            } else {
                int *tab = mars_fl_patterns[mars_get_level((int)w->boss)];

                w->burst_num = w->burst_left = tab[w->pattern_idx];
                if (mars_fl_slot_try(w, (int)(RAND_FLOAT_A() * 16.0f))) {
                    w->at_timer = (int)(CMD_FLOWER_AT_T * FPS);
                    w->pattern_idx++;
                    w->attack_st++;
                }
            }
        }
        break;
    case 2:
        if (--w->at_timer <= 0) {
            if (mars_fl_slot_try(w, w->last_slot))
                w->at_timer = (int)(CMD_FLOWER_AT_T * FPS);
            else
                w->attack_st = 1;
        }
        break;
    }
}

/* 0x00cab970 */
void mars_fl_break_step(MarsFlowerWork *w)
{
    Vec3 pos;
    int ang[3];
    int i;
    MarsFlSlot *slot;

    for (i = 0, slot = w->slot; i < 16; i++, slot++) {
        if (slot->break_timer && --slot->break_timer <= 0) {
            if (slot->break_cnt < 7)
                slot->break_timer = (int)(mars_fl_break_sec * FPS);
            slot->depth = 10 - slot->break_cnt;
            mars_pose_cb(w->boss, &pos, ang);
            ulMatrixPush();
            matrix_unit_set(0);
            MTX_TRANSF(pos.x, pos.y, pos.z);
            mars_fl_rotate_yxz(ang);
            vu0_matrix_rotate_z(ulMatrixCur, (int)DEG_TO_ANG((float)i * 360.0f * 0.0625f));
            vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, -mars_fl_ofs_z);
            vu0_matrix_rotate_x(ulMatrixCur, slot->sway_ang);
            vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, mars_fl_ofs_z);
            tree_node_pose_here((WireObj *)slot->part.root, (void *)slot->part.clip, &pos, ang, 0, slot->depth, slot->part.frame);
            cm_wire_break_simple(w->boss, 1, &w->break_parts[slot->break_cnt % 7], &pos, 0, ang, cmFWcol);
            cm_circle_create(w->boss, &pos, mars_fl_halo_col, mars_fl_halo_size, mars_fl_halo_fade, mars_fl_halo_sec);
            ulMatrixPop();
            slot->break_cnt++;
        }
    }
}

/* 0x00cabbb8 */
void mars_fl_exec(MarsFlowerWork *w)
{
    int n;

    for (n = enemy_status_get_ticks(); n != 0; n--) {
        if (w->start_wait) {
            if (--w->start_wait <= 0)
                w->attack_on = 1;
        }
        if (w->attack_on)
            mars_fl_attack_step(w);
        mars_fl_fire(w);
    }
    mars_fl_pose(w);
    mars_fl_hit_check(w);
    mars_fl_break_step(w);
    if (w->retract)
        w->ofs_z *= 0.98f;
}

/* 0x00cabc78 */
void mars_fl_release(MarsFlowerWork *w)
{
    mars_fl_locks_set(w, 0);
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)mars_flower_vardef);
}

/* steer(cast) */
/* 0x00cabca8 */
void mars_fl_disp(char *w)
{
    MarsFlowerWork *fl = (MarsFlowerWork *)w;
    float rgba[4];
    unsigned int col[2];
    Vec3 pos;
    int ang[3];
    int bang[3];
    Vec3 bpos;
    float rgba_clear[4];
    int save;
    char *slot;
    Vec3 *fl_pos;
    void *fog;
    WireObj *obj_b;
    float alpha;
    int i, j = 4096;

    if (!mars_wait_done((int)fl->boss))
        return;
    save = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    rgba[0] = mars_fog_progress(fl->boss, 1);
    rgba[1] = rgba[2] = rgba[3] = 0.0f;
    draw_set_override_color(rgba);
    alpha = rgba[0] + 1.0f;
    col[0] = (cmFlWireCol1[0] & 0xffffff) | ((int)(color_get_a(cmFlWireCol1[0]) * alpha) + 1) >> 1 << 24;
    col[1] = (cmFlWireCol1[1] & 0xffffff) | ((int)(color_get_a(cmFlWireCol1[1]) * alpha) + 1) >> 1 << 24;
    ulMatrixPush();
    mars_pose_cb(fl->boss, &bpos, bang);
    MTX_TRANS(&bpos);
    mars_fl_rotate_yxz(bang);
    mars_get_ref_pose(fl->boss, &pos, ang);
    MTX_TRANS(&pos);
    mars_fl_rotate_yxz(ang);
    memset(rgba_clear, 0, 16);
    ulMatrixPush();
    vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, -75.0f);
    draw_set_override_color(rgba_clear);
    dc_object_draw(fl->hub);
    draw_set_override_color(rgba);
    dc_object_draw(fl->hub_tinted);
    ulMatrixPop();
    vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, fl->ofs_z);
    for (i = 0, slot = w + 64; i < 16; i++, slot += 576) {
        tree_hide_depth((WireObj *)(fl->depth_part)->root, *(int *)(slot + 24));
        {
            WireObj *obj = dc_object_get_compiled(fl->depth_part);
            if (obj)
                tree_hide_depth(obj, *(int *)(slot + 24));
        }
        ulMatrixPush();
        vu0_matrix_rotate_z(ulMatrixCur, i * j);
        if (*(int *)slot) {
            vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, -mars_fl_ofs_z);
            vu0_matrix_rotate_x(ulMatrixCur, *(int *)(slot + 128));
            vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, mars_fl_ofs_z);
#ifdef VERSION_US
            if (dc_object_get_compiled((struct DcObjectCompiled *)(slot + 144)) && dc_object_get_compiled(fl->pair_part)
                && dc_object_get_compiled(fl->depth_part) && dc_object_get_wire(fl->wire_part)
                && modeltest_compile_obj) {
                float old = line_width_set(2.0f);
                WireObj *obj_a, *obj_c;
                void *wire;

                draw_set_color(color_lerp(col[0], col[1], beat_curve(1)));
                obj_a = dc_object_get_compiled((struct DcObjectCompiled *)(slot + 144));
                obj_b = dc_object_get_compiled(fl->pair_part);
                obj_c = dc_object_get_compiled(fl->depth_part);
                wire = dc_object_get_wire(fl->wire_part);
                tree_pair_draw_posed_culled(obj_a, obj_b, obj_c, wire, *(void **)(slot + 152), 0, (Vec3 *)(slot + 304),
                                      *(float *)(slot + 204));
                line_width_set(old);
            } else {
                tree_pair_draw_posed((WireObj *)((ModelPart *)(slot + 144))->root, (WireObj *)(fl->pair_part)->root, *(void **)(slot + 152), 0, (Vec3 *)(slot + 304), *(float *)(slot + 204));
                if (*(int *)(slot + 24) >= 3) {
                    dc_object_set_clip(fl->depth_part, *(struct DcClip **)(slot + 152));
                    model_object_draw(fl->depth_part, *(float *)(slot + 204));
                }
                ((void (*)(ModelPart *, int, float, int, int, float))part_draw_beat_motion)(*(ModelPart **)(w + 40), *(int *)(slot + 236), *(float *)(slot + 204), col[0], col[1], 2.0f);
            }
#else
            if (dc_object_get_compiled((ModelPart *)(slot + 144)) && dc_object_get_compiled(fl->pair_part) && modeltest_compile_obj) {
                WireObj *obj_a = dc_object_get_compiled((ModelPart *)(slot + 144));
                obj_b = dc_object_get_compiled(fl->pair_part);
                tree_pair_draw_posed_culled(obj_a, obj_b, *(void **)(slot + 152), 0, (Vec3 *)(slot + 304), *(float *)(slot + 204));
            } else {
                tree_pair_draw_posed((WireObj *)((ModelPart *)(slot + 144))->root, (WireObj *)(fl->pair_part)->root, *(void **)(slot + 152), 0, (Vec3 *)(slot + 304), *(float *)(slot + 204));
            }
            if (*(int *)(slot + 24) >= 3) {
                dc_object_set_clip(fl->depth_part, *(int *)(slot + 152));
                model_object_draw(fl->depth_part, *(float *)(slot + 204));
            }
            part_draw_beat_motion(fl->wire_part, *(int *)(slot + 236), *(float *)(slot + 204), col[0], col[1], 2.0f);
#endif
            vu0_matrix_mul(ulMatrixCur, ulMatrixCur, (float *)(slot + 240));
            vu0_matrix_rotate_y(ulMatrixCur, ANG_90);
            vu0_matrix_rotate_x(ulMatrixCur, -8192);
            vu0_matrix_rotate_z(ulMatrixCur, -ANG_180);
            model_object_draw(fl->tip_part, *(float *)(slot + 556));
        } else {
            vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, -mars_fl_ofs_z);
            vu0_matrix_rotate_x(ulMatrixCur, *(int *)(slot + 128));
            vu0_matrix_translate(ulMatrixCur, 0.0f, 0.0f, mars_fl_ofs_z);
            model_object_draw((struct DcObjectTreeView *)(slot + 144), *(float *)(slot + 204));
            if (*(int *)(slot + 24) >= 3) {
                dc_object_set_clip(fl->depth_part, *(struct DcClip **)(slot + 152));
                model_object_draw(fl->depth_part, *(float *)(slot + 204));
            }
            ((void (*)(ModelPart *, int, float, int, int, float))part_draw_beat_motion)(*(ModelPart **)(w + 40), *(int *)(slot + 236), *(float *)(slot + 204), col[0], col[1], 2.0f);
        }
        ulMatrixPop();
    }
    ulMatrixPop();
    fog = fog_set_enable(0);
    for (i = 0, slot = w + 64; i < 16; i++, slot += 576) {
        fl_pos = (Vec3 *)(slot + 32);
        if (*(int *)(slot + 12) && *(int *)slot) {
            ulMatrixPush();
            do {
                vu0_matrix_translate(ulMatrixCur, fl_pos->x, fl_pos->y, fl_pos->z);
            } while (0);
            ulMatrixUnitRotation((float *)ulMatrixCur);
            vu0_matrix_scale(ulMatrixCur, mars_fl_scl, mars_fl_scl, mars_fl_scl);
            model_object_draw(fl->fire_part, *(float *)((char *)fl->fire_part + 64) * (*(float *)(slot + 556) / *(float *)(slot + 560)));
            ulMatrixPop();
        }
    }
    fog_set_enable(fog);
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(save);
}

/* 0x00cac2b0 */
MarsFlowerWork *mars_fl_create(void *parent, int *src, int start_mot, int *mask, int mode)
{
    MarsFlowerCreateView *w = task_create_inherit(parent, &mars_fl_desc, 9344); /* steer(alias) */

    w->cfg[0] = src[1];
    w->cfg[1] = src[22];
    w->mask = mask;
    w->cfg[3] = src[30];
    w->boss = parent;
    w->at_timer = (int)(CMD_FLOWER_AT_ST * FPS);
    w->cfg[4] = src[31];
    w->cfg[5] = src[17];
    w->cfg[6] = src[18];
    w->cfg[7] = src[5];
    w->cfg[8] = src[2];
    w->cfg[2] = src[35];
    w->cfg[9] = src[27];
    w->cfg[10] = src[6];
    w->cfg[11] = src[7];
    w->ofs_z = mars_fl_decay_init;
    mars_fl_slots_copy_part((MarsFlowerWork *)w);
    mars_fl_locks_set((MarsFlowerWork *)w, 1);
    if (start_mot)
        mars_fl_slot_motion(w, 0, -4, 0);
    if (mode == 2)
        w->spawned = -1;
    mars_flower_vars_group = gw_varedit_add(&gw_varedit_root, 0, mars_flower_vardef);
    gw_varedit_add_table(&gw_varedit_root, mars_flower_vars_group, mars_flower_vardefs);
    return (MarsFlowerWork *)w;
}

/* 0x00cac428 */
int mars_fl_any_busy(MarsFlowerWork *w)
{
    return mars_fl_slots_state(w) != 0;
}

/* 0x00cac448 */
void mars_fl_command(MarsFlowerWork *w, int cmd)
{
    MarsFlSlotView *slots;
    int i;

    if (!w)
        return;
    switch (cmd) {
    case 5:
        w->start_wait = (int)(CMD_FIRST_WAIT * FPS);
        break;
    case 4:
        mars_fl_slot_motion(w, 0, -5, 0);
        break;
    case 3:
        w->attack_on = 1;
        break;
    case 2:
        mars_fl_slot_motion(w, 0, -3, 0);
        break;
    case 1:
        w->retract = 1;
        mars_fl_slot_motion(w, 0, -1, 0);
        cm_sc_blur_create(w, 2.0f, 200.0f, 1.0f, 2.0f);
        break;
    case 0:
        mars_fl_slot_motion(w, 0, -2, 0);
        w->attack_on = 0;
        slots = (MarsFlSlotView *)w;
        for (i = 0; i < 16; i++) {
            slots[i].s.stop = 2;
            if (*(int *)((char *)&slots[i] + 616)) { /* steer(cast): the slot's fired flag, past the view */
                boss_se_play(26, (int)&slots[i].s.lock, 0);
                *(int *)((char *)&slots[i] + 616) = 0;
            }
        }
        cm_sc_blur_create(w, 2.0f, 200.0f, 0.0f, 3.0f);
        break;
    case -1:
        FINISH_IF(w);
        break;
    }
}

/* 0x00cac5f0 */
void mars_fl_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}

/* steer(cast) */
/* 0x00cac640 */
void mars_score_vars_add(void)
{
    lock_table_set(mars_score_lock_defs, 9);
    mars_score_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, mars_score_vardef);
    gw_varedit_add_table(&gw_varedit_root, mars_score_vars_group, mars_score_vardefs);
}

/* 0x00cac6a0 */
void mars_score_vars_remove(void)
{
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)mars_score_vardef);
}

/* 0x00cac6b8 */
void mars_map_exec(int *w)
{
    float t;

    if (w[2] == 0)
        return;
    w[2]--;
    t = (float)w[2] / (float)w[3];
    game_pass_count_spd_set(mars_map_pass_spd[0] * t + mars_map_pass_spd[1] * (1.0f - t));
}

/* 0x00cac720 */
void mars_map_release(void)
{
    player_cell_move_clear();
    player_swap_fullrange(1);
}

/* steer(cast) */
/* 0x00cac740 */
int mars_map_create(char *w, int arg)
{
    void **map;
    int map_task, i;
    int *def;
    int *cell_from, *cell_to, *pass_a, *pass_b, *pass_c, *pass_d;

    map = task_create_sibling_inherit(w, &mars_map_desc, 20);
    def = mars_map_model_def;
    map_task = ((int (*)(void *, void *))map_task_create_mode1)(map, def);
    ((int *)map)[4] = map_task;
    map_set_sort_type((void *)map_task, 1);
    for (i = 0; i < def[1]; i++)
        map_part_set_color(map_task, i, mars_map_part_value);
    cell_from = pnb_get_entry(*map, 84);
    cell_to = pnb_get_entry(*map, 84);
    pass_a = pnb_get_entry(*map, 83);
    pass_b = pnb_get_entry(*map, 83);
    pass_c = pnb_get_entry(*map, 84);
    pass_d = pnb_get_entry(*map, 84);
    player_cell_move_start(*cell_from, *cell_to, *pass_a, *pass_b, *pass_c, *pass_d);
    player_swap_fullrange(0);
    ((int *)map)[2] = ((int *)map)[3] = 240;
    return (int)map;
}

/* 0x00cac888 */
void mars_map_set_visible(char *w, int on)
{
    void *map = *(void **)(w + 16);

    if (on) {
        if (map)
            TASK_OF(map)->flags &= ~TASK_F_SUSPENDED;
    } else {
        do {
            if (map)
                TASK_OF(map)->flags |= TASK_F_SUSPENDED;
        } while (0);
    }
}

/* 0x00cac8d0 */
void boss_se_play(int id, int lock, int pos)
{
    se_table_play(0, &se_table, id, lock, pos);
}

/* 0x00cac8f0 */
void boss_vol_play(int id, int arg)
{
    vol_table_play(0, vol_table, id, arg);
}

/* 0x00cac908 */
void boss_vol_stop(int id)
{
    vol_table_stop(0, vol_table, id);
}

/* 0x00cac920 */
void boss_bgm_exit_cb(BgmMan *bgm)
{
    se_table_stop(&se_table);
}

/* 0x00cac930 */
BgmMan *boss_bgm_start(void *parent)
{
    se_table_reset(&se_table);
    return cm_bgm_sound_man_create(parent, boss_bgm_exit_cb);
}

/* 0x00cac970 */
void boss_adx_request(void)
{
    cm_bgm_sound_man_add_file(0, adx_files[0], (int)adx_files[1]);
}

/* 0x00cac988 */
void boss_bgm_switch(int cb, int cb_arg, int idx)
{
    cm_bgm_sound_sub_start(0, cb, cb_arg, 0, 0, idx, idx);
}

/* 0x00cac9b0 */
void boss_bgm_seq_get(int i, int *seq_a, int *seq_b)
{
    if (seq_a)
        *seq_a = bgm_seq_table[i * 2];
    if (seq_b)
        *seq_b = bgm_seq_table[i * 2 + 1];
}

/* 0x00cac9f8 */
void boss_pcm_load(int i)
{
    boss_stream_load((int)pcm_files[i], 1);
}

/* 0x00caca10 */
void boss_pcm_start(void)
{
    boss_stream_start();
}
