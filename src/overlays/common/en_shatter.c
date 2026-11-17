/*
 * stg05 break effects and draw sorting: en_shatter_spawn breaks a model apart with one of seven
 * parameter sets; the "enSort" task collects up to 32 armoured-enemy draw callbacks and, while
 * en_sort_flag is set, draws them farthest first.
 */

#include "types_common.h"

#include "types_overlays.h"
#include "types_gw.h"
#include "debug.h"
#include "camera.h"
#include "gw_varedit.h"
#include "heap.h"
#include "math_util.h"
#include "task.h"
#include "en_shatter.h"
#include "enemy_shatter.h"
#include "enemy_vars.h"

extern struct GwVarNode *gw_varedit_root;

extern EnShatterSet en_shatter_sets[7];
extern VarDef en_shatter_vardef;
extern VarDef en_shatter_vardefs[15];
extern struct TaskDesc en_sort_desc;
extern EnSortWork *en_sort_list;
extern GwVarDef en_sort_vardef[];
extern GwVarDef en_sort_vardefs[];

/* "enSort" entry, 8 bytes: key is the negated squared distance from the eye */
typedef struct EnSortEntry {
    float key;
    EnArmorDispArg *e;            /* 0x04 */
} EnSortEntry;

/* en_shatter_sets record, 68 bytes */
typedef struct BreakRec {
    char *name;
    char *label;                /* 0x04 */
    ShatterParams p;            /* 0x08 */
    int fog[3];                 /* 0x38 */
} BreakRec;

extern char str_sort_regist_over[];
extern int en_sort_flag;

/* 0x00b12a98 stg05, 0x00b0d758 stg22 */
void en_shatter_vars_register(void)
{
    int top, grp;
    unsigned int i, j;
    VarDef *def;
    int *val;

    top = gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), (struct GwVarDef *)&en_shatter_vardef);
    for (i = 0; i < 7; i++) {
        def = en_shatter_vardefs;
        val = en_shatter_sets[i].v;
        en_shatter_vardef.name = en_shatter_sets[i].name;
        en_shatter_vardef.fmt = en_shatter_sets[i].label;
        grp = gw_varedit_add(&gw_varedit_root, top, (struct GwVarDef *)&en_shatter_vardef);
        for (j = 0; j < 15; j++) {
            def->ptr = val;
            val++;
            def++;
        }
        gw_varedit_add_table(&gw_varedit_root, grp, (struct GwVarDef *)en_shatter_vardefs);
    }
}

/* 0x00b12bb0 stg05, 0x00b0d870 stg22 */
void en_shatter_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b12bb8 stg05, 0x00b0d878 stg22 */
Shatter *en_shatter_spawn(void *obj, SpawnDesc *desc, int kind)
{
    ShatterParams prm;
    int fog[3];
    BreakRec *rec = (BreakRec *)&en_shatter_sets[kind];

    heap_bzero(&prm, 48);
    heap_bzero(fog, 12);
    prm.Gravity = rec->p.Gravity;
    prm.PowVec = rec->p.PowVec;
    prm.PowAng = rec->p.PowAng;
    prm.RtoVecDec = rec->p.RtoVecDec;
    prm.RtoAngDec = rec->p.RtoAngDec;
    prm.RotSpdPlayer = rec->p.RotSpdPlayer;
    prm.TimeAlphaStart = rec->p.TimeAlphaStart;
    prm.TimeAlphaEnd = rec->p.TimeAlphaEnd;
    prm.force_wire = rec->p.force_wire;
    prm.pcol_wire = rec->p.pcol_wire;
    prm.b_acc = rec->p.b_acc;
    prm.frm_acc = rec->p.frm_acc;
    fog[0] = rec->fog[0];
    fog[1] = rec->fog[1];
    fog[2] = rec->fog[2];
    return enemy_shatter_create(obj, desc, (float *)&prm, fog);
}

