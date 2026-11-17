/*
 * Chunk model compile: copies an object tree into one block with strip chunks as ready VIF
 * upload data for VU1; refresh after vertex changes; debug dumps.
 */

#include "types_nj.h"
#include "ninja.h"
#include "libc.h"
#include "libc_internal.h"

static void nj_cnk_plist_dump(short *p);
static void nj_cnk_model_dump(NjCnkModel *model);
static int nj_compile_strip(short *src, int type, NjVlistInfo *vi, unsigned int *dst);
static void nj_compile_plist_refresh(short *p, short *dst, NjVlistInfo *vi);
static short *nj_compile_plist(short *p, NjVlistInfo *vi, char *base, int *off);
static NjCnkModel *nj_compile_model(NjCnkModel *model, char *base, int *off);
static NjCnkObject *nj_compile_object_node(NjCnkObject *obj, char *base, int *off);
static unsigned int nj_compile_strip_size(int type, short *p, NjVlistInfo *vi);
static int nj_compile_plist_size(short *p, NjVlistInfo *vi);
static void nj_compile_vlist_info(int *vlist, NjVlistInfo *vi);
static int nj_compile_model_size(NjCnkModel *model);
static void nj_compile_object_size_add(NjCnkObject *obj, int *size);
static int nj_compile_object_size(NjCnkObject *obj);

extern void nj_cnk_plist_dump(short *p);
extern void nj_cnk_model_dump(NjCnkModel *model);
extern void nj_compile_vlist_info(int *vlist, NjVlistInfo *vi);
extern void nj_compile_plist_refresh(short *p, short *out, NjVlistInfo *vi);
extern unsigned int nj_compile_strip_size(int type, short *p, NjVlistInfo *vi);
extern short *nj_compile_plist(short *p, NjVlistInfo *vi, char *base, int *off);
extern NjCnkModel *nj_compile_model(NjCnkModel *model, char *base, int *off);
extern void *(*nj_compile_alloc)(int size);
extern int nj_compile_object_size(NjCnkObject *obj);
extern NjCnkObject *nj_compile_object_node(NjCnkObject *obj, char *base, int *off);
extern void (*nj_compile_hook)(void *obj);
extern int abs(int);
extern char str_nj_compile_assert[];
extern int nj_compile_plist_size(short *p, NjVlistInfo *vi);
extern int nj_compile_model_size(NjCnkModel *model);
extern int nj_compile_size;
extern void nj_compile_object_size_add(NjCnkObject *obj, int *size);
extern int nj_compile_strip(short *src, int type, NjVlistInfo *vi, unsigned int *dst);

/* Debug: type, head bits, data size per chunk. */
/* 0x001ad7c8 */
static void nj_cnk_plist_dump(short *p)
{
    int head, type, n;

    printf("<PLIST INFO>\n");
    while ((head = *p++) != 255) {
        type = head & 0xff;
        printf("CHUNK:%02X, HEADBITS:%02X, ", type, (head & 0xff00) >> 8);
        if (type < 8) {
            printf("SIZE:%04X, NJD_CB\n", 0);
        } else if (type < 16) {
            p++;
            printf("SIZE:%04X, NJD_TID\n", 1);
        } else if (type < 64) {
            n = *p++;
            p += n;
            printf("SIZE:%04X, NJD_CM\n", n & 0xffff);
        } else {
            n = *p++;
            printf("SIZE:%04X, NJD_CS\n", n & 0xffff);
            p += n;
        }
    }
}

/* Debug: bounding sphere, then the polygon list. */
/* 0x001ad8d8 */
static void nj_cnk_model_dump(NjCnkModel *model)
{
    printf("CENTER = (% f,% f,% f)\n", model->center.x, model->center.y, model->center.z);
    printf("RADIUS = (%f)\n", model->r);
    if (model->plist)
        nj_cnk_plist_dump(model->plist);
}

/* Debug: each node's position and model, children, siblings. */
/* 0x001ad988 */
static void nj_object_dump(NjObject *obj)
{
    while (obj) {
        printf("POS(% f,% f,% f)\n", obj->pos[0], obj->pos[1], obj->pos[2]);
        if (obj->model)
            nj_cnk_model_dump(obj->model);
        if (obj->child)
            nj_object_dump(obj->child);
        obj = obj->sibling;
    }
}

/* 0x001ada40 */
void nj_compile_call_hook(void *obj)
{
    nj_compile_hook(obj);
}

