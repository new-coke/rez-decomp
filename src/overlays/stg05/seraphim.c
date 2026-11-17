/*
 * Seraphim, Area 5 enemy: winged craft around a core with two eggs. Brakes and stops, opens its
 * wings and launches missiles (seraphim_missile), then closes. Wing parts and eggs are lock-on
 * targets that break off when shot.
 */

#include "stg05.h"
#include "mathdef.h"
#include "anim.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "enemy_task.h"
#include "fog.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "lockon.h"
#include "math_util.h"
#include "model.h"
#include "model_wire_break.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "sound_midise.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "seraphim.h"
#include "en_seraphim_dmg.h"
#include "en_vts_cmd_dmg_eff.h"
#include "open_eff.h"
#include "seraphim_missile.h"
#include "../common/en_shatter.h"
#include "../common/enemy_vars.h"

extern int seraphim_init_arg;
extern void (*seraphim_exec_table[])(void *o, TypeEnemyWork *w);

/* group name, label, 51 values */
typedef struct SeraphimSet {
    char *name;
    char *label;                /* 0x04 */
    int v[51];                  /* 0x08 */
} SeraphimSet;

extern SeraphimSet seraphim_types[2];
extern VarDef seraphim_vardef;
extern VarDef seraphim_vardefs[51];

struct SeraphimNodeCamView {
    char pad_00[1328];
    Vec3 pos;                   /* 0x530 */
    int ang[3];                 /* 0x53c */
};

/* 88 bytes */
struct SeraphimPartRec {
    LockObj lock;
    Vec3 pos;                   /* 0x40 */
    int ang[3];                 /* 0x4c */
};

/* 24 bytes: world position, camera angles */
struct SeraphimNode {
    Vec3 pos;
    int ang[3];                 /* 0x0c */
};

/* 44 bytes: launcher pose and its SeraphimMissile */
typedef struct SeraphimMissileSlot {
    Vec3 pos;
    int ang[3];                 /* 0x0c */
    void *obj;                  /* 0x18: the SeraphimMissile */
    int t;                      /* 0x1c: frames, counted every frame by seraphim_update_missiles */
    int killed;                 /* 0x20: set by the missile when shot down */
    int launched;               /* 0x24: set by the missile when launched */
    int done;                   /* 0x28: counted off once the missile is done */
} SeraphimMissileSlot;

/* 3712 bytes */
struct SeraphimWork {
    struct SeraphimRec *type;
    int state;                  /* 0x04: indexes seraphim_states */
    int count;                  /* 0x08: frames of the current state */
    int idx;                    /* 0x0c: the record */
    Vec3 hitv;                  /* 0x10: the break debris velocity */
    LockObj lock;                  /* 0x1c: the egg core */
    float speed;                /* 0x5c: approach speed per frame (placement vector length) */
    Vec3 hitpos;                /* 0x60: hit effect position, 300 below the object */
    Vec3 node_pos;                   /* 0x6c: a model node position (seraphim_node_cb) */
    int node_ang[3];                 /* 0x78: and its angles */
    ModelPart mdl[13];             /* 0x84: [11] the shell, [12] the path model, [1] the wings frame */
    Vec3 pos;                   /* 0x530: the path node position (seraphim_root_node_cb) */
    int ang[3];                 /* 0x53c */
    SeraphimNode wa[7];             /* 0x548: wing parts, first side */
    SeraphimNode wb[7];             /* 0x5f0: wing parts, second side */
    Vec3 npos[15];              /* 0x698: wing pose offsets from the open pose */
    int nang[15][3];            /* 0x74c */
    Vec3 mpos[15];              /* 0x800: and of the mirrored side */
    int mang[15][3];            /* 0x8b4 */
    struct SeraphimPartRec recs[8];       /* 0x968: [0..1] the eggs, [2..4]/[5..7] the wing parts */
    SeraphimMissileSlot ta[4];              /* 0xc28: missiles, first side */
    SeraphimMissileSlot tb[4];              /* 0xcd8: missiles, second side */
    int wait;                   /* 0xd88: missiles not yet done */
    int last_shot_down;                      /* 0xd8c: the last missile done was shot down */
    int last_wing_alive;                      /* 0xd90: a wing part was alive then */
    int wing_alive;                      /* 0xd94: a wing part is alive */
    float alpha;                /* 0xd98: fades in over alpha_sec, at most 1 */
    int roll;                   /* 0xd9c */
    float look;                 /* 0xda0: weight of the turn towards the player */
    float swing;                /* 0xda4: phase of the look-over swing */
    float swing0;               /* 0xda8: swing when the close began */
    int opened;                 /* 0xdac */
    int stage;                  /* 0xdb0: rounds with all wing parts down (0..2) */
    ModelPart mdl2[2];             /* 0xdb4: the shell wire, the wing part halo */
    float halo;                 /* 0xe6c: halo scale */
    int early;                  /* 0xe70: closing from the wing pose */
    float early_t;              /* 0xe74 */
    int breaks;                 /* 0xe78: wing parts destroyed */
    int eggs;                   /* 0xe7c: eggs destroyed */
};

extern void (*seraphim_disp_table[])(void *o, void *w);

struct SeraphimNodeArg {
    Vec3 *pos;
    int *ang;                   /* 0x04 */
    SeraphimPartRec *cur;                 /* 0x08 */
};

/* seraphim_mirror_node_cb state */
struct SeraphimMirrorCtx {
    Vec3 *pos;
    int *ang;                   /* 0x04 */
    Vec3 *out;                  /* 0x08 */
    int *aout;                  /* 0x0c */
};

/* seraphim_draw_part: part records, four models */
struct SeraphimPartDraw {
    SeraphimPartRec *recs;
    ModelPart *alive_a;                  /* 0x04: alive, first half */
    ModelPart *alive_b;                  /* 0x08: alive, second half */
    ModelPart *dead_a;                  /* 0x0c: destroyed */
    ModelPart *dead_b;                  /* 0x10: destroyed, second half */
    char pad_14[4];
    ModelPart *halo;               /* 0x18 */
};

extern Vec3 seraphim_wing_nodes[];
extern Vec3 seraphim_wing_nodes_mirror[];

/* 212 bytes; names in seraphim_vardefs */
typedef struct SeraphimRec {
    char *name;
    char *label;                /* 0x04 */
    int hp;                     /* 0x08 */
    int same;                   /* 0x0c */
    int hp_egg;                 /* 0x10 */
    int same_lock_egg;          /* 0x14 */
    int score;                  /* 0x18 */
    float hp_reduce;            /* 0x1c */
    float crash_r;              /* 0x20 */
    float crash_damage;         /* 0x24 */
    float alpha_sec;            /* 0x28 */
    int roll_spd;               /* 0x2c */
    float roll_pow;             /* 0x30 */
    float break_sec;            /* 0x34 */
    float stop_sec;             /* 0x38 */
    int roll_back;              /* 0x3c */
    float roll_back_pow;        /* 0x40 */
    float egg_break_sec;        /* 0x44 */
    float missile_appear_sec;   /* 0x48 */
    float open_wait_sec;        /* 0x4c */
    float open_sec;             /* 0x50 */
    int look_player_over_ang;   /* 0x54 */
    int look_player_over_spd;   /* 0x58 */
    float open_stop_sec;        /* 0x5c */
    float missile_motion_sec;   /* 0x60 */
    float shot_sec;             /* 0x64 */
    float close_wait_sec;       /* 0x68 */
    float close_sec;            /* 0x6c */
    float missile_dif_sec;      /* 0x70 */
    int beat;                   /* 0x74 */
    unsigned int wing_wire_col; /* 0x78 */
    unsigned int wing_wire_col2; /* 0x7c */
    float fall_wait_sec_wing;   /* 0x80 */
    float fall_wait_sec_body;   /* 0x84 */
    float fall_range_xz;        /* 0x88 */
    float fall_y;               /* 0x8c */
    int fall_num;               /* 0x90 */
    int fall_num_range;         /* 0x94 */
    float wire_break_vec_start; /* 0x98 */
    float wire_break_magni;     /* 0x9c */
    float wire_break_vec_ang;   /* 0xa0 */
    float wire_break_sec;       /* 0xa4 */
    int wire;                   /* 0xa8 */
    int wire_linear;            /* 0xac */
    unsigned int wire_col;      /* 0xb0 */
    unsigned int wire_col2;     /* 0xb4 */
    float wire_near;            /* 0xb8 */
    float wire_far;             /* 0xbc */
    float wire_near_rate;       /* 0xc0 */
    float wire_far_rate;        /* 0xc4 */
    int wire_beat;              /* 0xc8 */
    float early_swing_close_pow; /* 0xcc */
    float early_swing_close_rate; /* 0xd0 */
} SeraphimRec;

