/*
 * Camera passes: a stage's camera paths from stg*.pas. A pass loops over parts, each with a camera
 * curve, a look-at curve (linear, Bezier, B-spline, cardinal, polar) and a roll track, optionally
 * relative to another pass. Also path-editor drawing.
 */

#include "types_game.h"
#include "mathdef.h"
#include "libc.h"
#include "nj_system.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "pass.h"
#include "bspline.h"
#include "file.h"
#include "lockon.h"
#include "math_util.h"
#include "stage.h"
#include "yuki_tool.h"
#include "system.h"

typedef struct {
    unsigned int flags;
    int ref;
    int kind;
    int nkey;
    void *times;
    void *pts;
    float tension;
    void *knots;
    float v[3];
} PassCurve;

typedef struct {
    unsigned int flags;
    int ref;
    int nkey;
    void *key_count;
    void *key_angz;
} PassKeyTrack;

typedef struct {
    PassCurve t0;
    PassCurve t1;
    PassKeyTrack t2;
} PassPartView;

typedef struct {
    char pad_00[4];
    int nparts;
    PassPartView *ents;
    int start;
    int period;
} PassEntTimed;

typedef struct { char pad_00[8]; char *tension; } PassCurveBlk;

/* steer(alias): the code that matched reads PassCurve through these member types; one view
 * shared by those sites (src/game/pass.c). */
typedef struct PassCurveView {
    char pad_00[16];
    float *times;
    float *pts;
    char pad_18[8];
    float pivot[3];
} PassCurveView;

typedef struct {
    unsigned int flags;
    int ref;
    int nkey;
    int *key_count;
    char pad_10[4];
} PassKeyTrackView;

/* steer(alias): the code that matched reads PassPart through these member types; one view
 * shared by those sites (src/game/pass.c). */
typedef struct PassPartCamFlagsView {
    unsigned int cam_flags;
    int cam_ref;
    char pad_08[36];
    unsigned int in_flags;
    int in_ref;
    char pad_34[36];
    PassKeyTrackView run;
} PassPartCamFlagsView;

/* steer(alias): the code that matched reads PassPart through these member types; one view
 * shared by those sites (src/game/pass.c). */
typedef struct PassPartWordView {
    int word[27];
} PassPartWordView;

typedef struct {
    int count;
    PassEnt *recs;
} PassFile;

typedef struct {
    int dir;
    char *name;
} PassFileDesc;

#define FREE(p) do { yuki_free(p); (p) = 0; } while (0)
typedef struct PathTblRef { int count; void *tbl; } PathTblRef;

extern void (*curve_samplers[])(void *, void *, float);

extern int pass_count;
extern int pass_stage_count;
extern PassEnt *pass_stage_table;
extern int pass_default_count;
extern PassEnt *pass_default_table;
extern PassEnt *pass_table;

extern int ulMatrixPush(void);
extern int pass_saved_count;
extern void *pass_saved_table;
extern int *curve_track_load(int *p, PassCurve *track);
extern int *keyframe_track_load(int *in, PassKeyTrack *track);

/*
 * Outputs optional, skipped when the part lacks the channel. Returns the mode word: roll flags,
 * camera curve flags in bits 0..6, look-at in 7..13; 0x4081 (nothing) before the pass starts.
 */
/* 0x00144d18 */
int sequence_eval(int idx, float time, int *key_out, void *curve0_out, void *curve1_out)
{
    PassEntTimed *rec = &pass_table[idx];
    PassPartView *part;
    PassKeyTrack *track;
    float x;
    float len;
    float a;
    float b;
    int k;
    int n;
    int j;
    int lo;
    unsigned int flags;
    int *ta;
    int *tb;

    track = 0;
    len = 0.0f;
    x = time - (float)rec->start;
    part = rec->ents;
    if (x < len)
        return 16513;
    x = fmodf(x, (float)rec->period);
    n = rec->nparts;
    for (k = 0; k < n; k++, part++) {
        track = &part->t2;
        len = (float)((int *)track->key_count)[track->nkey - 1];
        if (x < len)
            break;
        x -= len;
    }
    do { } while (0);
    flags = track->flags;
    if (key_out) {
        if (!(flags & 0x4000)) {
            ta = track->key_count;
            j = 1;
            if ((float)ta[1] < x) {
                do {
                    j++;
                } while ((float)ta[j] < x);
            }
            tb = track->key_angz;
            lo = j - 1;
            a = (float)ta[j] - x;
            b = x - (float)ta[lo];
            *key_out = (int)(((float)tb[lo] * a + (float)tb[j] * b) / (b + a));
        }
    }
    flags |= part->t0.flags & 0x7f;
    if (curve0_out) {
        if (!(flags & 1))
            curve_samplers[part->t0.kind](part, curve0_out, x / len);
    }
    flags |= (part->t1.flags & 0x7f) << 7;
    if (curve1_out) {
        if (!(flags & 0x80)) {
            PassCurve *curve = &part->t1;
            curve_samplers[curve->kind](curve, curve1_out, x / len);
        }
    }
    return flags;
}

