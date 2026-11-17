/*
 * Land waves: music-driven line waves along the stage, one spawned at each marker node the player
 * passes: a line strip with heights from the sequencer's pulse levels, blended with the previous
 * wave, faded by distance. The look depends on the scene; each overlay draws its own style.
 */

#include "types_common.h"
#include "mathdef.h"

#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
#include "color.h"
#include "heap.h"
#include "line_batch.h"
#include "map_manager.h"
#include "math_util.h"
#include "model.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "player.h"
#include "player_status.h"
#include "sound_midiseq.h"
#include "task.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "land_wave.h"
#include "system.h"

static inline void v3_add(Vec3 *dst, Vec3 *src)
{
    dst->x += src->x;
    dst->y += src->y;
    dst->z += src->z;
}

#define ROT_X(ang, sc, y, z) \
    do { \
        float rot_u = y, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        y = rot_u * sc[1] - rot_v * sc[0]; \
        z = rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

#define ROT_Y(ang, sc, x, z) \
    do { \
        float rot_u = x, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] + rot_v * sc[0]; \
        z = -rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

static inline void v3_sub(Vec3 *dst, Vec3 *lhs, Vec3 *rhs)
{
    dst->x = lhs->x - rhs->x;
    dst->y = lhs->y - rhs->y;
    dst->z = lhs->z - rhs->z;
}

static inline void i3_copy(int *dst, int *src)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

extern int ulMatrixPush(void);

#define MAX_GE(a, b) ((a) >= (b) ? (a) : (b))

int map_get_current(void);

void *map_menu_get(void);

extern struct TaskDesc land_wave_desc;
extern struct TaskDesc land_wave_mng_desc;
extern int land_wave_path_offsets[];
extern struct LandWaveMngWork *land_wave_mng;
extern struct LandWaveWork *land_wave_prev;
extern unsigned char note_levels[10];

/* 0x00af7158 stg01, 0x00af75d8 stg02, 0x00af7018 stg03, 0x00af7258 stg04, 0x00af6ed0 stg23 */
/* steer(cast) */
void land_wave_heights(float *out)
{
    Vec3 cp[4];
    Vec3 pt;
    float hscl = 0.1f;
    float ex = 1.0f;
    float dscl = 0.0003f;
    float dt = 0.33333334f;
    float t;
    int i, j;

    i = 0;
    do {
        cp[0].x = 0;
        cp[0].y = note_levels[i];
        cp[1].y = note_levels[i];
        cp[3].y = note_levels[i + 1];
        cp[2].y = note_levels[i + 1];
        cp[0].z = 0;
        cp[1].x = ex;
        cp[1].z = 0;
        cp[3].x = ex;
        cp[3].z = 0;
        cp[2].x = 0;
        cp[2].z = 0;
        t = cp[0].x;
        j = 2;
        do {
            ((void (*)(float, Vec3 *, Vec3 *, int))bezier_cubic_eval)(t, cp, &pt, 0);
            t += dt;
            j--;
            *out++ = pt.y * hscl;
            hscl += dscl;
        } while (j >= 0);
        i++;
    } while (i < 9);
    *out = note_levels[9] * hscl;
}

/* 0x00af72d8 stg01, 0x00af7758 stg02, 0x00af7198 stg03, 0x00af73d8 stg04, 0x00af7050 stg23 */
void land_wave_init(LandWaveWork *w, LandWaveArg *prm)
{
    Vec3 pl;
    float *dst;
    float *src;
    int n;

    player_get_pos(&pl);
    w->arg = prm;
    w->src = *prm;
    i3_copy(w->ang, w->src.ang);
    v3_sub(&w->pos, &w->src.pos, &pl);
    land_wave_heights(w->level);
    if (land_wave_prev) {
        dst = w->level;
        src = land_wave_prev->level;
        n = 27;
        do {
            n--;
            *dst = *dst * 0.3f + *src++ * 0.7f;
            dst++;
        } while (n >= 0);
    }
    land_wave_prev = w;
    task_set_dlist(w, 4);
}

/* 0x00af7428 stg01, 0x00af78a8 stg02, 0x00af72e8 stg03, 0x00af7528 stg04, 0x00af71a0 stg23 */
void land_wave_release(LandWaveWork *w)
{
    if (land_wave_prev == w)
        land_wave_prev = 0;
}

/* 0x00af7440 stg01, 0x00af78c0 stg02, 0x00af7300 stg03, 0x00af7540 stg04, 0x00af71b8 stg23 */
void land_wave_exec(LandWaveWork *w)
{
    int ang[3];
    Vec3 shift;
    float sc[2][2];
    float x, y, z;
    Vec3 *pos;

    if (!(land_wave_mng->flags & 2)) {
        player_get_accel(&shift);
        v3_sub(&w->pos, &w->pos, &shift);
        pos = &w->pos;
        y = pos->y;
        z = pos->z;
        x = w->pos.x;
        do { player_get_rot(ang); } while (0);
        ROT_Y(-ang[1], sc[0], x, z);
        ROT_X(-ang[0], sc[1], y, z);
        if (z > 0.0f)
            FINISH_IF(w);
    }
}

/* 0x00af7558 stg01, 0x00af79d8 stg02, 0x00af7418 stg03, 0x00af7658 stg04, 0x00af72d0 stg23 */
#if defined(OVERLAY_STG01)
/* steer(cast) */
void land_wave_disp(LandWaveWork *w)
{
    float *bias = 0;
    Vec3 pa;
    Vec3 pb;
    Vec3 org;
    int ang[3];
    float sc[2][2];
    float x, y, z, alp, full;
    int r, g, b;
    void *ln;
    Vec3 *pos;
    Vec3 *cur = &pa, *prev = &pb, *swp;
    float *lv = w->level;
    int n;

    if (land_wave_mng)
        bias = land_wave_mng->bias;
    ulMatrixPush();
    player_get_pos(&org);
    v3_add(&org, &w->pos);
    do {
        Vec3 *p_ = &org;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    do { land_wave_rotate_zyx(w->ang); } while (0);
    full = 1.0f;
    do { vu0_matrix_scale(ulMatrixCur, *(float *)&w->src.scl[0], full, full); } while (0);
    pos = &w->pos;
    x = w->pos.x;
    y = pos->y;
    z = pos->z;
    do { player_get_rot(ang); } while (0);
    ROT_Y(-ang[1], sc[0], x, z);
    ROT_X(-ang[0], sc[1], y, z);
    if (z > -50.0f)
        alp = full - (z + 50.0f) / 80.0f;
    else if (z < -450.0f)
        alp = (z + 500.0f) / 50.0f;
    else
        alp = full;
    alp = MIN(alp >= 0.0f ? alp : 0.0f, 1.0f);
    draw_set_blend(4, 5);
    r = color_get_r(255);
    g = color_get_g(255);
    b = color_get_b(255);
    ln = line_set_color(((int)(alp * 255.0f) + 1) >> 1 << 24 | ((r & 0xff) | ((g & 0xff) << 8 | (b & 0xff) << 16)));
    prev->z = cur->z = 0.0f;
    prev->x = -1.0f;
    prev->y = *lv++ + (land_wave_mng ? *bias++ : 0.0f);
    n = 26;
    do {
        swp = cur;
        cur = prev;
        prev = swp;
        prev->x = cur->x + 0.074074075f;
        prev->y = *lv++ + (land_wave_mng ? *bias++ : 0.0f);
        ln = line_add_v(ln, cur, prev);
        n--;
    } while (n >= 0);
    ((void (*)())line_flush)();
    ulMatrixPop();
}
#elif defined(OVERLAY_STG03)
/* steer(cast) */
void land_wave_disp(LandWaveWork *w)
{
    float *bias = 0;
    Vec3 pa;
    Vec3 pb;
    Vec3 org;
    int ang[3];
    float sc[2][2];
    float x, y, z, alpha, full;
    int r, g, b;
    void *ln;
    Vec3 *pos;
    Vec3 *cur = &pa, *prev = &pb, *swp;
    float *lv = w->level;
    int n;

    if (land_wave_mng)
        bias = land_wave_mng->bias;
    ulMatrixPush();
    player_get_pos(&org);
    v3_add(&org, &w->pos);
    do {
        Vec3 *p_ = &org;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    do { land_wave_rotate_zyx(w->ang); } while (0);
    full = 1.0f;
    do { vu0_matrix_scale(ulMatrixCur, *(float *)&w->src.scl[0], full, full); } while (0);
    pos = &w->pos;
    x = w->pos.x;
    y = pos->y;
    z = pos->z;
    do { player_get_rot(ang); } while (0);
    ROT_Y(-ang[1], sc[0], x, z);
    ROT_X(-ang[0], sc[1], y, z);
    if (z > -50.0f)
        alpha = full - (z + 50.0f) / 80.0f;
    else if (z < -950.0f)
        alpha = (z + 1000.0f) / 50.0f;
    else
        alpha = full;
    alpha = MIN(alpha >= 0.0f ? alpha : 0.0f, 1.0f);
    draw_set_blend(4, 5);
    r = ((unsigned int (*)(int))color_get_r)(0x325600);
    g = color_get_g(0x325600);
    b = color_get_b(0x325600);
    ln = line_set_color(((int)(alpha * 255.0f) + 1) >> 1 << 24 | ((r & 0xff) | ((g & 0xff) << 8 | (b & 0xff) << 16)));
    prev->z = cur->z = 0.0f;
    prev->x = -1.0f;
    prev->y = *lv++ + (land_wave_mng ? *bias++ : 0.0f);
    n = 26;
    do {
        swp = cur;
        cur = prev;
        prev = swp;
        prev->x = cur->x + 0.074074075f;
        prev->y = *lv++ + (land_wave_mng ? *bias++ : 0.0f);
        ln = line_add_v(ln, cur, prev);
        n--;
    } while (n >= 0);
    ((void (*)())line_flush)();
    ulMatrixPop();
}
#elif defined(OVERLAY_STG04)
/* steer(cast) */
void land_wave_disp(LandWaveWork *w)
{
    float *bias = 0;
    Vec3 pa;
    Vec3 pb;
    Vec3 org;
    int ang[3];
    float sc[2][2];
    float x, y, z, alpha, full, lz;
    int r, g, b;
    void *ln;
    Vec3 *pos;
    Vec3 *cur = &pa, *prev = &pb, *swp;
    float *lv = w->level;
    int n;

    if (land_wave_mng)
        bias = land_wave_mng->bias;
    ulMatrixPush();
    player_get_pos(&org);
    v3_add(&org, &w->pos);
    do {
        Vec3 *p_ = &org;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    do { land_wave_rotate_zyx(w->ang); } while (0);
    full = 1.0f;
    do { vu0_matrix_scale(ulMatrixCur, *(float *)&w->src.scl[0], full, full); } while (0);
    pos = &w->pos;
    x = w->pos.x;
    y = pos->y;
    z = pos->z;
    do { player_get_rot(ang); } while (0);
    ROT_Y(-ang[1], sc[0], x, z);
    ROT_X(-ang[0], sc[1], y, z);
    if (z > -50.0f)
        alpha = full - (z + 50.0f) / 80.0f;
    else if (z < -950.0f)
        alpha = (z + 1000.0f) / 50.0f;
    else
        alpha = full;
    alpha = MIN(alpha >= 0.0f ? alpha : 0.0f, 1.0f);
    draw_set_blend(4, 5);
    switch (map_get_current()) {
    case 16:
        ln = line_set_color(((int)(alpha * 255.0f) + 1) >> 1 << 24 | (((unsigned int (*)(int))color_get_r)(0x415f00) & 0xff |
                          ((((unsigned int (*)(int))color_get_g)(0x415f00) & 0xff) << 8 | (color_get_b(0x415f00) & 0xff) << 16)));
        break;
    case 10:
        ln = line_set_color(((int)(alpha * 255.0f) + 1) >> 1 << 24 | (((unsigned int (*)(int))color_get_r)(RGBA(0x62, 0x1a, 0, 0x80)) & 0xff |
                          ((((unsigned int (*)(int))color_get_g)(RGBA(0x62, 0x1a, 0, 0x80)) & 0xff) << 8 | (color_get_b(RGBA(0x62, 0x1a, 0, 0x80)) & 0xff) << 16)));
        break;
    default:
        ln = line_set_color(((int)(alpha * 255.0f) + 1) >> 1 << 24 | (((unsigned int (*)(int))color_get_r)(RGBA(0x4a, 0x22, 0, 0x80)) & 0xff |
                          ((((unsigned int (*)(int))color_get_g)(RGBA(0x4a, 0x22, 0, 0x80)) & 0xff) << 8 | (color_get_b(RGBA(0x4a, 0x22, 0, 0x80)) & 0xff) << 16)));
        break;
    }
    lz = 0.0f;
    cur->z = lz;
    prev->x = -1.0f;
    prev->z = lz;
    prev->y = *lv++ + (land_wave_mng ? *bias++ : lz);
    n = 26;
    do {
        swp = cur;
        cur = prev;
        prev = swp;
        prev->x = cur->x + 0.074074075f;
        prev->y = *lv++ + (land_wave_mng ? *bias++ : 0.0f);
        ln = line_add_v(ln, cur, prev);
        n--;
    } while (n >= 0);
    ((void (*)())line_flush)();
    ulMatrixPop();
}
#elif defined(OVERLAY_STG23)
/* steer(cast) */
void land_wave_disp(LandWaveWork *w)
{
    float *bias = 0;
    Vec3 pa;
    Vec3 pb;
    Vec3 org;
    int ang[3];
    float sc[2][2];
    float x, y, z, alpha, full;
    int r, g, b;
    void *ln;
    Vec3 *pos;
    Vec3 *cur = &pa, *prev = &pb, *swp;
    float *lv = w->level;
    int n;

    if (land_wave_mng)
        bias = land_wave_mng->bias;
    ulMatrixPush();
    player_get_pos(&org);
    v3_add(&org, &w->pos);
    do {
        Vec3 *p_ = &org;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    do { land_wave_rotate_zyx(w->ang); } while (0);
    full = 1.0f;
    do { vu0_matrix_scale(ulMatrixCur, *(float *)&w->src.scl[0], full, full); } while (0);
    pos = &w->pos;
    x = w->pos.x;
    y = pos->y;
    z = pos->z;
    do { player_get_rot(ang); } while (0);
    ROT_Y(-ang[1], sc[0], x, z);
    ROT_X(-ang[0], sc[1], y, z);
    if (z > -50.0f)
        alpha = full - (z + 50.0f) / 80.0f;
    else if (z < -450.0f)
        alpha = (z + 500.0f) / 50.0f;
    else
        alpha = full;
    alpha = MIN(alpha >= 0.0f ? alpha : 0.0f, 1.0f);
    draw_set_blend(4, 5);
    r = color_get_r(128);
    g = color_get_g(128);
    b = color_get_b(128);
    ln = line_set_color(((int)(alpha * 255.0f) + 1) >> 1 << 24 | (((r + 1) >> 1 & 0xff) | (((g + 1) >> 1 & 0xff) << 8 | ((b + 1) >> 1 & 0xff) << 16)));
    prev->z = cur->z = 0.0f;
    prev->x = -1.0f;
    prev->y = *lv++ + (land_wave_mng ? *bias++ : 0.0f);
    n = 26;
    do {
        swp = cur;
        cur = prev;
        prev = swp;
        prev->x = cur->x + 0.074074075f;
        prev->y = *lv++ + (land_wave_mng ? *bias++ : 0.0f);
        ln = line_add_v(ln, cur, prev);
        n--;
    } while (n >= 0);
    ((void (*)())line_flush)();
    ulMatrixPop();
}
#else
/* steer(cast) */
void land_wave_disp(LandWaveWork *w)
{
    float *lv = w->level;
    float *bias = 0;
    Vec3 pa;
    Vec3 pb;
    Vec3 org;
    int ang[3];
    float sc[2][2];
    float x, y, z, full, alpha;
    void *ln = 0;
    Vec3 *cur = &pa, *prev = &pb, *swp;
    Vec3 *wp;
    int n;

    if (land_wave_mng)
        bias = land_wave_mng->bias;
    ulMatrixPush();
    player_get_pos(&org);
    v3_add(&org, &w->pos);
    wp = &w->pos;
    do {
        Vec3 *p_ = &org;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    land_wave_rotate_zyx(w->ang);
    full = 1.0f;
    vu0_matrix_scale(ulMatrixCur, *(float *)&w->src.scl[0], full, full);
    x = w->pos.x;
    do { z = wp->z; } while (0);
    y = wp->y;
    player_get_rot(ang);
    ROT_Y(-ang[1], sc[0], x, z);
    ROT_X(-ang[0], sc[1], y, z);
    if (z > -50.0f)
        alpha = full - (z + 50.0f) / 80.0f;
    else if (z < -950.0f)
        alpha = (z + 1000.0f) / 50.0f;
    else
        alpha = full;
    alpha = MIN(MAX_GE(alpha, 0.0f), 1.0f);
    x = alpha;
    draw_set_blend(4, 5);
    switch (map_get_current()) {
    case 2:
    case 4:
    case 6:
        ln = line_set_color((((int)(x * 255.0f) + 1) >> 1 << 24) |
                           (((unsigned int (*)(int))color_get_b)(RGBA(0x38, 0x2e, 0x64, 0x80)) & 0xff) << 16 | (color_get_g(RGBA(0x38, 0x2e, 0x64, 0x80)) & 0xff) << 8 | (((unsigned int (*)(int))color_get_r)(RGBA(0x38, 0x2e, 0x64, 0x80)) & 0xff));
        break;
    case 16:
        ln = line_set_color((((int)(x * 255.0f) + 1) >> 1 << 24) |
                           (((unsigned int (*)(int))color_get_b)(RGBA(0x1d, 0x3b, 0x59, 0x80)) & 0xff) << 16 | (color_get_g(RGBA(0x1d, 0x3b, 0x59, 0x80)) & 0xff) << 8 | (((unsigned int (*)(int))color_get_r)(RGBA(0x1d, 0x3b, 0x59, 0x80)) & 0xff));
        break;
    case 20:
        ln = line_set_color((((int)(x * 255.0f) + 1) >> 1 << 24) |
                           (((unsigned int (*)(int))color_get_b)(RGBA(0x1d, 0x3b, 0x59, 0x80)) & 0xff) << 16 | (color_get_g(RGBA(0x1d, 0x3b, 0x59, 0x80)) & 0xff) << 8 | (((unsigned int (*)(int))color_get_r)(RGBA(0x1d, 0x3b, 0x59, 0x80)) & 0xff));
        break;
    }
    prev->z = cur->z = 0.0f;
    prev->x = -1.0f;
    prev->y = *lv++ + (land_wave_mng ? *bias++ : 0.0f);
    n = 26;
    do {
        swp = cur;
        cur = prev;
        prev = swp;
        prev->x = cur->x + 0.074074075f;
        prev->y = *lv++ + (land_wave_mng ? *bias++ : 0.0f);
        ln = line_add_v(ln, cur, prev);
        n--;
    } while (n >= 0);
    line_flush(ln);
    ulMatrixPop();
}
#endif

/* 0x00af78c8 stg01, 0x00af7e68 stg02, 0x00af7798 stg03, 0x00af7a98 stg04, 0x00af7650 stg23 */
void *land_wave_create(void *parent, void *rec)
{
    return task_create_inherit_arg(parent, &land_wave_desc, 180, rec);
}

/* 0x00af78f0 stg01, 0x00af7e90 stg02, 0x00af77c0 stg03, 0x00af7ac0 stg04, 0x00af7678 stg23 */
void land_wave_mng_count_nodes(unsigned int *node, int *count)
{
    do {
        (*count)++;
        if (!(node[0] & 0x10))
            land_wave_mng_count_nodes((unsigned int *)node[11], count);
        node = (unsigned int *)node[12];
    } while (node);
}

/* 0x00af7950 stg01, 0x00af7ef0 stg02, 0x00af7820 stg03, 0x00af7b20 stg04, 0x00af76d8 stg23 */
/* steer(cast) */
int land_wave_mng_record_nodes(unsigned int *node, int **out, int idx)
{
    int *rec;

    do {
        ulMatrixPush();
        anim_apply_node(node);
        ulMatrixGetTranslation((float *)ulMatrixCur, (float *)*out);
        ((void (*)(int *))model_camera_angles)(*out + 3);
        rec = *out;
        rec[7] = idx;
        *(float *)&rec[6] = *(float *)&node[8];
        idx++;
        *out = rec + 8;
        if (!(node[0] & 0x10))
            idx = land_wave_mng_record_nodes((unsigned int *)node[11], out, idx);
        ulMatrixPop();
        node = (unsigned int *)node[12];
    } while (node);
    return idx;
}

/* 0x00af7a10 stg01, 0x00af7fb0 stg02, 0x00af78e0 stg03, 0x00af7be0 stg04, 0x00af7798 stg23 */
int land_wave_mng_next_node(float *recs, int n, Vec3 *pl, float rsq)
{
    float *rec = recs + 8;
    int i;

    pl->y = rec[1];
    for (i = 1; i < n; i++, rec += 8) {
        if (pl->z > rec[2] && vec3_distance_squared((struct Vec3 *)rec, pl) > rsq)
            return i;
    }
    return 0;
}

/* 0x00af7ab8 stg01, 0x00af8058 stg02, 0x00af7988 stg03, 0x00af7c88 stg04, 0x00af7840 stg23 */
void land_wave_mng_load_path(LandWaveMngWork *w)
{
    int *rec;
    ModelPart *part;

    if (w->recs) {
        yuki_free(w->recs);
        w->recs = 0;
        dc_object_release(&w->part);
    }
    w->restart = 1;
    w->last = 0;
    if (w->variant > 0) {
        part = &w->part;
        dc_object_init(part, (int)w->models + land_wave_path_offsets[w->variant], 0, 0);
        w->nrec = 0;
        land_wave_mng_count_nodes((unsigned int *)part->root, &w->nrec);
        rec = yuki_alloc(w->nrec << 5);
        w->recs = rec;
        ulMatrixPush();
        anim_start((void *)part->clip, 0.0f);
        matrix_unit_set(0);
        land_wave_mng_record_nodes((unsigned int *)part->root, &rec, 0);
        ulMatrixPop();
    }
}

/* 0x00af7b98 stg01, 0x00af8138 stg02, 0x00af7a68 stg03, 0x00af7d68 stg04, 0x00af7920 stg23 */
#if defined(OVERLAY_STG01) || defined(OVERLAY_STG03)
void land_wave_mng_init(LandWaveMngWork *w)
{
    w->recs = 0;
    w->variant = 1;
    land_wave_mng_load_path(w);
    w->fade = 0;
    land_wave_mng = w;
    heap_bzero(w->bias, sizeof w->bias);
    w->flags = 0;
}
#else
void land_wave_mng_init(LandWaveMngWork *w)
{
    w->variant = 0;
    w->recs = 0;
    land_wave_mng_load_path(w);
    w->fade = 0;
    land_wave_mng = w;
    heap_bzero(w->bias, sizeof w->bias);
    w->flags = 0;
}
#endif

/* 0x00af7be8 stg01, 0x00af8180 stg02, 0x00af7ab8 stg03, 0x00af7db0 stg04, 0x00af7968 stg23 */
void land_wave_mng_release(LandWaveMngWork *w)
{
    if (w->recs) {
        yuki_free(w->recs);
        w->recs = 0;
        do { dc_object_release(&w->part); } while (0);
    }
    land_wave_mng = 0;
    land_wave_prev = 0;
}

/* 0x00af7c40 stg01, 0x00af81d8 stg02, 0x00af7b10 stg03, 0x00af7e08 stg04, 0x00af79c0 stg23 */
#if defined(OVERLAY_STG01)
/* steer(cast) */
void land_wave_mng_exec(LandWaveMngWork *w)
{
    int scene = map_get_current();
    Vec3 dir, rpos, pl;
    int i, k, n;

    w->flags <<= 1;
    if (player_status_test_flags(8))
        w->flags |= 1;
    player_get_vec(&dir);
    if (dir.z > 0.0f)
        w->last = 0;
    if (!map_manager_scene_flag((int)map_menu_get())) {
        land_wave_mng_set_variant(0);
        task_end_children(w);
        return;
    }
    if (scene < 2)
        land_wave_mng_set_variant(1);
    else if (scene < 6)
        land_wave_mng_set_variant(2);
    else if (scene < 12)
        land_wave_mng_set_variant(3);
    else if (scene < 14)
        land_wave_mng_set_variant(0);
    else if (scene < 16)
        land_wave_mng_set_variant(4);
    else if (scene < 17)
        land_wave_mng_set_variant(0);
    else
        land_wave_mng_set_variant(5);
    if (w->variant > 0) {
        nj_copy8(note_levels, ((unsigned char *(*)(int))midiseq_get_pulse_levels)(0), 10);
        if ((w->flags & 2) || w->restart) {
            k = 1;
            task_end_children(w);
            player_get_pos(&rpos);
            n = land_wave_mng_next_node((float *)w->recs, w->nrec, &rpos, 250000.0f);
            if (n < 1) {
                w->last = n;
            } else {
                do
                    land_wave_create(w, (char *)w->recs + k++ * 32);
                while (k <= n);
                w->last = n;
            }
            w->restart = 0;
        } else {
            player_get_pos(&pl);
            i = land_wave_mng_next_node((float *)w->recs, w->nrec, &pl, 250000.0f);
            if (i > w->last && i > 0) {
                land_wave_create(w, (char *)w->recs + i * 32);
                w->last = i;
            }
        }
    }
}
#elif defined(OVERLAY_STG03)
/* steer(cast) */
void land_wave_mng_exec(LandWaveMngWork *w)
{
    int scene = map_get_current();
    Vec3 dir, rpos, pl;
    int i, k, n;

    w->flags <<= 1;
    if (player_status_test_flags(8))
        w->flags |= 1;
    player_get_vec(&dir);
    if (dir.z > 0.0f)
        w->last = 0;
    if (!map_manager_scene_flag((int)map_menu_get()) || map_get_current() == 21) {
        land_wave_mng_set_variant(0);
        task_end_children(w);
        return;
    }
    switch (scene) {
    case 2:
    case 4:
        land_wave_mng_set_variant(1);
        break;
    case 12:
        land_wave_mng_set_variant(2);
        break;
    case 14:
        land_wave_mng_set_variant(3);
        break;
    case 16:
        land_wave_mng_set_variant(4);
        break;
    case 18:
        land_wave_mng_set_variant(5);
        break;
    default:
        land_wave_mng_set_variant(0);
        break;
    }
    if (w->variant > 0) {
        nj_copy8(note_levels, ((unsigned char *(*)(int))midiseq_get_pulse_levels)(0), 10);
        if ((w->flags & 2) || w->restart) {
            k = 1;
            task_end_children(w);
            player_get_pos(&rpos);
            n = land_wave_mng_next_node((float *)w->recs, w->nrec, &rpos, 1000000.0f);
            if (n < 1) {
                w->last = n;
            } else {
                do
                    land_wave_create(w, (char *)w->recs + k++ * 32);
                while (k <= n);
                w->last = n;
            }
            w->restart = 0;
        } else {
            player_get_pos(&pl);
            i = land_wave_mng_next_node((float *)w->recs, w->nrec, &pl, 1000000.0f);
            if (i > w->last && i > 0) {
                land_wave_create(w, (char *)w->recs + i * 32);
                w->last = i;
            }
        }
    }
}
#elif defined(OVERLAY_STG04)
/* steer(cast) */
void land_wave_mng_exec(LandWaveMngWork *w)
{
    int scene = map_get_current();
    Vec3 dir, rpos, pl;
    int i, k, n;

    w->flags <<= 1;
    if (player_status_test_flags(8))
        w->flags |= 1;
    player_get_vec(&dir);
    if (dir.z > 0.0f)
        w->last = 0;
    if (!map_manager_scene_flag((int)map_menu_get()) || map_get_current() == 21) {
        land_wave_mng_set_variant(0);
        task_end_children(w);
        return;
    }
    switch (scene) {
    case 2:
        land_wave_mng_set_variant(1);
        break;
    case 4:
        land_wave_mng_set_variant(2);
        break;
    case 10:
        land_wave_mng_set_variant(3);
        break;
    case 16:
        land_wave_mng_set_variant(4);
        break;
    default:
        land_wave_mng_set_variant(0);
        break;
    }
    if (w->variant > 0) {
        nj_copy8(note_levels, ((unsigned char *(*)(int))midiseq_get_pulse_levels)(0), 10);
        if ((w->flags & 2) || w->restart) {
            k = 1;
            task_end_children(w);
            player_get_pos(&rpos);
            n = land_wave_mng_next_node((float *)w->recs, w->nrec, &rpos, 1000000.0f);
            if (n < 1) {
                w->last = n;
            } else {
                do
                    land_wave_create(w, (char *)w->recs + k++ * 32);
                while (k <= n);
                w->last = n;
            }
            w->restart = 0;
        } else {
            player_get_pos(&pl);
            i = land_wave_mng_next_node((float *)w->recs, w->nrec, &pl, 1000000.0f);
            if (i > w->last && i > 0) {
                land_wave_create(w, (char *)w->recs + i * 32);
                w->last = i;
            }
        }
    }
}
#elif defined(OVERLAY_STG23)
/* steer(cast) */
void land_wave_mng_exec(LandWaveMngWork *w)
{
    Vec3 dir, rpos, pl;
    int i, k, n;

    w->flags <<= 1;
    if (player_status_test_flags(8))
        w->flags |= 1;
    player_get_vec(&dir);
    if (dir.z > 0.0f)
        w->last = 0;
    if (!map_manager_scene_flag((int)map_menu_get())) {
        land_wave_mng_set_variant(0);
        task_end_children(w);
        return;
    }
    land_wave_mng_set_variant(1);
    if (w->variant > 0) {
        nj_copy8(note_levels, ((unsigned char *(*)(int))midiseq_get_pulse_levels)(0), 10);
        if ((w->flags & 2) || w->restart) {
            k = 1;
            task_end_children(w);
            player_get_pos(&rpos);
            n = land_wave_mng_next_node((float *)w->recs, w->nrec, &rpos, 250000.0f);
            if (n < 1) {
                w->last = n;
            } else {
                do
                    land_wave_create(w, (char *)w->recs + k++ * 32);
                while (k <= n);
                w->last = n;
            }
            w->restart = 0;
        } else {
            player_get_pos(&pl);
            i = land_wave_mng_next_node((float *)w->recs, w->nrec, &pl, 250000.0f);
            if (i > w->last && i > 0) {
                land_wave_create(w, (char *)w->recs + i * 32);
                w->last = i;
            }
        }
    }
}
#else
/* steer(cast) */
void land_wave_mng_exec(LandWaveMngWork *w)
{
    int scene = map_get_current();
    Vec3 dir, rpos, pl;
    int i, k, n;

    w->flags <<= 1;
    if (player_status_test_flags(8))
        w->flags |= 1;
    player_get_vec(&dir);
    if (dir.z > 0.0f)
        w->last = 0;
    if (!map_manager_scene_flag((int)map_menu_get()) || map_get_current() == 21) {
        land_wave_mng_set_variant(0);
        task_end_children(w);
        return;
    }
    switch (scene) {
    case 2:
    case 4:
    case 6:
        land_wave_mng_set_variant(1);
        break;
    case 16:
        land_wave_mng_set_variant(2);
        break;
    case 20:
        land_wave_mng_set_variant(3);
        break;
    default:
        land_wave_mng_set_variant(0);
        break;
    }
    if (w->variant > 0) {
        nj_copy8(note_levels, ((unsigned char *(*)(int))midiseq_get_pulse_levels)(0), 10);
        if ((w->flags & 2) || w->restart) {
            k = 1;
            task_end_children(w);
            player_get_pos(&rpos);
            n = land_wave_mng_next_node((float *)w->recs, w->nrec, &rpos, 1000000.0f);
            if (n < 1) {
                w->last = n;
            } else {
                do
                    land_wave_create(w, (char *)w->recs + k++ * 32);
                while (k <= n);
                w->last = n;
            }
            w->restart = 0;
        } else {
            player_get_pos(&pl);
            i = land_wave_mng_next_node((float *)w->recs, w->nrec, &pl, 1000000.0f);
            if (i > w->last && i > 0) {
                land_wave_create(w, (char *)w->recs + i * 32);
                w->last = i;
            }
        }
    }
}
#endif

/* 0x00af7f00 stg01, 0x00af8400 stg02, 0x00af7d58 stg03, 0x00af8058 stg04, 0x00af7b78 stg23 */
void land_wave_mng_disp(void)
{
}

/* 0x00af7f08 stg01, 0x00af8408 stg02, 0x00af7d60 stg03, 0x00af8060 stg04, 0x00af7b80 stg23 */
void *land_wave_mng_create(void *parent)
{
    return task_create_inherit(parent, &land_wave_mng_desc, 248);
}

/* 0x00af7fa0 stg01, 0x00af8430 stg02, 0x00af7d88 stg03, 0x00af8088 stg04, 0x00af7ba8 stg23 */
void land_wave_mng_set_variant(int variant)
{
    if (land_wave_mng && land_wave_mng->variant != variant) {
        land_wave_mng->variant = variant;
        land_wave_mng_load_path(land_wave_mng);
    }
}

/* 0x00af7fd0 stg01, 0x00af8460 stg02, 0x00af7db8 stg03, 0x00af80b8 stg04, 0x00af7bd8 stg23 */
void land_wave_rotate_zyx(int *ang)
{
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
}