extern void (*seraphim_states[])(void *o, SeraphimWork *w);

/* seraphim_wing_node_cb state: one side's node poses, optional blend */
struct SeraphimWalk {
    SeraphimNode *node;             /* nodes of flag 0x4000000 */
    SeraphimPartRec *rec;                 /* 0x04: nodes of flag 0x1000000 */
    SeraphimMissileSlot *sub;               /* 0x08: nodes of flag 0x2000000 */
    Vec3 *pos;                  /* 0x0c: the evaluated node positions */
    int *ang;                   /* 0x10: and angles */
    Vec3 *pos2;                 /* 0x14: offsets blended in with weight 1 - k, or 0 */
    int *ang2;                  /* 0x18 */
    float k;                    /* 0x1c */
};

/* 52 bytes, copied by wire_break_init */
typedef struct SeraphimWireBreak {
    ModelPart *model;
    int frames;                 /* 0x04 */
    Vec3 v;                     /* 0x08 */
    float magni;                /* 0x14 */
    float start;                /* 0x18 */
    float ang;                  /* 0x1c */
    unsigned int color;         /* 0x20 */
    int h;                      /* 0x24 */
    int mode0;                  /* 0x28 */
    int mode1;                  /* 0x2c */
    char pad_30[4];
} SeraphimWireBreak;

#define SEXT16(v) ((v) & 0x8000 ? (v) | 0xffff0000 : (v) & 0xffff)

/* en_vts_cmd_dmg_eff argument */
typedef struct SeraphimHitArg {
    int kind;
    Vec3 *pos;                  /* 0x04 */
} SeraphimHitArg;

extern char seraphim_lock_name[];
extern char seraphim_name[];
extern char seraphim_funcs[];

/* 0x00b0dd88 */
void seraphim_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&seraphim_vardef);
    for (i = 0; i < 2; i++) {
        def = seraphim_vardefs;
        val = seraphim_types[i].v;
        seraphim_vardef.name = seraphim_types[i].name;
        seraphim_vardef.fmt = seraphim_types[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&seraphim_vardef);
        for (j = 0; j < 51; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)seraphim_vardefs);
    }
}

/* 0x00b0dea0 */
void seraphim_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b0dea8 */
int seraphim_get_init_arg(void)
{
    return seraphim_init_arg;
}

/* 0x00b0deb8 */
void seraphim_models_load(void)
{
    seraphim_record_wing_nodes();
}

/* 0x00b0dec0 */
void seraphim_models_free(void)
{
    seraphim_wing_nodes_free();
}

/* 0x00b0dec8 */
int seraphim_vars_init(int *prm)
{
    seraphim_init_arg = *prm;
    seraphim_vars_register();
    return 0;
}

/* 0x00b0def0 */
int seraphim_vars_release(void)
{
    seraphim_vars_unregister();
    return 0;
}

/* 0x00b0df10 */
int seraphim_marker_draw(int obj, void *place)
{
    int *mdl;

    ulGxSetTextureList(stage_textures);
    mdl = pnb_get_entry(stage_models, 152);
    return obj_draw_at(obj, place, (void *)*mdl);
}

/* 0x00b0df68 */
int seraphim_part_node_cb(unsigned int *n, int idx, SeraphimPartRec **cur)
{
    if (*n & 0x1000000) {
        ulMatrixGetTranslation((float *)ulMatrixCur, &(*cur)->pos.x);
        (*cur)++;
    }
    return 0;
}

/* 0x00b0dfc0 */
void seraphim_pose_parts(void *obj, SeraphimWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    SeraphimPartRec *cur = w->recs;

    dc_object_set_callback(gfx, seraphim_part_node_cb, &cur);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    seraphim_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, 0);
}

/* steer(cast) */
/* 0x00b0e070 */
int seraphim_node_cb(unsigned int *n, int idx, SeraphimNodeArg *prm)
{
    if (n[0] & 0x2000000) {
        ulMatrixGetTranslation((float *)ulMatrixCur, &prm->pos->x);
        model_camera_angles(prm->ang);
    }
    if (n[0] & 0x1000000) {
        ulMatrixGetTranslation((float *)ulMatrixCur, &prm->cur->pos.x);
        ((void (*)(int *))model_camera_angles)(prm->cur->ang);
        prm->cur++;
    }
    return 0;
}

/* 0x00b0e108 */
void seraphim_pose_model(void *obj, SeraphimWork *w)
{
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    SeraphimNodeArg prm;

    prm.pos = &w->node_pos;
    prm.ang = w->node_ang;
    prm.cur = w->recs;
    dc_object_set_callback(gfx, seraphim_node_cb, &prm);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    seraphim_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(gfx);
    ulMatrixPop();
    dc_object_set_callback(gfx, 0, 0);
}

/* 0x00b0e1c8 */
void seraphim_mirror_node_cb(unsigned int *n, SeraphimMirrorCtx *ctx)
{
    do {
        v3_copy(ctx->out, ctx->pos);
        ulMatrixPush();
        matrix_unit_set(0);
        seraphim_rotate_zyx(ctx->ang);
        model_camera_angles(ctx->ang);
        i3_copy(ctx->aout, ctx->ang);
        ulMatrixPop();
        ctx->out->x = -ctx->out->x;
        ctx->aout[2] = -ctx->aout[2];
        ctx->aout[1] = -ctx->aout[1];
        ctx->pos++;
        ctx->out++;
        ctx->ang += 3;
        ctx->aout += 3;
        if (((unsigned int **)n)[11])
            seraphim_mirror_node_cb(((unsigned int **)n)[11], ctx);
        n = ((unsigned int **)n)[12];
    } while (n);
}

/* 0x00b0e2d0 */
void seraphim_eval_nodes_mirrored(ModelPart *part, Vec3 *src, Vec3 *dst)
{
    void *arg[3];
    int buf[48];
    SeraphimMirrorCtx ctx;

    arg[0] = src;
    arg[1] = src + 15;
    arg[2] = buf;
    anim_eval_hierarchy(part->root, part->clip, arg, part->frame);
    ctx.pos = src;
    ctx.ang = (int *)(src + 15);
    ctx.out = dst;
    ctx.aout = (int *)(dst + 15);
    seraphim_mirror_node_cb((unsigned int *)part->root, &ctx);
}

/* steer(cast) */
/* 0x00b0e358 */
void seraphim_wing_node_cb(unsigned int *n, SeraphimWalk *ctx)
{
    Vec3 p;
    int dang[3];
    float k;

    do {
        ulMatrixPush();
        if (!ctx->pos2) {
            do {
                Vec3 *p_ = ctx->pos;
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            seraphim_rotate_yxz(ctx->ang);
        } else {
            k = 1.0f - ctx->k;
            p.x = ctx->pos2->x * k;
            dang[0] = ctx->ang2[0] * k;
            p.y = ctx->pos2->y * k;
            dang[1] = ctx->ang2[1] * k;
            p.z = ctx->pos2->z * k;
            dang[2] = ctx->ang2[2] * k;
            p.x += ctx->pos->x;
            dang[0] += ctx->ang[0];
            p.y += ctx->pos->y;
            dang[1] += ctx->ang[1];
            p.z += ctx->pos->z;
            dang[2] += ctx->ang[2];
            do {
                Vec3 *p_ = &p;
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            seraphim_rotate_yxz(dang);
            ctx->pos2++;
            ctx->ang2 += 3;
        }
        if (n[0] & 0x4000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, &ctx->node->pos.x);
            ulMatrixPush();
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            model_camera_angles(ctx->node->ang);
            ulMatrixPop();
            ctx->node++;
        }
        if (n[0] & 0x1000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, &ctx->rec->pos.x);
            ((void (*)(int *))model_camera_angles)(ctx->rec->ang);
            ctx->rec++;
        }
        if (n[0] & 0x2000000) {
            ulMatrixGetTranslation((float *)ulMatrixCur, &ctx->sub->pos.x);
            ulMatrixPush();
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            model_camera_angles(ctx->sub->ang);
            ulMatrixPop();
            ctx->sub++;
        }
        ctx->pos++;
        ctx->ang += 3;
        if (n[11])
            seraphim_wing_node_cb((unsigned int *)n[11], ctx);
        ulMatrixPop();
        n = (unsigned int *)n[12];
    } while (n);
}