/* 0x00144fc0 */
void curve_sample_linear(float time, PassCurve *curve, float *out)
{
    int i = 0;
    float *times = curve->times;
    float *pts = curve->pts;
    float prev;
    float ofs;
    float rest;
    float span;
    float *p;
    float *pt_cur;

    if (time == 0.0f) {
        out[0] = pts[0];
        out[1] = pts[1];
        out[2] = pts[2];
        return;
    }
    while (*times < time) {
        times++;
        i++;
    }
    prev = times[-1];
    if (prev == *times) {
        out[0] = pts[i * 3];
        out[1] = pts[i * 3 + 1];
        out[2] = pts[i * 3 + 2];
        return;
    }
    rest = *times - time;
    ofs = time - prev;
    span = ofs + rest;
    p = pts + i * 3;
    pt_cur = p - 3;
    out[0] = (pt_cur[0] * rest + p[0] * ofs) / span;
    out[1] = (pt_cur[1] * rest + p[1] * ofs) / span;
    out[2] = (pt_cur[2] * rest + p[2] * ofs) / span;
}

/* Each key: point and two handles; cubic Bezier between keys. */
/* steer(cast) */
/* 0x001450d8 */
void curve_sample_bezier(float time, PassCurve *curve, float *out)
{
    float *times = curve->times;
    char *pts = curve->pts;
    int i = 0;
    float cur;
    float prev;
    float *pt_cur;
    int seg;
    float buf[12];

    if (time == 0.0f) {
        pt_cur = (float *)pts;
        out[0] = pt_cur[0];
        out[1] = pt_cur[1];
        out[2] = pt_cur[2];
        return;
    }
    while ((cur = *times) < time) {
        times++;
        i++;
    }
    prev = times[-1];
    if (prev == cur) {
        pt_cur = (float *)(pts + i * 36);
        out[0] = pt_cur[0];
        out[1] = pt_cur[1];
        out[2] = pt_cur[2];
        return;
    }
    seg = (i - 1) * 36;
    nj_copy8(buf, pts + seg, 12);
    nj_copy8(buf + 3, pts + (seg + 24), 12);
    nj_copy8(buf + 6, pts + (i * 36 + 12), 12);
    nj_copy8(buf + 9, pts + i * 36, 12);
    ((void (*)(float, float *, float *, int))bezier_cubic_eval)((time - prev) / (cur - prev), buf, out, 0);
}

/* 0x00145240 */
void bspline_eval_cubic_obj(float time, PassSpline *spline, void *out)
{
    bspline_eval(3, spline->nkey, spline->pts, spline->knots, out, time);
}

/* Cardinal spline with the curve's tension; extra control point before the first key. */
/* 0x00145260 */
void curve_sample(float time, PassCurve *curve, float *out)
{
    float *times = curve->times;
    float *pts = curve->pts;
    int i = 0;
    float prev;
    PassCurveBlk blk;
    float *pt_cur;

    if (time == 0.0f) {
        pt_cur = pts + 3;
        out[0] = pt_cur[0];
        out[1] = pt_cur[1];
        out[2] = pt_cur[2];
        return;
    }
    while (*times < time) {
        times++;
        i++;
    }
    prev = times[-1];
    if (prev == *times) {
        out[0] = pts[i * 3];
        out[1] = pts[i * 3 + 1];
        out[2] = pts[i * 3 + 2];
        return;
    }
    blk.tension = (char *)curve + 24;
    spline_cardinal_eval((time - prev) / (*times - prev), &pts[i * 3 - 6], out, &blk);
}

