/*
 * Venus boss: sets up the player view and bullet and ball managers, then runs the fight in beat
 * steps: hex shell wall, missile pods, core, break-up and ending. Also draws the body; debug keys.
 */

#include "types_gw.h"
#include "b_venus.h"
#include "mathdef.h"
#include "color.h"
#include "debug.h"
#include "enemy_status.h"
#include "file.h"
#include "gw_varedit.h"
#include "libc.h"
#include "light.h"
#include "model.h"
#include "node_draw_state.h"
#include "pad_vib.h"
#include "player_status.h"
#include "rez_wire_compile.h"
#include "sound_beat.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "venus.h"
#include "venus_ball.h"
#include "venus_bullet.h"
#include "venus_core.h"
#include "venus_mpod.h"
#include "venus_plate.h"
#include "venus_player.h"
#include "venus_ptcl.h"
#include "venus_shell.h"
#include "venus_thunder.h"
#include "../common/boss_common.h"
#include "../common/boss_demo_mes.h"
#include "../common/boss_sound.h"
#include "../common/boss_texture_anime.h"
#include "../common/boss_wire.h"

extern unsigned int stage_exit_flags;

extern int venus_level;
extern int venus_player_dmg;
extern struct TaskDesc venus_desc;
extern int venus_wire_files[];
extern char venus_files[];
extern BossModelDef venus_model_defs[];
extern float venus_part_radii[];
extern int test_loop;
extern char venus_vardef[];
extern const float cmd_base_dmg;
extern int venus_vars_group;
extern struct VenusWork *venus_work;
extern GwVarDef venus_vardefs[];
extern int cm_wire_color[];
extern float venus_wire_fade[4];
extern int cmd_end_seq_frm_3;
extern char str_venus[];
extern int venus_end_tex_anims[][2];
extern int cmd_end_seq_frm_1;
extern int cmd_end_seq_frm_2;
extern int cmd_end_seq_frm_4;

/* 6292 bytes */
typedef struct VenusWork {
    void *pack;
    void *textures;
    int arg0;
    int level;
    int state;
    ModelPart pool[64];
    ModelPart *parts[64];
    int unk_1814[3];                    /* 0x1814: passed to venus_ball_m_create, never read here */
    Vec3 pos;
    char pad_182c[12];
    int ang[3];                         /* 0x1838 */
    int unk_1844;                       /* 0x1844: set to 0 at init and 5 by venus_start, never read */
    char pad_1848[4];
    int player;                         /* 0x184c */
    char pad_1850[12];
    int gb_man;                         /* 0x185c */
    char pad_1860[4];
    BossStarCtl *star;                  /* 0x1864 */
    char pad_1868[4];
    int anim_flags;                     /* 0x186c */
    int draw_flags;
    int *subs[2];                       /* 0x1874: the plate manager, then the current stage */
    int step;
    int wait;
    int frame_cnt;
    int core_hide;
    char pad_188c[4];
    int disp_skip;                      /* 0x1890 */
} VenusWork;

/* 0x00ca0700 */
void *module_entry(void *parent, void *arg)
{
    return venus_create(parent, arg);
}

/* 0x00ca0720 */
int boss_main_demo_cb(BossMainWork *w)
{
    if (w->state == 0)
        w->state = 1;
    else if (w->state == 2)
        return 1;
    return 0;
}

/* 0x00ca0750 */
void venus_exec_start(VenusWork *w)
{
    if (w->state != 1)
        return;
    TASK_OF(w)->exec = venus_exec;
    venus_start(w, 0);
    venus_battle_init(w);
    w->state = 2;
    boss_adx_request();
}