/* 0x00b0e5f8 */
void seraphim_update_wings(void *obj, SeraphimWork *w)
{
    SeraphimWalk ctx;
    Vec3 src[30];
    Vec3 dst[30];
    ModelPart *part = &w->mdl[1];

    seraphim_eval_nodes_mirrored(part, src, dst);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &w->node_pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    seraphim_rotate_yxz(w->node_ang);
    ctx.pos = src;
    ctx.ang = (int *)(src + 15);
    ctx.node = w->wa;
    ctx.rec = &w->recs[2];
    ctx.sub = w->ta;
    if (!w->early)
        ctx.pos2 = 0;
    else {
        ctx.pos2 = w->npos;
        ctx.ang2 = w->nang[0];
        ctx.k = w->early_t;
    }
    seraphim_wing_node_cb((unsigned int *)part->root, &ctx);
    ctx.pos = dst;
    ctx.ang = (int *)(dst + 15);
    ctx.node = w->wb;
    ctx.rec = &w->recs[5];
    ctx.sub = w->tb;
    if (!w->early)
        ctx.pos2 = 0;
    else {
        ctx.pos2 = w->mpos;
        ctx.ang2 = w->mang[0];
        ctx.k = w->early_t;
    }
    seraphim_wing_node_cb((unsigned int *)part->root, &ctx);
    ulMatrixPop();
}

/* 0x00b0e738 */
int seraphim_part_record(int i, int *k)
{
    int half;

    switch (i) {
    case 0:
    case 1:
        *k = 0;
        half = 0;
        break;
    case 2:
        *k = 0;
        half = 1;
        break;
    case 3:
        *k = 1;
        half = 0;
        break;
    case 4:
        *k = 1;
        half = 1;
        break;
    case 5:
        *k = 2;
        half = 0;
        break;
    default:
        *k = 2;
        half = 1;
        break;
    }
    return half;

}

/* 0x00b0e7a8 */
void seraphim_draw_part(SeraphimPartDraw *p, int i)
{
    int k;
    int half = seraphim_part_record(i, &k);
    SeraphimPartRec *rec = &p->recs[k];

    if (rec->lock.life > 0)
        dc_object_draw(half ? p->alive_a : p->alive_b);
    else {
        draw_set_blend(4, 5);
        dc_object_draw(p->dead_a);
        if (!half)
            dc_object_draw(p->dead_b);
    }
}

/* 0x00b0e840 */
int seraphim_root_node_cb(unsigned int *n, int idx, SeraphimNodeCamView *w)
{
    ulMatrixGetTranslation((float *)ulMatrixCur, &w->pos.x);
    model_camera_angles(w->ang);
    return 0;
}

/* 0x00b0e880 */
void seraphim_update_egg(void *obj, SeraphimWork *w)
{
    Vec3 p;
    int ang[3];
    Vec3 pl;
    float sc[2];
    ModelPart *part = &w->mdl[12];
    SeraphimRec *typ;
    float k, base, dx, dz;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_rot(obj, ang);
    dc_object_set_callback(part, seraphim_root_node_cb, w);
    ulMatrixPush();
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &p;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    seraphim_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(part);
    ulMatrixPop();
    dc_object_set_callback(part, 0, 0);
    if (!(fabsf(w->look) < FLT_EPSILON)) {
        player_get_pos(&pl);
        k = w->look;
        dx = pl.x - w->pos.x;
        dz = pl.z - w->pos.z;
        base = w->ang[1] * (1.0f - k);
        /* sign-extended or masked angle, scaled by k, plus base */
        w->ang[1] = (int)(base + (float)(((int)RAD_TO_ANG(atan2f(dx, dz)) + 0x8000) & 0x8000 ? (int)(((int)RAD_TO_ANG(atan2f(dx, dz)) + 0x8000) | 0xffff0000) : (((int)RAD_TO_ANG(atan2f(dx, dz)) + 0x8000) & 0xffff)) * k);
        {
            float swing = w->swing;

            ulSinCos(w->type->look_player_over_spd * swing, &sc[0], &sc[1]);
            w->ang[1] += w->type->look_player_over_ang * sc[0] * (1.0f - swing);
        }

    }
}

/* 0x00b0eaf0 */
int seraphim_next_state(void *obj, TypeEnemyWork *w, int n, int state)
{
    if (++w->frames > n) {
        w->state = state;
        w->frames = 1;
        return 1;
    }
    return 0;
}

/* 0x00b0eb20 */
void seraphim_record_wing_nodes(void)
{
    ModelPart part;
    int *root;
    int *clip;

    root = pnb_get_entry(stage_models, 165);
    clip = pnb_get_entry(stage_models, 166);
    dc_object_init(&part, *root, *clip, 0);
    dc_object_rebuild(&part);
    part.frame = part.frames;
    seraphim_eval_nodes_mirrored(&part, seraphim_wing_nodes, seraphim_wing_nodes_mirror);
    dc_object_release(&part);
}

/* 0x00b0eba8 */
void seraphim_wing_nodes_free(void)
{
}

static inline short rec88_hits(SeraphimPartRec *r)
{
    return r->lock.life;
}

/* 0x00b0ebb0 */
void seraphim_update_missiles(void *obj, SeraphimWork *w)
{
    SeraphimMissileSlot *sub_a;
    SeraphimMissileSlot *sub_b;
    int i;
    SeraphimPartRec *p = &w->recs[2], *rec_b = &w->recs[5];

    sub_a = w->ta;
    sub_b = w->tb;
    for (i = 0; i < 4; i++, sub_a++, sub_b++) {
        if (sub_a->obj && TASK_OF(sub_a->obj)->flags & TASK_F_END)
            sub_a->obj = 0;
        if (sub_b->obj && TASK_OF(sub_b->obj)->flags & TASK_F_END)
            sub_b->obj = 0;
    }
    w->wing_alive = 0;
    for (i = 0; i < 3; i++, p++, rec_b++) {
        if (rec88_hits(p) > 0 || rec88_hits(rec_b) > 0) {
            w->wing_alive = 1;
            break;
        }
    }
    sub_a = w->ta;
    sub_b = w->tb;
    for (i = 0; i < 4; i++, sub_a++, sub_b++) {
        if ((!sub_a->obj || sub_a->launched || sub_a->killed) && !sub_a->done) {
            sub_a->done = 1;
            if (--w->wait <= 0) {
                w->last_shot_down = sub_a->killed;
                w->last_wing_alive = w->wing_alive;
            }
        }
        if ((!sub_b->obj || sub_b->launched || sub_b->killed) && !sub_b->done) {
            sub_b->done = 1;
            if (--w->wait <= 0) {
                w->last_shot_down = sub_b->killed;
                w->last_wing_alive = w->wing_alive;
            }
        }
    }
}

/* steer(cast) */
/* 0x00b0ed38 */
void seraphim_read_wing_nodes(void *obj, SeraphimWork *w)
{
    ModelPart *part = &w->mdl[1];
    Vec3 *mpos = w->mpos;
    Vec3 *p = w->npos;
    int *nang, *mang;
    Vec3 *src, *msrc;
    int *sa, *ua;
    int i;

    seraphim_eval_nodes_mirrored(part, p, mpos);
    nang = w->nang[0];
    mang = w->mang[0];
    src = seraphim_wing_nodes;
    msrc = seraphim_wing_nodes_mirror;
    sa = (int *)(src + 15);
    ua = (int *)(msrc + 15);
    for (i = 0; i < 15; i++) {
        p->x -= src->x;
        nang[0] -= sa[0];
        p->y -= src->y;
        nang[1] -= sa[1];
        p->z -= src->z;
        nang[2] -= sa[2];
        src++;
        p++;
        mpos->x -= msrc->x;
        mpos->y -= msrc->y;
        mpos->z -= msrc->z;
        msrc++;
        mpos++;
        do {
            nang[0] = SEXT16(nang[0]);
            nang[1] = SEXT16(nang[1]);
            nang[2] = SEXT16(nang[2]);
        } while (0);
        sa += 3;
        nang += 3;
        mang[0] -= ua[0];
        mang[1] -= ua[1];
        mang[2] -= ua[2];
        do {
            mang[0] = SEXT16(mang[0]);
            mang[1] = SEXT16(mang[1]);
            mang[2] = SEXT16(mang[2]);
        } while (0);
        ua += 3;
        mang += 3;
    }
    dc_object_set_clip(part, *((struct DcClip **)pnb_get_entry(stage_models, 166)));
    w->mdl[1].frame = w->mdl[1].frames;
    seraphim_next_state(obj, (TypeEnemyWork *)w, 0, 10);
    w->early = 1;
    w->swing0 = w->swing;
    w->early_t = 0;
}