/*
 * Chunk 64 no UV; 65, 66 UV in 1/256, 1/1024. Per strip: header (length, winding), positions,
 * normals (41, 42), UV, colours (35, 42, 0..1 floats), program continue. Words, quadword-rounded.
 */
/* 0x001ada60 */
static int nj_compile_strip(short *src, int type, NjVlistInfo *vi, unsigned int *dst)
{
    int vtype, nw, i, j, len, sgn, cnt, n;
    unsigned int code;
    short *idx;
    float uvs, k;

    switch (type) {
    case 64:
        uvs = 0.0f;
        nw = 1;
        break;
    case 65:
        uvs = 1.0f / 256.0f;
        nw = 3;
        break;
    case 66:
        uvs = 1.0f / 1024.0f;
        nw = 3;
        break;
    default:
        uvs = 0.0f;
        nw = 0;
        break;
    }
    k = 0.003921569f;
    vtype = vi->type;
    *dst++ = 0x01000104;
    cnt = *src++ & ~0xc000;
    n = 1;
    for (i = 0; i < cnt; i++) {
        len = *src++;
        if (len > 0) {
            sgn = 1;
        } else {
            len = -len;
            sgn = -1;
        }
        *dst++ = 0x6c018002;
        ((float *)dst)[0] = -1.0f;
        dst[1] = len;
        ((float *)dst)[2] = (float)sgn;
        dst[3] = 0;
        dst += 4;
        n += 5;
        code = len << 16;
        *dst++ = code | 0x68008004;
        idx = src;
        for (j = 0; j < len; j++) {
            int *pos = vi->pos + *idx * vi->stride;

            idx += nw;
            dst[0] = pos[0];
            dst[1] = pos[1];
            dst[2] = pos[2];
            dst += 3;
        }
        n += 1 + len * 3;
        if ((unsigned int)(vtype - 41) < 2) {
            *dst++ = code | 0x68008005;
            idx = src;
            for (j = 0; j < len; j++) {
                int *nrm = vi->nrm + *idx * vi->stride;

                idx += nw;
                dst[0] = nrm[0];
                dst[1] = nrm[1];
                dst[2] = nrm[2];
                dst += 3;
            }
            n += 1 + len * 3;
        }
        if ((unsigned int)(type - 65) < 2) {
            *dst++ = code | 0x64008006;
            idx = src;
            for (j = 0; j < len; j++) {
                ((float *)dst)[0] = (float)idx[1] * uvs;
                ((float *)dst)[1] = (float)idx[2] * uvs;
                idx += nw;
                dst += 2;
            }
            n += 1 + len * 2;
        }
        if (vtype == 35 || vtype == 42) {
            *dst++ = code | 0x6c008007;
            idx = src;
            for (j = 0; j < len; j++) {
                int *cp = vi->col + *idx * vi->stride;
                int rgba = *cp;

                idx += nw;
                ((float *)dst)[0] = (float)(rgba & 0xff) * k;
                ((float *)dst)[1] = (float)(rgba >> 8 & 0xff) * k;
                ((float *)dst)[2] = (float)(rgba >> 16 & 0xff) * k;
                ((float *)dst)[3] = (float)(int)((unsigned int)rgba >> 24) * k;
                dst += 4;
            }
            n += 1 + len * 4;
        }
        *dst++ = 0x17000000;
        n++;
        src += len * nw;
    }
    dst[0] = 0x01000101;
    dst[1] = 0x10000000;
    n += 2;
    while (n & 3)
        n++;
    return n;
}

/* dst's strip data from source list p. */
/* 0x001ade90 */
static void nj_compile_plist_refresh(short *p, short *dst, NjVlistInfo *vi)
{
    int type, head, n, dst_len;
    short *out = dst;

    while ((head = *p++) != 255) {
        out++;
        type = head & 0xff;
        if (type < 8) {
        } else if (type < 16) {
            p++;
            out++;
        } else if (type < 64) {
            n = *p++;
            dst_len = *out++;
            p += n;
            out += dst_len;
        } else {
            n = *p++;
            dst_len = *out++;
            switch (type) {
            case 64:
            case 65:
            case 66:
                nj_compile_strip(p, type, vi, out + (out[4] + 5));
                break;
            }
            p += n;
            out += dst_len;
        }
    }
}