static inline void csp_vadd(float *dst, float *va, float *vb)
{
    dst[0] = va[0] + vb[0];
    dst[1] = va[1] + vb[1];
    dst[2] = va[2] + vb[2];
}

/*
 * Keys (distance, pitch, yaw) in degrees, linear; point at distance down -Z, turned by pitch then
 * yaw, around the pivot.
 */
/* 0x00145340 */
void curve_sample_polar(float time, PassCurveView *curve, float *out)
{
    float *times = curve->times;
    float *pts = curve->pts;
    int i = 0;
    float prev;
    float dist;
    float ang_x;
    float ang_y;
    float x;
    float y;
    float rest;
    int rot_x;
    int rot_y;
    float ofs[3];
    float sc[8];
    float na;
    float dist0_w, ax0_w, ay0_w, dist1_w, ax1_w, ay1_w, dist_sum, ax_sum, ay_sum, ax_deg, ay_deg;

    if ((((int)pts) & 1) >> 1) {
        ofs[0] = 0.0f;
    }
    while (*times < time) {
        times++;
        i++;
    }
    prev = times[-1];
    if (prev == *times) {
        float *p = pts + i * 3;

        ang_y = DEG_TO_ANG(p[2]);
        p = (float *)((char *)p + (((int)p & 1) >> 1)); /* steer(zero) */
        dist = p[0];
        ang_x = DEG_TO_ANG(p[1]);
    } else {
        float *p;
        float *pt_cur;
        float deg_scale;
        float ay1, ax0, dist1, ay0, dist0;

        x = time - prev;
        i += (i & 1) >> 1; /* steer(zero) */
        do { } while (0);
        p = pts + i * 3;
        ay1 = p[2];
        rest = *times - time;
        ay1_w = ay1 * x;
        p = (float *)((char *)p + ((i & 1) >> 1)); /* steer(zero) */
        pt_cur = p - 3;
        p = (float *)((char *)p + (((int)p & 1) >> 1)); /* steer(zero) */
        ax1_w = p[1] * x;
        ax0 = pt_cur[1];
        dist1 = p[0];
        ay0 = pt_cur[2];
        dist0 = p[-3];
        ay0_w = ay0 * rest;
        ax0_w = ax0 * rest;
        ax_sum = ax0_w + ax1_w;
        ay_sum = ay0_w + ay1_w;
        y = x + rest;
        deg_scale = ANG_PER_DEG;
        dist1_w = dist1 * x;
        ax_deg = ax_sum / y;
        dist0_w = dist0 * rest;
        ay_deg = ay_sum / y;
        do { } while (0);
        dist_sum = dist0_w + dist1_w;
        do { } while (0);
        ang_y = ay_deg * deg_scale;
        dist = dist_sum / y;
        ang_x = ax_deg * deg_scale;
    }
    rot_y = (int)ang_y;
    rot_x = (int)ang_x;
    do { ulSinCos(rot_x, &sc[0], &sc[1]); na = -dist; } while (0);
    rot_x += (rot_x & 1) >> 1; /* steer(zero) */
    ulSinCos(rot_y, &sc[2], &sc[3]);
    ofs[0] = 0.0f;
    ofs[1] = 0.0f;
    ofs[2] = na;
    do {
        float ox = ofs[0], oz = ofs[2];

        rot_y += (rot_y & 1) >> 1; /* steer(zero) */
        ulSinCos(rot_x, &sc[4], &sc[5]);
        ofs[1] = ox * sc[5] - oz * sc[4];
        ofs[2] = ox * sc[4] + oz * sc[5];
    } while (0);
    do {
        float ox = ofs[0], oz = ofs[2];

        ulSinCos(rot_y, &sc[6], &sc[7]);
        ofs[0] = ox * sc[7] + oz * sc[6];
        ofs[2] = -ox * sc[6] + oz * sc[7];
    } while (0);
    csp_vadd(out, curve->pivot, ofs);
}

/*
 * Passes the part at time is relative to, per channel; outputs optional, written only when
 * relative.
 */