/* 0x00b0ef70 */
void seraphim_check_wing_nodes(void *obj, SeraphimWork *w)
{
    if (w->wait > 0)
        return;
    if (w->last_wing_alive) {
        if (w->wing_alive)
            return;
    } else if (!w->last_shot_down)
        return;
    seraphim_read_wing_nodes(obj, w);
}

/* steer(cast) */
/* 0x00b0efb8 */
void seraphim_init(void *obj, SeraphimWork *w, void *prm, int *k)
{
    Vec3 v;
    SeraphimPartRec *core = w->recs;
    SeraphimPartRec *rec_a = &w->recs[2];
    SeraphimPartRec *rec_b = &w->recs[5];
    SeraphimRec *typ;
    int i;

    w->idx = *k;
    w->type = typ = (SeraphimRec *)&seraphim_types[*k];
    do {
        void *m_ = stage_models, *t_ = stage_textures;
        ((ObjMT *)obj)->models = m_;
        ((ObjMT *)obj)->tex = t_;
    } while (0);
    dc_object_init(&w->mdl[11], *((struct DcNode **)pnb_get_entry(stage_models, 163)), *((struct DcClip **)pnb_get_entry(stage_models, 164)), 0);
    dc_object_rebuild(&w->mdl[11]);
    w->mdl[11].radius = 2600.0f;
    dc_object_init(&w->mdl[0], *((struct DcNode **)pnb_get_entry(stage_models, 158)), *((struct DcClip **)pnb_get_entry(stage_models, 159)), 0);
    dc_object_rebuild(&w->mdl[0]);
    dc_object_init(&w->mdl[1], *((struct DcNode **)pnb_get_entry(stage_models, 165)), *((struct DcClip **)pnb_get_entry(stage_models, 166)), 0);
    dc_object_init(&w->mdl[2], *((struct DcNode **)pnb_get_entry(stage_models, 168)), 0, 0);
    dc_object_rebuild(&w->mdl[2]);
    dc_object_init(&w->mdl[3], *((struct DcNode **)pnb_get_entry(stage_models, 171)), 0, 0);
    dc_object_rebuild(&w->mdl[3]);
    dc_object_init(&w->mdl[4], *((struct DcNode **)pnb_get_entry(stage_models, 170)), 0, 1280);
    dc_object_set_wire(&w->mdl[4], model_pnb_get_wire(((void **)obj)[0], 170));
    dc_object_init(&w->mdl[5], *((struct DcNode **)pnb_get_entry(stage_models, 172)), 0, 0);
    dc_object_rebuild(&w->mdl[5]);
    dc_object_init(&w->mdl[6], *((struct DcNode **)pnb_get_entry(stage_models, 165)), 0, 0);
    dc_object_init(&w->mdl[7], *((struct DcNode **)pnb_get_entry(stage_models, 174)), 0, 0);
    dc_object_rebuild(&w->mdl[7]);
    dc_object_init(&w->mdl[8], *((struct DcNode **)pnb_get_entry(stage_models, 177)), 0, 0);
    dc_object_rebuild(&w->mdl[8]);
    dc_object_init(&w->mdl[9], *((struct DcNode **)pnb_get_entry(stage_models, 176)), 0, 1280);
    dc_object_set_wire(&w->mdl[9], model_pnb_get_wire(((void **)obj)[0], 176));
    dc_object_init(&w->mdl[10], *((struct DcNode **)pnb_get_entry(stage_models, 178)), 0, 0);
    if (*k == 0) {
        dc_object_init(&w->mdl[12], *((struct DcNode **)pnb_get_entry(stage_models, 192)), *((struct DcClip **)pnb_get_entry(stage_models, 193)), 4096);
    } else {
        dc_object_init(&w->mdl[12], *((struct DcNode **)pnb_get_entry(stage_models, 194)), *((struct DcClip **)pnb_get_entry(stage_models, 195)), 4096);
    }
    ((ModelPartAnim *)&w->mdl[12])->time = ((ModelPartAnim *)&w->mdl[12])->length * RAND_FLOAT_A();
    dc_object_init(&w->mdl2[0], *((struct DcNode **)pnb_get_entry(stage_models, 197)), *((struct DcClip **)pnb_get_entry(stage_models, 198)), 0);
    dc_object_set_wire(&w->mdl2[0], model_pnb_get_wire(((void **)obj)[0], 197));
    dc_object_init(&w->mdl2[1], *((struct DcNode **)pnb_get_entry(stage_models, 199)), *((struct DcClip **)pnb_get_entry(stage_models, 200)), 0);
    dc_object_rebuild(&w->mdl2[1]);
    lockon_obj_init(&w->lock, obj, &w->pos, 0, seraphim_lock_name, enemy_task_get_status_slot(obj), typ->hp_egg, typ->same_lock_egg, 128);
    lockon_entry(&w->lock);
    for (i = 0; i < 2; i++, core++) {
        lockon_obj_init(&core->lock, obj, &core->pos, 0, seraphim_lock_name, enemy_task_get_status_slot(obj), typ->hp, typ->same, 128);
        lockon_entry(&core->lock);
        core->lock.flags &= ~LOCK_LOCKABLE;
    }
    for (i = 0; i < 3; i++, rec_a++, rec_b++) {
        lockon_obj_init(&rec_a->lock, obj, &rec_a->pos, 0, seraphim_lock_name, enemy_task_get_status_slot(obj), typ->hp, typ->same, 128);
        lockon_entry(&rec_a->lock);
        rec_a->lock.flags &= ~LOCK_LOCKABLE;
        lockon_obj_init(&rec_b->lock, obj, &rec_b->pos, 0, seraphim_lock_name, enemy_task_get_status_slot(obj), typ->hp, typ->same, 128);
        lockon_entry(&rec_b->lock);
        rec_b->lock.flags &= ~LOCK_LOCKABLE;
    }
    enemy_task_set_status_pos(obj, &w->pos);
    {
        SeraphimMissileSlot *sub_a = w->ta;
        SeraphimMissileSlot *sub_b = w->tb;

        for (i = 0; i < 4; i++, sub_a++, sub_b++) {
            sub_a->obj = 0;
            sub_a->done = 0;
            sub_b->obj = 0;
            sub_b->done = 0;
        }
    }
    w->wait = 0;
    w->wing_alive = 1;
    enemy_task_clear_flags(obj, 64);
    enemy_task_get_param_vec(obj, &v);
    if (v3_is_zero(&v)) {
        v3_set(&v, 0, -1.0f, 0);
        w->speed = 0;
    } else {
        w->speed = vec3_length(&v);
        vec3_normalize(&v, &v);
    }
    enemy_task_set_param_vec(obj, &v);
    w->alpha = 0;
    w->count = 1;
    w->state = 0;
    w->swing = w->alpha;
    w->look = w->alpha;
    w->roll = 0;
    w->opened = 0;
    w->stage = 0;
    w->early = 0;
    w->eggs = 0;
    w->halo = w->alpha;
    w->breaks = 0;
}

/* 0x00b0f780 */
void seraphim_release(void *obj, SeraphimWork *w)
{
    SeraphimPartRec *core = w->recs;
    SeraphimPartRec *rec_a = &w->recs[2];
    SeraphimPartRec *rec_b = &w->recs[5];
    int i;

    dc_object_release(&w->mdl[11]);
    dc_object_release(&w->mdl[12]);
    dc_object_release(&w->mdl[0]);
    dc_object_release(&w->mdl[2]);
    dc_object_release(&w->mdl[3]);
    dc_object_release(&w->mdl[4]);
    dc_object_release(&w->mdl[5]);
    dc_object_release(&w->mdl[7]);
    dc_object_release(&w->mdl[8]);
    dc_object_release(&w->mdl[9]);
    dc_object_release(&w->mdl[10]);
    dc_object_release(&w->mdl2[0]);
    dc_object_release(&w->mdl2[1]);
    dc_object_release(&w->mdl[1]);
    dc_object_release(&w->mdl[6]);
    lockon_remove(&w->lock);
    for (i = 0; i < 2; i++, core++)
        lockon_remove(&core->lock);
    for (i = 0; i < 3; i++, rec_a++, rec_b++) {
        lockon_remove(&rec_a->lock);
        lockon_remove(&rec_b->lock);
    }
}