/* steer(cast) */
/* 0x00ca07b0 */
void venus_init(VenusWork *w, int *arg)
{
    FogArg fa;

    w->unk_1844 = 0;
    w->arg0 = arg[0];
    w->level = arg[1];
    w->state = 0;
    boss_files_request((int)venus_files, (int)(venus_files + 8));
    boss_start((int *)w, (int)w->parts, w->arg0, w->level);
    boss_bgm_start(w);
    cm_se_laser_create(venus_files + 16);
    venus_1st_demo_create(w, (int)boss_main_demo_cb, (int)w);
    fa.range = 500.0f;
    fa.k = 1.0f;
    fa.count = 500;
    fa.color = 0;
    w->star = boss_star_create(w, &fa, 20.0f);
    venus_level = w->arg0;
    score_vars_add();
    venus_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, 0, venus_vardef);
    gw_varedit_add_table(&gw_varedit_root, venus_vars_group, venus_vardefs);
    venus_work = w;
    venus_pl_vars_add();
    venus_ptcl_vars_add();
    venus_fog_vars_add();
    venus_laser_vars_add();
    venus_shell_vars_add();
    venus_thunder_vars_add();
    venus_bullet_vars_add();
    venus_plate_vars_add();
    venus_mpod_vars_add();
    venus_core_vars_add();
    duos_vars_add();
}

/* 0x00ca0920 */
void venus_start(VenusWork *w, int mode)
{
    w->player = venus_player_create(w, mode);
    w->gb_man = venus_gb_man_create(w, &w->parts[0], &w->pos.x);
    venus_ball_m_create(w, &w->unk_1814[0], &w->parts[0]);
    w->unk_1844 = 5;
}

/* 0x00ca0980 */
void venus_exec_load(VenusWork *w)
{
    if (boss_files_ready(venus_files, venus_files + 8) && file_request_status(venus_files + 16)) {
        do {
            w->pack = model_pnb_load(venus_files);
            w->textures = model_load_timed(venus_files + 8, w->arg0);
        } while (0);
        boss_parts_build((void **)w, venus_model_defs, w->parts, w->pool);
        parts_set_radii(w->parts, venus_part_radii);
        TASK_OF(w)->exec = (void (*)(void *))venus_exec_start;
    }
}

/* 0x00ca0a38 */
void venus_release(VenusWork *w)
{
    light_reset();
    light_fog_set();
    if (boss_parts_release(w->parts)) {
        model_unload_timed(w->textures);
        model_pnb_unload(w->pack);
        do {
            w->textures = 0;
            w->pack = 0;
        } while (0);
    }
    boss_end(w);
    score_vars_remove();
    gw_varedit_remove(&gw_varedit_root, 0, (struct GwVarDef *)venus_vardef);
    venus_pl_vars_remove();
    venus_ptcl_vars_remove();
    venus_fog_vars_remove();
    venus_laser_vars_remove();
    venus_shell_vars_remove();
    venus_thunder_vars_remove();
    venus_bullet_vars_remove();
    venus_plate_vars_remove();
    venus_mpod_vars_remove();
    venus_core_vars_remove();
    duos_vars_remove();
}

/* 0x00ca0b08 */
void venus_pre_exec(Earth1stDemoWork *w)
{
}

/* 0x00ca0b10 */
void venus_get_pos_ang(VenusWork *w, void **pos_out, void **ang_out)
{
    *ang_out = &w->ang[0];
    *pos_out = &w->pos.x;
}

/* 0x00ca0b28 */
void venus_seq_update(VenusWork *w)
{
    int **subs = w->subs;
    int i;
    int n;

    for (i = 0; i < 2; i++) {
        if (subs[i] && (TASK_OF(subs[i])->flags & TASK_F_END))
            subs[i] = 0;
    }
    if (test_loop == 1) {
        (w->frame_cnt)++;
        return;
    }
    n = w->wait;
    if (n) {
        if (n == -1) {
            if (beat_grid_hit(0) && !(beat_clock_get_haku() & 3)) {
                w->wait = 0;
                venus_seq_step((int)w);
            }
        } else {
            w->wait = n - 1;
            if (n - 1 <= 0)
                venus_seq_step((int)w);
        }
    }
    (w->frame_cnt)++;
}

/* 0x00ca0bf8 */
void venus_exec(VenusWork *w)
{
    int n;

    venus_pre_exec((Earth1stDemoWork *)w);
    venus_seq_update(w);
    for (n = enemy_status_get_ticks(); n; n--) {
        if (w->anim_flags & 8)
            dc_object_advance_time(w->parts[3]);
        if (w->anim_flags & 0x40)
            dc_object_advance_time(w->parts[6]);
        if (w->anim_flags & 0x100000)
            dc_object_advance_time(w->parts[20]);
    }
    if (w->core_hide)
        (w->core_hide)--;
    if (debug_flags_test(4))
        venus_debug_keys(w);
}

