/*
 * Model break-up: a posed model compiled into loose triangles (centre, normal velocity, spin) and a
 * short command list, scattered and drawn flat or textured as they fly apart and fade.
 */

#include "types_game.h"
#include "mathdef.h"
#include "nj_cnk_vu1.h"
#include "ul_gx.h"
#include "ul_gx_prim.h"
#include "ul_matrix.h"
#include "anim.h"
#include "file.h"
#include "heap.h"
#include "light.h"
#include "math_util.h"
#include "nj_view.h"
#include "debug.h"
#include "system.h"

typedef struct {
    int vlist;
    int plist;
    float bound[4];
} MeshState;

typedef struct ModbreakStrip3Idx {
    short v[3];
} ModbreakStrip3Idx;

extern int modbreak_project_vertex(ModbreakOut *, ModbreakVtx *, float, float, float, float, float, float, float);
extern int modbreak_project_vertex_tex(ModbreakOutTex *, ModbreakVtx *, float, float, float, float, float, float, float);
extern unsigned nj_override_flags;

extern unsigned short *modbreak_decode_attrs(unsigned flags, unsigned short *p, float *a, float *b, float sx,
                                     float sy, float sz);

extern void modbreak_draw_tris(ModbreakVtx *vtx, ModbreakTri *t, int n, float a, float b, float c, float d, float e,
                          float g, float h);

extern void modbreak_draw_tris_tex(ModbreakVtx *vtx, ModbreakTri *t, int n, float a, float b, float c, float d, float e,
                          float g, float h);

extern int modbreak_strip_size(short *);
extern int modbreak_strip3_size(short *);
extern void modbreak_count_tree(DcNode *, int *, int *);
extern ModbreakGxVtx gx_vertex_buffer[];
extern short *modbreak_cmd_out;
extern unsigned short *modbreak_sub_streams[];
extern int modbreak_compile_commands(unsigned short *);
extern int ulMatrixPush(void);
extern int *modbreak_tri_out;
extern int *modbreak_vtx_out;
extern int modbreak_compile_tree(DcNode *);
extern void modbreak_count(MapPart *, int *, int *);
extern void modbreak_build_buffers(MapPart *w, int *tbl, int *buf, short *sbuf);
extern int modbreak_compile_strips(short *p, int flag);
extern int modbreak_compile_strips3(short *p);
extern void modbreak_count_commands(DcNodeData *pay, int *ntri, int *nshort);
extern void modbreak_draw_compiled(ModbreakTri *tri, short *cmd, int vtx_buf);
extern void modbreak_transform_points(int *p);

/* Colour = vertex * mul + add, alpha halved to PS2 range. 0 behind near plane or past +-1000. */
/* 0x00136280 */
int modbreak_project_vertex(ModbreakOut *out, ModbreakVtx *vtx, float mul_a, float mul_r, float mul_g, float mul_b, float add_r, float add_g, float add_b)
{
    float p[3];
    int ca, cg, cb, cr;

    project_point(vtx, p);
    if (p[2] < 1.0f) {
        return 0;
    }
    out->x = p[0];
    if (p[0] < -1000.0f || p[0] > 1000.0f) {
        return 0;
    }
    out->y = p[1];
    if (p[1] < -1000.0f || p[1] > 1000.0f) {
        return 0;
    }
    out->w = 1.0f / p[2];
    cg = (int)((mul_g * vtx->col_g + add_g) * 255.0f);
    cr = (int)((mul_r * vtx->col_r + add_r) * 255.0f);
    ca = (int)((mul_a * vtx->col_a) * 255.0f);
    cb = (int)((mul_b * vtx->col_b + add_b) * 255.0f);
    if (cg < 0) {
        cg = 0;
    }
    if (cg > 255) {
        cg = 255;
    }
    if (ca < 0) {
        ca = 0;
    }
    if (ca > 255) {
        ca = 255;
    }
    if (cr < 0) {
        cr = 0;
    }
    if (cr > 255) {
        cr = 255;
    }
    if (cb < 0) {
        cb = 0;
    }
    if (cb > 255) {
        cb = 255;
    }
    out->color = ((((ca + 1) >> 1) << 24) | (((cg & 255) << 8) | ((cb & 255) << 16))) | (cr & 255);
    return 1;
}

