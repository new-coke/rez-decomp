/*
 * Ending: staff roll. A command file per ending type shows credit lines and images; on most
 * endings a butterfly flies over water behind it. The closing message follows.
 */

#include "ending_shared.h"
#include "mathdef.h"
#include "anim.h"
#include "approach.h"
#include "billboard.h"
#include "color.h"
#include "debug.h"
#include "fade.h"
#include "file.h"
#include "fog.h"
#include "game.h"
#include "gw_varedit.h"
#include "libc.h"
#include "math_util.h"
#include "model.h"
#include "node_draw_state.h"
#include "node_tree.h"
#include "pnb.h"
#include "rez_wire_compile.h"
#include "system.h"
#include "task.h"
#include "ul_gx.h"
#include "ul_math.h"
#include "ul_matrix.h"
#include "ul_vu0.h"
#include "end_staff_roll.h"
#include "end_circle.h"
#include "end_message.h"
#include "ending.h"

#define ROT_Y(ang, sc, x, z) \
    do { \
        float rot_u = x, rot_v = z; \
        ulSinCos(ang, &sc[0], &sc[1]); \
        x = rot_u * sc[1] + rot_v * sc[0]; \
        z = -rot_u * sc[0] + rot_v * sc[1]; \
    } while (0)

extern void font_set(int id);
extern unsigned char kbd_state[];

/* staff-roll file entry, 20 bytes */
typedef struct RollEntry {
    char pad_00[4];
    int type;                   /* 0x04: 1 = offset, 9 = colour */
    char pad_08[8];
    unsigned int val;           /* 0x10 */
} RollEntry;

typedef struct RollFile {
    unsigned int count;
    RollEntry e[1];
} RollFile;

extern RollFile *end_staff_roll_file;
extern int end_staff_roll_dummy[1];
extern int end_staff_roll_image_req;
extern void *end_staff_roll_image;
extern int end_staff_roll_vars_group;
extern VarDef end_staff_roll_vardefs[1];
extern VarDef end_staff_roll_vars_group_def;

/* staff-roll file command, 20 bytes */
typedef struct RollCmd {
    float time;                 /* seconds */
    int type;                   /* 0x04 */
    float x, y;                 /* 0x08 */
    unsigned int val;           /* 0x10 */
} RollCmd;

/* from a command's val, below 4 */
typedef enum RollMode { ROLL_MODE_0 } RollMode;

typedef struct Roll {
    char pad_00[8];
    int frame;                  /* 0x08 */
    int index;                  /* 0x0c: the next command */
    int done;                   /* 0x10: commands finished */
    RollMode mode;              /* 0x14 */
    unsigned int color;         /* 0x18 */
    int hold;                   /* 0x1c: frames */
    int fade;                   /* 0x20: frames */
    float x, y;                 /* 0x24 */
    int align;                  /* 0x2c: 1 = centred, 2 = right */
    float width, h;                 /* 0x30: image size */
    float uv[4];                /* 0x38 */
    int jp;                     /* 0x48 */
} Roll;

/* 52 bytes */
typedef struct RollStr {
    char pad_00[8];
    int state;                  /* 0x08: 0 fading in, then frames shown */
    int count;                  /* 0x0c */
    RollCmd *cmd;               /* 0x10 */
    RollMode mode;              /* 0x14 */
    unsigned int color;         /* 0x18 */
    int hold;                   /* 0x1c */
    int fade;                   /* 0x20 */
    float x, y;                 /* 0x24 */
    int align;                  /* 0x2c */
    int jp;                     /* 0x30 */
} RollStr;

/* 64 bytes */
typedef struct RollTex {
    char pad_00[8];
    int state;                  /* 0x08 */
    int count;                  /* 0x0c */
    RollCmd *cmd;               /* 0x10 */
    int hold;                   /* 0x14 */
    int fade;                   /* 0x18 */
    float x, y;                 /* 0x1c */
    int align;                  /* 0x24 */
    float width, h;                 /* 0x28 */
    float uv[4];                /* 0x30 */
} RollTex;

extern struct TaskDesc str_desc;
extern struct TaskDesc tex_desc;
extern int cut_done;

/* a bubble of the closing scene, 100 bytes */
typedef struct RollButterflyChild {
    Vec3 base;                  /* drifts by k * the path end each loop */
    Vec3 pos;                   /* 0x0c */
    int ang[3];                 /* 0x18 */
    Vec3 mpos;                  /* 0x24: mirrored below the water */
    int mang[3];                /* 0x30 */
    float phase;                /* 0x3c */
    int state;                  /* 0x40: 1 drawn, 2 left the view */
    Vec3 start;                 /* 0x44 */
    Vec3 k;                     /* 0x50 */
    float rate;                 /* 0x5c */
    int alt;                    /* 0x60: the second motion */
} RollButterflyChild;

