/*
 * Plane cut, Area 5: draws a model part cut by a plane at a height (vertices behind it moved onto
 * it, restored after), so Virtues walls rise out of the ground.
 */

#include "stg05.h"
#include "anim.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_vu1.h"
#include "nj_system.h"
#include "node_tree.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "plane_cut.h"

extern SVtx gx_vertex_buffer[];

/* stage mesh packet vertex, 24 bytes */
typedef struct MVtxPlain {
    float x, y, z;
    char pad_0c[12];
} MVtxPlain;

/* gx_vertex_buffer vertex as the clip pass uses it, 32 bytes */
typedef struct PlaneCutScreenVertex {
    float q;                    /* depth */
    float x, y;                 /* 0x04: screen position */
    float rq;                   /* 0x0c: 1 / depth */
    char pad_10[16];
} PlaneCutScreenVertex;

extern float plane_cut_nx;
extern float plane_cut_ny;
extern float plane_cut_nz;
extern float plane_cut_d;

/* 0x00b21528 */
void plane_cut_mesh(int *pkt, Vec3 *org, Vec3 *nrm)
{
    Vec3 pt;
    Vec3 pnrm;
    int ang[4];
    MVtxPlain *v;
    float pd, side;
    float x, y, z;
    float nx, ny, nz;
    int i, step;

    ulMatrixPush();
    ulMatrixGetTranslation((float *)ulMatrixCur, &pt.x);
    model_camera_angles(ang);
    matrix_unit_set(0);
    pt.x = -pt.x;
    pt.y = -pt.y;
    pt.z = -pt.z;
    vu0_matrix_rotate_z(ulMatrixCur, -ang[2]);
    vu0_matrix_rotate_x(ulMatrixCur, -ang[0]);
    vu0_matrix_rotate_y(ulMatrixCur, -ang[1]);
    do {
        Vec3 *p_ = &pt;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ulMatrixApplyPoint((float *)ulMatrixCur, &org->x, &pt.x);
    ulMatrixApplyVector((float *)ulMatrixCur, &nrm->x, &pnrm.x);
    ulMatrixPop();
    nx = pnrm.x;
    ny = pnrm.y;
    nz = pnrm.z;
    pd = -nx * pt.x - ny * pt.y - nz * pt.z;
    while (*pkt != 255) {
        v = (MVtxPlain *)(pkt + 2);
        i = ((short *)pkt)[3];
        step = (short)(((unsigned short *)pkt)[1] + 1);
        while (i-- > 0) {
            x = v->x;
            y = v->y;
            z = v->z;
            do { side = x * nx + y * ny + z * nz + pd; } while (0);
            if (side < 0.0f) {
                x -= side * nx;
                y -= side * ny;
                z -= side * nz;
                v->z = z;
                v->x = x;
                v->y = y;
            }
            v++;
        }
        pkt += step;
    }
}

/* 0x00b21710 */
void plane_cut_mesh_saved(int *pkt, int *save, Vec3 *org, Vec3 *nrm)
{
    Vec3 pt;
    Vec3 pnrm;
    int ang[4];
    MVtxPlain *v, *sv;
    float x, y, z;
    float nx, ny, nz;
    float pd, side;
    int i, step;

    ulMatrixPush();
    ulMatrixGetTranslation((float *)ulMatrixCur, &pt.x);
    model_camera_angles(ang);
    matrix_unit_set(0);
    pt.x = -pt.x;
    pt.y = -pt.y;
    pt.z = -pt.z;
    vu0_matrix_rotate_z(ulMatrixCur, -ang[2]);
    vu0_matrix_rotate_x(ulMatrixCur, -ang[0]);
    vu0_matrix_rotate_y(ulMatrixCur, -ang[1]);
    do {
        Vec3 *p_ = &pt;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    ulMatrixApplyPoint((float *)ulMatrixCur, &org->x, &pt.x);
    ulMatrixApplyVector((float *)ulMatrixCur, &nrm->x, &pnrm.x);
    ulMatrixPop();
    nx = pnrm.x;
    ny = pnrm.y;
    nz = pnrm.z;
    pd = -nx * pt.x - ny * pt.y - nz * pt.z;
    while (*pkt != 255) {
        v = (MVtxPlain *)(pkt + 2);
        sv = (MVtxPlain *)(save + 2);
        i = ((short *)pkt)[3];
        step = (short)(((unsigned short *)pkt)[1] + 1);
        while (i-- > 0) {
            x = sv->x;
            y = sv->y;
            z = sv->z;
            do { side = x * nx + y * ny + z * nz + pd; } while (0);
            if (side < 0.0f) {
                x -= side * nx;
                y -= side * ny;
                z -= side * nz;
                v->z = z;
                v->x = x;
                v->y = y;
            } else {
                v->x = x;
                v->y = y;
                v->z = z;
            }
            v++;
            sv++;
        }
        pkt += step;
        save += step;
    }
}

/* 0x00b21920 */
void plane_cut_tree(YNode *n, YSave *save, unsigned int mask, Vec3 *org, Vec3 *nrm, float nx, float ny, float nz, float pd)
{
    Vec3 ctr;
    YMesh *mesh;
    void *data;

    do {
        data = save->data;
        ulMatrixPush();
        anim_apply_node((struct AnimNode *)n);
        if (data) {
            mesh = n->mesh;
            ulMatrixApplyPoint((float *)ulMatrixCur, &mesh->center.x, &ctr.x);
            if (ctr.x * nx + ctr.y * ny + ctr.z * nz + pd < mesh->radius) {
                if (n->flags & mask)
                    plane_cut_mesh_saved(mesh->data, save->data, org, nrm);
                else
                    plane_cut_mesh(mesh->data, org, nrm);
                n->flags |= mask;
            } else if (n->flags & mask) {
                nj_copy32(mesh->data, save->data, save->count);
                n->flags &= ~mask;
            }
        }
        if (n->child)
            plane_cut_tree(n->child, save->child, mask, org, nrm, nx, ny, nz, pd);
        ulMatrixPop();
        n = n->next;
        save = save->next;
    } while (n);
}

/* 0x00b21ad0 */
void plane_cut_draw_part(ModelPart *part, YSave *save, unsigned int mask, Vec3 *pos, int *ang, float dy)
{
    Vec3 pt;
    Vec3 n;
    Vec3 *pn;
    float unit, z;

    ulMatrixPush();
    unit = 1.0f;
    pn = &n;
    pt.x = 0.0f;
    pt.y = dy;
    pt.z = 0.0f;
    z = pt.x;
    dy = -dy;
    n.x = z;
    pn->y = unit;
    n.z = z;
    matrix_unit_set(0);
    anim_start((void *)part->clip, part->frame);
    plane_cut_tree((YNode *)part->root, save, mask, &pt, pn, z, unit, z, dy);
    ulMatrixPop();
    ulMatrixPush();
    do {
        Vec3 *p_ = pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    plane_cut_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    dc_object_draw(part);
    ulMatrixPop();
    yuki_save_restore(save, (YNode *)part->root, mask);
}

/* 0x00b21c08 */
void plane_cut_vertices(int i, int n)
{
    ScreenCfg *scr = &screen_cfg;
    PlaneCutScreenVertex *v = (PlaneCutScreenVertex *)gx_vertex_buffer;
    float rx, ry, cx, cy;
    float x, y, z, k;
    Vec3 out;
    int j;

    v += i;
    rx = 1.0f / scr->sx;
    ry = 1.0f / scr->sy;
    cx = scr->cx;
    cy = scr->cy;
    for (j = i; j < n; j++, v++) {
        z = -v->q;
        x = -(v->x - cx) * z * rx;
        y = -(v->y - cy) * z * ry;
        k = x * plane_cut_nx + y * plane_cut_ny + z * plane_cut_nz + plane_cut_d;
        if (k < 0.0f) {
            project_xyz(x - k * plane_cut_nx, y - k * plane_cut_ny, z - k * plane_cut_nz, &out.x);
            v->x = out.x;
            v->y = out.y;
            v->q = out.z;
            v->rq = 1.0f / out.z;
        }
    }
}

/* 0x00b21da8 */
void plane_cut_draw_tree(YNode *n)
{
    YDrawPrm prm;
    YDrawPrm tex_prm;
    YMesh *mesh;
    int *data;
    int tex;

    do {
        mesh = n->mesh;
        ulMatrixPush();
        anim_apply_node((struct AnimNode *)n);
        if (mesh) {
            data = mesh->data;
            tex = mesh->tex;
            if (data) {
                prm.data = data;
                prm.tex = 0;
                prm.center.x = mesh->center.x;
                prm.center.y = mesh->center.y;
                prm.center.z = mesh->center.z;
                prm.radius = mesh->radius;
                nj_cnk_vu1_draw(&prm);
                plane_cut_vertices(((short *)data)[2], ((short *)data)[2] + ((short *)data)[3]);
            }
            if (tex) {
                tex_prm.data = 0;
                tex_prm.tex = tex;
                tex_prm.center = mesh->center;
                tex_prm.radius = mesh->radius;
                nj_cnk_vu1_draw(&tex_prm);
            }
        }
        if (n->child)
            plane_cut_draw_tree(n->child);
        ulMatrixPop();
        n = n->next;
    } while (n);
}

/* 0x00b21ea8 */
void plane_cut_draw_part_flat(ModelPart *part, Vec3 *pos, int *ang, float height)
{
    Vec3 pt;
    Vec3 n;

    pt.x = 0.0f;
    pt.y = height;
    pt.z = 0.0f;
    ulMatrixApplyPoint((float *)ulMatrixCur, &pt.x, &pt.x);
    v3_set(&n, 0.0f, 1.0f, 0.0f);
    ulMatrixApplyVector((float *)ulMatrixCur, &n.x, &n.x);
    plane_cut_nx = n.x;
    plane_cut_ny = n.y;
    plane_cut_nz = n.z;
    plane_cut_d = -n.x * pt.x - n.y * pt.y - n.z * pt.z;
    ulMatrixPush();
    do {
        Vec3 *p_ = pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    plane_cut_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    anim_start((void *)part->clip, part->frame);
    plane_cut_draw_tree((YNode *)part->root);
    ulMatrixPop();
}

/* 0x00b21fc8 */
void plane_cut_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
