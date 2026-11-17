/* Keyframe animation player: keyed channels, camera, node hierarchies, vertex/colour blends. */

#include "types_game.h"
#include "nj_camera.h"
#include "nj_matrix.h"
#include "anim.h"
#include "math_util.h"

typedef struct AnimHeaderOut {
    float *pos;
    float *view;
    short *roll;
    short *ang;
} AnimHeaderOut;

typedef struct AnimOutTab {
    IVec3 *pos;
    IVec3 *view;
    short *roll;
    short *ang;
} AnimOutTab;

typedef struct AnimCameraWork {
    IVec3 pos;
    IVec3 view;
    short roll;
    short ang;
    int mode;
    AnimOutTab tab;
} AnimCameraWork;

typedef struct AnimHeader {
    int chan_tbl;
    int unk_04; /* unread */
    unsigned short chan_flags;
    unsigned short flags;
} AnimHeader;

typedef struct AnimRecordBytes { char b[72]; } AnimRecordBytes;

typedef struct AnimKeyInt {
    unsigned int key;
    int val;
} AnimKeyInt;

typedef union AnimIntHalf { int ang; unsigned short ang_lo; } AnimIntHalf;

typedef struct AnimKeyInt3 {
    unsigned int key;
    AnimIntHalf val[3];
} AnimKeyInt3;

typedef struct AnimKeyCatmull {
    unsigned int key;
    float v[3];
} AnimKeyCatmull;

typedef struct AnimKeyHermite {
    unsigned int key;
    float v[3];
} AnimKeyHermite;

typedef struct AnimQuat { int w[4]; } AnimQuat;

#define NODE_ROT(n, rot, quat, ret)                                                         \
    {                                                                                        \
        if ((n)->flags & 0x400) {                                                            \
            (quat)[0] = (n)->w;                                                              \
            ret = 2;                                                                         \
            (quat)[1] = *(float *)&(n)->rot[0];                                              \
            (quat)[2] = *(float *)&(n)->rot[1];                                              \
            (quat)[3] = *(float *)&(n)->rot[2];                                              \
        } else {                                                                             \
            (rot)[0] = (n)->rot[0];                                                          \
            (rot)[1] = (n)->rot[1];                                                          \
            (rot)[2] = (n)->rot[2];                                                          \
            ret = (int)(n)->flags >> 5;                                                      \
            ret &= 1;                                                                        \
        }                                                                                    \
    }

typedef struct AnimCursorSetSrc {
    char *in0;                  /* 0 */
    int *out;                   /* 4 */
    char *in1;                  /* 8 */
} AnimCursorSetSrc;

typedef struct C17WNode C17WNode;

struct C17WNode {
    char pad[44];
    C17WNode *child;            /* 44 */
    C17WNode *next;             /* 48 */
};

typedef struct AnimCursorView {
    int *ptr;                   /* 0: read cursor, two-word steps */
    float time;                   /* 4 */
    int user;                     /* 8 */
    int nrec;                    /* 12 */
    void *track;                  /* 16 */
    int frame;                    /* 20 */
    int slot;                    /* 24 */
} AnimCursorView;

typedef struct AnimStartRec {
    int *ptr;
    int user;
} AnimStartRec;

typedef union {
    int i;
    float f;
} AnimWord;

typedef struct AnimCursorBytes { char b[28]; } AnimCursorBytes;
typedef struct AnimKey { unsigned int key; int val; } AnimKey;

typedef struct AnimTrack {
    AnimKey *keys;
    unsigned short key_cnt;
} AnimTrack;

typedef struct AnimSlotEnt {
    char pad_00[4];
    unsigned short slot;
    char pad_06[2];
} AnimSlotEnt;

typedef struct AnimCacheEnt {
    int frame;
    int val0;
    int val1;
    float frac;
} AnimCacheEnt;

extern AnimRecord *anim_cur_record;
extern AnimHeader anim_default_header;
extern AnimEvalFn anim_eval9_funcs[];
extern AnimEvalFn anim_eval10_funcs[];
extern AnimEvalFn anim_eval11_func;
extern AnimEvalFn anim_eval12_funcs[];
extern AnimRecordBytes anim_records[];
extern int anim_cur_user;

extern int anim_step_enable;
extern short anim_step_flags;
extern AnimCursorSet *anim_cursors;
extern AnimCursor *anim_cur_cursor;
extern AnimCursorBytes anim_cursor_table[];
extern char *anim_cache;
extern int **anim_slot_table;
static void anim_blend_frame(float *dst, float *pa, float *pb, int nf, float frac);

static void anim_load_header(float time, AnimHeader *src);
static void anim_select_record(int idx);
static int anim_play_vec3(void * out);
static int anim_play_s16(short * out);
static void anim_start_record(void *src, float t);
static int anim_eval_node(AnimNode *node, float *pos, int *rot, float *scale, float *quat);
static int anim_play_none(void);
static int anim_play_rot_none(void);
static void anim_eval_tree(C17WNode *node);
static void anim_cache_refresh(void);
static void anim_blend_keys(float *dst, unsigned char type);