/* steer(cast) */
/* 0x00ca0cd8 */
void venus_disp(VenusWork *w)
{
    int old_blend;
    float old_lw;
    ModelPart *part;

    ulGxSetTextureList(w->textures);
    light_update();
    boss_texlist_bind((int *)w);
    if (w->disp_skip) {
        (w->disp_skip)--;
        return;
    } else {
        venus_fog_set(w);
        old_blend = draw_set_override_flags(0);
        ulMatrixPush();
        MTX_TRANS(&w->pos);
        venus_rotate_yxz(w->ang);
        if (w->draw_flags & 0x20)
            dc_object_draw(w->parts[5]);
        if (w->draw_flags & 0x8)
            dc_object_draw(w->parts[3]);
        if (w->draw_flags & 0x10)
            dc_object_draw(w->parts[4]);
        if (w->draw_flags & 0x100)
            dc_object_draw(w->parts[8]);
        if (w->draw_flags & 0x40)
            dc_object_draw(w->parts[6]);
        if (w->draw_flags & 0x80)
            dc_object_draw(w->parts[7]);
        if (w->draw_flags & 0x100000) {
            model_object_draw(w->parts[20], w->parts[20]->frame);
            part_draw_beat_frame(w->parts[21], w->parts[20]->frame, cm_wire_color[3], cm_wire_color[4], 2.0f);
        }
        ulMatrixPop();
        if (w->draw_flags & 0xc0) {
            draw_set_blend(4, 1);
            draw_set_color(cm_wire_color[0]);
            old_lw = line_width_set(1.0f);
            dc_object_draw(w->parts[40]);
            line_width_set(old_lw);
        }
        if (w->parts[3]->frame < w->parts[3]->frames) {
            if (w->core_hide == 0)
                dc_object_draw(w->parts[41]);
            draw_push_strip_flags();
            draw_set_override_flags(DRAW_STRIP_OVERRIDE);
            ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
            part = w->parts[26];
            **(short **)(*(char **)(*(char **)(*(char **)((char *)part + 4) + 44) + 4) + 4) = 8467;
            do {
                dc_object_draw(part);
            } while (0);
            draw_pop_strip_flags();
            draw_set_blend(4, 1);
            old_lw = line_width_set(2.0f);
            dc_object_draw(w->parts[27]);
            line_width_set(old_lw);
            dc_object_draw(w->parts[25]);
        }
        draw_set_override_flags(old_blend);
    }
    if (debug_flags_test(4))
        venus_debug_disp(w);
}

/* steer(cast) */
/* 0x00ca0f68 */
void venus_disp_final(VenusWork *w)
{
    unsigned int cols[2];
    unsigned int col;
    float t;
    float old_lw;
    int old_blend;

    ulGxSetTextureList(w->textures);
    light_update();
    boss_texlist_bind((int *)w);
    if (w->step == 10) {
        t = (float)w->wait / (float)cmd_end_seq_frm_3;
        cols[0] = (cm_wire_color[1] & 0xffffff) | (((int)(color_get_a(cm_wire_color[1]) * (1.0f - t)) + 1) >> 1 << 24);
        cols[1] = (cm_wire_color[2] & 0xffffff) | (((int)(color_get_a(cm_wire_color[2]) * (1.0f - t)) + 1) >> 1 << 24);
        col = color_lerp(cols[0], cols[1], beat_curve(1));
    } else if (w->step < 10) {
        col = 0;
        t = 1.0f;
    } else {
        t = 0.0f;
        col = color_lerp(cm_wire_color[1], cm_wire_color[2], beat_curve(1));
    }
    venus_fog_set_far(w, 0.5f);
    old_blend = draw_set_override_flags(DRAW_STRIP_OVERRIDE | DRAW_COLOR_ADD);
    draw_push_override_color();
    draw_push_strip_flags();
    ((void (*)(unsigned int, unsigned int))draw_set_strip_flags)(0xffffffff, 2048);
    ulMatrixPush();
    MTX_TRANS(&w->pos);
    venus_rotate_yxz(w->ang);
    if (w->draw_flags & 0x20) {
        if (!(fabsf(t) < FLT_EPSILON)) {
            venus_wire_fade[0] = t + -1.0f;
            draw_set_override_color(venus_wire_fade);
            tree_draw_bake_w((WireObj *)w->parts[5]->root);
        }
        draw_set_override_flags(0);
        draw_set_color(col);
        draw_set_blend(4, 1);
        old_lw = line_width_set(2.0f);
        dc_object_draw(w->parts[28]);
        line_width_set(old_lw);
    }
    draw_set_override_flags(0);
    if (w->draw_flags & 0x80)
        dc_object_draw(w->parts[7]);
    ulMatrixPop();
    draw_pop_strip_flags();
    draw_pop_override_color();
    draw_set_override_flags(old_blend);
}

