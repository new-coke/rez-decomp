/*
 * Attack ray: the flash model an enemy shows at its gun tip as it fires, played once at the
 * owner's position and scale; attack_ray_set_model picks the model, motion and textures.
 */

#include "types_common.h"

#include "types_overlays.h"
#include "types_gw.h"

#include "debug.h"
#include "gw_varedit.h"
#include "libc.h"
#include "model.h"
#include "render.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "task.h"
#include "attack_ray.h"
#include "enemy_vars.h"
#include "system.h"

extern int ulMatrixPush(void);
extern struct GwVarNode *gw_varedit_root;

#include "mathdef.h"

extern TaskDesc attack_ray_desc;

extern VarName attack_ray_group;
extern GwVarDef attack_ray_vardefs[];
extern float attack_ray_radius;
extern int attack_ray_model;
extern int attack_ray_motion;
extern void *attack_ray_textures;

/* 0x00b0b788 stg01, 0x00b16a18 stg02, 0x00b0fb70 stg03, 0x00b163e8 stg04, 0x00b2ad68 stg05, 0x00b1f0f8 stg22 */
void attack_ray_vars_register(void)
{
    VarDef hdr;

    memset(&hdr, 0, sizeof hdr);
    hdr.name = attack_ray_group.name;
    hdr.fmt = attack_ray_group.fmt;
    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(),
                                                     (struct GwVarDef *)&hdr), attack_ray_vardefs);
}

/* 0x00b0b800 stg01, 0x00b16a90 stg02, 0x00b0fbe8 stg03, 0x00b16460 stg04, 0x00b2ade0 stg05, 0x00b1f170 stg22 */
void attack_ray_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b0b808 stg01, 0x00b16a98 stg02, 0x00b0fbf0 stg03, 0x00b16468 stg04, 0x00b2ade8 stg05, 0x00b1f178 stg22 */
int attack_ray_vars_init(void)
{
    attack_ray_vars_register();
    return 0;
}

/* 0x00b0b828 stg01, 0x00b16ab8 stg02, 0x00b0fc10 stg03, 0x00b16488 stg04, 0x00b2ae08 stg05, 0x00b1f198 stg22 */
int attack_ray_vars_release(void)
{
    attack_ray_vars_unregister();
    return 0;
}

/* 0x00b0b848 stg01, 0x00b16ad8 stg02, 0x00b0fc30 stg03, 0x00b164a8 stg04, 0x00b2ae28 stg05, 0x00b1f1b8 stg22 */
void attack_ray_set_model(int model, int motion, void *tex)
{
    attack_ray_model = model;
    attack_ray_motion = motion;
    attack_ray_textures = tex;
    attack_ray_radius = 0;
}

/* 0x00b0b870 stg01, 0x00b16b00 stg02, 0x00b0fc58 stg03, 0x00b164d0 stg04, 0x00b2ae50 stg05, 0x00b1f1e0 stg22 */
void attack_ray_set_radius(float radius)
{
    attack_ray_radius = radius;
}

/* 0x00b0b880 stg01, 0x00b16b10 stg02, 0x00b0fc68 stg03, 0x00b164e0 stg04, 0x00b2ae60 stg05, 0x00b1f1f0 stg22 */
void attack_ray_exec(AttackRayWork *w)
{
    float frm = w->t;

    w->t = frm + 1.0f;
    w->body.frame = w->body.frames * frm / w->dur;
    if (w->dur < frm + 1.0f)
        FINISH_IF(w);
}

/* 0x00b0b8d8 stg01, 0x00b16b68 stg02, 0x00b0fcc0 stg03, 0x00b16538 stg04, 0x00b2aeb8 stg05, 0x00b1f248 stg22 */
void attack_ray_disp(AttackRayWork *w)
{
    int dl;

    if (view_sphere_visible(w->pos, w->body.radius)) {
        dl = RenderSetCurrentDisplayList(6);
        ulGxSetTextureList(attack_ray_textures);
        ulMatrixPush();
        do {
            Vec3 *p_ = w->pos;
            vu0_matrix_translate(ulMatrixCur, p_->x, p_->y, p_->z);
        } while (0);
        ulMatrixUnitRotation((float *)ulMatrixCur);
        vu0_matrix_scale(ulMatrixCur, w->scale->x, w->scale->y, w->scale->z);
        dc_object_draw(&w->body);
        ulMatrixPop();
        RenderSetCurrentDisplayList(dl);
    }
}

/* 0x00b0b9a0 stg01, 0x00b16c30 stg02, 0x00b0fd88 stg03, 0x00b16600 stg04, 0x00b2af80 stg05, 0x00b1f310 stg22 */
void attack_ray_init(AttackRayWork *w, AttackRayArg *prm)
{
    Vec3 *pos = prm->pos;
    Vec3 *scale = prm->scale;

    w->pos = pos;
    w->scale = scale;
    w->dur = prm->dur;
    dc_object_init(&w->body, attack_ray_model, attack_ray_motion, 0x1000);
    if (!(fabsf(attack_ray_radius) < FLT_EPSILON))
        w->body.radius = attack_ray_radius;
    w->t = 0;
}

/* 0x00b0ba20 stg01, 0x00b16cb0 stg02, 0x00b0fe08 stg03, 0x00b16680 stg04, 0x00b2b000 stg05, 0x00b1f390 stg22 */
void attack_ray_release(char *w)
{
    dc_object_release((struct MapPart *)(w + 8));
}

/* 0x00b0ba28 stg01, 0x00b16cb8 stg02, 0x00b0fe10 stg03, 0x00b16688 stg04, 0x00b2b008 stg05, 0x00b1f398 stg22 */
void attack_ray_create(void *parent, AttackRayArg *prm)
{
    TaskDesc desc = attack_ray_desc;

    task_create_arg(parent, &desc, 108, prm);
}