/* Skips faded (alpha < 1/256) or unprojectable triangles. */
/* 0x00136470 */
void modbreak_draw_tris(ModbreakVtx *vtx, ModbreakTri *tri, int n, float mul_a, float mul_r, float mul_g, float mul_b, float add_r, float add_g, float add_b)
{
    ModbreakOut out[3];
    float alpha;

    ulGxBeginStrip(1);
    tri--;
    while (n-- != 0) {
        tri++;
        alpha = mul_a * tri->alpha;
        if (alpha < 0.00390625f) {
            vtx += 3;
            continue;
        }
        if (!modbreak_project_vertex(&out[0], vtx, alpha, mul_r, mul_g, mul_b, add_r, add_g, add_b)) {
            vtx += 3;
            continue;
        }
        vtx++;
        if (!modbreak_project_vertex(&out[1], vtx, alpha, mul_r, mul_g, mul_b, add_r, add_g, add_b)) {
            vtx += 2;
            continue;
        }
        vtx++;
        if (!modbreak_project_vertex(&out[2], vtx, alpha, mul_r, mul_g, mul_b, add_r, add_g, add_b)) {
            vtx++;
            continue;
        }
        vtx++;
        ulGxAddStrip(out, 3);
    }
    ulGxEndStrip();
}

/* Also copies UVs; all four channels halved (textures modulate at 0x80 = full). */
/* 0x00136620 */
int modbreak_project_vertex_tex(ModbreakOutTex *out, ModbreakVtx *vtx, float mul_a, float mul_r, float mul_g, float mul_b, float add_r, float add_g, float add_b)
{
    float p[3];
    int ca, cg, cb, cr;

    project_point(vtx, p);
    if (p[2] < 1.0f) {
        return 0;
    }
    out->x = p[0];
    if (p[0] < -1000.0f || p[0] > 1000.0f) {
        return 0;
    }
    out->y = p[1];
    if (p[1] < -1000.0f || p[1] > 1000.0f) {
        return 0;
    }
    out->w = 1.0f / p[2];
    out->u = vtx->u;
    out->v = vtx->v;
    {
        float prod_b, prod_a, prod_g, prod_r;

        prod_b = mul_b * vtx->col_b;
        prod_a = mul_a * vtx->col_a;
        prod_g = mul_g * vtx->col_g;
        prod_r = mul_r * vtx->col_r;
        cb = (int)((prod_b + add_b) * 255.0f);
        ca = (int)(prod_a * 255.0f);
        cr = (int)((prod_r + add_r) * 255.0f);
        cg = (int)((prod_g + add_g) * 255.0f);
    }
    if (cb < 0) {
        cb = 0;
    }
    if (ca < 0) {
        ca = 0;
    }
    if (cr < 0) {
        cr = 0;
    }
    if (cg < 0) {
        cg = 0;
    }
    if (cb > 255) {
        cb = 255;
    }
    if (ca > 255) {
        ca = 255;
    }
    if (cg > 255) {
        cg = 255;
    }
    if (cr > 255) {
        cr = 255;
    }
    out->color = ((((ca + 1) >> 1) << 24) | (((cg + 1) >> 1 & 255) << 8 | ((cb + 1) >> 1 & 255) << 16)) | ((cr + 1) >> 1 & 255);
    return 1;
}

/* 0x00136838 */
void modbreak_draw_tris_tex(ModbreakVtx *vtx, ModbreakTri *tri, int n, float mul_a, float mul_r, float mul_g, float mul_b, float add_r, float add_g, float add_b)
{
    ModbreakOutTex out[3];
    float alpha;

    ulGxBeginTexStrip(1);
    tri--;
    while (n-- != 0) {
        tri++;
        alpha = mul_a * tri->alpha;
        if (alpha < 0.00390625f) {
            vtx += 3;
            continue;
        }
        if (!modbreak_project_vertex_tex(&out[0], vtx, alpha, mul_r, mul_g, mul_b, add_r, add_g, add_b)) {
            vtx += 3;
            continue;
        }
        vtx++;
        if (!modbreak_project_vertex_tex(&out[1], vtx, alpha, mul_r, mul_g, mul_b, add_r, add_g, add_b)) {
            vtx += 2;
            continue;
        }
        vtx++;
        if (!modbreak_project_vertex_tex(&out[2], vtx, alpha, mul_r, mul_g, mul_b, add_r, add_g, add_b)) {
            vtx++;
            continue;
        }
        vtx++;
        ulGxAddTexStrip(out, 3);
    }
    ulGxEndTexStrip();
}