/* Unkeyed channels default from src. Flag 0x40: view point relative to position. */
/* 0x0017ea68 */
void anim_eval_header(float t, float *src, void *hdr, AnimHeaderOut *out)
{
    short *val0;
    float *pos2;
    short *val1;
    float *pos;
    unsigned int flags;

    val1 = out->ang;
    pos2 = out->view;
    val0 = out->roll;
    pos = out->pos;

    anim_select_record(0);
    anim_load_header(t, hdr);
    flags = anim_cur_record->flags;
    if (flags & 1) {
        if (anim_play_vec3(pos) == 0) {
            pos[0] = src[0];
            pos[1] = src[1];
            pos[2] = src[2];
        }
    } else {
        pos[0] = src[0];
        pos[1] = src[1];
        pos[2] = src[2];
    }
    if (flags & 0x2022)
        anim_cur_record->idx++;
    if (flags & 4)
        anim_cur_record->idx++;
    if (flags & 0x40) {
        if (anim_play_vec3(pos2)) {
            pos2[0] = pos2[0] - pos[0];
            pos2[1] = pos2[1] - pos[1];
            pos2[2] = pos2[2] - pos[2];
        } else {
            pos2[0] = src[3];
            pos2[1] = src[4];
            pos2[2] = src[5];
        }
    } else if (flags & 8) {
        if (anim_play_vec3(pos2) == 0) {
            pos2[0] = src[3];
            pos2[1] = src[4];
            pos2[2] = src[5];
        }
    } else {
        pos2[0] = src[3];
        pos2[1] = src[4];
        pos2[2] = src[5];
    }
    if (flags & 0x8000)
        anim_cur_record->idx++;
    if (flags & 0x80) {
        if (anim_play_s16(val0) == 0)
            *val0 = ((short *)src)[12];
    }
    if (flags & 0x100) {
        if (anim_play_s16(val1) == 0)
            *val1 = ((short *)src)[13];
    } else {
        *val1 = ((short *)src)[13];
    }
}

/* 0x0017ec40 */
static void anim_camera_apply(float t, int *obj, int x)
{
    AnimCameraWork k;

    k.tab.pos = &k.pos;
    k.tab.view = &k.view;
    k.tab.roll = &k.roll;
    k.tab.ang = &k.ang;
    k.mode = obj[7];
    anim_eval_header(t, (float *)obj, (void *)x, (AnimHeaderOut *)&k.tab); /* steer(cast) */
    nj_camera_apply(&k.pos);
}

/* rec: {source, time}. */
/* 0x0017ec88 */
static void anim_camera_apply_rec(float t, int *rec)
{
    anim_camera_apply(t, (int *)rec[0], rec[1]);
}

/* rec: {defaults, header}. */
/* 0x0017ec98 */
static void anim_eval_header_rec(float t, void **rec, AnimHeaderOut *out)
{
    anim_eval_header(t, rec[0], rec[1], out);
}

/* 0x0017eca8 */
static void anim_record_size(int n)
{
    AnimStream *strm = anim_cur_record;

    n = n << 2;
    strm->stride = n << 1;
    strm->len = n;
    strm->start = strm->pos;
    strm->end = strm->pos + n;
    strm->idx = 0;
}

/* 0x0017ecd8 */
void anim_record_advance(void)
{
    AnimStream *strm = anim_cur_record;
    int pos = strm->pos + strm->stride;

    strm->pos = pos;
    strm->start = pos;
    strm->end = pos + strm->len;
    strm->idx = 0;
}

/* src null -> anim_default_header. Flag bits 0xc0 pick the second entry of each eval hook table. */
/* 0x0017ed08 */
static void anim_load_header(float time, AnimHeader *src)
{
    int sel;
    AnimRecord *rec;
    AnimEvalFn f9, f10, f11, f12;
    int fl, size;
    int word0, word1, flags;

    if (!src)
        src = &anim_default_header;
    sel = src->flags;
    fl = (unsigned short)sel;
    size = fl & 0xff3f;
    fl += (fl & 1) >> 1; /* steer(zero) */
    sel = (sel & 0xc0) != 0;
    word0 = src->chan_tbl;
    flags = src->chan_flags;
    word1 = src->unk_04;
    anim_cur_record->pos = word0;
    anim_cur_record->unk_18 = word1;
    anim_cur_record->flags = flags;
    anim_cur_record->time = time;
    anim_cur_record->idx = 0;
    anim_record_size(size + ((fl & 1) >> 1)); /* steer(zero) */
    f9 = anim_eval9_funcs[sel];
    rec = anim_cur_record;
    f10 = anim_eval10_funcs[sel];
    f11 = anim_eval11_func;
    f12 = anim_eval12_funcs[sel];
    rec->eval9 = f9;
    rec->eval10 = f10;
    rec->eval11 = f11;
    rec->eval12 = f12;
}