/* closing scene, 10552 bytes */
typedef struct RollButterfly {
    void *models;
    void *textures;             /* 0x04 */
    ModelPart body;                /* 0x08 */
    Vec3 pos;                   /* 0x64 */
    int ang[3];                 /* 0x70 */
    ModelPart water;               /* 0x7c */
    Vec3 mpos;                  /* 0xd8: pos mirrored */
    int mang[3];                /* 0xe4 */
    float level;                /* 0xf0: the body model's radius */
    ModelPart path;                /* 0xf4 */
    Vec3 tgt;                   /* 0x150: the path's end */
    ModelPart child_part;                 /* 0x15c */
    ModelPart mchild_part;                /* 0x1b8 */
    int count;                  /* 0x214 */
    RollButterflyChild children[100];  /* 0x218 */
    int timer;                  /* 0x2928 */
    int tick;                   /* 0x292c */
    float speed;                /* 0x2930 */
    int ready;                  /* 0x2934 */
} RollButterfly;

/* debug variables */
typedef struct RollButterflyPrm {
    unsigned int color;
    int count;                  /* 0x04 */
    float child_pos_x_min, child_pos_x_max, child_pos_y_min, child_pos_y_max, child_pos_z_min, child_pos_z_max;   /* 0x08: start ranges */
    float child_spd_scl_min, child_spd_scl_max;               /* 0x20: rate range */
    float child_path_scl_x_min, child_path_scl_x_max, child_path_scl_y_min, child_path_scl_y_max, child_path_scl_z_min, child_path_scl_z_max;     /* 0x28 */
    float chance;               /* 0x40: of the second motion */
} RollButterflyPrm;

/* butterfly flying across, 160 bytes */
typedef struct Cut {
    void *models;
    void *textures;             /* 0x04 */
    ModelPart body;                /* 0x08 */
    Vec3 pos;                   /* 0x64 */
    int ang[3];                 /* 0x70 */
    int timer;                  /* 0x7c */
    unsigned int id;            /* 0x80: the path, 0..3 */
    int *done;                  /* 0x84: counted up at the end */
    Vec3 from;                  /* 0x88 */
    Vec3 to;                    /* 0x94 */
} Cut;

extern struct TaskDesc end_staff_roll_desc;
extern struct TaskDesc end_staff_roll_butterfly_desc;
extern struct TaskDesc cut_desc;
extern VarDef end_staff_roll_butterfly_vars_group_def;
extern RollButterflyPrm end_staff_roll_butterfly_prm;
extern int end_staff_roll_butterfly_vars_group;
extern VarDef end_staff_roll_butterfly_vardefs[17];
extern Vec3 end_staff_roll_butterfly_pos_water;
extern Vec3 end_staff_roll_butterfly_pos_leave;
extern Vec3 end_staff_roll_butterfly_pos_enter;

/* steer(cast) */
/* 0x00af8788 */
void end_staff_roll_vars_register(void)
{
    unsigned int i;
    VarDef *vd;
    int *word;

    end_staff_roll_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, game_varedit_get(), (char *)&end_staff_roll_vars_group_def);
    vd = end_staff_roll_vardefs;
    word = end_staff_roll_dummy;
    for (i = 0; i < 1; i++) {
        vd->ptr = word;
        word++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, end_staff_roll_vars_group, (struct GwVarDef *)end_staff_roll_vardefs);
}

/* 0x00af8808 */
void end_staff_roll_vars_unregister(void)
{
    gw_varedit_remove(&gw_varedit_root, game_varedit_get(), (struct GwVarDef *)&end_staff_roll_vars_group_def);
}

/* Text offsets become pointers; colours halved to GS 0..128. Then requests the image file. */
/* 0x00af8840 */
void end_staff_roll_load(void)
{
    RollEntry *ent;
    unsigned int j, rgb;
    int v, file_no;

    end_staff_roll_vars_register();
    switch (ending_type_get()) {
    case 0:
    case 1:
    case 2:
        end_staff_roll_image_req = 5;
        file_no = 2;
        break;
    default:
        end_staff_roll_image_req = 4;
        file_no = 1;
        break;
    }
    end_staff_roll_file = (RollFile *)file_load_sync(ending_files[file_no], 0, 0);
    ent = end_staff_roll_file->e;
    for (j = 0; j < end_staff_roll_file->count; j++, ent++) {
        switch (ent->type) {
        case 1:
            ent->val += (int)&end_staff_roll_file->e[end_staff_roll_file->count];
            break;
        case 9:
            rgb = ent->val;
            ent->val = ((rgb >> 24) + 1) >> 1 << 24 | ((rgb & 0xff) + 1) >> 1 << 16 | ((rgb >> 8 & 0xff) + 1) >> 1 << 8 | ((rgb >> 16 & 0xff) + 1) >> 1;
            break;
        }
    }
    end_staff_roll_butterfly_vars_init();
    end_staff_roll_image = 0;
    file_request_simple(ending_files[end_staff_roll_image_req], 0);
}