/* Flags: 0x10 diffuse (forced override), 0x20 ambient * light, 0x40 skipped. Returns p past it. */
/* 0x001369e8 */
unsigned short *modbreak_decode_attrs(unsigned flags, unsigned short *p, float *mul, float *add, float sx, float sy, float sz)
{
    float *src;

    if (flags & 0x10) {
        if (nj_override_flags & 0x10) {
            src = nj_cnk_override_color;
            *mul++ = *src++;
            *mul++ = *src++;
            mul[0] = src[0];
            mul[1] = src[1];
        } else {
            unsigned short first, second;

            first = p[0];
            second = p[1];
            *mul++ = (first >> 8) * 0.00392157f;
            *mul++ = (first & 255) * 0.00392157f;
            mul[0] = (second >> 8) * 0.00392157f;
            mul[1] = (second & 255) * 0.00392157f;
        }
        p += 2;
    }
    if (flags & 0x20) {
        unsigned short first, second;

        first = *p++;
        second = *p++;
        *add++ = (first >> 8) * 0.00392157f;
        *add++ = (first & 255) * 0.00392157f * sx;
        add[0] = (second >> 8) * 0.00392157f * sy;
        add[1] = (second & 255) * 0.00392157f * sz;
    }
    return flags & 0x40 ? p + 2 : p;
}

/* Commands until 0: >= 16 material colours, 1 texture, 2 n flat triangles, 3 or 4 n textured. */
/* 0x00136b50 */
void modbreak_draw_compiled(ModbreakTri *tri, short *cmd, int vtx_buf)
{
    MeshState st;
    float mul[4];
    float add[4];
    volatile float lx;
    float ly, lz;
    float mul_a, mul_r, mul_g, mul_b, add_r, add_g, add_b;
    int op, n;
    ModbreakVtx *vtx;
    float *amb;

    amb = &ly;
    /* Never true: (x & 1) >> 1 is 0. */
    if ((vtx_buf & 1) >> 1) {
        ((int *)amb)[0] = vtx_buf;
        ((int *)amb)[1] = vtx_buf;
        ((int *)amb)[2] = vtx_buf;
        ((int *)amb)[3] = vtx_buf;
        ((int *)amb)[4] = vtx_buf;
        ((int *)amb)[5] = vtx_buf;
        ((int *)amb)[6] = vtx_buf;
    }
    add_r = add_g = add_b = mul_r = mul_g = mul_b = 0.0f;
    light_get_ambient(&lx, amb, &lz);
    mul_a = 1.0f;
    st.vlist = vtx_buf;
    st.plist = 0;
    st.bound[0] = add_b;
    st.bound[1] = add_b;
    st.bound[2] = add_b;
    st.bound[3] = add_b;
    nj_cnk_vu1_draw_clip(&st);
    add[0] = mul_a;
    mul[0] = mul_a;
    mul[3] = add_b;
    mul[2] = add_b;
    mul[1] = add_b;
    add[3] = add_b;
    add[2] = add_b;
    add[1] = add_b;
    vtx = gx_vertex_buffer;
    while ((op = *cmd++) != 0) {
        if (op >= 16) {
            cmd = (short *)modbreak_decode_attrs(op, (unsigned short *)cmd, mul, add, lx, ly, lz);
            mul_a = mul[0];
            mul_r = mul[1];
            mul_g = mul[2];
            mul_b = mul[3];
            add_r = add[1];
            add_g = add[2];
            add_b = add[3];
        } else {
            switch (op) {
            case 3:
            case 4:
                n = *cmd;
                modbreak_draw_tris_tex(vtx, tri, n, mul_a, mul_r, mul_g, mul_b, add_r, add_g, add_b);
                tri += n;
                vtx += n * 3;
                cmd++;
                break;
            case 1:
                ulGxSelectTexture(ulGxGetTextureList(), *cmd++);
                break;
            case 2:
                {
                    int k = *cmd;

                    modbreak_draw_tris(vtx, tri, k, mul_a, mul_r, mul_g, mul_b, add_r, add_g, add_b);
                    tri += k;
                    vtx += k * 3;
                    cmd++;
                }
                break;
            }
        }
    }
}

