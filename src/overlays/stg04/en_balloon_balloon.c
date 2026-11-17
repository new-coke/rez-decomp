/*
 * Balloon break-up, Area 4: burst skin as wire segments that fly apart and fade, the tumbling
 * control bars (balloon_ctrlbar), and the debris of the attack.
 */

#include "stg04.h"
#include "mathdef.h"
#include "anim.h"
#include "approach.h"
#include "camera.h"
#include "color.h"
#include "debug.h"
#include "gw_varedit.h"
#include "heap.h"
#include "libc.h"
#include "math_util.h"
#include "model.h"
#include "model_wire_break.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "pnb.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "en_balloon_balloon.h"
#include "../common/enemy_shatter.h"
#include "../common/enemy_vars.h"

extern void *en_balloon_balloon_nodes;
extern float en_balloon_balloon_frm_life;
extern void (*balloon_ctrlbar_states[])(BalloonCtrlbarWork *w);
extern float balloon_ctrlbar_color[];
extern struct TaskDesc balloon_ctrlbar_desc;
extern ParamRec en_balloon_balloon_params;
extern VarDef en_balloon_balloon_vardefs[3];
extern struct TaskDesc en_balloon_balloon_desc;

typedef struct EnBalloonPiece {
    int type;
    float time;                 /* 0x04 */
} EnBalloonPiece;

extern EnBalloonPiece en_balloon_debris_pieces[2];

/* en_balloon_balloon_params, 3 words */
typedef struct BalloonBurstRec {
    char *name;
    char *label;                /* 0x04 */
    float frm_life;             /* 0x08 */
    float spd_move;             /* 0x0c */
    float spd_rotate;           /* 0x10 */
} BalloonBurstRec;

#define EN_BALLOON_BALLOON_REC ((BalloonBurstRec *)&en_balloon_balloon_params)

/* 44 bytes, heap block at wb[0] */
typedef struct BalloonWireSeg {
    Vec3 p0;
    Vec3 p1;                     /* 0x0c */
    Vec3 vel;                   /* 0x18 */
    int rx;                     /* 0x24 */
    int ry;                     /* 0x28 */
} BalloonWireSeg;

extern char str_en_balloon_balloon_c[];

/* 0x00affcf8 */
void en_balloon_balloon_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_balloon_balloon_params.name;
    def.fmt = en_balloon_balloon_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_balloon_balloon_vardefs;
    fld = en_balloon_balloon_params.v;
    for (i = 0; i < 3; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_balloon_balloon_vardefs);
}

/* 0x00affda8 */
void en_balloon_balloon_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00affdb0 */
int en_balloon_balloon_count_edges(int *lst)
{
    int cnt = 0;

    while (lst[0]) {
        cnt += lst[1];
        lst = (int *)lst[0];
    }
    return cnt;
}

/* 0x00affde8 */
void en_balloon_balloon_init_edges(BalloonBurstWork *w, int cache, Vec3 *pos, void *src)
{
    int *blk = (int *)cache;
    int *wb = w->wb;
    int n = en_balloon_balloon_count_edges(blk);
    BalloonWireSeg *seg = heap_block_tag(game_malloc(n * 44), str_en_balloon_balloon_c, SRC_LINE(208, 209));
#ifdef VERSION_US
    Vec3 *base = (Vec3 *)src;
#else
    Vec3 *base = &((Vec3 *)src)[41];
#endif
    unsigned short *ei;
    Vec3 mid;
    float spd;
    int i;

    *(BalloonWireSeg **)&wb[0] = seg;
    wb[1] = n;
    *(float *)&wb[2] = 1.0f;
    for (; *(int **)blk; blk = *(int **)blk) {
        ei = (unsigned short *)(blk + 2);
        i = blk[1];
        for (i--; i != -1; i--) {
#ifdef VERSION_US
            v3_copy(&seg->p0, &base[*ei++]);
            v3_copy(&seg->p1, &base[*ei++]);
#else
            v3_copy(&seg->p0, base - *ei++);
            v3_copy(&seg->p1, base - *ei++);
#endif
            v3_sum(&mid, &seg->p0, &seg->p1);
            mid.x = mid.x * 0.5f - pos->x;
            mid.y = mid.y * 0.5f - pos->y;
            mid.z = mid.z * 0.5f - pos->z;
            spd = EN_BALLOON_BALLOON_REC->spd_move * FRAME_SEC;
            v3_mul(&seg->vel, &mid, spd);
            seg->rx = (int)DEG_TO_ANG(EN_BALLOON_BALLOON_REC->spd_rotate * FRAME_SEC * (1.0f - RAND_FLOAT_A()));
            seg->ry = (int)DEG_TO_ANG(EN_BALLOON_BALLOON_REC->spd_rotate * FRAME_SEC * (1.0f - RAND_FLOAT_A()));
            seg++;
        }
    }
}