/* 0x00af89d8 */
void end_staff_roll_unload(void)
{
    end_staff_roll_vars_unregister();
    end_staff_roll_butterfly_vars_release();
    file_heap_free(end_staff_roll_file);
    if (end_staff_roll_image)
        model_unload_timed(end_staff_roll_image);
    else
        file_request_cancel(ending_files[end_staff_roll_image_req]);
}

/* 0x00af8a38 */
void str_init(RollStr *w, RollCmd *rcmd)
{
    Task *ptask = TASK_OF(w)->parent;
    Roll *rw = ptask ? (Roll *)(ptask + 1) : 0;

    task_set_dlist(w, 4);
    w->color = rw->color;
    w->x = rw->x;
    w->jp = rw->jp;
    w->mode = rw->mode;
    w->y = rw->y;
    w->hold = rw->hold > 0 ? rw->hold : 1;
    w->fade = rw->fade > 0 ? rw->fade : 1;
    w->align = rw->align;
    w->count = 0;
    w->state = 0;
    w->cmd = rcmd;
}

/* 0x00af8ae8 */
void str_release(void)
{
}

/* 0x00af8af0 */
void str_exec(RollStr *w)
{
    Task *ptask;

    if (w->state == 0) {
        if (++w->count == w->fade)
            w->state = 1;
    } else if (w->state == w->hold) {
        if (--w->count == 0) {
            w->state = 0;
            FINISH_IF(w);
            ptask = TASK_OF(w)->parent;
            ((Roll *)(ptask ? ptask + 1 : 0))->done++;
        }
    } else {
        w->state++;
    }
}

/* 0x00af8b70 */
void str_disp(RollStr *w)
{
    RollCmd *rcmd = w->cmd;
    char *str = (char *)rcmd->val;
    int len;
    unsigned int col;
    float x, y;

    len = line_length(str);
    col = (w->color & 0xffffff) | (color_get_a(w->color) * w->count / w->fade + 1) >> 1 << 24;
    switch (w->align) {
    default:
        x = w->x + rcmd->x;
        y = w->y + rcmd->y;
        break;
    case 1:
        x = w->x - floorf(len * 12 * 0.5f) + rcmd->x;
        y = w->y + rcmd->y;
        break;
    case 2:
        x = w->x - floorf(len * 12) + rcmd->x;
        y = w->y + rcmd->y;
        break;
    }
    draw_set_blend(4, 5);
    font_set(2);
    font_print(x, y, 2.2222223f, col, str);
}

/* 0x00af8cd0 */
void *str_create(void *parent, RollCmd *rcmd)
{
    return task_create_inherit_arg(parent, &str_desc, 52, rcmd);
}

/* 0x00af8cf8 */
void tex_init(RollTex *w, RollCmd *rcmd)
{
    Task *ptask = TASK_OF(w)->parent;
    Roll *rw = ptask ? (Roll *)(ptask + 1) : 0;

    task_set_dlist(w, 4);
    w->x = rw->x;
    w->cmd = rcmd;
    w->y = rw->y;
    w->hold = rw->hold > 0 ? rw->hold : 1;
    w->width = rw->width;
    w->fade = rw->fade > 0 ? rw->fade : 1;
    w->h = rw->h;
    w->align = rw->align;
    w->uv[0] = rw->uv[0];
    w->count = 0;
    w->uv[1] = rw->uv[1];
    w->state = 0;
    w->uv[2] = rw->uv[2];
    w->uv[3] = rw->uv[3];
    if (!end_staff_roll_image)
        end_staff_roll_image = model_load_timed((char *)ending_files[end_staff_roll_image_req], 0xd100);
}

/* 0x00af8df8 */
void tex_release(void)
{
}

/* 0x00af8e00 */
void tex_exec(RollTex *w)
{
    Task *ptask;

    if (w->state == 0) {
        if (++w->count == w->fade)
            w->state = 1;
    } else if (w->state == w->hold) {
        if (--w->count == 0) {
            w->state = 0;
            FINISH_IF(w);
            ptask = TASK_OF(w)->parent;
            ((Roll *)(ptask ? ptask + 1 : 0))->done++;
        }
    } else {
        w->state++;
    }
}

/* 0x00af8e80 */
void tex_disp(RollTex *w)
{
    RollCmd *rcmd = w->cmd;
    unsigned int col;
    float x, y;
    float uv[4];
    int old_fc;

    col = (w->count * 255 / w->fade + 1) >> 1 << 24 | 0x808080;
    switch (w->align) {
    default:
        x = w->x + rcmd->x;
        y = w->y + rcmd->y;
        break;
    case 1:
        x = w->x - w->width * 0.5f + rcmd->x;
        y = w->y + rcmd->y;
        break;
    case 2:
        x = w->x - w->width + rcmd->x;
        y = w->y + rcmd->y;
        break;
    }
    ulGxSetTextureList(end_staff_roll_image);
    draw_set_blend(4, 5);
    old_fc = draw_set_fc_mode(8192);
    uv[0] = w->uv[0];
    uv[1] = w->uv[1];
    uv[2] = w->uv[2];
    uv[3] = w->uv[3];
    tex_rect_draw_uv(rcmd->val, col, uv, x, y, 2.2222223f, w->width, w->h);
    draw_set_fc_mode(old_fc);
}