/* 0x00b0f8b0 */
void seraphim_launch_missiles(void *obj, SeraphimWork *w)
{
    SeraphimMissileArg prm;
    SeraphimRec *typ = w->type;
    SeraphimMissileSlot *sub_a;
    SeraphimMissileSlot *sub_b;
    int i;

    prm.type = w->idx;
    prm.open = typ->missile_appear_sec * FPS;
    prm.hold = (typ->open_wait_sec + typ->open_sec + typ->open_stop_sec) * FPS;
    w->wait = 0;
    w->last_shot_down = 0;
    prm.close = typ->missile_motion_sec * FPS;
    w->last_wing_alive = 0;
    prm.wait = typ->shot_sec * FPS;
    prm.lock_at = (typ->missile_appear_sec + typ->open_wait_sec) * FPS;
    sub_a = w->ta;
    sub_b = w->tb;
    for (i = 0; i < 4; i++, sub_a++, sub_b++) {
        prm.pos = &sub_a->pos;
        prm.ang = sub_a->ang;
        prm.killed = &sub_a->killed;
        prm.launched = &sub_a->launched;
        sub_a->killed = 0;
        sub_a->launched = 0;
        sub_a->obj = (void *)seraphim_missile_create(obj, &prm);
        sub_a->t = 0;
        sub_a->done = 0;
        w->wait++;
        prm.pos = &sub_b->pos;
        prm.ang = sub_b->ang;
        prm.killed = &sub_b->killed;
        prm.launched = &sub_b->launched;
        sub_b->killed = 0;
        sub_b->launched = 0;
        sub_b->obj = (void *)seraphim_missile_create(obj, &prm);
        sub_b->t = 0;
        sub_b->done = 0;
        w->wait++;
        prm.hold += typ->missile_dif_sec * FPS;
    }
}

/* 0x00b0fa58 */
void seraphim_state_approach(void *obj, SeraphimWork *w)
{
    Vec3 p;
    Vec3 v;
    SeraphimRec *typ = w->type;
    int n = enemy_task_get_param(obj, 0) * 60.0f / 60.0f + 1.0f;

    enemy_task_get_pos(obj, &p);
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, w->speed);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    w->roll += typ->roll_spd / FPS;
    dc_object_advance_time(&w->mdl[12]);
    seraphim_next_state(obj, (TypeEnemyWork *)w, n, 1);
}

/* 0x00b0fb88 */
void seraphim_state_break(void *obj, SeraphimWork *w)
{
    Vec3 p;
    Vec3 v;
    SeraphimRec *typ = w->type;
    int n = typ->break_sec * FPS;
    float rate = w->count / (float)n;
    float spd, acc;
    float k;

    enemy_task_get_pos(obj, &p);
    spd = w->speed * FPS;
    acc = -spd / typ->break_sec;
    k = typ->break_sec * rate;
    spd += acc * k;
    k = 1.0f / FPS;
    spd *= k;
    spd += acc * 0.5f * k * k;
    enemy_task_get_param_vec(obj, &v);
    v3_scale(&v, spd);
    v3_add(&p, &v);
    enemy_task_set_pos(obj, &p);
    w->roll += typ->roll_spd / FPS * safe_powf(1.0f - rate, typ->roll_pow);
    dc_object_advance_time(&w->mdl[12]);
    seraphim_next_state(obj, (TypeEnemyWork *)w, n, 2);
}

/* 0x00b0fd28 */
void seraphim_state_stop(void *obj, SeraphimWork *w)
{
    float sc[3];
    SeraphimRec *typ = w->type;
    int n = typ->stop_sec * FPS;
    float rate = w->count / (float)n;
    float prev = (w->count - 1) / (float)n;

    ulSinCos(safe_powf(rate, typ->roll_back_pow) * ANG_HALF - ANG_QUARTER, &sc[0], &sc[1]);
    ulSinCos(safe_powf(prev, typ->roll_back_pow) * ANG_HALF - ANG_QUARTER, &sc[2], &sc[1]);
    w->roll -= typ->roll_back * (sc[0] - sc[2]) * 0.5f;
    dc_object_advance_time(&w->mdl[12]);
    if (seraphim_next_state(obj, (TypeEnemyWork *)w, n, 3))
        w->lock.flags &= ~LOCK_LOCKABLE;
}

/* 0x00b0fea0 */
void seraphim_state_egg_break(void *obj, SeraphimWork *w)
{
    int n = w->type->egg_break_sec * FPS;
    ModelPart *part = &w->mdl[11];

    part->frame = part->frames * (w->count / (float)n);
    dc_object_advance_time(&w->mdl[12]);
    if (seraphim_next_state(obj, (TypeEnemyWork *)w, n, 4))
        enemy_task_set_state(obj, 0);
}

/* 0x00b0ff58 */
void seraphim_state_missile_appear(void *obj, SeraphimWork *w)
{
    OpenEffArg prm;
    int n = w->type->missile_appear_sec * FPS;
    float rate = w->count / (float)n;
    int i, j;

    if (w->count == 1)
        seraphim_launch_missiles(obj, w);
    dc_object_advance_time(&w->mdl[12]);
    if (w->stage == 1) {
        if (w->count == 1) {
            prm.frames = n;
            for (i = 0; i < 2; i++) {
                prm.pos = &w->recs[i].pos;
                prm.ang = w->recs[i].ang;
                open_eff_create(obj, &prm);
            }
        }
        w->mdl[0].frame = w->mdl[0].frames * rate;
    }
    if (seraphim_next_state(obj, (TypeEnemyWork *)w, n, 5) && w->stage) {
        for (j = 0; j < 2; j++) {
            LockObj *lock = &w->recs[j].lock;
            lock->flags |= LOCK_LOCKABLE;
        }
    }
}

/* 0x00b100a0 */
void seraphim_state_open_wait(void *obj, SeraphimWork *w)
{
    int n = w->type->open_wait_sec * FPS;
    SeraphimPartRec *rec_a;
    SeraphimPartRec *rec_b;
    int i;

    dc_object_advance_time(&w->mdl[12]);
    if (seraphim_next_state(obj, (TypeEnemyWork *)w, n, 6)) {
        rec_a = &w->recs[2];
        rec_b = &w->recs[5];
        for (i = 0; i < 3; i++, rec_a++, rec_b++) {
            rec_a->lock.flags |= LOCK_LOCKABLE;
            rec_b->lock.flags |= LOCK_LOCKABLE;
        }
    }
}

/* steer(cast) */
/* 0x00b10148 */
void seraphim_state_open(void *obj, SeraphimWork *w)
{
    SeraphimRec *typ = w->type;
    int n = typ->open_sec * FPS;
    float rate = w->count / (float)n;
    ModelPart *gfx;
    ModelPart *wing;
    float frm;

    if (!w->opened) {
        w->look = rate;
        w->swing = w->count / ((typ->open_sec + typ->open_stop_sec) * FPS);
    }
    gfx = enemy_task_gfx_ptr(obj);
    wing = &w->mdl[1];
    frm = gfx->frames * rate;
    if (frm <= gfx->frame)
        frm = gfx->frame;
    gfx->frame = frm;
    wing->frame = wing->frames * rate;
    dc_object_advance_time(&w->mdl[12]);
    if (seraphim_next_state(obj, (TypeEnemyWork *)w, n, 7)) {
        w->look = 1.0f;
        dc_object_set_clip(wing, *((struct DcClip **)pnb_get_entry(stage_models, 196)));
        w->mdl[1].frame = 0;
    }
}

/* 0x00b10278 */
void seraphim_state_open_stop(void *obj, SeraphimWork *w)
{
    SeraphimRec *typ = w->type;
    int n = typ->open_stop_sec * FPS;
    float rate = w->count / (float)n;
    ModelPart *gfx;
    ModelPart *wing;
    float k;

    if (!w->opened)
        w->swing = (w->count + typ->open_sec * FPS) / ((typ->open_sec + typ->open_stop_sec) * FPS);
    gfx = enemy_task_gfx_ptr(obj);
    wing = &w->mdl[1];
    k = w->count / (float)(int)((typ->open_stop_sec + typ->shot_sec + typ->close_wait_sec) * FPS);
    w->halo = rate;
    dc_object_advance_time(&w->mdl2[1]);
    wing->frame = gfx->frames * k;
    dc_object_advance_time(&w->mdl[12]);
    if (seraphim_next_state(obj, (TypeEnemyWork *)w, n, 8))
        w->swing = 1.0f;
    seraphim_check_wing_nodes(obj, w);
}