/* 0x00ca1210 */
void *venus_create(void *parent, void *arg)
{
    return task_create_inherit_arg(parent, &venus_desc, 6292, arg);
}

/* 0x00ca1238 */
void venus_task_finish_if(void *w)
{
    FINISH_IF(w);
}

/* 0x00ca1258 */
int venus_level_add(int i)
{
    return venus_level + i;
}

/* 0x00ca1268 */
void venus_player_damage(void)
{
    player_status_add_damage(venus_player_dmg);
}

/* 0x00ca1278 */
void venus_battle_init(VenusWork *w)
{
    ModelPart *part_a = w->parts[3];
    ModelPart *part_b = w->parts[6];

    part_a->speed = 0.8f;
    part_a->frame = 0.0f;
    part_b->frames = 550.0f;
    part_b->frame = 0.0f;
    w->parts[20]->frame = 0.0f;
    w->draw_flags = 0xffefffaf;
    w->parts[14]->frame = 0.0f;
    w->anim_flags = 0;
    {
        int *task = w->subs[0];

        FINISH_IF(task);
    }
    w->subs[0] = (int *)plate_man_create(w, (int *)w->parts);
    if (w->subs[1]) {
        FINISH(w->subs[1]);
        *(int *)&w->subs[1] = 0; /* steer(type) */
    }
    w->core_hide = 3;
    TASK_OF(w)->disp = venus_disp;
    w->step = 0;
    w->wait = 0;
    venus_player_dmg = cmd_base_dmg;
    w->disp_skip = 2;
}

/* 0x00ca1360 */
void venus_seq_wait_beat(VenusWork *w)
{
    w->wait = -1;
}

/* 0x00ca1370 */
void venus_seq_regist(void *w, int idx, int id)
{
    venus_cue_set(idx, id, venus_seq_step, (int)boss_get_task());
}