/* 0x001455f8 */
void sequence_lookup(float time, int idx, int *key_out, int *curve0_out, int *curve1_out)
{
    PassEnt *rec = &pass_table[idx];
    PassPartCamFlagsView *part = rec->parts;
    float x = 0;
    float pr;
    float len;
    int i;

    pr = (float)rec->len;
    time -= (float)rec->start;
    if (!(0.0f <= time))
        time = 0.0f;
    x = fmodf(time, pr);
    for (i = 0; i < rec->nparts; i++, part++) {
        PassKeyTrackView *track = &part->run;

        len = (float)track->key_count[track->nkey - 1];
        if (x < len)
            break;
        x -= len;
    }
    if (key_out) {
        if (part->cam_flags & ~3U)
            *key_out = pass_find_by_symbol(part->cam_ref);
    }
    if (curve0_out) {
        if (part->in_flags & ~3U)
            *curve0_out = pass_find_by_symbol(part->in_ref);
    }
    if (curve1_out) {
        if (part->run.flags & 0x10000)
            *curve1_out = pass_find_by_symbol(part->run.ref);
    }
}

/*
 * Flag 1: unused, nothing follows. Bezier: three points per key; cardinal adds tension, B-spline
 * builds knots, polar adds pivot. Returns the data after the curve.
 */
/* 0x00145758 */
int *curve_track_load(int *p, PassCurve *track)
{
    unsigned int flags;
    float *src;
    float *in;
    int i;
    int npts;

    flags = *p++;
    track->flags = flags;
    if (flags & 1)
        return p;
    if (flags & ~3U)
        track->ref = *p++;
    track->kind = *p++;
    track->nkey = *p++;
    track->times = yuki_alloc(track->nkey * 4);
    src = track->times;
    in = (float *)p;
    for (i = 0; i < track->nkey; i++)
        *src++ = *in++;
    npts = track->nkey;
    if (track->kind == 1)
        npts = track->nkey * 3;
    track->pts = yuki_alloc(npts * 12);
    src = track->pts;
    for (i = 0; i < npts; i++) {
        src[0] = *in++;
        src[1] = *in++;
        src[2] = *in++;
        src += 3;
    }
    if (track->kind == 3) {
        track->tension = *in++;
    } else if (track->kind == 2) {
        bspline_make_knots(track->nkey, 3, &track->knots);
    } else if (track->kind == 4) {
        {
            float *dst = track->v;

            dst[0] = *in++;
            dst[1] = *in++;
            dst[2] = *in++;
        }
    }
    return (int *)in;
}

/* Symbol with flag 0x10000; angles unless flag 0x4000 (no roll). Returns the data after it. */
/* 0x00145908 */
int *keyframe_track_load(int *in, PassKeyTrack *track)
{
    int *p;
    int *src;
    int i;

    track->flags = *in++;
    if (track->flags & 0x10000)
        track->ref = *in++;
    track->nkey = *in++;
    track->key_count = yuki_alloc(track->nkey << 2);
    p = track->key_count;
    for (i = 0; i < track->nkey; i++)
        *p++ = *in++;
    if (!(track->flags & 0x4000)) {
        track->key_angz = yuki_alloc(track->nkey << 2);
        src = track->key_angz;
        for (i = 0; i < track->nkey; i++)
            *src++ = *in++;
    }
    return in;
}

/* "stg<name>.pas"; also kept as the stage's table. Returns the pass count. */
/* 0x001459f8 */
int sequence_load_stage(void)
{
    char buf[256];
    PassFileDesc desc;
    int *data;
    int *p;
    PassEnt *rec;
    PassPartWordView *part;
    int i;
    int j;

    sprintf(buf, "stg%s.pas", stage_get_name());
    desc.dir = 0;
    desc.name = buf;
    data = file_load_sync(&desc, 0, 0);
    p = data + 1;
    pass_count = data[0];
    pass_table = 0;
    if (pass_count > 0) {
        pass_table = yuki_alloc(pass_count * 20);
        rec = pass_table;
        for (i = 0; i < pass_count; i++, rec++) {
            rec->sym = *p++;
            rec->start = *p++;
            rec->nparts = *p++;
            rec->len = 0;
            rec->parts = yuki_alloc(rec->nparts * 108);
            part = rec->parts;
            for (j = 0; j < rec->nparts; j++, part++) {
                p = curve_track_load(p, part);
                p = curve_track_load(p, (char *)part + 44);
                p = keyframe_track_load(p, (char *)part + 88);
                rec->len += ((int **)part)[25][part->word[24] - 1];
            }
        }
    }
    file_heap_free(data);
    pass_stage_count = pass_count;
    pass_stage_table = pass_table;
    return pass_stage_count;
}