/* 0x00af9000 */
void *tex_create(void *parent, RollCmd *rcmd)
{
    return task_create_inherit_arg(parent, &tex_desc, 64, rcmd);
}

/* steer(cast) */
/* 0x00af9028 */
void end_staff_roll_init(Roll *w)
{
    Task *ptask;

    task_set_dlist(w, 4);
    end_staff_roll_load();
    ptask = TASK_OF(w)->parent;
    ending_stop_scene((struct Ending *)(ptask ? ptask + 1 : 0));
    w->frame = 0;
    w->done = 0;
    w->index = 0;
    switch (ending_type_get()) {
    case 0:
    case 1:
    case 2:
        w->color = COL_WHITE;
        ((void (*)(unsigned int))fog_req_color)(COL_BLACK);
        ((void (*)(unsigned int))BackColReq)(COL_BLACK);
        w->jp = 1;
        break;
    default:
        w->color = COL_BLACK;
        end_staff_roll_butterfly_create(w);
        w->jp = 0;
        break;
    }
    w->fade = time_scale * FPS;
    w->uv[3] = 0;
    w->mode = 2;
    w->x = 0;
    w->y = 0;
    w->align = 0;
    w->width = 12.0f;
    w->uv[0] = 0;
    w->uv[1] = 0;
    w->uv[2] = 0;
    w->h = 19.0f;
    w->hold = time_scale * 5.0f * FPS;
}

/* 0x00af9140 */
void end_staff_roll_release(void)
{
    end_staff_roll_unload();
}

/* 0x00af9148 */
void end_staff_roll_exec_end(Roll *w)
{
    Task *ptask;

    if (fade_is_idle() == 1) {
        fade_start_out((int)(time_scale * FPS));
        ptask = TASK_OF(w)->parent;
        end_message_create(ptask ? ptask + 1 : 0);
        FINISH_IF(w);
    }
}

/*
 * Commands: 1 text line, 2 image, 3 butterfly cut, 4 position/alignment, 5 size/text mode, 6-7
 * texture coordinates, 8 hold/fade times, 9 colour.
 */
/* 0x00af91c0 */
void end_staff_roll_exec(Roll *w)
{
    RollCmd *rcmd;

    if (!(w->done < end_staff_roll_file->count)) {
        TASK_OF(w)->exec = (void (*)(void *))end_staff_roll_exec_end;
        fade_start(0, 1, (int)(time_scale * FPS));
        return;
    }
    if (w->index < end_staff_roll_file->count) {
        w->frame++;
        rcmd = (RollCmd *)&end_staff_roll_file->e[w->index];
        for (; w->index < end_staff_roll_file->count; w->index++, rcmd++) {
                if (w->frame < rcmd->time * time_scale * FPS)
                    break;
                switch (rcmd->type) {
                case 1:
                    str_create(w, rcmd);
                    break;
                case 2:
                    tex_create(w, rcmd);
                    break;
                case 3:
                    if (!w->jp) {
                        cut_create(w, rcmd->val, &w->done);
                        break;
                    }
                    w->done++;
                    break;
                case 4:
                    w->x = rcmd->x;
                    w->y = rcmd->y;
                    if (rcmd->val < 3)
                        w->align = rcmd->val;
                    w->done++;
                    break;
                case 5:
                    if (0.0f < rcmd->x)
                        w->width = rcmd->x;
                    if (0.0f < rcmd->y)
                        w->h = rcmd->y;
                    if (rcmd->val < 4)
                        w->mode = rcmd->val;
                    w->done++;
                    break;
                case 6:
                    w->uv[0] = rcmd->x;
                    w->uv[2] = rcmd->y;
                    w->done++;
                    break;
                case 7:
                    w->uv[1] = rcmd->x;
                    w->uv[3] = rcmd->y;
                    w->done++;
                    break;
                case 8:
                    if (0.0f < rcmd->x)
                        w->hold = rcmd->x * time_scale * FPS;
                    if (0.0f < rcmd->y)
                        w->fade = rcmd->y * time_scale * FPS;
                    w->done++;
                    break;
                case 9:
                    w->color = rcmd->val;
                    w->done++;
                    break;
                default:
                    w->done++;
                    break;
                }
            }
    }
    if (debug_flags_test(8)) {
        if (kbd_state[5] == 39)
            cut_create(w, 0, &cut_done);
        if (kbd_state[5] == 30)
            cut_create(w, 1, &cut_done);
        if (kbd_state[5] == 31)
            cut_create(w, 2, &cut_done);
        if (kbd_state[5] == 32)
            cut_create(w, 3, &cut_done);
    }
}