/* A strip of n vertices has n - 2. */
/* 0x00136d88 */
int modbreak_strip_size(short *p)
{
    int n = *p++;
    int total = 0;
    int len;

    while (n-- != 0) {
        len = *p++;
        if (len < 0) {
            len = -len;
        }
        total += len - 2;
        p += len;
    }
    return total;
}

/* Three words per vertex. */
/* 0x00136de0 */
int modbreak_strip3_size(short *p)
{
    int n = *p++;
    int total = 0;
    int len;

    while (n-- != 0) {
        len = *p++;
        if (len < 0) {
            len = -len;
        }
        total += len - 2;
        p = (short *)((ModbreakStrip3Idx *)p + len);
    }
    return total;
}

/* Triangles into *ntri, compiled command words into *nshort. */
/* 0x00136e40 */
void modbreak_count_commands(DcNodeData *pay, int *ntri, int *nshort)
{
    short *p = (short *)pay->cmds;
    int shorts = 0;
    int tris = 0;
    short len;

    if (p) {
        while (*p != 255) {
            len = p[1];
            switch (*(unsigned short *)p & 255) {
            case 0:
            case 4:
            case 5:
                p += 1;
                continue;
            case 8:
                shorts += 2;
                p += 2;
                continue;
            case 17:
            case 18:
            case 20:
                shorts += 3;
                break;
            case 19:
            case 21:
            case 22:
                shorts += 5;
                break;
            case 23:
                shorts += 7;
                break;
            case 64:
                shorts += 2;
                tris += modbreak_strip_size(p + 2);
                break;
            case 65:
                shorts += 2;
                tris += modbreak_strip3_size(p + 2);
                break;
            default:
                break;
            }
            p += len + 2;
        }
    }
    *ntri += tris;
    *nshort += shorts;
}

/* 0x00136f58 */
void modbreak_count_tree(DcNode *node, int *ntri, int *nshort)
{
    if (node) {
        if (node->data) {
            modbreak_count_commands(node->data, ntri, nshort);
        }
        if (node->child) {
            modbreak_count_tree(node->child, ntri, nshort);
        }
        if (node->next) {
            modbreak_count_tree(node->next, ntri, nshort);
        }
    }
}

/* 0x00136fd0 */
void modbreak_count(MapPart *w, int *ntri, int *nshort)
{
    *ntri = 0;
    *nshort = 0;
    modbreak_count_tree(w->root, ntri, nshort);
}

/* Position and normal into the shared vertex buffer at each block's first index. */
/* 0x00136fe8 */
void modbreak_transform_points(int *p)
{
    char *tbl = gx_vertex_buffer;
    char *src, *dst;
    int cnt, adv;
    short *rec;

    while (*p != 255) {
        rec = (short *)p;
        src = (char *)(rec + 4);
        dst = tbl + rec[2] * 24;
        cnt = rec[3];
        adv = (short)((unsigned short)rec[1] + 1);
        while (cnt-- > 0) {
            ulMatrixApplyPoint(ulMatrixCur, src, dst);
            src += 12;
            dst += 12;
            ulMatrixApplyVector(ulMatrixCur, src, dst);
            src += 12;
            dst += 12;
        }
        p += adv;
    }
}

/*
 * Per triangle: centre, last vertex's normal as velocity, spin, full alpha. Appends a draw command
 * (2 flat, 4 with flag) with the count; returns it.
 */