/* Every model's strips from src's current vertices (after vertex changes). Returns dst. */
/* 0x001adf98 */
NjCnkObject *nj_compile_object_refresh(NjCnkObject *dst, NjCnkObject *src)
{
    NjVlistInfo vi;
    NjCnkObject *root;
    NjCnkModel *sm, *dm;

    root = dst;
    while (src) {
        if ((sm = src->model) && (dm = dst->model) && sm->vlist && sm->plist) {
            nj_compile_vlist_info(sm->vlist, &vi);
            nj_compile_plist_refresh(sm->plist, dm->plist, &vi);
        }
        if (src->child)
            nj_compile_object_refresh(dst->child, src->child);
        src = src->sibling;
        dst = dst->sibling;
    }
    return root;
}

/* To base + *off; strip chunks -> header (type, size, pad) + strips on 16 bytes. Advances *off. */
/* 0x001ae058 */
static short *nj_compile_plist(short *p, NjVlistInfo *vi, char *base, int *off)
{
    short *start, *dst, *chunk_hdr, *strip_hdr;
    int head, type, n, len;
    unsigned int size, k;

    n = 0;
    start = dst = (short *)(base + *off);
    while ((head = *p++) != 255) {
        type = head & 0xff;
        *dst++ = head;
        n++;
        if (type < 8) {
        } else if (type < 16) {
            *dst++ = *p++;
            n++;
        } else if (type < 64) {
            len = *p++;
            *dst++ = len;
            memcpy(dst, p, len * 2);
            p += len;
            dst += len;
            n += len + 1;
        } else {
            chunk_hdr = dst - 1;
            len = *p++;
            n++;
            switch (type) {
            case 64:
            case 65:
            case 66:
                dst++;
                dst[0] = 0;
                dst[1] = 0;
                dst[2] = vi->type;
                n += 5;
                strip_hdr = dst;
                size = nj_compile_strip_size(type, p, vi);
                dst[3] = size >> 4;
                if (n & 7)
                    dst[4] = 8 - (n & 7);
                else
                    dst[4] = 0;
                n += dst[4];
                dst += dst[4] + 5;
                if ((int)dst & 0xf)
                    printf("nj_compile_plist: d_pl is not 16byte align![%08x]", dst);
                nj_compile_strip(p, type, vi, dst);
                k = size >> 1;
                chunk_hdr[1] = strip_hdr[4] + (k + 5);
                dst += k;
                n += k;
                break;
            default:
                *dst++ = len;
                memcpy(dst, p, len * 2);
                n += len;
                dst += len;
                break;
            }
            p += len;
        }
    }
    *dst = 255;
    n = (n + 8) & 0xfffffff8;
    *off += n * 2;
    return start;
}

/* To base + *off with its compiled list; no vertex list needed (vertices are in the strips). */
/* 0x001ae2a8 */
static NjCnkModel *nj_compile_model(NjCnkModel *model, char *base, int *off)
{
    NjVlistInfo vi;
    NjCnkModel *dst;

    dst = (NjCnkModel *)(base + *off);
    *dst = *model;
    dst->vlist = 0;
    *off += 32;
    if (model->vlist && model->plist) {
        nj_compile_vlist_info(model->vlist, &vi);
        dst->plist = nj_compile_plist(model->plist, &vi, base, off);
    }
    return dst;
}

/* To base + *off, then model, children, siblings. */
/* 0x001ae368 */
static NjCnkObject *nj_compile_object_node(NjCnkObject *obj, char *base, int *off)
{
    NjCnkObject *dst;

    dst = (NjCnkObject *)(base + *off);
    *dst = *obj;
    *off += 64;
    if (obj->model)
        dst->model = nj_compile_model(obj->model, base, off);
    if (obj->child)
        dst->child = nj_compile_object_node(obj->child, base, off);
    if (obj->sibling)
        dst->sibling = nj_compile_object_node(obj->sibling, base, off);
    return dst;
}

/* One block from nj_compile_alloc; 0 if marked not to compile or empty. */
/* 0x001ae478 */
NjCnkObject *nj_compile_object(NjCnkObject *obj)
{
    int size, off;
    NjCnkObject *dst;

    if (obj->evalflags & 0x2000)
        return 0;
    size = nj_compile_object_size(obj);
    if (!size)
        return 0;
    dst = nj_compile_alloc(size);
    memset(dst, 0, size);
    off = 0;
    nj_compile_object_node(obj, (char *)dst, &off);
    return dst;
}

/* Allocator for nj_compile_object, hook for nj_compile_call_hook. */
/* 0x001ae508 */
void nj_compile_set_alloc(void *(*alloc)(int size), void (*hook)(void *obj))
{
    nj_compile_alloc = alloc;
    nj_compile_hook = hook;
}