/* 0x0017edd0 */
static void anim_set_record(int rec)
{
    anim_cur_record = rec;
}

/* 0x0017ede0 */
static void anim_select_record(int idx)
{
    anim_cur_record = &anim_records[idx];
}

/* Last index in [0, hi) with time <= key; 0 when fewer than two keys. */
/* 0x0017ee00 */
static unsigned int anim_key_search(char *base, int stride, unsigned int hi, unsigned int key)
{
    unsigned int lo = 0;

    while (hi - lo >= 2) {
        unsigned int mid = (lo + hi) >> 1;

        if (key >= *(unsigned int *)(base + mid * stride))
            lo = mid;
        else
            hi = mid;
    }
    return lo;
}

/* Second key wraps to the table start past the last key. frac = (t - k0) / (k1 - k0). */
/* 0x0017ee50 */
static void anim_key_pair(char *base, int stride, int count, float t, unsigned int **out0,
                   unsigned int **out1, float *frac)
{
    unsigned int idx = anim_key_search(base, stride, count, (int)t);
    unsigned int *key0 = (unsigned int *)(base + idx * stride);
    unsigned int *key1;
    unsigned int time0, time1;
    int span;

    if (idx < count - 1U)
        key1 = (unsigned int *)(base + (idx + 1) * stride);
    else
        key1 = (unsigned int *)base;
    time0 = *key0;
    time1 = *key1;
    *out0 = key0;
    span = time1 - time0;
    *out1 = key1;
    if (span > 0)
        *frac = (t - (float)time0) / (float)span;
    else
        *frac = t - (float)time0;
}

/* 16-bit key times. */
/* 0x0017ef88 */
static unsigned int anim_key_search16(char *base, int stride, unsigned int hi, unsigned short key)
{
    unsigned int lo = 0;

    while (hi - lo >= 2) {
        unsigned int mid = (lo + hi) >> 1;

        if (key >= *(unsigned short *)(base + mid * stride))
            lo = mid;
        else
            hi = mid;
    }
    return lo;
}

/* 16-bit key times; signed 16-bit difference. */
/* 0x0017efd8 */
static void anim_key_pair16(float t, char *base, int stride, int count, unsigned short **out0,
                   unsigned short **out1, float *frac)
{
    unsigned int idx = anim_key_search16(base, stride, count, (int)t);
    unsigned short *key0 = (unsigned short *)(base + idx * stride);
    unsigned short *key1;
    unsigned short time0, time1;
    short span;

    if (idx < count - 1U)
        key1 = (unsigned short *)(base + (idx + 1) * stride);
    else
        key1 = (unsigned short *)base;
    time0 = *key0;
    time1 = *key1;
    *out0 = key0;
    span = time1 - time0;
    *out1 = key1;
    if (span > 0)
        *frac = (t - (float)time0) / (float)span;
    else
        *frac = t - (float)time0;
}

/* 0x0017f0d0 */
static void anim_set_user(int user)
{
    anim_cur_user = user;
}

/* Normalized lerp. */
/* 0x0017f0e0 */
static void anim_sample_quat(float t, char *base, int count, int x)
{
    char *key0;
    char *key1;
    float frac;

    anim_key_pair(base, 20, count, t, &key0, &key1, &frac);
    quat_nlerp(key0 + 4, key1 + 4, frac, x);
}

/* 0x0017f138 */
static void anim_sample_vec3(float t, char *base, int count, float *out)
{
    float *key0;
    float *key1;
    float frac;

    anim_key_pair(base, 16, count, t, &key0, &key1, &frac);
    out[0] = key0[1] + (key1[1] - key0[1]) * frac;
    out[1] = key0[2] + (key1[2] - key0[2]) * frac;
    out[2] = key0[3] + (key1[3] - key0[3]) * frac;
}

/* 0x0017f1c8 */
static void anim_sample_vec2(float t, char *base, int count, float *out)
{
    float *key0;
    float *key1;
    float frac;

    anim_key_pair(base, 12, count, t, &key0, &key1, &frac);
    out[0] = key0[1] + (key1[1] - key0[1]) * frac;
    out[1] = key0[2] + (key1[2] - key0[2]) * frac;
}

/* Difference on the low 16 bits: angles take the short way round. */
/* 0x0017f240 */
static void anim_sample_int(float t, char *base, int count, int *out)
{
    AnimKeyInt *key0;
    AnimKeyInt *key1;
    float frac;

    anim_key_pair(base, 8, count, t, &key0, &key1, &frac);
    *out = (int)((float)key0->val
                 + (float)(short)(*(unsigned short *)&key1->val - *(unsigned short *)&key0->val) * frac);
}