/* 0x001370f8 */
int modbreak_compile_strips(short *p, int flag)
{
    ModbreakGxVtx *tbl = gx_vertex_buffer;
    ModbreakVtx6Out *out = modbreak_vtx_out;
    int total = 0;
    int n, cnt, k;
    short *idx;
    ModbreakGxVtx *vtx;
    ModbreakTriRec *rec;
    float x, y, z;
    float vx, vy, vz, va, vb, vc;

    n = *p++;
    while (n-- != 0) {
        cnt = *p;
        if (cnt < 0)
            cnt = -cnt;
        idx = p + 1;
        p = idx + cnt;
        cnt -= 2;
        total += cnt;
        while (cnt-- != 0) {
            vtx = &tbl[idx[0]];
            vx = vtx->x;
            vy = vtx->y;
            vz = vtx->z;
            va = vtx->nx;
            vb = vtx->ny;
            vc = vtx->nz;
            out->x = vx;
            out->y = vy;
            out->z = vz;
            out->nx = va;
            out->ny = vb;
            out->nz = vc;
            x = vx;
            y = vy;
            z = vz;
            out++;
            vtx = &tbl[idx[1]];
            vx = vtx->x;
            vy = vtx->y;
            vz = vtx->z;
            va = vtx->nx;
            vb = vtx->ny;
            vc = vtx->nz;
            out->x = vx;
            out->y = vy;
            out->z = vz;
            out->nx = va;
            out->ny = vb;
            out->nz = vc;
            x += vx;
            y += vy;
            z += vz;
            out++;
            vtx = &tbl[idx[2]];
            vx = vtx->x;
            vy = vtx->y;
            vz = vtx->z;
            va = vtx->nx;
            vb = vtx->ny;
            vc = vtx->nz;
            out->x = vx;
            out->y = vy;
            out->z = vz;
            out->nx = va;
            out->ny = vb;
            out->nz = vc;
            x += vx;
            y += vy;
            z += vz;
            out++;
            idx++;
            rec = modbreak_tri_out;
            rec->center[0] = x * 0.3333333f;
            rec->center[1] = y * 0.3333333f;
            rec->center[2] = z * 0.3333333f;
            rec->vel_x = va;
            rec->vel_y = vb;
            rec->vel_z = vc;
            rec->spin_x = 182;
            rec->unk_24 = 0;
            rec->spin_y = 182;
            rec->alpha = 1.0f;
            modbreak_tri_out = rec + 1;
        }
    }
    modbreak_vtx_out = out;
    if (flag)
        *modbreak_cmd_out++ = 4;
    else
        *modbreak_cmd_out++ = 2;
    *modbreak_cmd_out++ = total;
    return total;
}

/* Three words per vertex; draw command 3. */
/* 0x00137330 */
int modbreak_compile_strips3(short *p)
{
    short *next = modbreak_cmd_out + 2;
    ModbreakGxVtx *tbl = gx_vertex_buffer;
    ModbreakVtx6Out *out = modbreak_vtx_out;
    int total = 0;
    int n, cnt, k;
    short *idx;
    short *hdr;
    ModbreakGxVtx *vtx;
    ModbreakTriRec *rec;
    float x, y, z;
    float vx, vy, vz, va, vb, vc;

    n = *p++;
    while (n-- != 0) {
        cnt = *p;
        if (cnt < 0)
            cnt = -cnt;
        idx = p + 1;
        p = idx + cnt * 3;
        cnt -= 2;
        total += cnt;
        while (cnt-- != 0) {
            vtx = &tbl[idx[0]];
            vx = vtx->x;
            vy = vtx->y;
            vz = vtx->z;
            va = vtx->nx;
            vb = vtx->ny;
            vc = vtx->nz;
            out->x = vx;
            out->y = vy;
            out->z = vz;
            out->nx = va;
            out->ny = vb;
            out->nz = vc;
            x = vx;
            y = vy;
            z = vz;
            out++;
            vtx = &tbl[idx[3]];
            vx = vtx->x;
            vy = vtx->y;
            vz = vtx->z;
            va = vtx->nx;
            vb = vtx->ny;
            vc = vtx->nz;
            out->x = vx;
            out->y = vy;
            out->z = vz;
            out->nx = va;
            out->ny = vb;
            out->nz = vc;
            x += vx;
            y += vy;
            z += vz;
            out++;
            vtx = &tbl[idx[6]];
            vx = vtx->x;
            vy = vtx->y;
            vz = vtx->z;
            va = vtx->nx;
            vb = vtx->ny;
            vc = vtx->nz;
            out->x = vx;
            out->y = vy;
            out->z = vz;
            out->nx = va;
            out->ny = vb;
            out->nz = vc;
            x += vx;
            y += vy;
            z += vz;
            out++;
            idx += 3;
            rec = modbreak_tri_out;
            rec->center[0] = x * 0.3333333f;
            rec->center[1] = y * 0.3333333f;
            rec->center[2] = z * 0.3333333f;
            rec->vel_x = va;
            rec->vel_y = vb;
            rec->vel_z = vc;
            rec->spin_y = 182;
            rec->alpha = 1.0f;
            modbreak_tri_out = rec + 1;
            rec->unk_24 = 0;
            rec->spin_x = 182;
        }
    }
    hdr = modbreak_cmd_out;
    modbreak_vtx_out = out;
    modbreak_cmd_out = next;
    hdr[0] = 3;
    hdr[1] = total;
    return total;
}