/* 0x00b103d8 */
void seraphim_state_shot(void *obj, SeraphimWork *w)
{
    SeraphimRec *typ = w->type;
    int n = typ->shot_sec * FPS;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ModelPart *wing = &w->mdl[1];
    float r = (w->count + typ->open_stop_sec * FPS)
              / (int)((typ->open_stop_sec + typ->shot_sec + typ->close_wait_sec) * FPS);

    dc_object_advance_time(&w->mdl2[1]);
    wing->frame = gfx->frames * r;
    dc_object_advance_time(&w->mdl[12]);
    seraphim_next_state(obj, (TypeEnemyWork *)w, n, 9);
    seraphim_check_wing_nodes(obj, w);
}

/* steer(cast) */
/* 0x00b104d8 */
void seraphim_state_close_wait(void *obj, SeraphimWork *w)
{
    SeraphimRec *typ = w->type;
    int n = typ->close_wait_sec * FPS;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ModelPart *wing = &w->mdl[1];
    float r = (w->count + (typ->open_stop_sec + typ->shot_sec) * FPS)
              / (int)((typ->open_stop_sec + typ->shot_sec + typ->close_wait_sec) * FPS);

    dc_object_advance_time(&w->mdl2[1]);
    wing->frame = gfx->frames * r;
    dc_object_advance_time(&w->mdl[12]);
    if (seraphim_next_state(obj, (TypeEnemyWork *)w, n, 10)) {
        dc_object_set_clip(wing, *((struct DcClip **)pnb_get_entry(stage_models, 166)));
        w->mdl[1].frame = w->mdl[1].frames;
    }
    seraphim_check_wing_nodes(obj, w);
}

/* 0x00b10600 */
void seraphim_state_close(void *obj, SeraphimWork *w)
{
    float sc[2];
    SeraphimRec *typ = w->type;
    int n = typ->close_sec * FPS;
    float rate = w->count / (float)n;
    float k = 1.0f - rate;
    ModelPart *wing = &w->mdl[1];
    SeraphimPartRec *rec_a;
    SeraphimPartRec *rec_b;
    int i, cnt;

    w->halo = k;
    dc_object_advance_time(&w->mdl2[1]);
    wing->frame = wing->frames * k;
    dc_object_advance_time(&w->mdl[12]);
    if (w->early) {
        ulSinCos(safe_powf(rate, typ->early_swing_close_pow) * ANG_TURN - ANG_QUARTER, &sc[0],
                 &sc[1]);
        sc[0] = (sc[0] + 1.0f) * 0.5f * typ->early_swing_close_rate;
        w->early_t = rate;
        w->swing = rate + w->swing0 * k + sc[0];
    }
    if (seraphim_next_state(obj, (TypeEnemyWork *)w, n, 4)) {
        w->opened = 1;
        cnt = 0;
        rec_a = &w->recs[2];
        rec_b = &w->recs[5];
        for (i = 0; i < 3; i++, rec_a++, rec_b++) {
            cnt += rec_a->lock.life > 0;
            cnt += rec_b->lock.life > 0;
            rec_a->lock.flags &= ~LOCK_LOCKABLE;
            rec_b->lock.flags &= ~LOCK_LOCKABLE;
        }
        if (cnt <= 0) {
            w->stage = MIN(w->stage + 1, 2);
        }
        w->early = 0;
    }
}

/* 0x00b107d8 */
void seraphim_update(void *obj, SeraphimWork *w)
{
    void (*fn)(void *obj, SeraphimWork *w) = seraphim_states[w->state];
    SeraphimMissileSlot *sub_a;
    SeraphimMissileSlot *sub_b;
    int i;
    float alpha;

    if (fn)
        fn(obj, w);
    seraphim_update_egg(obj, w);
    if ((unsigned int)enemy_task_get_pstate(obj) >= 2)
        seraphim_pose_parts(obj, w);
    else {
        seraphim_pose_model(obj, w);
        seraphim_update_wings(obj, w);
    }
    sub_a = w->ta;
    sub_b = w->tb;
    for (i = 0; i < 4; i++, sub_a++, sub_b++) {
        sub_a->t++;
        sub_b->t++;
    }
    alpha = enemy_task_get_time(obj) / (w->type->alpha_sec * FPS);
    w->alpha = alpha;
    w->alpha = MIN(1.0f, w->alpha);
}

/* 0x00b108f0 */
void seraphim_exec_appear(void *obj, void *w)
{
    int n = enemy_task_get_phase(obj);

    if (n && n != 2)
        seraphim_update(obj, w);
}

/* 0x00b10950 */
void seraphim_exec_normal(void *obj, SeraphimWork *w)
{
    ModelPart *gfx;
    int *root;
    int *clip;

    switch (enemy_task_get_phase(obj)) {
    case 0:
        gfx = enemy_task_gfx_ptr(obj);
        dc_object_release(gfx);
        root = pnb_get_entry(stage_models, 152);
        clip = pnb_get_entry(stage_models, 153);
        dc_object_init(gfx, *root, *clip, 0);
        dc_object_rebuild(gfx);
        gfx->radius = 2000.0f;
        break;
    case 2:
        break;
    default:
        seraphim_update(obj, w);
    }
}

/* 0x00b10a18 */
void seraphim_exec_leave(void *obj, void *w)
{
    int n = enemy_task_get_phase(obj);

    if (n && n != 2)
        seraphim_update(obj, w);
}