/* Differences on 16 bits, as anim_sample_int. */
/* 0x0017f2b8 */
static void anim_sample_int3(float t, char *base, int count, int *out)
{
    AnimKeyInt3 *key0;
    AnimKeyInt3 *key1;
    float frac;

    anim_key_pair(base, 16, count, t, &key0, &key1, &frac);
    out[0] = (int)((float)key0->val[0].ang + (float)(short)(key1->val[0].ang_lo - key0->val[0].ang_lo) * frac);
    out[1] = (int)((float)key0->val[1].ang + (float)(short)(key1->val[1].ang_lo - key0->val[1].ang_lo) * frac);
    out[2] = (int)((float)key0->val[2].ang + (float)(short)(key1->val[2].ang_lo - key0->val[2].ang_lo) * frac);
}

/* 16-bit key times. */
/* 0x0017f398 */
static void anim_sample_s16x3(float t, char *base, int count, short *out)
{
    unsigned short *key0;
    unsigned short *key1;
    float frac;

    anim_key_pair16(t, base, 8, count, &key0, &key1, &frac);
    out[0] = (int)((float)(short)key0[1] + (float)(short)(key1[1] - key0[1]) * frac);
    out[1] = (int)((float)(short)key0[2] + (float)(short)(key1[2] - key0[2]) * frac);
    out[2] = (int)((float)(short)key0[3] + (float)(short)(key1[3] - key0[3]) * frac);
}

/* 16-bit key times. */
/* 0x0017f490 */
static void anim_sample_s16(float t, char *base, int count, short *out)
{
    unsigned short *key0;
    unsigned short *key1;
    float frac;

    anim_key_pair16(t, base, 4, count, &key0, &key1, &frac);
    *out = (int)((float)(short)key0[1] + (float)(short)(key1[1] - key0[1]) * frac);
}

/* No interpolation: previous key's value, next key's once frac passes 1. */
/* 0x0017f510 */
static void anim_sample_step16(float t, char *base, int count, unsigned short *out)
{
    unsigned short *key0;
    unsigned short *key1;
    float frac;

    anim_key_pair16(t, base, 4, count, &key0, &key1, &frac);
    if (frac <= 1.0f)
        *out = key0[1];
    else
        *out = key1[1];
}

/* Looping key list (indices wrap modulo n). Span <= 0 divides by 1. */
/* 0x0017f570 */
static void anim_sample_vec3_catmull(float t, char *base, int n, float *out)
{
    float tab[2] = {1.0f, 1.0f};
    unsigned int idx;
    AnimKeyCatmull *p0;
    AnimKeyCatmull *p1;
    AnimKeyCatmull *pm;
    AnimKeyCatmull *p2;
    float u;
    float u_sq;
    float u_cube;
    float b0;
    float b1;
    float b2;
    float b3;

    idx = anim_key_search(base, 16, n, (unsigned int)t);
    p0 = (AnimKeyCatmull *)(base + idx * 16);
    p1 = (AnimKeyCatmull *)(base + ((idx + 1) % n) * 16);
    pm = (AnimKeyCatmull *)(base + ((idx + n - 1) % n) * 16);
    p2 = (AnimKeyCatmull *)(base + ((idx + 2) % n) * 16);
    tab[1] = (float)(p1->key - p0->key);
    u = (t - (float)p0->key) / tab[(int)(p1->key - p0->key) <= 0 ? 0 : 1];
    u_sq = u * u;
    u_cube = u_sq * u;
    b0 = u_cube * 1.5f - u_sq * 2.5f + 1.0f;
    b1 = u_cube * -1.5f + u_sq * 2.0f + u * 0.5f;
    b2 = (-u_cube + u_sq * 2.0f - u) * 0.5f;
    b3 = (u_cube - u_sq) * 0.5f;
    out[0] = b2 * pm->v[0] + b0 * p0->v[0] + b1 * p1->v[0] + b3 * p2->v[0];
    out[1] = b2 * pm->v[1] + b0 * p0->v[1] + b1 * p1->v[1] + b3 * p2->v[1];
    out[2] = b2 * pm->v[2] + b0 * p0->v[2] + b1 * p1->v[2] + b3 * p2->v[2];
}