/* 0x00af9558 */
void end_staff_roll_disp(void)
{
}

/* 0x00af9560 */
void *end_staff_roll_create(void *parent)
{
    return task_create_inherit(parent, &end_staff_roll_desc, 76);
}

/* steer(cast) */
/* 0x00af9588 */
void end_staff_roll_butterfly_vars_register(void)
{
    unsigned int i;
    VarDef *vd;
    int *word;

    end_staff_roll_butterfly_vars_group = ((int (*)(struct GwVarNode **, int, char *))gw_varedit_add)(&gw_varedit_root, game_varedit_get(), (char *)&end_staff_roll_butterfly_vars_group_def);
    vd = end_staff_roll_butterfly_vardefs;
    word = (int *)&end_staff_roll_butterfly_prm;
    for (i = 0; i < 17; i++) {
        vd->ptr = word;
        word++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, end_staff_roll_butterfly_vars_group, (struct GwVarDef *)end_staff_roll_butterfly_vardefs);
}

/* 0x00af9608 */
void end_staff_roll_butterfly_vars_unregister(void)
{
    gw_varedit_remove(&gw_varedit_root, game_varedit_get(), (struct GwVarDef *)&end_staff_roll_butterfly_vars_group_def);
}

/* 0x00af9640 */
void end_staff_roll_butterfly_vars_init(void)
{
    end_staff_roll_butterfly_vars_register();
}

/* 0x00af9648 */
void end_staff_roll_butterfly_vars_release(void)
{
    end_staff_roll_butterfly_vars_unregister();
}

/* 0x00af9650 */
void end_staff_roll_butterfly_path_eval(ModelPart *path, Vec3 *pos, int *ang, float t)
{
    void *res[3];
    Vec3 dummy;
    float sc[2];
    unsigned int turn = 0xffffc000;

    res[0] = pos;
    res[1] = ang;
    res[2] = &dummy;
    anim_eval_hierarchy(path->root, path->clip, res, t);
    ROT_Y(turn, sc, pos->x, pos->z);
    ulMatrixPush();
    matrix_unit_set(0);
    cut_rotate_yxz(ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_90);
    model_camera_angles(ang);
    ulMatrixPop();
}

/* 0x00af9728 */
void end_staff_roll_butterfly_mirror(Vec3 *spos, int *sang, Vec3 *dpos, int *dang)
{
    float water_y = -10.0f;

    dpos->x = spos->x;
    dpos->z = spos->z;
    dpos->y = -(spos->y - water_y) + water_y;
    i3_copy(dang, sang);
    dang[0] = -dang[0];
    dang[2] = -dang[2];
}

/* 0x00af9780 */
void end_staff_roll_butterfly_mirror_body(RollButterfly *w)
{
    end_staff_roll_butterfly_mirror(&w->pos, w->ang, &w->mpos, w->mang);
}

static inline void i3_set(int *dst, int x, int y, int z)
{
    dst[0] = x;
    dst[1] = y;
    dst[2] = z;
}