/* 0x00b10a78 */
void seraphim_break_wing_part(void *obj, SeraphimWork *w, LockObj *lock, SeraphimNode *n, int half, ModelPart *mdl_a, ModelPart *mdl_b,
                          ModelPart *wire, int type_a, int type_b, int pose)
{
    EnSeraphimDmgArg dmg;
    SeraphimWireBreak wb;
    SeraphimRec *typ = w->type;

    if (pose) {
        ulMatrixPush();
        matrix_unit_set(0);
        seraphim_rotate_yxz(n->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        model_camera_angles(n->ang);
        ulMatrixPop();
    }
    if (lock->life > 0) {
        heap_bzero(&dmg, sizeof dmg);
        dmg.pos = &n->pos;
        dmg.ang = n->ang;
        dmg.set = w->idx;
        dmg.mode = 0;
        if (half) {
            dmg.src = mdl_a;
            dmg.type = type_a;
        } else {
            dmg.src = mdl_b;
            dmg.type = type_b;
        }
        dmg.delay = RAND_FLOAT_B() * typ->fall_wait_sec_wing * FPS;
        en_seraphim_dmg_create(obj, &dmg);
    } else {
        nj_fill8(&wb, 0, sizeof wb);
        wb.model = wire;
        wb.color = typ->wing_wire_col;
        wb.h = 0;
        wb.start = typ->wire_break_vec_start * (1.0f / FPS);
        wb.magni = typ->wire_break_magni;
        wb.ang = typ->wire_break_vec_ang;
        wb.v.x = 0;
        wb.v.y = 0;
        wb.v.z = 0;
        wb.frames = typ->wire_break_sec * FPS;
        ulMatrixPush();
        matrix_unit_set(0);
        do {
            Vec3 *p_ = &n->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        seraphim_rotate_yxz(n->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        wire_break_task_create(TASK_OF(obj)->parent ? TASK_OF(obj)->parent + 1 : 0, &wb);
        ulMatrixPop();
    }
}

/* 0x00b10ca8 */
void seraphim_exec_destroyed(void *obj, SeraphimWork *w)
{
    SpawnDesc desc;
    int k;
    SeraphimPartRec *rec_a = &w->recs[2];
    SeraphimPartRec *rec_b = &w->recs[5];
    SeraphimNode *na = w->wa;
    SeraphimNode *nb = w->wb;
    ModelPart *gfx;
    int i, half;

    if (enemy_task_get_phase(obj))
        return;
    seraphim_update_egg(obj, w);
    seraphim_pose_model(obj, w);
    seraphim_update_wings(obj, w);
    for (i = 0; i < 7; i++) {
        half = seraphim_part_record(i, &k);
        seraphim_break_wing_part(obj, w, &rec_a[k].lock, na, half, &w->mdl[2], &w->mdl[3], &w->mdl[4], 169, 173, 0);
        na++;
        seraphim_break_wing_part(obj, w, &rec_b[k].lock, nb, half, &w->mdl[7], &w->mdl[8], &w->mdl[9], 175, 179, 1);
        nb++;
    }
    gfx = enemy_task_gfx_ptr(obj);
    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = 0x40000;
    desc.models = stage_models;
    desc.type = 160;
    desc.motion = 161;
    desc.time = gfx->frame;
    enemy_task_get_pos(obj, &desc.pos);
    enemy_task_get_rot(obj, desc.ang);
    enemy_task_get_scl(obj, &desc.scale);
    v3_copy(&desc.v, &w->hitv);
    en_shatter_spawn(obj, &desc, 4);
    FINISH_IF(obj);
}

/* 0x00b10f10 */
void seraphim_pre_exec(void)
{
}

/* 0x00b10f18 */
void seraphim_exec(void *obj, TypeEnemyWork *w)
{
    unsigned int i = enemy_task_get_pstate(obj);

    if (seraphim_exec_table[i])
        seraphim_exec_table[i](obj, w);
}

/* 0x00b10f70 */
void seraphim_spawn_debris(void *obj, Vec3 *pos, LockObj *lock)
{
    SpawnDesc desc;

    nj_fill8(&desc, 0, sizeof desc);
    v3_copy(&desc.pos, pos);
    v3_set(&desc.scale, 1.0f, 1.0f, 1.0f);
    desc.lock = lock;
    desc.models = stage_models;
    desc.type = 189;
    desc.kind = 0;
    i3_set(desc.ang, 0, 0, 0);
    v3_set(&desc.v, 0, 0, 0);
    en_shatter_spawn(obj, &desc, 4);
}

/* 0x00b11030 */
void seraphim_drop_wing_parts(void *obj, SeraphimWork *w, int idx, SeraphimNode *n, ModelPart *mdl_a, ModelPart *mdl_b, int type_a, int type_b)
{
    EnSeraphimDmgArg dmg;
    int k;
    SeraphimRec *typ = w->type;
    int i, half;

    for (i = 0; i < 7; i++) {
        half = seraphim_part_record(i, &k);
        if (k == idx) {
            heap_bzero(&dmg, sizeof dmg);
            dmg.pos = &n[i].pos;
            dmg.set = w->idx;
            dmg.ang = n[i].ang;
            dmg.mode = 0;
            if (half) {
                dmg.type = type_a;
                dmg.src = mdl_a;
            } else {
                dmg.src = mdl_b;
                dmg.type = type_b;
            }
            dmg.delay = RAND_FLOAT_B() * typ->fall_wait_sec_wing * FPS;
            en_seraphim_dmg_create(obj, &dmg);
        }
    }
}

/* 0x00b11198 */
void seraphim_wing_hit_check(void *obj, SeraphimWork *w, SeraphimPartRec *rec, SeraphimNode *n, ModelPart *mdl_a, ModelPart *mdl_b, int type_a,
                          int type_b)
{
    Vec3 scr;
    int i;

    for (i = 0; i < 3; i++, rec++) {
        if (rec->lock.flags & LOCK_HIT) {
            seraphim_spawn_debris(obj, &rec->pos, &rec->lock);
            ulMatrixApplyPoint((float *)ulMatrixCur, &rec->pos.x, &scr.x);
            project_point(&scr, &scr);
            switch (rec->lock.life) {
            case 0:
                seraphim_drop_wing_parts(obj, w, i, n, mdl_a, mdl_b, type_a, type_b);
                switch (w->breaks++) {
                case 0:
                    SoundRhythmSe_EnemyPos(seraphim_init_arg, 9, &scr.x);
                    break;
                case 1:
                    SoundRhythmSe_EnemyPos(seraphim_init_arg, 17, &scr.x);
                    break;
                case 2:
                    SoundRhythmSe_EnemyPos(seraphim_init_arg, 18, &scr.x);
                    break;
                case 3:
                    SoundRhythmSe_EnemyPos(seraphim_init_arg, 19, &scr.x);
                    break;
                case 4:
                    SoundRhythmSe_EnemyPos(seraphim_init_arg, 20, &scr.x);
                    break;
                case 5:
                    SoundRhythmSe_EnemyPos(seraphim_init_arg, 21, &scr.x);
                    break;
                }
                break;
            case 1:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 15, &scr.x);
                break;
            case 2:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 14, &scr.x);
                break;
            case 3:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 13, &scr.x);
                break;
            case 4:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 12, &scr.x);
                break;
            case 5:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 11, &scr.x);
                break;
            case 6:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 10, &scr.x);
                break;
            case 7:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 8, &scr.x);
                break;
            }
        }
    }
}

/* 0x00b113a0 */
int seraphim_core_hit_check(void *obj, SeraphimWork *w, Vec3 **hit_pos)
{
    Vec3 scr;
    SeraphimPartRec *rec = w->recs;
    ModelPart *gfx;
    float frm;
    int i;
    int n = 0;

    for (i = 0; i < 2; i++, rec++) {
        if (rec->lock.flags & LOCK_HIT) {
            seraphim_spawn_debris(obj, &rec->pos, &rec->lock);
            ulMatrixApplyPoint((float *)ulMatrixCur, &rec->pos.x, &scr.x);
            if (rec->lock.life <= 0) {
                gfx = enemy_task_gfx_ptr(obj);
                frm = gfx->frame;
                dc_object_release(gfx);
                if (!i) {
                    {
                        int *root = pnb_get_entry(stage_models, 156);
                        int *clip = pnb_get_entry(stage_models, 157);

                        dc_object_init(gfx, *root, *clip, 0);
                    }
                } else {
                    {
                        int *root = pnb_get_entry(stage_models, 154);
                        int *clip = pnb_get_entry(stage_models, 155);

                        dc_object_init(gfx, *root, *clip, 0);
                    }
                }
                dc_object_rebuild(gfx);
                gfx->frame = frm;
                gfx->radius = 2000.0f;
                n = lockon_hits_max_shot_cnt(&rec->lock);
                *hit_pos = &rec->pos;
                if (!rec->lock.life && ++w->eggs < 2)
                    SoundRhythmSe_EnemyPos(seraphim_init_arg, 1, &scr.x);
            }
            switch (rec->lock.life) {
            case 0:
                break;
            case 1:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 7, &scr.x);
                break;
            case 2:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 6, &scr.x);
                break;
            case 3:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 5, &scr.x);
                break;
            case 4:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 4, &scr.x);
                break;
            case 5:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 3, &scr.x);
                break;
            case 6:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 2, &scr.x);
                break;
            case 7:
                SoundRhythmSe_EnemyPos(seraphim_init_arg, 0, &scr.x);
                break;
            }
        }
    }
    return n;
}

/* 0x00b115f0 */
void seraphim_post_exec(void *obj, SeraphimWork *w)
{
    Vec3 scr;
    SeraphimHitArg prm;
    Vec3 scr2;
    Vec3 *hit_pos;
    SeraphimRec *t = w->type;
    int n;

    if (!enemy_status_get_ticks()) {
        seraphim_update_egg(obj, w);
        if ((unsigned int)enemy_task_get_pstate(obj) >= 2)
            seraphim_pose_parts(obj, w);
        else {
            seraphim_pose_model(obj, w);
            seraphim_update_wings(obj, w);
        }
    }
    seraphim_update_missiles(obj, w);
    enemy_task_get_pos(obj, &w->hitpos);
    w->hitpos.y += -300.0f;
    if (w->lock.flags & LOCK_HIT) {
        w->lock.life = t->hp;
        prm.kind = w->idx + 1;
        prm.pos = &w->hitpos;
        en_vts_cmd_dmg_eff_create(obj, &prm);
        ulMatrixApplyPoint((float *)ulMatrixCur, &w->hitpos.x, &scr.x);
        project_point(&scr, &scr);
        SoundRhythmSe_EnemyPos(seraphim_init_arg, 16, &scr.x);
    }
    seraphim_wing_hit_check(obj, w, &w->recs[2], w->wa, &w->mdl[2], &w->mdl[3], 169, 173);
    seraphim_wing_hit_check(obj, w, &w->recs[5], w->wb, &w->mdl[7], &w->mdl[8], 175, 179);
    n = seraphim_core_hit_check(obj, w, &hit_pos);
    if (w->eggs == 2 && (unsigned int)enemy_task_get_pstate(obj) < 2) {
        enemy_task_set_state(obj, 3);
        {
            SeraphimRec *typ = w->type;

            enemy_status_score_popup(enemy_task_get_status_slot(obj), hit_pos, typ->score, n);
        }
        ulMatrixApplyPoint((float *)ulMatrixCur, &hit_pos->x, &scr2.x);
        project_point(&scr2, &scr2);
        SoundRhythmSe_EnemyPos(seraphim_init_arg, 1, &scr2.x);
    }
}