/* Strips (64, 65) -> triangles; 4 stores a sub-list 5 recompiles. Returns the triangle count. */
/* 0x00137538 */
int modbreak_compile_commands(unsigned short *p)
{
    int total = 0;
    short first;
    short len;

    while ((first = *p) != 255) {
        len = ((short *)p)[1];
        switch (p[0] & 255) {
        case 0:
            p += 1;
            continue;
        case 8:
            *modbreak_cmd_out++ = 1;
            *modbreak_cmd_out++ = p[1] & 0x1fff;
            p += 2;
            continue;
        case 4:
            {
                int k = (p[0] & 0xf00) >> 8;

                p += 1;
                modbreak_sub_streams[k] = p;
            }
            return total;
        case 5:
            total += modbreak_compile_commands(modbreak_sub_streams[(*p++ & 0xf00) >> 8]);
            continue;
        case 17:
            *modbreak_cmd_out++ = 16;
            *modbreak_cmd_out++ = p[2];
            *modbreak_cmd_out++ = p[3];
            break;
        case 18:
            *modbreak_cmd_out++ = 32;
            *modbreak_cmd_out++ = p[2];
            *modbreak_cmd_out++ = p[3];
            break;
        case 19:
            *modbreak_cmd_out++ = 48;
            *modbreak_cmd_out++ = p[2];
            *modbreak_cmd_out++ = p[3];
            *modbreak_cmd_out++ = p[4];
            *modbreak_cmd_out++ = p[5];
            break;
        case 20:
            *modbreak_cmd_out++ = 64;
            *modbreak_cmd_out++ = p[2];
            *modbreak_cmd_out++ = p[3];
            break;
        case 21:
            *modbreak_cmd_out++ = 80;
            *modbreak_cmd_out++ = p[2];
            *modbreak_cmd_out++ = p[3];
            *modbreak_cmd_out++ = p[4];
            *modbreak_cmd_out++ = p[5];
            break;
        case 22:
            *modbreak_cmd_out++ = 96;
            *modbreak_cmd_out++ = p[2];
            *modbreak_cmd_out++ = p[3];
            *modbreak_cmd_out++ = p[4];
            *modbreak_cmd_out++ = p[5];
            break;
        case 23:
            *modbreak_cmd_out++ = 112;
            *modbreak_cmd_out++ = p[2];
            *modbreak_cmd_out++ = p[3];
            *modbreak_cmd_out++ = p[4];
            *modbreak_cmd_out++ = p[5];
            *modbreak_cmd_out++ = p[6];
            *modbreak_cmd_out++ = p[7];
            break;
        case 64:
            total += modbreak_compile_strips(p + 2, (short)p[0] & 0x4000);
            break;
        case 65:
            total += modbreak_compile_strips3(p + 2);
            break;
        default:
            break;
        }
        p += len + 2;
    }
    return total;
}

/* Returns the triangle count. */
/* 0x001377d0 */
int modbreak_compile_tree(DcNode *node)
{
    int total = 0;
    DcNodeData *data;

    do {
        ulMatrixPush();
        data = node->data;
        anim_apply_node(node);
        if (data) {
            if (data->points) {
                modbreak_transform_points(data->points);
            }
            if (data->cmds) {
                total += modbreak_compile_commands(data->cmds);
            }
        }
        if (node->child) {
            total += modbreak_compile_tree(node->child);
        }
        ulMatrixPop();
        node = node->next;
    } while (node);
    return total;
}