/* Hermite, Bezier form, looping keys; spans <= 0 count as 1; one-sided tangents at both ends. */
/* 0x0017f7c8 */
static void anim_sample_vec3_hermite(float t, char *base, int n, float *out)
{
    unsigned int idx;
    AnimKeyHermite *p0;
    AnimKeyHermite *p1;
    AnimKeyHermite *pm;
    AnimKeyHermite *p2;
    float span;
    float span_next;
    float span_prev;
    float u;
    float rate_next;
    float rate_prev;
    float handle;
    float inv;
    float u_cube;
    float inv_cube;
    float b1;
    float b2;
    int i;

    idx = anim_key_search(base, 16, n, (unsigned int)t);
    p1 = (AnimKeyHermite *)(base + ((idx + 1) % n) * 16);
    p0 = (AnimKeyHermite *)(base + idx * 16);
    pm = (AnimKeyHermite *)(base + ((idx + n - 1) % n) * 16);
    p2 = (AnimKeyHermite *)(base + ((idx + 2) % n) * 16);
    span = (float)p1->key - (float)p0->key;
    if (span <= 0.0f)
        span = 1.0f;
    span_next = (float)p2->key - (float)p1->key;
    if (span_next <= 0.0f)
        span_next = 1.0f;
    span_prev = (float)p0->key - (float)pm->key;
    if (span_prev <= 0.0f)
        span_prev = 1.0f;
    u = (t - (float)p0->key) / span;
    rate_next = span / (span + span_next);
    rate_prev = span / (span + span_prev);
    handle = span * 0.33333f;
    inv = 1.0f - u;
    u_cube = u * u * u;
    inv_cube = inv * inv * inv;
    b1 = u * u * 3.0f * inv;
    b2 = inv * inv * 3.0f * u;
    if (idx == n - 2) {
        for (i = 0; i < 3; i++) {
            float start, end, prev, slope;

            end = p1->v[i];
            start = p0->v[i];
            prev = pm->v[i];
            slope = (rate_prev * (start - prev)) / span_prev;
            out[i] = inv_cube * start + b2 * (start + (((1.0f - rate_prev) * (end - start)) / span + slope) * handle)
                   + b1 * (end - (((rate_prev + 1.0f) * (end - start)) / span - slope) * handle) + u_cube * end;
        }
    } else if (idx == 0) {
        for (i = 0; i < 3; i++) {
            float start, end, slope;

            end = p1->v[i];
            start = p0->v[i];
            slope = (rate_next * (p2->v[i] - end)) / span_next;
            out[i] = inv_cube * start + b2 * (start + (((rate_next + 1.0f) * (end - start)) / span - slope) * handle)
                   + b1 * (end - (((1.0f - rate_next) * (end - start)) / span + slope) * handle) + u_cube * end;
        }
    } else {
        for (i = 0; i < 3; i++) {
            float start, end;

            end = p1->v[i];
            start = p0->v[i];
            out[i] = inv_cube * start
                   + b2 * (start + (((1.0f - rate_prev) * (end - start)) / span + (rate_prev * (start - pm->v[i])) / span_prev) * handle)
                   + b1 * (end - (((1.0f - rate_next) * (end - start)) / span + (rate_next * (p2->v[i] - end)) / span_next) * handle)
                   + u_cube * end;
        }
    }
}

/* Through the eval9 hook. */
/* 0x0017fcd8 */
static int anim_play_vec3(void * out)
{
    AnimRecord *rec = anim_cur_record;
    int i = rec->idx;

    if (rec->tabs[i]) {
        rec->eval9(rec->time, rec->tabs[i], rec->counts[i], out);
        anim_cur_record->idx++;
        return 1;
    }
    rec->idx = i + 1;
    return 0;
}

/* 1 if the channel has keys, else 0 and out untouched; either way moves to the next channel. */
/* 0x0017fd58 */
static int anim_play_vec2(float * out)
{
    AnimRecord *rec = anim_cur_record;
    int i = rec->idx;

    if (rec->tabs[i]) {
        anim_sample_vec2(rec->time, rec->tabs[i], rec->counts[i], out);
        anim_cur_record->idx++;
        return 1;
    }
    rec->idx = i + 1;
    return 0;
}

/* Through the eval11 hook. */
/* 0x0017fdd0 */
static int anim_play_int(void * out)
{
    AnimRecord *rec = anim_cur_record;
    int i = rec->idx;

    if (rec->tabs[i]) {
        rec->eval11(rec->time, rec->tabs[i], rec->counts[i], out);
        anim_cur_record->idx++;
        return 1;
    }
    rec->idx = i + 1;
    return 0;
}

/* 0x0017fe50 */
static int anim_play_s16(short * out)
{
    AnimRecord *rec = anim_cur_record;
    int i = rec->idx;

    if (rec->tabs[i]) {
        anim_sample_s16(rec->time, rec->tabs[i], rec->counts[i], out);
        anim_cur_record->idx++;
        return 1;
    }
    rec->idx = i + 1;
    return 0;
}

/* Through the eval10 hook. */
/* 0x0017fec8 */
static int anim_play_int3(void * out)
{
    AnimRecord *rec = anim_cur_record;
    int i = rec->idx;

    if (rec->tabs[i]) {
        rec->eval10(rec->time, rec->tabs[i], rec->counts[i], out);
        anim_cur_record->idx++;
        return 1;
    }
    rec->idx = i + 1;
    return 0;
}

