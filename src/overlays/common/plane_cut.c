/*
 * Ground cut (Ramses): before drawing, vertices below the ground plane are pressed onto it and
 * made transparent, then restored. A second path cuts the vertices as they are transformed.
 */

#include "types_common.h"

#include "types_overlays.h"

#include "debug.h"
#include "anim.h"
#include "math_util.h"
#include "model.h"
#include "nj_cnk_vu1.h"
#include "nj_system.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "yuki_tool.h"
#include "plane_cut.h"
#include "system.h"

extern int ulMatrixPush(void);

extern SVtx vtx_buf[];
extern float plane_cut_nx, plane_cut_ny, plane_cut_nz, plane_cut_d;

#if defined(OVERLAY_STG22)
extern SVtx gx_vertex_buffer[];
#endif

/* 0x00b056f8 stg01, 0x00b0cb58 stg22 */
void plane_cut_mesh(int *pkt, Vec3 *org, Vec3 *nrm)
{
    Vec3 pt;
    Vec3 pnrm;
    int ang[4];
    MVtx *v;
    float pd, side, nx, ny, nz;
    float x, y, z;
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
        v = (MVtx *)(pkt + 2);
        i = ((short *)pkt)[3];
        step = (short)(((unsigned short *)pkt)[1] + 1);
        while (i-- > 0) {
            x = v->x;
            y = v->y;
            z = v->z;
            do { side = x * nx + y * ny + z * nz + pd; } while (0);
            if (side < 0.0f) {
                v->col &= 0xffffff;
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

/* 0x00b058f8 stg01, 0x00b0cd58 stg22 */
void plane_cut_mesh_saved(int *pkt, int *save, Vec3 *org, Vec3 *nrm)
{
    Vec3 pt;
    Vec3 pnrm;
    int ang[4];
    MVtx *v, *sv;
    float x, y, z;
    float pd, side, nx, ny, nz;
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
        v = (MVtx *)(pkt + 2);
        sv = (MVtx *)(save + 2);
        i = ((short *)pkt)[3];
        step = (short)(((unsigned short *)pkt)[1] + 1);
        while (i-- > 0) {
            x = sv->x;
            y = sv->y;
            z = sv->z;
            do { side = x * nx + y * ny + z * nz + pd; } while (0);
            if (side < 0.0f) {
                v->col &= 0xffffff;
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
                v->col = sv->col;
            }
            v++;
            sv++;
        }
        pkt += step;
        save += step;
    }
}

/* 0x00b05b20 stg01, 0x00b0cf80 stg22 */
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

/* 0x00b05cd0 stg01, 0x00b0d130 stg22 */
void plane_cut_draw_part(ModelPart *part, YSave *save, unsigned int mask, Vec3 *pos, int *ang, Vec3 *scale, int rz, Vec3 *obj_pos, int *obj_ang, int up, float ground)
{
    Vec3 ofs;
    Vec3 org;
    Vec3 nrm;
    float nx, ny, nz, pd;

    ulMatrixPush();
    ofs.x = -pos->x;
    ofs.y = -pos->y + ground;
    ofs.z = -pos->z;
    matrix_unit_set(0);
    do {
        Vec3 *p_ = &ofs;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    vu0_matrix_rotate_z(ulMatrixCur, -ang[2]);
    vu0_matrix_rotate_x(ulMatrixCur, -ang[0]);
    vu0_matrix_rotate_y(ulMatrixCur, -ang[1]);
    org.x = 0.0f;
    org.y = 0.0f;
    org.z = 0.0f;
    ulMatrixApplyPoint((float *)ulMatrixCur, &org.x, &org.x);
    {
        float y = -1.0f;

        if (up)
            y = 1.0f;
        nrm.x = 0.0f;
        nrm.y = y;
        nrm.z = 0.0f;
    }
    ulMatrixApplyVector((float *)ulMatrixCur, &nrm.x, &nrm.x);
    nx = nrm.x;
    ny = nrm.y;
    nz = nrm.z;
    pd = -nx * org.x - ny * org.y - nz * org.z;
    matrix_unit_set(0);
    anim_start((void *)part->clip, part->frame);
    vu0_matrix_scale(ulMatrixCur, scale->x, scale->y, scale->z);
    plane_cut_tree((YNode *)part->root, save, mask, &org, &nrm, nx, ny, nz, pd);
    ulMatrixPop();
    ulMatrixPush();
    do {
        Vec3 *p_ = obj_pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    plane_cut_rotate_yxz(obj_ang);
    do {
        Vec3 *p_ = pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    plane_cut_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, rz);
    vu0_matrix_scale(ulMatrixCur, scale->x, scale->y, scale->z);
    dc_object_draw(part);
    ulMatrixPop();
    yuki_save_restore(save, (YNode *)part->root, mask);
}

/* 0x00b05f58 stg01, 0x00b0d3b8 stg22 */
#if defined(OVERLAY_STG22)
void plane_cut_vertices(int i, int n)
{
    SVtx *v = gx_vertex_buffer;
    float x, y, z, side;
    int j;

    v += i;
    for (j = i; j < n; j++, v++) {
        float *st = &v->scr[1];   /* st[3] is q */

        x = v->x;
        y = v->y;
        z = v->z;
        side = x * plane_cut_nx + y * plane_cut_ny + z * plane_cut_nz + plane_cut_d;
        v->col[3] = v->col[0] = v->col[1] = v->col[2] = 0.0f;
        if (side < 0.0f) {
            x -= side * plane_cut_nx;
            y -= side * plane_cut_ny;
            z -= side * plane_cut_nz;
            v->x = x;
            v->y = y;
            v->z = z;
            st[3] = -z;
            v->col[3] = 0.0f;
            v->rq = -1.0f / z;
        }
    }
}
#else
void plane_cut_vertices(int i, int n)
{
    SVtx *v = vtx_buf;
    float x, y, z, side;
    int j;

    v += i;
    for (j = i; j < n; j++, v++) {
        float *st = &v->scr[1];

        x = v->x;
        y = v->y;
        z = v->z;
        side = x * plane_cut_nx + y * plane_cut_ny + z * plane_cut_nz + plane_cut_d;
        v->col[3] = v->col[0] = v->col[1] = v->col[2] = 0.0f;
        if (side < 0.0f) {
            x -= side * plane_cut_nx;
            y -= side * plane_cut_ny;
            z -= side * plane_cut_nz;
            v->x = x;
            v->y = y;
            v->z = z;
            st[3] = -z;
            v->col[3] = 0.0f;
            v->rq = -1.0f / z;
        }
    }
}
#endif

/* 0x00b06040 stg01, 0x00b0d4a0 stg22 */
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
                draw_set_blend(4, 5);
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

/* 0x00b06150 stg01, 0x00b0d5b0 stg22 */
void plane_cut_draw_part_flat(ModelPart *part, Vec3 *pos, int *ang, Vec3 *scale, int rz, int up, float ground)
{
    Vec3 org;
    Vec3 nrm;

    org.y = ground;
    org.x = 0.0f;
    org.z = 0.0f;
    ulMatrixApplyPoint((float *)ulMatrixCur, &org.x, &org.x);
    {
        float y = -1.0f;

        if (up)
            y = 1.0f;
        nrm.x = 0.0f;
        nrm.y = y;
        nrm.z = 0.0f;
    }
    ulMatrixApplyVector((float *)ulMatrixCur, &nrm.x, &nrm.x);
    plane_cut_nx = nrm.x;
    plane_cut_ny = nrm.y;
    plane_cut_nz = nrm.z;
    plane_cut_d = -nrm.x * org.x - nrm.y * org.y - nrm.z * org.z;
    do {
        Vec3 *p_ = pos;
        vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
    } while (0);
    plane_cut_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, rz);
    do {
        Vec3 *s_ = scale;
        vu0_matrix_scale(ulMatrixCur, s_->x, s_->y, s_->z);
    } while (0);
    anim_start((void *)part->clip, part->frame);
    plane_cut_draw_tree((YNode *)part->root);
}

/* 0x00b062a8 stg01, 0x00b0d708 stg22 */
void plane_cut_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