/* steer(cast) */
/* 0x00af9798 */
void end_staff_roll_butterfly_init(RollButterfly *w)
{
    int *ent;
    int root;
    int i;
    RollButterflyPrm *prm;
    RollButterflyChild *child;
    Vec3 ofs;
    int ang[3];
    float rad;

    task_set_dlist(w, 4);
    rand_lcg_seed_b(22136);
    if (ending_type_get() == 4) {
        dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(w->models, 7)), *((struct DcClip **)pnb_get_entry(w->models, 10)), 4096);
    } else {
        dc_object_init(&w->body, *((struct DcNode **)pnb_get_entry(w->models, 8)), *((struct DcClip **)pnb_get_entry(w->models, 10)), 4096);
    }
    rad = 30.0f;
    w->level = w->body.radius;
    w->body.radius = rad;
    dc_object_init(&w->water, *((struct DcNode **)pnb_get_entry(w->models, 9)), *((struct DcClip **)pnb_get_entry(w->models, 11)), 1280);
    w->body.speed *= frame_scale;
    w->water.radius = rad;
    dc_object_set_wire(&w->water, model_pnb_get_wire(w->models, 9));
    ent = ((int *)pnb_get_entry(w->models, 13));
    dc_object_init(&w->child_part, *ent, *((struct DcClip **)pnb_get_entry(w->models, 14)), 4096);
    w->child_part.speed *= frame_scale;
    w->child_part.radius = rad;
    ent = ((int *)pnb_get_entry(w->models, 15));
    dc_object_init(&w->mchild_part, *ent, *((struct DcClip **)pnb_get_entry(w->models, 16)), 1280);
    w->mchild_part.radius = rad;
    dc_object_set_wire(&w->mchild_part, model_pnb_get_wire(w->models, 15));
    root = system_pnb_entry_get(0);
    dc_object_init(&w->path, root, *((struct DcClip **)pnb_get_entry(w->models, 12)), 0);
    w->path.speed *= frame_scale;
    v3_set(&w->pos, 0.0f, -5.0f, -40.0f);
    w->timer = 0;
    i3_set(w->ang, 0, ANG_90, 0);
    w->ready = 0;
    w->speed = w->body.speed;
    end_staff_roll_butterfly_path_eval(&w->path, &w->tgt, ang, w->path.frames);
    w->count = end_staff_roll_butterfly_prm.count;
    child = w->children;
    for (i = 0; i < w->count; i++, child++) {
        prm = &end_staff_roll_butterfly_prm;
        child->base.x = RAND_FLOAT_B() * (prm->child_pos_x_max - prm->child_pos_x_min) + prm->child_pos_x_min;
        child->base.y = RAND_FLOAT_B() * (prm->child_pos_y_max - prm->child_pos_y_min) + prm->child_pos_y_min;
        child->base.z = RAND_FLOAT_B() * (prm->child_pos_z_max - prm->child_pos_z_min) + prm->child_pos_z_min;
        child->k.x = RAND_FLOAT_B() * (prm->child_path_scl_x_max - prm->child_path_scl_x_min) + prm->child_path_scl_x_min;
        child->k.y = RAND_FLOAT_B() * (prm->child_path_scl_y_max - prm->child_path_scl_y_min) + prm->child_path_scl_y_min;
        child->k.z = RAND_FLOAT_B() * (prm->child_path_scl_z_max - prm->child_path_scl_z_min) + prm->child_path_scl_z_min;
        child->state = 0;
        ofs.x = w->tgt.x * child->k.x;
        ofs.y = w->tgt.y * child->k.y;
        ofs.z = w->tgt.z * child->k.z;
        v3_sub(&child->base, &child->base, &ofs);
        v3_copy(&child->start, &child->base);
        child->phase = RAND_FLOAT_B() * w->child_part.frames;
        child->rate = RAND_FLOAT_B() * (prm->child_spd_scl_max - prm->child_spd_scl_min) + prm->child_spd_scl_min;
        child->alt = 0;
    }
    w->tick = 0;
}

/* 0x00af9c48 */
void end_staff_roll_butterfly_release(RollButterfly *w)
{
    dc_object_release(&w->body);
    dc_object_release(&w->water);
    dc_object_release(&w->child_part);
    dc_object_release(&w->mchild_part);
}

/* steer(cast) */
/* 0x00af9c88 */
void end_staff_roll_butterfly_exec_children(RollButterfly *w)
{
    ModelPart *cpart, *mpart, *path;
    RollButterflyChild *child;
    int i;
    Vec3 ofs;

    w->ready = 1;
    child = w->children;
    for (i = 0; i < w->count; i++, child++) {
        cpart = &w->child_part;
        mpart = &w->mchild_part;
        path = &w->path;
        if (child->state == 2) {
            child->state = 0;
            v3_copy(&child->base, &child->start);
        }
        if (child->alt) {
            dc_object_set_clip(cpart, *((struct DcClip **)pnb_get_entry(w->models, 14)));
            dc_object_set_clip(mpart, *((struct DcClip **)pnb_get_entry(w->models, 16)));
        } else {
            dc_object_set_clip(cpart, *((struct DcClip **)pnb_get_entry(w->models, 10)));
            dc_object_set_clip(mpart, *((struct DcClip **)pnb_get_entry(w->models, 11)));
        }
        child->phase += w->child_part.speed * child->rate;
        if (w->child_part.frames < child->phase) {
            child->phase -= w->child_part.frames;
            ofs.x = w->tgt.x * child->k.x;
            ofs.y = w->tgt.y * child->k.y;
            ofs.z = w->tgt.z * child->k.z;
            v3_add(&child->base, &ofs);
            child->alt = RAND_FLOAT_B() < end_staff_roll_butterfly_prm.chance;
        }
        end_staff_roll_butterfly_path_eval(path, &ofs, child->ang, w->path.frames * child->phase / w->child_part.frames);
        ofs.x *= child->k.x;
        ofs.y *= child->k.y;
        ofs.z *= child->k.z;
        v3_sum(&child->pos, &ofs, &child->base);
        end_staff_roll_butterfly_mirror(&child->pos, child->ang, &child->mpos, child->mang);
    }
}

/* 0x00af9f20 */
void end_staff_roll_butterfly_exec_leave(RollButterfly *w)
{
    Vec3 cur, goal;
    float sec, fps;

    sec = 3.0f;
    fps = FPS;
    cur = end_staff_roll_butterfly_pos_water;
    goal = end_staff_roll_butterfly_pos_leave;
    cur.y = w->level + -10.0f;
    vec3_approach(&goal, &cur, w->timer / (time_scale * sec * fps));
    v3_copy(&w->pos, &cur);
    end_staff_roll_butterfly_mirror_body(w);
    if (++w->timer > time_scale * sec * fps) {
        w->timer = 0;
        TASK_OF(w)->exec = (void (*)(void *))end_staff_roll_butterfly_exec_children;
    }
    dc_object_advance_time(&w->body);
    end_staff_roll_butterfly_exec_children(w);
}