/* Through the eval12 hook. */
/* 0x0017ff48 */
static int anim_play_s16x3(void * out)
{
    AnimRecord *rec = anim_cur_record;
    int i = rec->idx;

    if (rec->tabs[i]) {
        rec->eval12(rec->time, rec->tabs[i], rec->counts[i], out);
        anim_cur_record->idx++;
        return 1;
    }
    rec->idx = i + 1;
    return 0;
}

/* 0x0017ffc8 */
static int anim_play_quat(int out)
{
    AnimRecord *rec = anim_cur_record;
    int i = rec->idx;

    if (rec->tabs[i]) {
        anim_sample_quat(rec->time, rec->tabs[i], rec->counts[i], out);
        anim_cur_record->idx++;
        return 1;
    }
    rec->idx = i + 1;
    return 0;
}

/* 0x00180040 */
static int anim_play_step16(unsigned short * out)
{
    AnimRecord *rec = anim_cur_record;
    int i = rec->idx;

    if (rec->tabs[i]) {
        anim_sample_step16(rec->time, rec->tabs[i], rec->counts[i], out);
        anim_cur_record->idx++;
        return 1;
    }
    rec->idx = i + 1;
    return 0;
}

/* Channel player 0. */
/* 0x001800b8 */
void anim_start(void *src, float t)
{
    anim_select_record(0);
    anim_start_record(src, t);
}

/* Header flag bits: 0 position, 1 angles, 5 shorts, 13 quaternion, 2 scale, 15 step flag. */
/* 0x001800f8 */
static void anim_start_record(void *src, float t)
{
    unsigned int flags;

    anim_load_header(t, src);
    flags = anim_cur_record->flags;
    if (flags & 1)
        anim_cur_record->play13 = anim_play_vec3;
    else
        anim_cur_record->play13 = (AnimPlayFn)anim_play_none;
    if (flags & 2) {
        anim_cur_record->play14 = anim_play_int3;
        anim_cur_record->play16 = 0;
        anim_cur_record->play17 = 0;
    } else if (flags & 0x20) {
        anim_cur_record->play17 = anim_play_s16x3;
        anim_cur_record->play16 = 0;
        anim_cur_record->play14 = 0;
    } else if (flags & 0x2000) {
        anim_cur_record->play16 = anim_play_quat;
        anim_cur_record->play14 = 0;
        anim_cur_record->play17 = 0;
    } else {
        anim_cur_record->play14 = (AnimPlayFn)anim_play_rot_none;
        anim_cur_record->play16 = 0;
        anim_cur_record->play17 = 0;
    }
    if (flags & 4)
        anim_cur_record->play15 = anim_play_vec3;
    else
        anim_cur_record->play15 = (AnimPlayFn)anim_play_none;
    if (flags & 0x48)
        anim_cur_record->idx++;
    if (flags & 0x8000) {
        anim_step_enable = 1;
        anim_step_flags = 0;
    } else {
        anim_step_enable = 0;
        anim_step_flags = 0;
    }
}

/* Translation, rotation (angles or quaternion), scale. */
/* steer(cast) */
/* 0x00180218 */
void anim_apply_node(AnimNode *node)
{
    AnimQuat pos;
    AnimQuat scl;
    AnimQuat rot;
    AnimQuat quat;
    int kind;

    kind = anim_eval_node(node, &pos, &rot, &scl, &quat);
    njTranslateEx(&pos);
    switch (kind) {
    case 0:
        ((void (*)(AnimQuat *, int))nj_matrix_rotate_cur_ang3)(&rot, 0);
        break;
    case 1:
        ((void (*)(AnimQuat *, int))nj_matrix_rotate_cur_ang3)(&rot, 1);
        break;
    default:
        quat_to_matrix(&quat);
        break;
    }
    njScaleEx(&scl);
}

/*
 * Node flag 0x40: not animated; 0x400: quaternion.
 * Returns 2 for a quaternion, else flag 0x20 (rotation order).
 */
