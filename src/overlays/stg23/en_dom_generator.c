/*
 * Trance Mission dom generator: one dom per point of a letter's glyph (the placement's character
 * or a test code), each heading to its point laid out ahead of the player to spell the letter.
 */

#include "stg23.h"
#include "bs_glyph.h"
#include "gw_varedit.h"
#include "libc.h"
#include "node_tree.h"
#include "player.h"
#include "ul_vu0.h"
#include "en_dom_generator.h"
#include "en_dom.h"
#include "../common/enemy_vars.h"

extern ParamRec en_dom_generator_params;
extern VarDef en_dom_generator_vardefs[3];

/* 3 words after name and label */
typedef struct EnDomGeneratorParams {
    char *name;
    char *label;                /* 0x04 */
    float scale;                /* 0x08: of the glyph */
    int test;                   /* 0x0c: use code below instead of the placement's scene */
    int code;                   /* 0x10 */
} EnDomGeneratorParams;

#define EN_DOM_GENERATOR_REC ((EnDomGeneratorParams *)&en_dom_generator_params)

/* 0x00afae10 */
void en_dom_generator_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_dom_generator_params.name;
    def.fmt = en_dom_generator_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_dom_generator_vardefs;
    fld = en_dom_generator_params.v;
    for (i = 0; i < 3; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_dom_generator_vardefs);
}

/* 0x00afaec0 */
void en_dom_generator_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* out: velocity taking a dom from base to its glyph point (scaled, at org). */
/* 0x00afaec8 */
void en_dom_generator_glyph_dir(float *vtx, Vec3 *base, Vec3 *org, Vec3 *out)
{
    Vec3 pt;
    Vec3 ofs;

    pt.x = org->x + vtx[0] * EN_DOM_GENERATOR_REC->scale;
    pt.y = org->y + vtx[1] * EN_DOM_GENERATOR_REC->scale;
    pt.z = org->z;
    v3_sub(&ofs, &pt, base);
    vec3_scale_to_length(vec3_length(&ofs) * 0.029999971f, &ofs.x, &out->x);
}

/* 0x00afaf78 */
int en_dom_generator_create(void *parent, EnPlacement *place)
{
    Vec3 org;
    Vec3 scr;
    float buf[8];
    void *verts;
    int count;
    int i;

    player_get_frame_pos(&org);
    v3_add(&place->dir, &org);
    player_get_frame_vel(&scr);
    v3_sub(&place->pos, &place->pos, &scr);
    v3_copy(&org, &place->dir);
    verts = buf;
    if (EN_DOM_GENERATOR_REC->test)
        glyph_lookup(EN_DOM_GENERATOR_REC->code, &verts, &count);
    else
        glyph_lookup((int)place->scene, &verts, &count);
    for (i = 0; i < count; i++) {
        en_dom_generator_glyph_dir(verts, &place->pos, &org, &place->dir);
        en_dom_create(parent, place);
        verts = (char *)verts + 24;
    }
    return 0;
}