/* 0x00afa060 */
void end_staff_roll_butterfly_exec_rings(RollButterfly *w)
{
    RingArg ring_arg;
    Vec3 ring_pos;
    int ang[3];
    float fps;

    fps = FPS;
    if (++w->tick % (int)(time_scale * fps) == 0) {
        ring_pos.x = w->pos.x;
        ring_pos.z = w->pos.z;
        ring_pos.y = -10.0f;
        ring_arg.pos = &ring_pos;
        ring_arg.ang = ang;
        ang[0] = 0;
        ang[1] = 0;
        ang[2] = 0;
        end_circle_create(w, &ring_arg);
    }
    if (++w->timer > time_scale * 3.0f * fps) {
        w->timer = 0;
        TASK_OF(w)->exec = (void (*)(void *))end_staff_roll_butterfly_exec_leave;
        w->body.speed = w->speed;
    }
    dc_object_advance_time(&w->body);
    end_staff_roll_butterfly_exec_children(w);
}

/* 0x00afa170 */
void end_staff_roll_butterfly_exec_sink(RollButterfly *w)
{
    Vec3 cur, goal;
    RingArg ring_arg;
    Vec3 ring_pos;
    int ang[3];
    float sec, fps, water_y;

    sec = 4.0f;
    fps = FPS;
    water_y = -10.0f;
    cur = end_staff_roll_butterfly_pos_water;
    goal = end_staff_roll_butterfly_pos_water;
    goal.y = w->level + water_y;
    vec3_approach(&goal, &cur, w->timer / (time_scale * sec * fps));
    v3_copy(&w->pos, &cur);
    end_staff_roll_butterfly_mirror_body(w);
    if (++w->timer > time_scale * sec * fps) {
        ring_pos.x = w->pos.x;
        ring_pos.z = w->pos.z;
        ring_pos.y = water_y;
        ring_arg.pos = &ring_pos;
        ring_arg.ang = ang;
        ang[0] = 0;
        ang[1] = 0;
        ang[2] = 0;
        end_circle_create(w, &ring_arg);
        w->timer = 0;
        TASK_OF(w)->exec = (void (*)(void *))end_staff_roll_butterfly_exec_rings;
    }
    w->body.speed = (w->body.speed - w->speed) * 0.95f + w->speed;
    dc_object_advance_time(&w->body);
}

/* 0x00afa308 */
void end_staff_roll_butterfly_exec(RollButterfly *w)
{
    Vec3 cur, goal;
    float fps;

    fps = FPS;
    cur = end_staff_roll_butterfly_pos_enter;
    goal = end_staff_roll_butterfly_pos_water;
    vec3_approach(&goal, &cur, w->timer / (time_scale * fps));
    v3_copy(&w->pos, &cur);
    end_staff_roll_butterfly_mirror_body(w);
    if (++w->timer > time_scale * fps) {
        w->timer = 0;
        TASK_OF(w)->exec = (void (*)(void *))end_staff_roll_butterfly_exec_sink;
    }
    dc_object_advance_time(&w->body);
}

/* 0x00afa410 */
int end_staff_roll_butterfly_draw_mirrored(void *w, Vec3 *pos, int *ang, ModelPart *mdl, Vec3 *mpos, int *mang, ModelPart *mmdl, int state, float t)
{
    int drawn = 0;

    if (view_sphere_visible(pos, mdl->radius)) {
        do {
            ulMatrixPush();
        } while (0);
        do { vu0_matrix_translate(ulMatrixCur, pos->x, pos->y, pos->z); } while (0);
        cut_rotate_yxz(ang);
        drawn = 1;
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        model_object_draw(mdl, t);
        ulMatrixPop();
    }
    if (view_sphere_visible(mpos, mmdl->radius)) {
        draw_set_color(RGBA(0xff, 0, 0, 0x20));
        draw_set_blend(4, 5);
        do {
            ulMatrixPush();
        } while (0);
        do { vu0_matrix_translate(ulMatrixCur, mpos->x, mpos->y, mpos->z); } while (0);
        cut_rotate_yxz(mang);
        drawn = 1;
        vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
        model_object_draw(mmdl, t);
        ulMatrixPop();
    }
    if (drawn)
        state = 1;
    else if (state == 1)
        state = 2;
    return state;
}