/* Triangles -> tbl, vertex packet with VU header words -> buf, commands -> sbuf. */
/* 0x00137880 */
void modbreak_build_buffers(MapPart *w, int *tbl, int *buf, short *sbuf)
{
    int n;

    anim_start(w->clip, w->time);
    modbreak_cmd_out = sbuf;
    modbreak_vtx_out = buf + 2;
    modbreak_tri_out = tbl;
    n = modbreak_compile_tree(w->root);
    *modbreak_cmd_out++ = 0;
    *modbreak_vtx_out++ = 255;
    *modbreak_vtx_out++ = 255;
    *modbreak_vtx_out++ = 255;
    *modbreak_vtx_out++ = 255;
    modbreak_vtx_out = buf;
    *modbreak_vtx_out++ = ((n * 18 + 1) << 16) | 0x29;
    *modbreak_vtx_out++ = (n * 3) << 16;
}

/* One of 16 random offsets within +-range (around center if given); spin <= amp. */
/* 0x00137970 */
void modbreak_scatter(ModbreakTriRec *tri, int n, int amp, float *center, float scale, float range)
{
    float ofs[16][3];
    int jit[16][3];
    float (*cur_ofs)[3];
    int (*cur_jit)[3];
    int k;

    k = 16;
    cur_ofs = ofs;
    if (center) {
        do {
            k--;
            (*cur_ofs)[0] = RAND_FLOAT_A() * (range + range) - range + center[0];
            (*cur_ofs)[1] = RAND_FLOAT_A() * (range + range) - range + center[1];
            (*cur_ofs)[2] = RAND_FLOAT_A() * (range + range) - range + center[2];
            cur_ofs++;
        } while (k);
    } else {
        do {
            k--;
            (*cur_ofs)[0] = RAND_FLOAT_A() * (range + range) - range;
            (*cur_ofs)[1] = RAND_FLOAT_A() * (range + range) - range;
            (*cur_ofs)[2] = RAND_FLOAT_A() * (range + range) - range;
            cur_ofs++;
        } while (k);
    }
    k = 16;
    cur_jit = jit;
    do {
        k--;
        (*cur_jit)[0] = (int)((RAND_FLOAT_A() * 2.0f + -1.0f) * amp);
        (*cur_jit)[1] = (int)((RAND_FLOAT_A() * 2.0f + -1.0f) * amp);
        cur_jit++;
    } while (k);
    k = 16;
    cur_ofs = ofs;
    cur_jit = jit;
    do {
        tri->vel_x = tri->vel_x * scale + (*cur_ofs)[0];
        tri->spin_x = (*cur_jit)[0];
        tri->vel_y = tri->vel_y * scale + (*cur_ofs)[1];
        tri->spin_y = (*cur_jit)[1];
        tri->vel_z = tri->vel_z * scale + (*cur_ofs)[2];
        cur_ofs++;
        cur_jit++;
        tri++;
        if (--k == 0) {
            k = 16;
            cur_ofs = ofs;
            cur_jit = jit;
        }
    } while (--n);
}

/* 0x00137c70 */
void modbreak_compile(ModbreakWork *obj, MapPart *w)
{
    modbreak_count(w, &obj->ntri, &obj->ncmd);
    obj->cmd_size = (obj->ncmd + 1) * 2;
    obj->vtx_size = obj->ntri * 72 + 44;
    obj->tri_size = obj->ntri * 40;
    obj->tris = heap_block_tag(file_heap_alloc(obj->tri_size), "modbreak.c", 1467);
    obj->vtxs = heap_block_tag(file_heap_alloc(obj->vtx_size), "modbreak.c", 1468);
    obj->cmds = heap_block_tag(game_malloc(obj->cmd_size), "modbreak.c", 1469);
    modbreak_build_buffers(w, obj->tris, obj->vtxs, obj->cmds);
}

/* 0x00137d50 */
void modbreak_work_scatter(ModbreakWork *w, int amp, float *center, float scale, float range)
{
    modbreak_scatter(w->tris, w->ntri, amp, center, scale, range);
}

/* 0x00137d68 */
void modbreak_work_free(ModbreakWork *w)
{
    heap_free((void *)w->cmds, 0);
    file_heap_free(w->vtxs);
    file_heap_free(w->tris);
}

/* 0x00137da0 */
void modbreak_work_draw(ModbreakWork *w)
{
    modbreak_draw_compiled(w->tris, w->cmds, w->vtxs);
}