/* Bytes nj_compile_strip writes; other chunk types assert(0). */
/* 0x001ae518 */
static unsigned int nj_compile_strip_size(int type, short *p, NjVlistInfo *vi)
{
    int n, nstrip, i, len, step;

    n = 1;
    nstrip = (*p++ << 2) >> 2 & 0x3fff;
    switch (type) {
    case 64:
        step = 1;
        break;
    case 65:
    case 66:
        step = 3;
        break;
    default:
        __assert("nj_compile.c", 706, str_nj_compile_assert);
        break;
    }
    for (i = 0; i < nstrip; i++) {
        len = *p++;
        len = abs(len);
        n += 6 + len * 3;
        if (vi->type == 41 || vi->type == 42)
            n += 1 + len * 3;
        if (type == 65 || type == 66)
            n += 1 + len * 2;
        if (vi->type == 35 || vi->type == 42)
            n += 1 + len * 4;
        p += len * step;
        n++;
    }
    n += 2;
    while (n & 3)
        n++;
    return n * 4;
}

/* Bytes nj_compile_plist writes. */
/* 0x001ae680 */
static int nj_compile_plist_size(short *p, NjVlistInfo *vi)
{
    int type, n, len, rem;

    n = 0;
    while ((type = *p++) != 255) {
        type &= 0xff;
        n++;
        if (type < 8) {
        } else if (type < 16) {
            p++;
            n++;
        } else if (type < 64) {
            len = *p++;
            n += len + 1;
            p += len;
        } else {
            len = *p++;
            n++;
            switch (type) {
            case 64:
            case 65:
            case 66:
                n += 5;
                if ((rem = n & 7) != 0)
                    n += 8 - rem;
                n += nj_compile_strip_size(type, p, vi) >> 1;
                break;
            default:
                n += len;
                break;
            }
            p += len;
        }
    }
    n = (n + 8) & 0xfffffff8;
    return n * 2;
}

/* Position/normal/colour starts, stride in words (types 34, 35, 41, 42); last chunk wins. */
/* 0x001ae798 */
static void nj_compile_vlist_info(int *vlist, NjVlistInfo *vi)
{
    unsigned int head;
    int type, n;

    while ((head = *vlist) != 255) {
        type = head & 0xff;
        n = head >> 16;
        vi->type = type;
        switch (type) {
        case 34:
            vi->data = vlist + 2;
            vi->stride = 3;
            vi->pos = vlist + 2;
            vi->nrm = 0;
            vi->col = 0;
            break;
        case 41:
            vi->data = vlist + 2;
            vi->stride = 6;
            vi->pos = vlist + 2;
            vi->nrm = vlist + 5;
            vi->col = 0;
            break;
        case 35:
            vi->data = vlist + 2;
            vi->stride = 4;
            vi->pos = vlist + 2;
            vi->nrm = 0;
            vi->col = vlist + 5;
            break;
        case 42:
            vi->data = vlist + 2;
            vi->stride = 7;
            vi->pos = vlist + 2;
            vi->nrm = vlist + 5;
            vi->col = vlist + 8;
            break;
        default:
            vi->data = 0;
            vi->stride = 0;
            vi->pos = 0;
            vi->nrm = 0;
            vi->col = 0;
            break;
        }
        vlist += n + 1;
    }
}

/* Bytes, model plus polygon list. */
/* 0x001ae8a8 */
static int nj_compile_model_size(NjCnkModel *model)
{
    NjVlistInfo vi;
    int n;

    n = 32;
    if (model->vlist && model->plist) {
        nj_compile_vlist_info(model->vlist, &vi);
        n = nj_compile_plist_size(model->plist, &vi) + 32;
    }
    return n;
}

/* obj, children, siblings into *size. */
/* 0x001ae900 */
static void nj_compile_object_size_add(NjCnkObject *obj, int *size)
{
    *size += 64;
    if (obj->model)
        *size += nj_compile_model_size(obj->model);
    if (obj->child)
        nj_compile_object_size_add(obj->child, size);
    if (obj->sibling)
        nj_compile_object_size_add(obj->sibling, size);
}

/* 0 if marked not to compile. */
/* 0x001ae978 */
static int nj_compile_object_size(NjCnkObject *obj)
{
    nj_compile_size = 0;
    if (obj->evalflags & 0x2000)
        return 0;
    nj_compile_object_size_add(obj, &nj_compile_size);
    return nj_compile_size;
}