/* 0x00b00080 */
void en_balloon_balloon_free(BalloonBurstWork *w)
{
    heap_free((void *)w->wb[0], 0);
}

/* 0x00b00090 */
void en_balloon_balloon_exec(BalloonBurstWork *w)
{
    float lim, sec;
    float *life = &en_balloon_balloon_frm_life;

    sec = FPS;
    w->t += 1.0f;
    lim = *life * sec;
    if (lim < w->t) {
        FINISH_IF(w);
        return;
    }
    w->color = (w->color & 0xffffff) | (((int)((1.0f - w->t / lim) * 255.0f) + 1) >> 1) << 24;
    wire_break_step_default(w->wb, 0, 0.98f);
}

/* 0x00b00150 */
void en_balloon_balloon_disp(BalloonBurstWork *w)
{
#ifdef VERSION_US
    draw_set_blend(4, 1);
#else
    draw_set_blend(4, 5);
#endif
    wire_break_draw(w->wb, w->color);
}

/* 0x00b00188 */
void en_balloon_balloon_init(BalloonBurstWork *w, Vec3 *pos, void *src, unsigned int color)
{
    w->color = color;
    en_balloon_balloon_init_edges(w, model_pnb_get_wire(stage_models, 12), pos, src);
#ifdef VERSION_US
    task_set_dlist(w, 6);
#endif
}

/* 0x00b001f0 */
void en_balloon_balloon_release(BalloonBurstWork *w)
{
    en_balloon_balloon_free(w);
}

/* 0x00b001f8 */
BalloonBurstWork *en_balloon_balloon_create(void *parent, Vec3 *pos, void *src, unsigned int color)
{
    BalloonBurstWork *w = task_create_sibling_inherit(parent, &en_balloon_balloon_desc, 564);

    en_balloon_balloon_init(w, pos, src, color);
    return w;
}

/* 0x00b00268 */
int balloon_ctrlbar_eye_outside(BalloonCtrlbarWork *w)
{
    Vec3 eye;
    float scl;

    camera_get_eye(&eye);
    scl = camera_get_far();
    return scl * scl < vec3_distance_squared(&eye, &w->pos) ? 1 : 0;
}

/* 0x00b002c8 */
void balloon_ctrlbar_state_fall(BalloonCtrlbarWork *w)
{
    int to[3];

    switch ((unsigned int)w->cnt) {
    case 0:
        w->cnt++;
        w->cnt_end++;
    case 1:
        w->fall += 0.0055555557f;
        w->pos.y -= w->fall;
        v3_scale(&w->vel, 0.98f);
        i3_set(to, ANG_90, 0, 0);
        v3_add(&w->pos, &w->vel);
        ang3_approach(to, w->ang, w->turn);
        if (balloon_ctrlbar_eye_outside(w) && w)
            FINISH(w);
        if (w->prev != w->state || w->cnt != w->cnt_end)
            w->cnt++;
        break;
    case 2:
        break;
    }
}

/* 0x00b00410 */
void balloon_ctrlbar_exec(BalloonCtrlbarWork *w)
{
    Vec3 shift;

    player_get_frame_vel(&shift);
    v3_add(&w->pos, &shift);
    do {
        w->prev = w->state;
        w->cnt = w->cnt_end;
    } while (0);
    if (balloon_ctrlbar_states[w->state])
        balloon_ctrlbar_states[w->state](w);
}