/* Pass length = sum of its parts' lengths. */
/* 0x00145bb8 */
PassFile *sequence_load(int name)
{
    PassFile *hdr = yuki_alloc(8);
    int *data = file_load_sync(name, 0, 0);
    int *p = data + 1;
    PassEnt *rec;
    PassPartWordView *part;
    int i;
    int j;

    hdr->recs = 0;
    hdr->count = data[0];
    if (hdr->count > 0) {
        hdr->recs = yuki_alloc(hdr->count * 20);
        rec = hdr->recs;
        for (i = 0; i < hdr->count; i++, rec++) {
            rec->sym = *p++;
            rec->start = *p++;
            rec->nparts = *p++;
            rec->len = 0;
            rec->parts = yuki_alloc(rec->nparts * 108);
            part = rec->parts;
            for (j = 0; j < rec->nparts; j++, part++) {
                p = curve_track_load(p, part);
                p = curve_track_load(p, (char *)part + 44);
                p = keyframe_track_load(p, (char *)part + 88);
                rec->len += ((int **)part)[25][part->word[24] - 1];
            }
        }
    }
    file_heap_free(data);
    return hdr;
}

/* Then back to the default pass table. */
/* 0x00145d48 */
void sequence_free_stage(void)
{
    PassEnt *rec;
    PassPartView *part;
    int i;
    int j;

    rec = pass_table;
    for (i = 0; i < pass_count; i++, rec++) {
        part = rec->parts;
        for (j = 0; j < rec->nparts; j++, part++) {
            if (!(part->t0.flags & 1)) {
                FREE(part->t0.times);
                FREE(part->t0.pts);
                if (part->t0.kind == 2) {
                    FREE(part->t0.knots);
                }
            }
            if (!(part->t1.flags & 1)) {
                FREE(part->t1.times);
                FREE(part->t1.pts);
                if (part->t1.kind == 2) {
                    FREE(part->t1.knots);
                }
            }
            FREE(part->t2.key_count);
            if (!(part->t2.flags & 0x4000)) {
                FREE(part->t2.key_angz);
            }
        }
        FREE(rec->parts);
    }
    if (pass_table) {
        FREE(pass_table);
    }
    pass_table = pass_default_table;
    pass_count = pass_default_count;
    pass_stage_table = pass_stage_count = 0;
}

/* 0x00145f28 */
void sequence_free(PassFile *hdr)
{
    PassEnt *rec = hdr->recs;
    PassPartView *part;
    int i;
    int j;

    for (i = 0; i < hdr->count; i++, rec++) {
        part = rec->parts;
        for (j = 0; j < rec->nparts; j++, part++) {
            if (!(part->t0.flags & 1)) {
                FREE(part->t0.times);
                FREE(part->t0.pts);
                if (part->t0.kind == 2) {
                    FREE(part->t0.knots);
                }
            }
            if (!(part->t1.flags & 1)) {
                FREE(part->t1.times);
                FREE(part->t1.pts);
                if (part->t1.kind == 2) {
                    FREE(part->t1.knots);
                }
            }
            FREE(part->t2.key_count);
            if (!(part->t2.flags & 0x4000)) {
                FREE(part->t2.key_angz);
            }
        }
        FREE(rec->parts);
    }
    if (hdr->recs) {
        FREE(hdr->recs);
    }
    hdr->count = 0;
    FREE(hdr);
}

/* -1 if none. */
/* 0x001460c8 */
int pass_find_by_symbol(int sym)
{
    PassEnt *p = pass_table;
    int i;

    for (i = 0; i < pass_count; i++, p++) {
        if (p->sym == sym)
            return i;
    }
    return -1;
}