/* 0x00b12cb0 stg05, 0x00b0d970 stg22 */
void en_sort_vars_register(void)
{
    gw_varedit_add_table(&gw_varedit_root,
                         gw_varedit_add(&gw_varedit_root, enemy_vars_group_acquire(), en_sort_vardef),
                         en_sort_vardefs);
}

/* 0x00b12d00 stg05, 0x00b0d9c0 stg22 */
void en_sort_vars_unregister(void)
{
    enemy_vars_group_release();
}

/* 0x00b12d08 stg05, 0x00b0d9c8 stg22 */
void en_sort_insertion_sort(EnSortEntry *list, int n)
{
    int i, j;
    EnSortEntry rec;
    EnSortEntry *p;
    float key;

    for (i = 1; i < n; i++) {
        key = list[i].key;
        rec = list[i];
        j = i - 1;
        p = &list[j];
        do {
            if (!(key < p->key))
                break;
            p[1] = p[0];
            p--;
        } while (--j >= 0);
        list[j + 1] = rec;
    }
}

/* 0x00b12da8 stg05, 0x00b0da68 stg22 */
void en_sort_init(EnSortWork *w)
{
    w->n = 0;
    en_sort_list = w;
}

/* 0x00b12db8 stg05, 0x00b0da78 stg22 */
void en_sort_release(EnSortWork *w)
{
    en_sort_list = 0;
}

/* 0x00b12dc8 stg05, 0x00b0da88 stg22 */
void en_sort_exec(EnSortWork *w)
{
    EnArmorDispArg *ent;
    EnArmorDispArg *dst;
    int i, j;

    for (i = 0, ent = w->e; i < w->n; i++, ent++) {
        if (ent->owner && (TASK_OF(ent->owner)->flags & TASK_F_END)) {
            w->n--;
            dst = ent;
            for (j = i; j < w->n; j++) {
                dst[0] = dst[1];
                dst++;
            }
            i--;
            ent--;
        }
    }
}

/* 0x00b12e90 stg05, 0x00b0db50 stg22 */
void en_sort_disp(EnSortWork *w)
{
    EnSortEntry list[32];
    Vec3 eye;
    EnArmorDispArg *ent = w->e;
    EnSortEntry *rec;
    int i;

    if (!en_sort_flag) {
        for (i = 0; i < w->n; i++, ent++)
            ent->fn(ent->work);
    } else {
        camera_get_eye(&eye);
        for (i = 0, rec = list; i < w->n; i++, rec++, ent++) {
            rec->e = ent;
            rec->key = -vec3_distance_squared(&eye, ent->pos);
        }
        en_sort_insertion_sort(list, w->n);
        for (i = 0, rec = list; i < w->n; i++, rec++)
            rec->e->fn(rec->e->work);
    }
}

/* 0x00b12fb0 stg05, 0x00b0dc70 stg22 */
void *en_sort_create(void *parent)
{
    return task_create_inherit(parent, &en_sort_desc, 652);
}

/* 0x00b12fd8 stg05, 0x00b0dc98 stg22 */
int en_sort_add(EnArmorDispArg *ent)
{
    EnSortWork *sort = en_sort_list;
    int n;

    if (sort) {
        if (!en_sort_flag)
            return 0;
        n = sort->n;
        if (n >= 32) {
            DEBUG_PRINTF(str_sort_regist_over);
            return 0;
        }
        sort->e[n] = *ent;
        sort->n = n + 1;
        if (ent->owner)
            TASK_OF(ent->owner)->disp = 0;
        return en_sort_list->n;
    }
    return 0;
}

/* 0x00b13080 stg05, 0x00b0dd40 stg22 */
void en_sort_remove(int idx)
{
    EnSortWork *w = en_sort_list;
    EnArmorDispArg *ent;

    if (w && idx) {
        ent = &w->e[idx - 1];
        if (ent->owner)
            TASK_OF(ent->owner)->disp = ent->fn;
        en_sort_list->n--;
        for (idx--; idx < en_sort_list->n; idx++, ent++)
            ent[0] = ent[1];
    }
}