/* 0x001802c0 */
static int anim_eval_node(AnimNode *node, float *pos, int *rot, float *scale, float *quat)
{
    int ret;
    short sh[3];

    if (node->flags & 0x40)
        goto stat;
    if (anim_cur_record->play13(pos) == 0) {
        pos[0] = node->pos[0];
        pos[1] = node->pos[1];
        pos[2] = node->pos[2];
    }
    if (anim_cur_record->play14) {
        if (anim_cur_record->play14(rot)) {
            ret = (int)node->flags >> 5;
            ret &= 1;
        } else
            NODE_ROT(node, rot, quat, ret);
    } else if (anim_cur_record->play17) {
        if (anim_cur_record->play17(sh)) {
            rot[0] = sh[0];
            rot[1] = sh[1];
            rot[2] = sh[2];
            ret = (int)node->flags >> 5;
            ret &= 1;
        } else
            NODE_ROT(node, rot, quat, ret);
    } else {
        if (anim_cur_record->play16(quat))
            ret = 2;
        else
            NODE_ROT(node, rot, quat, ret);
    }
    if (anim_cur_record->play15(scale) == 0) {
        scale[0] = node->scale[0];
        scale[1] = node->scale[1];
        scale[2] = node->scale[2];
    }
    if (anim_step_enable) {
        if (anim_play_step16(&anim_step_flags) == 0) {
            if (node->flags & 8)
                anim_step_flags |= 1;
            else
                anim_step_flags &= 0xfffe;
        }
    }
    anim_record_advance();
    return ret;
stat:
    pos[0] = node->pos[0];
    pos[1] = node->pos[1];
    pos[2] = node->pos[2];
    scale[0] = node->scale[0];
    scale[1] = node->scale[1];
    scale[2] = node->scale[2];
    NODE_ROT(node, rot, quat, ret);
    return ret;
}

/* 0x00180550 */
static int anim_play_none(void)
{
    return 0;
}

/* 0x00180558 */
static int anim_play_rot_none(void)
{
    return 0;
}

/* Into bit 3 of *flags. */
/* 0x00180560 */
static void anim_apply_step_flag(unsigned int *flags)
{
    if (anim_step_enable) {
        if (anim_step_flags & 1)
            *flags |= 8;
        else
            *flags &= ~8;
    }
}

/* 0x001805a8 */
static int anim_is_step_enabled(void)
{
    return anim_step_enable;
}

/* 0x001805b8 */
void anim_eval_hierarchy(int node, int motion, AnimCursorSetSrc *cursors, float t)
{
    AnimCursorSet *cur = anim_cursors;

    cur->in0 = cursors->in0;
    cur->out = cursors->out;
    cur->in1 = cursors->in1;
    anim_start(motion, t);
    anim_eval_tree(node);
}

/* Depth first, children before siblings; one cursor entry per node. */
/* 0x00180608 */
static void anim_eval_tree(C17WNode *node)
{
    int loc[4];

    do {
        int kind = anim_eval_node(node, anim_cursors->in0, anim_cursors->out, anim_cursors->in1, loc);

        anim_cursors->in0 += 12;
        anim_cursors->in1 += 12;
        if (kind == 2) {
            *anim_cursors->out++ = loc[1];
            *anim_cursors->out++ = loc[2];
            *anim_cursors->out++ = loc[3];
        } else {
            anim_cursors->out += 3;
        }
        if (node->child)
            anim_eval_tree(node->child);
        node = node->next;
    } while (node);
}

/* 0x00180700 */
void anim_cursor_next(void)
{
    AnimCursorView *cur = anim_cur_cursor;

    cur->ptr += 2;
    cur->track = (void *)cur->ptr[0];
    cur->nrec = cur->ptr[1];
}

/* Starts a new frame so the interpolation cache is rebuilt. */
/* 0x00180728 */
void anim_cursor_start(AnimStartRec *rec, float t)
{
    AnimCursor *cur = anim_cur_cursor;
    int *data;
    int track, nrec;

    data = rec->ptr;
    cur->user = rec->user;
    cur->frame++;
    track = ((AnimWord *)data)[0].i;
    nrec = ((AnimWord *)data)[1].i;
    cur->time = t;
    cur->nrec = nrec;
    cur->ptr = data;
    cur->track = track;
}

/* 0x00180768 */
static void anim_set_cursor(int cur)
{
    anim_cur_cursor = cur;
}

/* 0x00180778 */
void anim_select_cursor(int idx)
{
    anim_cur_cursor = &anim_cursor_table[idx];
}

/* Clears only the first entry's frame: the loop over entries has an empty body. */
/* 0x00180798 */
void anim_cache_init(int *buf, unsigned int bytes)
{
    unsigned int n = bytes >> 4;

    anim_cache = buf;
    if (n) {
        while (--n != 0)
            ;
        *buf = 0;
    }
}

/* 0x001807d0 */
void anim_set_slot_table(int table)
{
    anim_slot_table = table;
}

/* Record: type byte, word count, data. */
/* 0x001807e0 */
void anim_cursor_play(unsigned char **pp)
{
    unsigned char *p = *pp;
    int i = 0;
    unsigned int type;
    unsigned int n;

    if (anim_cur_cursor->nrec > 0) {
        do {
            i++;
            anim_cache_refresh();
            type = *p;
            p += 2;
            n = *(unsigned short *)p;
            p += 6;
            anim_blend_keys((float *)p, type);
            p += n * 4 - 4;
            anim_cur_cursor->track += 8;
        } while (i < anim_cur_cursor->nrec);
    }
}