/* steer(cast) */
/* 0x00b004a8 */
void balloon_ctrlbar_disp(BalloonCtrlbarWork *w)
{
    Vec3 *pos = &w->pos;
    int blend;

    if (!view_sphere_visible(pos, w->part.radius))
        return;
    ulGxSetTextureList(stage_textures);
    blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_strip_flags();
    draw_push_override_color();
    ulMatrixPush();
    do {
        Vec3 *p_ = pos;

        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    en_balloon_balloon_rotate_yxz(w->ang);
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    draw_set_override_color(balloon_ctrlbar_color);
    dc_object_draw(&w->part);
    ulMatrixPop();
    draw_pop_override_color();
    draw_pop_strip_flags();
    draw_set_override_flags(blend);
}

/* steer(cast) */
/* 0x00b00598 */
void balloon_ctrlbar_init(BalloonCtrlbarWork *w, Vec3 *pos, int *ang)
{
    float sc[3][2];
    float x, y, z;

    v3_copy(&w->pos, pos);
    i3_copy(w->ang, ang);
    x = 0.0f;
    y = 0.0f;
    z = RAND_FLOAT_A() + 0.0f;
    ROT_Z(ang[2], sc[0], x, y);
    ROT_X(ang[0], sc[1], y, z);
    ROT_Y(ang[1], sc[2], x, z);
    v3_set(&w->vel, x, 0, z);
    w->turn = RAND_FLOAT_A() * 0.003f + 0.001f;
    dc_object_init(&w->part, *((struct DcNode **)pnb_get_entry(stage_models, 9)), *((struct DcClip **)pnb_get_entry(stage_models, 10)), 0);
    dc_object_rebuild(&w->part);
    w->part.frame = w->part.frames;
}

/* 0x00b00788 */
void balloon_ctrlbar_release(BalloonCtrlbarWork *w)
{
    dc_object_release(&w->part);
}

/* 0x00b00790 */
BalloonCtrlbarWork *balloon_ctrlbar_create(void *parent, Vec3 *pos, int *ang)
{
    BalloonCtrlbarWork *w = task_create_sibling_inherit(parent, &balloon_ctrlbar_desc, 168);

    balloon_ctrlbar_init(w, pos, ang);
    return w;
}

/* steer(cast) */
/* 0x00b007f0 */
Shatter *en_balloon_balloon_spawn_debris(void *obj, int piece)
{
    SpawnDesc desc;
    ShatterParams prm;
    float mtx[2][16];
    Shatter *shat;

    nj_fill8(&desc, 0, sizeof desc);
    nj_fill8(&prm, 0, sizeof prm);
    desc.kind = 0x90000;
    desc.type = en_balloon_debris_pieces[piece].type;
    desc.models = stage_models;
    desc.v.x = 0;
    prm.Gravity = 10.0f;
    prm.PowVec = 0.9f;
    prm.PowAng = 6.7f;
    desc.v.y = 0;
    prm.RtoVecDec = 0.94f;
    desc.v.z = 0;
    prm.RtoAngDec = 0.98f;
    prm.RotSpdPlayer = 1.0f;
    prm.TimeAlphaStart = 1.0f;
    prm.force_wire = 0;
    prm.b_acc = 0;
    prm.TimeAlphaEnd = 80.0f;
    prm.pcol_wire = RGBA(0x34, 0x0f, 0, 0x80);
    prm.frm_acc = 0;
    anim_start((void *)*((int *)pnb_get_entry(stage_models, 5)), en_balloon_debris_pieces[piece].time);
    node_tree_draw_matching_at(*((int *)pnb_get_entry(stage_models, 4)), 0x2000000, 2, (int)mtx, 1);
    ulMatrixPush();
    vu0_matrix_mul(ulMatrixCur, ulMatrixCur, mtx[piece]);
    shat = enemy_shatter_create(obj, &desc, (float *)&prm, 0);
    ulMatrixPop();
    return shat;
}

/* steer(cast) */
/* 0x00b00998 */
void en_balloon_balloon_nodes_collect(void)
{
    node_tree_collect_to((void *)*((int *)pnb_get_entry(stage_models, 4)), 0x2000000, &en_balloon_balloon_nodes);
}

/* 0x00b009d0 */
void en_balloon_balloon_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
