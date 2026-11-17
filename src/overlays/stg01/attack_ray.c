/*
 * Area 1 model setup: stage textures and model bank, the attack ray (the beam Khafra and Menkaure
 * charge before a shot), backfire textures and shatter variables; freed at stage end.
 */

#include "stg01.h"
#include "mathdef.h"
#include "model.h"
#include "pnb.h"
#include "ul_matrix.h"
#include "attack_ray.h"
#include "../common/attack_ray.h"
#include "../common/en_backfire.h"
#include "../common/enemy_shatter.h"

extern TaskDesc attack_ray_desc;
extern char stage_textures_file[];
extern int attack_ray_model;
extern int attack_ray_motion;
extern void *attack_ray_textures;
extern float attack_ray_radius;
extern VarName attack_ray_group;
extern char attack_ray_vardefs[];

/* 0x00b0b6b0 */
void models_load(void)
{
    int *ray_mdl, *ray_mot;

    stage_textures = model_load_timed(stage_textures_file, 0x13000);
    stage_models = model_pnb_load(stage_textures_file - 40);
    ray_mdl = pnb_get_entry(stage_models, 58);
    ray_mot = pnb_get_entry(stage_models, 59);
    attack_ray_set_model(*ray_mdl, *ray_mot, stage_textures);
    attack_ray_set_radius(50.0f);
    en_backfire_set_texture(2);
    en_backfire_beam_set_texture(0);
    enemy_shatter_vars_register();
}

/* 0x00b0b758 */
void models_free(void)
{
    enemy_shatter_vars_unregister();
    model_pnb_unload(stage_models);
    model_unload_timed(stage_textures);
}