/* Slot via anim_slot_table on the track's first key; built once per frame, shared by tracks. */
/* 0x00180878 */
static void anim_cache_refresh(void)
{
    AnimCursor *cur;
    AnimTrack *trk;
    AnimKey *keys;
    int slot;
    AnimKey *key0;
    AnimKey *key1;
    float frac;

    cur = anim_cur_cursor;
    trk = (AnimTrack *)cur->track;
    keys = trk->keys;
    slot = (*(*(AnimSlotEnt * * *)&anim_slot_table))[keys->val].slot; /* steer(cast) */
    cur->slot = slot;
    if ((*(AnimCacheEnt * *)&anim_cache)[slot].frame != cur->frame) { /* steer(cast) */
        anim_key_pair(keys, 8, trk->key_cnt, cur->time, &key0, &key1, &frac);
        (*(AnimCacheEnt * *)&anim_cache)[slot].frame = anim_cur_cursor->frame; /* steer(cast) */
        (*(AnimCacheEnt * *)&anim_cache)[slot].val0 = key0->val; /* steer(cast) */
        (*(AnimCacheEnt * *)&anim_cache)[slot].val1 = key1->val; /* steer(cast) */
        (*(AnimCacheEnt * *)&anim_cache)[slot].frac = frac; /* steer(cast) */
    }
}

/* dst type: elements held; payload type: elements carried, words each. Unknown: no-op. */
/* 0x00180948 */
static void anim_blend_keys(float *dst, unsigned char type)
{
    AnimCursor *cur = anim_cur_cursor;
    char *cache = anim_cache + cur->slot * 16;
    int trk = cur->track;
    int idx = *(unsigned short *)(trk + 6) * 4;
    unsigned char *pa = *(unsigned char **)(idx + *(int *)(*(int *)(cache + 4) * 8 + (int)*anim_slot_table));
    unsigned char *pb = *(unsigned char **)(idx + *(int *)(*(int *)(cache + 8) * 8 + (int)*anim_slot_table));
    int flags;
    int div;
    int fmt;

    switch (type) {
    case 0x22:
        flags = 1;
        break;
    case 0x23:
        flags = 5;
        break;
    case 0x24:
        flags = 9;
        break;
    case 0x29:
        flags = 3;
        break;
    case 0x2a:
        flags = 7;
        break;
    case 0x2b:
    case 0x2c:
        flags = 0xb;
        break;
    default:
        return;
    }
    fmt = *pa;
    pa += 2;
    switch (fmt) {
    case 0x22:
        flags |= 0x10;
        div = 3;
        break;
    case 0x23:
        flags |= 0x50;
        div = 4;
        break;
    case 0x29:
        flags |= 0x30;
        div = 6;
        break;
    case 0x2a:
        flags |= 0x70;
        div = 7;
        break;
    case 0x82:
        flags |= 0x40;
        div = 1;
        break;
    case 0x80:
        flags |= 0x20;
        div = 3;
        break;
    case 0x81:
        flags |= 0x60;
        div = 4;
        break;
    default:
        return;
    }
    anim_blend_frame(dst, (float *)(pa + 2), (float *)(pb + 4), *(unsigned short *)pa / div | flags << 16,
              *(float *)(cache + 12));
}

/* nf: count in low 16 bits, element flags above (1, 2 vec3; 4 byte colour; 8 skipped word). */
/* 0x00180b08 */
static void anim_blend_frame(float *dst, float *pa, float *pb, int nf, float frac)
{
    int flags = nf >> 16;
    unsigned int n = nf & 0xffff;
    float inv = 1.0f - frac;

    do {
        if (flags & 1) {
            if (flags & 0x10) {
                dst[0] = *pa++ * inv + *pb++ * frac;
                dst[1] = *pa++ * inv + *pb++ * frac;
                dst[2] = *pa++ * inv + *pb++ * frac;
            }
            dst += 3;
        }
        if (flags & 2) {
            if (flags & 0x20) {
                dst[0] = *pa++ * inv + *pb++ * frac;
                dst[1] = *pa++ * inv + *pb++ * frac;
                dst[2] = *pa++ * inv + *pb++ * frac;
            }
            dst += 3;
        }
        if (flags & 4) {
            if (flags & 0x40) {
                unsigned char *ca = (unsigned char *)pa;
                unsigned char *cb = (unsigned char *)pb;
                unsigned char *cd = (unsigned char *)dst;

                cd[0] = (int)(ca[0] * inv + cb[0] * frac);
                cd[1] = (int)(ca[1] * inv + cb[1] * frac);
                cd[2] = (int)(ca[2] * inv + cb[2] * frac);
                cd[3] = (int)(ca[3] * inv + cb[3] * frac);
                pa++;
                pb++;
            }
            dst++;
        }
        if (flags & 8)
            dst++;
    } while (--n != 0);
}