/* steer(cast) */
/* 0x00afa588 */
void end_staff_roll_butterfly_disp(RollButterfly *w)
{
    ModelPart *cpart, *mpart;
    RollButterflyChild *child;
    int i;

    ulGxSetTextureList(w->textures);
    end_staff_roll_butterfly_draw_mirrored(w, &w->pos, w->ang, &w->body, &w->mpos, w->mang, &w->water, 0, w->body.frame);
    if (w->ready) {
        child = w->children;
        for (i = 0; i < w->count; i++, child++) {
            cpart = &w->child_part;
            mpart = &w->mchild_part;
            if (child->alt) {
                dc_object_set_clip(cpart, *((struct DcClip **)pnb_get_entry(w->models, 14)));
                dc_object_set_clip(mpart, *((struct DcClip **)pnb_get_entry(w->models, 16)));
            } else {
                dc_object_set_clip(cpart, *((struct DcClip **)pnb_get_entry(w->models, 10)));
                dc_object_set_clip(mpart, *((struct DcClip **)pnb_get_entry(w->models, 11)));
            }
            child->state = end_staff_roll_butterfly_draw_mirrored(w, &child->pos, child->ang, cpart, &child->mpos, child->mang, mpart, child->state, child->phase);
        }
    }
}

/* 0x00afa6c8 */
void *end_staff_roll_butterfly_create(void *parent)
{
    return task_create_inherit(parent, &end_staff_roll_butterfly_desc, 10552);
}

/* steer(cast) */
/* 0x00afa6f0 */
void cut_init(Cut *w, int *prm)
{
    Vec3 dir;
    ModelPart *body_part;
    int *ent;
    int ang[3];

    body_part = &w->body;
    task_set_dlist(w, 4);
    if (ending_type_get() == 4) {
        ent = ((int *)pnb_get_entry(w->models, 7));
        dc_object_init(body_part, *ent, *((struct DcClip **)pnb_get_entry(w->models, 10)), 4096);
    } else {
        ent = ((int *)pnb_get_entry(w->models, 8));
        dc_object_init(body_part, *ent, *((struct DcClip **)pnb_get_entry(w->models, 10)), 4096);
    }
    w->timer = 0;
    body_part->radius = 30.0f;
    switch ((unsigned int)prm[0]) {
    case 1:
        v3_set(&w->from, 30.0f, -5.0f, -5.0f);
        v3_set(&w->to, -30.0f, 5.0f, -5.0f);
        break;
    case 2:
        v3_set(&w->from, -30.0f, -5.0f, -5.0f);
        v3_set(&w->to, 30.0f, 5.0f, -5.0f);
        break;
    case 3:
        v3_set(&w->from, 5.0f, -30.0f, -5.0f);
        v3_set(&w->to, -5.0f, 30.0f, -5.0f);
        break;
    default:
        v3_set(&w->from, 30.0f, -30.0f, -5.0f);
        v3_set(&w->to, -30.0f, 30.0f, -5.0f);
        break;
    }
    v3_sub(&dir, &w->to, &w->from);
    do {
        ang[1] = (int)RAD_TO_ANG(atan2f(-dir.x, -dir.z));
        ang[0] = (int)RAD_TO_ANG(atan2f(dir.y, sqrtf(dir.x * dir.x + dir.z * dir.z)));
    } while (0);
    ang[2] = 0;
    ulMatrixPush();
    matrix_unit_set(0);
    cut_rotate_yxz(ang);
    vu0_matrix_rotate_z(ulMatrixCur, ANG_90);
    model_camera_angles(w->ang);
    ((void (*)(void))ulMatrixPop)();
    w->done = (int *)prm[1];
    w->id = prm[0];
}

/* 0x00afa9f8 */
void cut_release(Cut *w)
{
    dc_object_release(&w->body);
}

/* 0x00afaa00 */
void cut_exec(Cut *w)
{
    Vec3 cur;

    v3_copy(&cur, &w->from);
    vec3_approach(&w->to, &cur, w->timer / (time_scale * 4.0f * FPS));
    v3_copy(&w->pos, &cur);
    if (++w->timer > time_scale * 4.0f * FPS) {
        FINISH_IF(w);
        (*w->done)++;
    }
}

/* 0x00afab00 */
void cut_disp(Cut *w)
{
    ModelPart *body_part = &w->body;
    Vec3 *bpos = &w->pos;

    if (!view_sphere_visible(bpos, body_part->radius))
        return;
    ulMatrixPush();
    MTX_TRANS(bpos);
    cut_rotate_yxz(w->ang);
    vu0_matrix_rotate_y(ulMatrixCur, ANG_180);
    model_object_draw(body_part, 8.0f);
    ulMatrixPop();
}

/* 0x00afabb0 */
void *cut_create(Roll *rw, int id, int *done)
{
    int prm[2];

    prm[0] = id;
    prm[1] = (int)done;
    return task_create_inherit_arg(rw, &cut_desc, 160, prm);
}

/* 0x00afabe0 */
void cut_rotate_yxz(int *ang)
{
    vu0_matrix_rotate_y(ulMatrixCur, ang[1]);
    vu0_matrix_rotate_x(ulMatrixCur, ang[0]);
    vu0_matrix_rotate_z(ulMatrixCur, ang[2]);
}