/*
 * Turns vec by the other pass's camera pitch/yaw (camera toward look-at) and roll per mode bits,
 * adds its camera or look-at position. 0 when the other pass lacks a needed channel.
 */
/* 0x00146110 */
int pass_eval_relative(float time, int pass, int mode, float *vec, int mask_cam, int mask_look, int mask_pitch, int mask_yaw, int mask_roll)
{
    float cam_pos[4];
    float look_at[4];
    int rot[2];
    float delta[3];
    int x;
    int cnt;
    unsigned flags;
    unsigned use_cam = mode & mask_cam;

    if (use_cam != 0)
        sequence_lookup(time, pass, &x, 0, 0);
    else
        sequence_lookup(time, pass, 0, &x, 0);
    flags = pass_eval_part(x, time, &cnt, cam_pos, look_at);
    if (mode & (mask_pitch | mask_yaw)) {
        if ((flags & 3) != 3)
            return 0;
        delta[0] = look_at[0] - cam_pos[0];
        delta[1] = look_at[1] - cam_pos[1];
        delta[2] = look_at[2] - cam_pos[2];
        do { } while (0);
        rot[1] = (int)RAD_TO_ANG(atan2f(-delta[0], -delta[2]));
        rot[0] = (int)RAD_TO_ANG(atan2f(delta[1], sqrtf(delta[0] * delta[0] + delta[2] * delta[2])));
        do { } while (0);
        if (mode & mask_pitch) {
            float y = vec[1];
            float z = vec[2];
            float s, c;

            ulSinCos(rot[0], &s, &c);
            vec[1] = y * c - z * s;
            vec[2] = y * s + z * c;
        }
        do { } while (0);
        if (mode & mask_yaw) {
            float xx = vec[0];
            float z = vec[2];
            float s, c;

            ulSinCos(rot[1], &s, &c);
            vec[0] = xx * c + z * s;
            vec[2] = -xx * s + z * c;
        }
    }
    if (mode & mask_roll) {
        if (!(flags & 4))
            return 0;
        {
            float xx = vec[0];
            float y = vec[1];
            float s, c;

            ulSinCos(cnt, &s, &c);
            vec[0] = xx * c - y * s;
            vec[1] = xx * s + y * c;
        }
    }
    if (use_cam != 0) {
        if (!(flags & 1))
            return 0;
        vec[0] = vec[0] + cam_pos[0];
        vec[1] = vec[1] + cam_pos[1];
        vec[2] = vec[2] + cam_pos[2];
    } else if (mode & mask_look) {
        if (!(flags & 2))
            return 0;
        vec[0] = vec[0] + look_at[0];
        vec[1] = vec[1] + look_at[1];
        vec[2] = vec[2] + look_at[2];
    }
    return 1;
}

/*
 * mask_*: this channel's bits in mode. Unused: zero vector, bit cleared; absolute: as sampled;
 * relative: pass_eval_relative, bit cleared on failure.
 */
/* 0x00146410 */
void pass_eval_channel(int pass, float time, int mode, float *src, float *out, unsigned *flags,
                       unsigned bit, int mask_none, int mask_abs, int mask_cam, int mask_look, int mask_pitch, int mask_yaw,
                       int mask_roll)
{
    unsigned old = *flags;

    *flags = old | bit;
    if (mode & mask_none) {
        out[0] = 0;
        out[1] = 0;
        *flags = (old | bit) & ~bit;
        out[2] = 0;
    } else {
        if (mode & mask_abs) {
            if (out == 0)
                return;
        } else {
            if (pass_eval_relative(time, pass, mode, src, mask_cam, mask_look, mask_pitch, mask_yaw, mask_roll) == 0) {
                *flags = *flags & ~bit;
                return;
            }
        }
        out[0] = src[0];
        out[1] = src[1];
        out[2] = src[2];
    }
}

/*
 * World camera, look-at and roll. Roll absolute with mode 0x8000, else added to the other pass's.
 * Returns flags: 1 camera, 2 look-at, 4 roll valid.
 */
