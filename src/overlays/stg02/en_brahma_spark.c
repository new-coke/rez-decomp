/*
 * Brahma's sparks, Area 2: up to 50 strands of light falling off Brahma under gravity; how many
 * live, and their colours, follow its speed along the rail.
 */

#include "stg02.h"
#include "mathdef.h"
#include "color.h"
#include "enemy_status.h"
#include "gw_varedit.h"
#include "libc.h"
#include "line_batch.h"
#include "node_draw_state.h"
#include "player.h"
#include "task.h"
#include "en_brahma_spark.h"
#include "../common/enemy_vars.h"

extern ParamRec en_brahma_spark_params;
extern VarDef en_brahma_spark_vardefs[6];
extern struct TaskDesc en_brahma_spark_desc;

/* 108 bytes */
typedef struct EnBrahmaSparkStrand {
    int on;
    Vec3 vel;                   /* 0x04 */
    int col0;                      /* 0x10 */
    int col1;                      /* 0x14 */
    float life;                 /* 0x18: frames */
    Vec3 pts[6];                /* 0x1c */
    int tail;                     /* 0x64 */
    int head;                     /* 0x68 */
} EnBrahmaSparkStrand;

/* 5420 bytes */
typedef struct EnBrahmaSparkWork {
    Vec3 *pos;
    void *src;                  /* 0x04 */
    float *rate;                   /* 0x08: 0..1, how many strands */
    EnBrahmaSparkStrand strands[50];            /* 0x0c */
    int n;                      /* 0x1528 */
} EnBrahmaSparkWork;

/* en_brahma_spark_params: name, label, values */
typedef struct EnBrahmaSparkRec {
    char *name;
    char *label;                /* 0x04 */
    float fall;                 /* 0x08: per second squared */
    int col_a0;                 /* 0x0c */
    int col_b0;                 /* 0x10 */
    int col_a1;                 /* 0x14 */
    int col_b1;                 /* 0x18 */
} EnBrahmaSparkRec;

#define EN_BRAHMA_SPARK_REC ((EnBrahmaSparkRec *)&en_brahma_spark_params)

/* 0x00aff658 */
void en_brahma_spark_vars_register(void)
{
    VarDef def;
    VarDef *vd;
    int *fld;
    unsigned int i;
    struct GwVarNode *grp;

    memset(&def, 0, sizeof def);
    def.name = en_brahma_spark_params.name;
    def.fmt = en_brahma_spark_params.label;
    grp = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&def);
    vd = en_brahma_spark_vardefs;
    fld = en_brahma_spark_params.v;
    for (i = 0; i < 6; i++) {
        vd->ptr = fld;
        fld++;
        vd++;
    }
    gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_brahma_spark_vardefs);
}

/* 0x00aff708 */
void en_brahma_spark_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00aff710 */
void en_brahma_spark_start_strand(EnBrahmaSparkWork *w, int i)
{
    EnBrahmaSparkStrand *strand = &w->strands[i];
    float sc[2][2];
    Vec3 *pos;
    float rate, x, y, z;

    strand->on = 1;
    rate = *w->rate;
    pos = w->pos;
    x = RAND_FLOAT_A() + -0.5f;
    y = RAND_FLOAT_A() * 0.5f + 0.0f;
    z = RAND_FLOAT_A() * 0.6666667f + 0.6666667f;
    ROT_X(((int *)w->src)[0], sc[0], y, z);
    ROT_Y(((int *)w->src)[1], sc[1], x, z);
    v3_set(&strand->vel, x, y, z);
    strand->col0 = color_blend(EN_BRAHMA_SPARK_REC->col_a0, EN_BRAHMA_SPARK_REC->col_a1, rate);
    strand->col1 = color_blend(EN_BRAHMA_SPARK_REC->col_b0, EN_BRAHMA_SPARK_REC->col_b1, rate);
    strand->life = RAND_FLOAT_A() * 12.0f + 12.0f;
    strand->tail = 0;
    strand->head = 0;
    v3_copy(&strand->pts[0], pos);
}