/* steer(cast) */
/* 0x00b117f0 */
void seraphim_disp_shell(void *obj, SeraphimWork *w)
{
    float col_add[4];
    ModelPart *sh = &w->mdl[11];
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    SeraphimRec *typ;
    void *fog;
    int prev;

    if (view_sphere_visible(&w->pos, sh->radius)) {
        fog = fog_set_enable(0);
        typ = w->type;
        prev = 0;
        if (!(fabsf(w->alpha - 1.0f) < FLT_EPSILON)) {
            prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            heap_bzero(col_add, 16);
            col_add[0] = w->alpha - 1.0f;
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
            draw_set_override_color(col_add);
        }
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        seraphim_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(gfx);
        vu0_matrix_rotate_y(ulMatrixCur, w->roll);
        if (typ->wire) {
            draw_set_blend(4, 5);
            draw_set_color(yuki_beat_color(typ->wire_col, typ->wire_col2, typ->wire_beat, typ->wire_linear, w->alpha,
                                           typ->wire_near, typ->wire_far, typ->wire_near_rate, typ->wire_far_rate));
            model_object_draw(&w->mdl2[0], gfx->frame);
        }
        dc_object_draw(sh);
        ulMatrixPop();
        if (!(fabsf(w->alpha - 1.0f) < FLT_EPSILON)) {
            draw_pop_override_color();
            draw_pop_strip_flags();
            draw_set_override_flags(prev);
        }
        fog_set_enable(fog);
    }
}

/* steer(cast) */
/* 0x00b119f0 */
void seraphim_disp_body(void *obj, SeraphimWork *w)
{
    Vec3 p;
    void *arg[3];
    Vec3 pos[15];
    int buf[48];
    int ang[15][3];
    float col_add[4];
    SeraphimPartDraw draw_a;
    SeraphimPartDraw draw_b;
    void *fog;
    int prev;
    ModelPart *gfx = enemy_task_gfx_ptr(obj);
    ModelPart *wing;
    ModelPart *halo;
    SeraphimRec *typ;
    SeraphimNode *na;
    SeraphimNode *nb;
    SeraphimPartRec *rec_a;
    SeraphimPartRec *rec_b;
    int i;
    int j;

    enemy_task_get_pos(obj, &p);
    if (view_sphere_visible(&p, 2000.0f)) {
        fog = fog_set_enable(0);
        wing = &w->mdl[1];
        typ = w->type;
        prev = 0;
        if (!(fabsf(w->alpha - 1.0f) < FLT_EPSILON)) {
            prev = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
            heap_bzero(col_add, 16);
            col_add[0] = w->alpha - 1.0f;
            draw_push_strip_flags();
            draw_push_override_color();
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
            draw_set_override_color(col_add);
        }
        arg[0] = pos;
        arg[1] = ang;
        arg[2] = buf;
        anim_eval_hierarchy(wing->root, wing->clip, arg, wing->frame);
        ulGxSetTextureList(((void **)obj)[1]);
        ulMatrixPush();
        do {
            Vec3 *p_ = &w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        seraphim_rotate_yxz(w->ang);
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        dc_object_draw(gfx);
        dc_object_draw(&w->mdl[0]);
        if (typ->wire) {
            draw_set_blend(4, 5);
            draw_set_color(yuki_beat_color(typ->wire_col, typ->wire_col2, typ->wire_beat, typ->wire_linear, w->alpha,
                                           typ->wire_near, typ->wire_far, typ->wire_near_rate, typ->wire_far_rate));
            model_object_draw(&w->mdl2[0], gfx->frame);
        }
        ulMatrixPop();
        halo = &w->mdl2[1];
        draw_set_color(color_blend(typ->wing_wire_col, typ->wing_wire_col2, yuki_beat_phase(typ->beat)));
        draw_a.recs = &w->recs[2];
        draw_a.alive_a = &w->mdl[2];
        draw_a.alive_b = &w->mdl[3];
        draw_a.dead_a = &w->mdl[4];
        draw_a.dead_b = &w->mdl[5];
        draw_a.halo = halo;
        draw_b.recs = &w->recs[5];
        draw_b.alive_a = &w->mdl[7];
        draw_b.alive_b = &w->mdl[8];
        draw_b.dead_a = &w->mdl[9];
        draw_b.dead_b = &w->mdl[10];
        draw_b.halo = halo;
        nb = w->wb;
        na = w->wa;
        for (i = 0; i < 7; i++, na++, nb++) {
            ulMatrixPush();
            do {
                Vec3 *p_ = &na->pos;
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            seraphim_rotate_yxz(na->ang);
            vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
            seraphim_draw_part(&draw_a, i);
            ulMatrixPop();
            ulMatrixPush();
            do {
                Vec3 *p_ = &nb->pos;
                vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
            } while (0);
            seraphim_rotate_yxz(nb->ang);
            seraphim_draw_part(&draw_b, i);
            ulMatrixPop();
        }
        if (0.0f < w->halo) {
            rec_b = &w->recs[5];
            rec_a = &w->recs[2];
            for (j = 0; j < 3; j++, rec_a++, rec_b++) {
                if (rec_a->lock.life > 0) {
                    ulMatrixPush();
                    do {
                        Vec3 *p_ = &rec_a->pos;
                        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
                    } while (0);
                    seraphim_rotate_yxz(rec_a->ang);
                    vu0_matrix_scale(ulMatrixCur, w->halo, w->halo, w->halo);
                    model_node_draw_tree2_anim(halo->root, halo->clip, halo->frames);
                    ulMatrixPop();
                }
                if (rec_b->lock.life > 0) {
                    ulMatrixPush();
                    do {
                        Vec3 *p_ = &rec_b->pos;
                        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
                    } while (0);
                    seraphim_rotate_yxz(rec_b->ang);
                    vu0_matrix_scale(ulMatrixCur, w->halo, w->halo, w->halo);
                    model_node_draw_tree2_anim(halo->root, halo->clip, halo->frame);
                    ulMatrixPop();
                }
            }
        }
        if (!(fabsf(w->alpha - 1.0f) < FLT_EPSILON)) {
            draw_pop_override_color();
            draw_pop_strip_flags();
            draw_set_override_flags(prev);
        }
        fog_set_enable(fog);
    }
}

/* 0x00b11e60 */
void seraphim_disp(void *obj, void *w)
{
    void (*fn)(void *obj, void *w);
    unsigned int i;

    i = enemy_task_get_pstate(obj);
    fn = seraphim_disp_table[i];
    if (fn) {
        model_clip_far_off();
        fn(obj, w);
    }
}

/* 0x00b11ec8 */
int seraphim_create(void *obj, ShotArg *place)
{
    ObjDesc desc;
    ObjFuncs funcs = *(ObjFuncs *)seraphim_funcs;
    int k;
    SeraphimRec *typ;
    int none;
    float x = place->delay;
    int i = 0.0f <= x ? (x < 1.0f ? (0.0f <= x ? x : 0.0f) : 1.0f) : 0.0f;

    none = -1;
    {
        int j = i > none ? i : 0;
        k = j != 0;
    }
    typ = (SeraphimRec *)&seraphim_types[k];
    nj_fill8(&desc, 0, sizeof desc);
    desc.kind = &k;
    desc.name = seraphim_name;
    desc.funcs = &funcs;
    desc.arg = place;
    desc.models = stage_models;
    desc.tex = stage_textures;
    desc.hp = typ->hp;
    desc.same_lock = typ->same;
    desc.flags = 18;
    desc.dc_flags = 0;
    desc.model_no = 162;
    desc.model_no2 = none;
    desc.score = typ->score;
    desc.hp_reduce = typ->hp_reduce;
    desc.crash_r = typ->crash_r;
    desc.crash_damage = typ->crash_damage;
    desc.time = 0;
    desc.size = 3712;
    return enemy_task_task_create(obj, &desc);
}

/* 0x00b12060 */
void seraphim_rotate_zyx(int *ang)
{
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
}

/* 0x00b120b0 */
void seraphim_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