/* 0x001464e8 */
unsigned pass_eval_part(int ctx, float time, int *count, float *cam_pos, float *look_at)
{
    float vec_a[4];
    float vec_b[4];
    int base;
    unsigned flags;
    int parent;
    int pcount;
    int mode;

    flags = 0;
    mode = sequence_eval(ctx, time, &base, vec_a, vec_b);
    pass_eval_channel(ctx, time, mode, vec_a, cam_pos, &flags, 1, 1, 2, 4, 8, 16, 32, 64);
    pass_eval_channel(ctx, time, mode, vec_b, look_at, &flags, 2, 0x80, 0x100, 0x200, 0x400, 0x800, 0x1000,
                  0x2000);
    flags = flags | 4;
    if (mode & 0x4000) {
        flags = flags & ~4u;
    } else if (mode & 0x8000) {
        if (count)
            *count = base;
    } else {
        sequence_lookup(time, ctx, 0, 0, &parent);
        if ((pass_eval_part(parent, time, &pcount, 0, 0) & 4) == 0) {
            flags = flags & ~4u;
        } else if (count) {
            *count = base + pcount;
        }
    }
    return flags;
}

/* X red, Y green, 10 units; Z blue, as long as the distance to look_at. */
/* steer(cast) */
/* 0x001466b8 */
void pass_draw_camera_axes(PassVec3 *pos, PassVec3 *look_at, int roll)
{
    PassVec3 org;
    PassVec3 axis;
    PassVec3 delta;
    int rot[3];

    memset(&org, 0, 12);
    delta.x = pos->x - look_at->x;
    delta.y = pos->y - look_at->y;
    delta.z = pos->z - look_at->z;
    do { } while (0);
    rot[1] = (int)RAD_TO_ANG(atan2f(-delta.x, -delta.z));
    rot[0] = (int)RAD_TO_ANG(atan2f(delta.y, sqrtf(delta.x * delta.x + delta.z * delta.z)));
    do {
        rot[2] = roll;
    } while (0);
    ulMatrixPush();
    do {
        vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z);
    } while (0);
    pass_rotate_zyx(rot);
    axis.x = 10.0f;
    axis.y = 0;
    axis.z = 0;
    lockon_draw_line_3d(&org, &axis, 0x80000080, 0x80000080);
    axis.y = 10.0f;
    axis.x = 0;
    axis.z = 0;
    lockon_draw_line_3d(&org, &axis, 0x80008000, 0x80008000);
    axis.x = 0;
    axis.y = 0;
    axis.z = vec3_length(&delta);
    ((void (*)(PassVec3 *, PassVec3 *, unsigned, unsigned))lockon_draw_line_3d)(&org, &axis, 0x80800000, 0x80800000);
    ulMatrixPop();
}

/*
 * Every step frames from time to the end, at most last frames: camera path grey to red with
 * markers, else look-at grey to blue. Channels unresolved.
 */
/* steer(cast) */
/* 0x00146888 */
void pass_draw_path(int pass, float time, float step, float last)
{
    PassVec3 prev_cam;
    PassVec3 prev_look;
    PassVec3 cur_cam;
    PassVec3 cur_look;
    int base;
    int pbase;
    float end;
    float start;
    int prev_mode;
    int cur_mode;

    if (time <= 0.0f)
        start = 0.0f;
    else
        start = time;
    time = start;
    if (last <= 0.0f) {
        end = (float)(pass_table[pass].start + pass_table[pass].len);
    } else {
        float stop = time + last;
        float lim = (float)(pass_table[pass].start + pass_table[pass].len);

        if (lim < stop)
            end = lim;
        else
            end = stop;
    }
    cur_mode = sequence_eval(pass, start, &base, &cur_cam, &cur_look);
    while (time < end) {
        time = time + step;
        pbase = base;
        prev_cam.x = cur_cam.x;
        prev_cam.y = cur_cam.y;
        prev_cam.z = cur_cam.z;
        prev_look.x = cur_look.x;
        prev_look.y = cur_look.y;
        prev_look.z = cur_look.z;
        prev_mode = cur_mode;
        cur_mode = sequence_eval(pass, time, &base, &cur_cam, &cur_look);
        if ((prev_mode & 0x4081) == 0) {
            if ((cur_mode & 1) == 0)
                lockon_draw_line_3d(&prev_cam, &cur_cam, 0x80606060, 0x80000080);
            pass_draw_camera_axes(&prev_cam, &prev_look, pbase);
        } else if ((prev_mode & 0x81) == 0) {
            if ((cur_mode & 1) == 0)
                lockon_draw_line_3d(&prev_cam, &cur_cam, 0x80606060, 0x80000080);
            pass_draw_camera_axes(&prev_cam, &prev_look, 0);
        } else if ((prev_mode & 1) == 0) {
            if ((cur_mode & 1) == 0)
                lockon_draw_line_3d(&prev_cam, &cur_cam, 0x80606060, 0x80000080);
        } else if ((prev_mode & 0x80) == 0 && (cur_mode & 0x80) == 0) {
            ((void (*)(PassVec3 *, PassVec3 *, unsigned, unsigned))lockon_draw_line_3d)(&prev_look, &cur_look, 0x80606060, 0x80800000);
        }
    }
}