/* 0x00aff910 */
int en_brahma_spark_can_emit(int *w)
{
    if (enemy_status_get_ticks() <= 0)
        return 0;
    return w[1353] < w[1354];
}

/* 0x00aff950 */
void en_brahma_spark_move_strand(char *wp, int i)
{
    EnBrahmaSparkWork *w = (EnBrahmaSparkWork *)wp;
    EnBrahmaSparkStrand *strand = &w->strands[i];
    Vec3 pl_vel;
    Vec3 *src;
    int k, tail, head, nxt;

    if (!strand->on) {
        if (!en_brahma_spark_can_emit((int *)w))
            return;
        en_brahma_spark_start_strand(w, i);
        w->n++;
    } else {
        player_get_frame_vel(&pl_vel);
        {
            float dx = pl_vel.x, dy = pl_vel.y, dz = pl_vel.z;
            Vec3 *dst, *base;
            do { base = strand->pts; } while (0); /* steer(barrier) */
            k = 6;
            dst = (Vec3 *)((char *)base + ((i & 1) >> 1)); /* steer(zero) */
            src = base;
            do {
                src->y += dy;
                src->z += dz;
                dst->x = src->x + dx;
                dst++;
                src++;
            } while (--k);
        }
    }
    strand->life -= 1.0f;
    if (strand->life < 0.0f) {
        if (en_brahma_spark_can_emit((int *)w)) {
            en_brahma_spark_start_strand(w, i);
        } else {
            strand->on = 0;
            w->n--;
            return;
        }
    }
    head = strand->head;
    tail = strand->tail;
    strand->vel.y -= EN_BRAHMA_SPARK_REC->fall * 0.00027777778f;
    nxt = (head + 1) % 6;
    if ((nxt - tail + 6) % 6 >= 5)
        tail = (tail + 1) % 6;
    strand->tail = tail;
    strand->head = nxt;
    v3_copy(&strand->pts[nxt], &strand->pts[head]);
    v3_add(&strand->pts[nxt], &strand->vel);
}

/* 0x00affb70 */
void en_brahma_spark_exec(char *w)
{
    float rate = **(float **)(w + 8);
    int i;

    *(int *)(w + 5416) = MIN(MAX_GE(rate, 0.0f), 1.0f) * 50.0f; /* steer(alias) */
    for (i = 49; i >= 0; i--)
        en_brahma_spark_move_strand(w, i);
}

/* steer(cast) */
/* 0x00affc18 */
void en_brahma_spark_disp(EnBrahmaSparkWork *w)
{
    EnBrahmaSparkStrand *strand;
    int line;
    int i;

    draw_set_blend(4, 1);
    line = line_set_translucent(1);
    for (i = 0, strand = w->strands; i < 50; i++, strand++)
        if (strand->on)
            line = ((int (*)(int, int, int, float, Vec3 *, Vec3 *))line_add_v_rgb)(line, strand->col0, strand->col1, 2.0f, &strand->pts[strand->tail], &strand->pts[strand->head]);
    line_end(line);
}

/* 0x00affcc0 */
void en_brahma_spark_init(void *wp, Vec3 *pos, void *src, unsigned int rate)
{
    EnBrahmaSparkWork *w = wp;
    int n;
    int i;

    w->pos = pos;
    w->src = src;
    w->rate = (float *)rate;
    task_set_dlist(w, 6);
    n = MIN(MAX_GE(*w->rate, 0.0f), 1.0f) * 50.0f;
    for (n--; n >= 0; n--)
        en_brahma_spark_start_strand(w, n);
}

/* 0x00affd70 */
void en_brahma_spark_release(void)
{
}

/* 0x00affd78 */
void *en_brahma_spark_create(void *parent, Vec3 *pos, void *src, unsigned int rate)
{
    void *w = task_create(parent, &en_brahma_spark_desc, 5420);

    en_brahma_spark_init(w, pos, src, rate);
    return w;
}