/* steer(cast) */
/* 0x00ca13c0 */
void venus_seq_step(int work)
{
    VenusWork *w = (VenusWork *)work;
    int *anim;
    ModelPart *part;
    int step;

    step = w->step;
    w->step = step + 1;
    switch (step) {
    case 0:
        boss_demo_mes_create(w, str_venus);
        w->anim_flags |= 8;
        w->subs[1] = (int *)venus_hwall_create(w, &w->pos, (int *)w->parts);
        break;
    case 1:
        w->draw_flags = (w->draw_flags | 0x10) & ~8;
        if (w->subs[0])
            FINISH(w->subs[0]);
        break;
    case 2:
        venus_hwall_shatter((int)w, (char *)w->subs[1]);
        FINISH_IF(w->subs[1]);
        w->subs[1] = (int *)venus_mpod_create(w, w->gb_man, &w->pos, (int *)w->parts);
        w->anim_flags |= 0x100040;
        w->draw_flags = (w->draw_flags | 0x100040) & ~0x80;
        w->parts[6]->frames = 550.0f;
        w->wait = venus_player_ent_next((int *)w->player);
        break;
    case 3:
        venus_player_ent_next((int *)w->player);
        break;
    case 4:
        {
            ModelPart *part = w->parts[3];

            part->frame = part->frames;
        }
        if (w->subs[0])
            FINISH(w->subs[0]);
        venus_gb_man_break_all((void *)w->gb_man);
        if (w->subs[1])
            venus_mpod_break_all((struct VenusMpodWork *)w->subs[1]);
        FINISH_IF(w->subs[1]);
        w->subs[1] = venus_core_create(w, (int)w->parts, &w->pos);
        w->anim_flags |= 0x40;
        w->parts[6]->frames = 964.0f;
        w->wait = venus_player_ent_next((int *)w->player);
        w->anim_flags &= ~0x100000;
        w->draw_flags &= ~0x100000;
        part = w->parts[20];
        part->frame = part->frames;
        break;
    case 5:
        if (w->subs[1])
            venus_core_start((char *)w->subs[1], 1);
        w->draw_flags &= ~0x40;
        w->draw_flags &= ~0x100;
        break;
    case 6:
        w->draw_flags |= 0x140;
        venus_player_dmg = 0;
        boss_pcm_load(0);
        boss_vol_play(5, 0);
        w->wait = -1;
        if (boss_get_scoring())
            boss_demo_mes2_start(w, str_venus, 0);
        else
            boss_demo_mes2_start(w, str_venus, 2);
        anim = venus_end_tex_anims[(unsigned int)player_status_get_level() % 6];
        if (anim[1])
            tex_anime_start(w, anim, boss_get_arg0(), 120);
        else
            tex_anime_start(0, 0, 0, 0);
        break;
    case 7:
        boss_pcm_start();
        if (w->subs[1])
            venus_core_break((char *)w->subs[1]);
        venus_player_ent_next((int *)w->player);
        TASK_OF(w)->disp = venus_disp_final;
        {
            int k = cmd_end_seq_frm_1;
            w->frame_cnt = 0;
            w->draw_flags |= 0x80;
            w->anim_flags |= 0x20000000;
            w->wait = k;
        }
        motion_t_create(w, w->parts[29], &w->pos, w->ang);
        break;
    case 8:
        do {
            if (w->subs[1]) {
                FINISH(w->subs[1]);
                *(int *)&w->subs[1] = 0; /* steer(type) */
            }
        } while (0);
        venus_dust_create(w, (int)w->parts);
        venus_end_core_create(w, (int)w->parts, &w->pos, cm_wire_color[0], cmd_end_seq_frm_2, cmd_end_seq_frm_3);
        w->wait = cmd_end_seq_frm_2;
        break;
    case 9: {
        Vec3 v;
        v.x = 0.0f;
        v.y = 0.0f;
        v.z = 0.0f;
        lock_score_level(0, &v);
        ((struct { int f; } *)&w->draw_flags)->f &= ~0x80; /* steer(alias) */
        cm_vcc_man2_create(w, (int *)w->parts[5], cmd_end_seq_frm_3 + 1);
        w->wait = cmd_end_seq_frm_3;
        venus_fog_create(w, 1, 1.0f);
        tex_anime_fade_in_out();
        break;
    }
    case 10:
        peri_act_play_boss_finish();
        if (boss_get_scoring())
            boss_demo_mes2_start(w, str_venus, 1);
        else
            boss_demo_mes2_start(w, str_venus, 3);
        w->wait = cmd_end_seq_frm_4;
        break;
    case 11:
        ((void (*)(void *))boss_fx_release)(w);
        stage_exit_flags |= 0xa;
        TASK_OF(w)->exec = 0;
        TASK_OF(w)->disp = 0;
        break;
    }
}

/* 0x00ca1898 */
void venus_player_stop_cb(VenusWork *w, int k)
{
    venus_player_view_rand((void *)w->player, 0);
}

/* 0x00ca18a8 */
void venus_debug_keys(VenusWork *w)
{
    if (kbd_state[7] & 0x44) {
        switch (kbd_state[5]) {
        case 16:
            wire_export_list((void **)w, venus_wire_files, 3);
            break;
        case 8:
            sound_debug_toggle();
            break;
        case 17:
            venus_seq_step((int)w);
            break;
        }
    }
}

/* 0x00ca1918 */
void venus_debug_disp(void *w)
{
    duos_plot_debug();
}

/* 0x00ca1920 */
void venus_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