/* Resolved to world positions (pass_eval_part). */
/* steer(cast) */
/* 0x00146b30 */
void pass_draw_part_path(int pass, float time, float step, float last)
{
    PassVec3 prev_cam;
    PassVec3 prev_look;
    PassVec3 cur_cam;
    PassVec3 cur_look;
    int base;
    int pbase;
    float end;
    float start;
    unsigned prev_flags;
    unsigned cur_flags;

    if (time <= 0.0f)
        start = 0.0f;
    else
        start = time;
    time = start;
    if (last <= 0.0f) {
        end = (float)(pass_table[pass].start + pass_table[pass].len);
    } else {
        float stop = time + last;
        float lim = (float)(pass_table[pass].start + pass_table[pass].len);

        if (lim < stop)
            end = lim;
        else
            end = stop;
    }
    cur_flags = pass_eval_part(pass, start, &base, (float *)&cur_cam, (float *)&cur_look);
    while (time < end) {
        time = time + step;
        pbase = base;
        prev_cam.x = cur_cam.x;
        prev_cam.y = cur_cam.y;
        prev_cam.z = cur_cam.z;
        prev_look.x = cur_look.x;
        prev_look.y = cur_look.y;
        prev_look.z = cur_look.z;
        prev_flags = cur_flags;
        cur_flags = pass_eval_part(pass, time, &base, (float *)&cur_cam, (float *)&cur_look);
        if ((prev_flags & 7) == 7) {
            if (cur_flags & 1)
                lockon_draw_line_3d(&prev_cam, &cur_cam, 0x80606060, 0x80000080);
            pass_draw_camera_axes(&prev_cam, &prev_look, pbase);
        } else if ((prev_flags & 3) == 3) {
            if (cur_flags & 1)
                lockon_draw_line_3d(&prev_cam, &cur_cam, 0x80606060, 0x80000080);
            pass_draw_camera_axes(&prev_cam, &prev_look, 0);
        } else if (prev_flags & 1) {
            if (cur_flags & 1)
                lockon_draw_line_3d(&prev_cam, &cur_cam, 0x80606060, 0x80000080);
        } else if ((prev_flags & 2) && (cur_flags & 2)) {
            ((void (*)(PassVec3 *, PassVec3 *, unsigned, unsigned))lockon_draw_line_3d)(&prev_look, &cur_look, 0x80606060, 0x80800000);
        }
    }
}

/* 0x00146dd8 */
void *pass_table_get(void)
{
    return pass_table;
}

/* 0x00146de8 */
void pass_table_set(void *tbl, int count)
{
    pass_count = count;
    pass_table = tbl;
}

/* 0x00146e00 */
int pass_table_count(void)
{
    return pass_count;
}

/* 0x00146e10 */
float pass_table_start(int i)
{
    return (float)pass_table[i].start;
}

/* 0x00146e30 */
float pass_table_end(int i)
{
    return (float)(pass_table[i].start + pass_table[i].len);
}

/* Saves the current one (one level). */
/* 0x00146e60 */
void pass_table_push(PathTblRef *src)
{
    pass_saved_table = pass_table;
    pass_saved_count = pass_count;
    pass_table = src->tbl;
    pass_count = src->count;
}

/* 0x00146e98 */
void pass_table_pop(void)
{
    pass_table = pass_saved_table;
    pass_count = pass_saved_count;
}

/* 0x00146ec0 */
void pass_rotate_zyx(int *ang)
{
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
}
