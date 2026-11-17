/*
 * Vibration player: tables (channel + duration/strength steps) on the pad motors (channels 0, 1)
 * and the Trance Vibrator (2); per-slot records sum their queued tables each frame, clamp, send.
 * Also the action pack loader and the debug vibration editor.
 */

#include "types_game.h"
#include "types_gw.h"
#include "mathdef.h"
#include "libc.h"
#include "gw_varedit.h"
#include "libpad.h"
#include "peri_act.h"
#include "callback.h"
#include "config.h"
#include "debug.h"
#include "file.h"
#include "game.h"
#include "heap.h"
#include "pad.h"
#include "pad_vib.h"
#include "peri_vib.h"
#include "sound_beat.h"
#include "system.h"
#include "task.h"

typedef struct PeriNode {
    int idx;
    int tick;
    int data;
    int next;
} PeriNode;

typedef struct PeriAcc {
    char pad_00[16];
    int ch[8];                  /* 16 */
} PeriAcc;

typedef struct PeriActPeriBlk PeriActPeriBlk;

struct PeriActPeriBlk {
    char pad_00[12];
    PeriActPeriBlk *next;              /* 12 */
};

typedef struct PeriList {
    int cnt;
    int *tables[1];
} PeriList;

typedef struct EvObj {
    char pad_00[6];
    short life;
    char pad_08[16];
    char *name;                 /* 24 */
} EvObj;

typedef struct EvRef {
    EvObj *obj;
} EvRef;

typedef struct VibShort {
    int channel;
    int count;
    int dur;
    int val;
} VibShort;

typedef struct VibRampTab {
    int channel;
    int count;
    VibPair pair[4];
} VibRampTab;

typedef struct VibBind {
    char pad_00[16];
    int *dur;                   /* 16 */
    char pad_14[36];
    int *val;                   /* 56 */
    char pad_3c[20];
} VibBind;

typedef struct PeriActPadState {
    char pad_00[68];
    unsigned int buttons;       /* 68 */
} PeriActPadState;

extern PeriRec *peri_act_records[][4];
extern char peri_act_off_levels[];
extern int peri_act_stop_bytes;
extern int peri_act_level_max[3];

extern unsigned char peri_act_cur_bytes;

extern unsigned int system_flags;
extern char peri_act_align_table[];
extern int vibration_assign_test;
extern unsigned char peri_act_all_bytes;
extern int str_peri_act_maxi;
extern PadState *pad_main;
extern VibSet vibration_sets[];
extern VibTab vibration_pad_press_tabs[3];
extern int vibration_assign_pad_press;
extern VibTab vibration_haku0_tabs[3];
extern VibTab vibration_haku1_tabs[3];
extern VibTab vibration_haku2_tabs[3];
extern VibTab vibration_haku3_tabs[3];
extern int vibration_assign_haku[4];

extern int vibration_beat_flag;

extern VibTab vibration_symbol_hit_tabs[3];
extern int vibration_assign_symbol_hit;
extern VibTab vibration_enemy_dead_tabs[3];
extern int vibration_assign_enemy_dead;
extern VibTab vibration_player_hit_tabs[3];
extern int vibration_assign_player_hit;
extern VibShort vibration_symbol_effect_tabs[3];
extern int vibration_assign_symbol_effect;
extern VibRampTab vibration_symbol_effect_ramp_tabs[3];
extern VibTab vibration_symbol_end_tabs[3];
extern int vibration_assign_symbol_end;
extern VibTab vibration_player_dead_tabs[3];
extern int vibration_assign_player_dead;
extern VibBind vibration_set_vardefs[15];
extern int vibration_symbol_hit_listener;
extern int vibration_enemy_dead_listener;
extern int vibration_player_hit_listener;
extern int vibration_symbol_effect_listener;
extern int vibration_symbol_effect_ramp_listener;
extern int vibration_symbol_end_listener;
extern int vibration_player_dead_listener;
extern int vibration_editor_vars_group;
extern int vibration_play_vars_group;
extern int vibration_assign_vars_group;
extern int vibration_set_vars_groups[9];
extern char gw_varedit_root[];
extern GwVarDef vibration_editor_vardef[];
extern char vibration_assign_test_vardef[];
extern char vibration_play_vardef[];
extern GwVarDef vibration_play_vardefs[];
extern char vibration_set_vardef[];
extern char vibration_assign_vardef[];
extern GwVarDef vibration_assign_vardefs[];
extern VibTab vibration_editor_tabs[3];
extern int vibration_editor_desc;
extern int vibration_editor_task;
extern void *system1_work;
extern VibTab vibration_play_no_tabs[3];
extern int vibration_play_no_value;

static int vibration_editor_toggle(void);

/* Plays table data from its start. */
/* 0x00176a68 */
static PeriNode *peri_act_block_new(int data)
{
    PeriNode *blk = heap_block_tag(game_malloc(16), "peri_act.c", 111);

    blk->data = data;
    blk->idx = 0;
    blk->tick = 0;
    blk->next = 0;
    return blk;
}

/* 0x00176ab8 */
static void peri_act_free(void *blk)
{
    heap_free(blk, 0);
}

/*
 * Adds the step's strength to the table's channel in dst; next step once its duration passes.
 * Returns 0 when finished.
 */
/* 0x00176ac0 */
static int peri_act_block_step(PeriAcc *dst, int *st)
{
    int *tbl = (int *)st[2];
    int idx = st[0];
    int ch = tbl[0];
    int tick = st[1] + 1;
    int *ent;

    st[1] = tick;
    ent = tbl + 2 + idx * 2;
    if (ent[0] < tick) {
        st[0] = idx + 1;
        if (idx + 1 < tbl[1]) {
            st[1] = 0;
            ent += 2;
        } else {
            return 0;
        }
    }
    dst->ch[ch] += ent[1];
    return 1;
}

/* 0x00176b38 */
static void peri_act_record_init(PeriRec *rec, int port, int slot)
{
    memset(rec, 0, 44);
    rec->port = port;
    rec->slot = slot;
    peri_act_records[port][slot] = rec;
}

/* Drops queued tables, stops its pad's motors and the Trance Vibrator if connected. */
/* 0x00176ba0 */
static void peri_act_record_destroy(PeriRec *rec)
{
    PeriActPeriBlk *blk = rec->list;
    int port = rec->port;
    int slot = rec->slot;

    while (blk != 0) {
        PeriActPeriBlk *cur = blk;

        blk = blk->next;
        peri_act_free(cur);
    }
    scePadSetActDirect(port, slot, peri_act_off_levels);
    if (peri_vib_is_connected())
        peri_vib_set_act_all(&peri_act_stop_bytes);
    peri_act_records[port][slot] = 0;
    heap_free(rec, 0);
}

/*
 * Sums queued tables per channel, drops finished ones, frees the record when empty. Vibration on,
 * unpaused, game type set: clamps to peri_act_level_max, sends changed levels.
 */
/* 0x00176c88 */
static void peri_act_record_update(PeriRec *rec)
{
    PeriBlk *blk = rec->list;
    PeriBlk *prev = 0;
    unsigned char *act;
    int *acc;
    int *zp;
    int i;
    int changed;
    int val;
    int lim;

    if (blk == 0) {
        peri_act_record_destroy(rec);
        return;
    }
    zp = (int *)&rec->list;
    i = 6;
    do {
        zp--;
        i--;
        *zp = 0;
    } while (i != 0);
    do {
        if (peri_act_block_step(rec, blk)) {
            prev = blk;
            blk = blk->next;
        } else if (prev != 0) {
            PeriBlk *nx = blk->next;

            prev->next = nx;
            peri_act_free(blk);
            blk = prev->next;
        } else {
            PeriBlk *nx = blk->next;

            rec->list = nx;
            peri_act_free(blk);
            blk = rec->list;
        }
    } while (blk != 0);
    if (ConfigGetVibration() == 0)
        return;
    if (system_flags & SYS_NO_VIBRATION)
        return;
    if (game_type_get() == 0)
        return;
    changed = 0;
    for (i = 0; i < 2; i++) {
        rec->acc[i] = rec->acc[i] < peri_act_level_max[i] ? rec->acc[i] : peri_act_level_max[i];
        if (rec->act[i] != rec->acc[i]) {
            rec->act[i] = rec->acc[i];
            changed = 1;
        }
    }
    if (changed)
        scePadSetActDirect(rec->port, rec->slot, rec->act);
    act = rec->act;
    acc = rec->acc;
    acc[2] = acc[2] < peri_act_level_max[2] ? acc[2] : peri_act_level_max[2];
    if (act[2] != acc[2]) {
        act[2] = acc[2];
        if (peri_vib_is_connected()) {
            peri_act_cur_bytes = act[2];
            peri_vib_set_act_all(&peri_act_cur_bytes);
        }
    }
}

/* Debug flag 16 also toggles the vibration editor. */
/* 0x00176e50 */
void peri_act_close_all(void)
{
    unsigned int port;
    int slot;

    for (port = 0; port < 2; port++) {
        for (slot = 0; slot < 4; slot++)
            peri_act_reset_slot(port, slot);
    }
    if (debug_flags_test(16))
        vibration_editor_toggle();
}

/* Sets motor alignment and stops its vibration record. */
/* 0x00176ed8 */
void peri_act_reset_slot(int port, int slot)
{
    int rec;

    scePadSetActAlign(port, slot, peri_act_align_table);
    rec = peri_act_records[port][slot];
    if (rec != 0)
        peri_act_record_destroy(rec);
}

/*
 * Unless paused: stage cues (not during the editor's assign test) and every record. Vibration
 * off, paused or no game type: motors kept stopped.
 */
/* 0x00176f40 */
void peri_act_update_all(void)
{
    PeriRec **slot;
    PeriRec *rec;
    int j;
    int i;
    int changed;

    peri_vib_refresh();
    if (!(system_flags & SYS_NO_VIBRATION)) {
        if (vibration_assign_test == 0)
            peri_act_update();
        slot = peri_act_records;
        i = 8;
        do {
            if (*slot != 0)
                peri_act_record_update(*slot);
            slot++;
            i--;
        } while (i != 0);
    }
    if (ConfigGetVibration() != 0 && !(system_flags & SYS_NO_VIBRATION) && game_type_get() != 0)
        return;
    slot = peri_act_records;
    i = 8;
    do {
        rec = *slot;
        if (rec != 0) {
            slot++;
            i--;
            changed = 0;
            for (j = 0; j < 2; j++) {
                if (rec->act[j] != 0) {
                    rec->act[j] = 0;
                    changed = 1;
                }
            }
            if (changed)
                scePadSetActDirect(rec->port, rec->slot, rec->act);
            if (rec->act[2] != 0) {
                rec->act[2] = 0;
                if (peri_vib_is_connected())
                    peri_vib_set_act_all(&peri_act_all_bytes);
            }
        } else {
            slot++;
            i--;
        }
    } while (i != 0);
}

/* 0x001770a0 */
static int peri_act_slot_has_actuator(int port, int slot)
{
    return scePadInfoComb(port, slot, -1, 0) > 0;
}

/* For each connected pad with n > 0 modes, until cb returns 0. */
/* 0x001770c8 */
static void peri_act_enumerate(int (*cb)(int, int, int, int), int data)
{
    unsigned int port;
    int slot;
    int n;

    if (cb == 0)
        return;
    for (port = 0; port < 2; port++) {
        for (slot = 0; slot < 4; slot++) {
            if (scePadGetState(port, slot) & 6) {
                n = scePadInfoMode(port, slot, 2, 0);
                if (n > 0) {
                    if (cb(port, slot, n, data) == 0)
                        return;
                }
            }
        }
    }
}

/* Indexes its "MAXI" entries (one per action); 0 when none. */
/* 0x00177180 */
PeriPack *peri_act_pack_load(void *name)
{
    void *pack = file_load_sync(name, 0, 0);
    int n = PackFileCountId(pack, str_peri_act_maxi);
    PeriPack *pk;
    void **ent;
    int i;

    if (n > 0) {
        pk = heap_block_tag(game_malloc(n * 4 + 8), "peri_act.c", 537);
        pk->pack = pack;
        pk->count = n;
        ent = pk->entry;
        for (i = 0; i < n; i++) {
            *ent = PackFileGetById(pack, str_peri_act_maxi, i, 0);
            ent++;
        }
        return pk;
    }
    return 0;
}

/* Also stops all vibration. */
/* 0x00177258 */
void peri_act_pack_free(int *pk)
{
    int *slot;
    int i;

    if (pk != 0) {
        i = 8;
        slot = peri_act_records;
        do {
            if (*slot != 0)
                peri_act_record_destroy(*slot);
            slot++;
        } while (--i != 0);
        heap_free((void *)pk[0], 0);
        heap_free(pk, 0);
    }
}

/* Queues every table of the action. */
/* 0x001772e0 */
void peri_act_play_pack_entry(PeriPack *pack, int idx, int port, int slot)
{
    PeriRec *rec;
    int *tab;
    int n;

    if (pack == 0)
        return;
    if (idx < 0 || idx >= pack->count)
        return;
    if (peri_act_slot_has_actuator(port, slot) == 0)
        return;
    rec = peri_act_records[port][slot];
    if (rec == 0) {
        rec = heap_block_tag(game_malloc(44), "peri_act.c", 606);
        peri_act_record_init(rec, port, slot);
    }
    tab = pack->entry[idx];
    n = *tab;
    tab++;
    if (n > 0) {
        int cnt = n;

        do {
            PeriBlk *blk = peri_act_block_new(tab);
            int sz;

            cnt--;
            blk->next = rec->list;
            sz = tab[1] * 8 + 8;
            rec->list = blk;
            tab = (int *)((char *)tab + sz);
        } while (cnt != 0);
    }
}

/* Only when that pad can vibrate. */
/* 0x001773f8 */
void peri_act_queue_table(int port, int slot, int *data)
{
    PeriRec *rec;
    PeriBlk *blk;

    if (peri_act_slot_has_actuator(port, slot) == 0)
        return;
    rec = peri_act_records[port][slot];
    if (rec == 0) {
        rec = heap_block_tag(game_malloc(44), "peri_act.c", 644);
        peri_act_record_init(rec, port, slot);
    }
    blk = peri_act_block_new(data);
    blk->next = rec->list;
    rec->list = blk;
}

/* 0x001774a8 */
static void peri_act_queue_tables(int port, int slot, PeriList *list)
{
    PeriRec *rec;
    PeriBlk *blk;
    int i;

    if (peri_act_slot_has_actuator(port, slot) == 0)
        return;
    rec = peri_act_records[port][slot];
    if (rec == 0) {
        rec = heap_block_tag(game_malloc(44), "peri_act.c", 672);
        peri_act_record_init(rec, port, slot);
    }
    for (i = 0; i < list->cnt; i++) {
        blk = peri_act_block_new(list->tables[i]);
        blk->next = rec->list;
        rec->list = blk;
    }
}

/* 0x00177580 */
static void peri_act_destroy_at(int port, int slot)
{
    int rec = peri_act_records[port][slot];

    if (rec != 0)
        peri_act_record_destroy(rec);
}

/* 0x001775b8 */
void peri_act_destroy_all(void)
{
    unsigned int port;
    int slot;

    for (port = 0; port < 2; port++) {
        for (slot = 0; slot < 4; slot++) {
            if (peri_act_records[port][slot] != 0)
                peri_act_record_destroy(peri_act_records[port][slot]);
        }
    }
}

/* Editor: button 4 press plays the button's set, first four steps cut to one frame. */
/* 0x00177638 */
static void vibration_pad_press(void)
{
    int n;
    int i;

    if (pad_main->edge & 4) {
        n = vibration_assign_pad_press - 1;
        if (n >= 0) {
            for (i = 0; i < 3; i++) {
                vibration_pad_press_tabs[i].channel = i;
                vibration_pad_press_tabs[i].count = 5;
                /* ch[0][i * 5 + k] is ch[i][k]: rows contiguous. */
                vibration_pad_press_tabs[i].pair[0] = vibration_sets[n].ch[i][0];
                vibration_pad_press_tabs[i].pair[1] = vibration_sets[n].ch[0][i * 5 + 1];
                vibration_pad_press_tabs[i].pair[2] = vibration_sets[n].ch[0][i * 5 + 2];
                vibration_pad_press_tabs[i].pair[3] = vibration_sets[n].ch[0][i * 5 + 3];
                vibration_pad_press_tabs[i].pair[4] = vibration_sets[n].ch[0][i * 5 + 4];
                vibration_pad_press_tabs[i].pair[0].dur = 1;
                vibration_pad_press_tabs[i].pair[1].dur = 1;
                vibration_pad_press_tabs[i].pair[2].dur = 1;
                vibration_pad_press_tabs[i].pair[3].dur = 1;
                peri_act_queue_table(0, 0, &vibration_pad_press_tabs[i]);
            }
        }
    }
}

static inline void vib_queue_beat(int k)
{
    VibPair *src;
    VibTab *dst;
    int n = vibration_assign_haku[k] - 1;
    int i;

    if (n >= 0) {
        src = vibration_sets[n].ch[0];
        dst = k == 0 ? vibration_haku0_tabs : k == 1 ? vibration_haku1_tabs : k == 2 ? vibration_haku2_tabs : vibration_haku3_tabs;
        for (i = 0; i < 3; i++) {
            dst->channel = i;
            dst->count = 5;
            dst->pair[0] = src[0];
            dst->pair[1] = src[1];
            dst->pair[2] = src[2];
            dst->pair[3] = src[3];
            dst->pair[4] = src[4];
            peri_act_queue_table(0, 0, dst);
            src += 5;
            dst++;
        }
    }
}

/* Editor: at 0.9 of the previous beat, the set assigned to that beat of the bar. */
/* 0x00177778 */
static void vibration_editor_beat_pulse(void)
{
    if (beat_grid_hit(0))
        vibration_beat_flag = 0;
    if (vibration_beat_flag == 0) {
        if (beat_clock_get_phase() > 0.9f) {
            vibration_beat_flag = 1;
            switch (beat_clock_get_haku() % 4) {
            case 0:
                vib_queue_beat(0);
                break;
            case 1:
                vib_queue_beat(1);
                break;
            case 2:
                vib_queue_beat(2);
                break;
            case 3:
                vib_queue_beat(3);
                break;
            }
        }
    }
}

/* Editor listener: assigned set for a live "Network Opening" object. */
/* 0x00177b28 */
static void vibration_symbol_hit(EvRef *ev)
{
    VibPair *src;
    VibTab *dst;
    int n;
    int i;

    if (strcmp(ev->obj->name, "Network Opening") == 0) {
        if (ev->obj->life > 0) {
            n = vibration_assign_symbol_hit - 1;
            if (n >= 0) {
                src = vibration_sets[n].ch[0];
                dst = vibration_symbol_hit_tabs;
                for (i = 0; i < 3; i++) {
                    dst->channel = i;
                    dst->count = 5;
                    dst->pair[0] = src[0];
                    dst->pair[1] = src[1];
                    dst->pair[2] = src[2];
                    dst->pair[3] = src[3];
                    dst->pair[4] = src[4];
                    peri_act_queue_table(0, 0, dst);
                    src += 5;
                    dst++;
                }
            }
        }
    }
}

/* Editor listener, enemy death: three channels, five steps each, port 0. */
/* 0x00177c38 */
static void vibration_enemy_dead(void)
{
    VibPair *src;
    VibTab *dst;
    int n = vibration_assign_enemy_dead - 1;
    int i;

    if (n >= 0) {
        src = vibration_sets[n].ch[0];
        dst = vibration_enemy_dead_tabs;
        for (i = 0; i < 3; i++) {
            dst->channel = i;
            dst->count = 5;
            dst->pair[0] = src[0];
            dst->pair[1] = src[1];
            dst->pair[2] = src[2];
            dst->pair[3] = src[3];
            dst->pair[4] = src[4];
            peri_act_queue_table(0, 0, dst);
            src += 5;
            dst++;
        }
    }
}

/* Editor listener, player hit (w[1] below w[0]). */
/* 0x00177d20 */
static void vibration_player_hit(int *w)
{
    VibPair *src;
    VibTab *dst;
    int n = vibration_assign_player_hit - 1;
    int i;

    if (n >= 0 && w[1] < w[0]) {
        src = vibration_sets[n].ch[0];
        dst = vibration_player_hit_tabs;
        for (i = 0; i < 3; i++) {
            dst->channel = i;
            dst->count = 5;
            dst->pair[0] = src[0];
            dst->pair[1] = src[1];
            dst->pair[2] = src[2];
            dst->pair[3] = src[3];
            dst->pair[4] = src[4];
            peri_act_queue_table(0, 0, dst);
            src += 5;
            dst++;
        }
    }
}

/*
 * Editor listener: first step as a steady vibration to the next bar line, or the one after past
 * a quarter of the bar.
 */
/* 0x00177e18 */
static void vibration_symbol_effect(void)
{
    int n = vibration_assign_symbol_effect - 1;
    int frames;
    float bar;
    int i;

    if (n >= 0) {
        frames = (int)(240.0f / beat_clock_get_bpm() * FPS);
        bar = beat_clock_get_bar();
        if (bar <= 0.25f)
            vibration_symbol_effect_tabs[0].dur = vibration_symbol_effect_tabs[1].dur = vibration_symbol_effect_tabs[2].dur = (int)((1.0f - bar) * (float)frames);
        else
            vibration_symbol_effect_tabs[0].dur = vibration_symbol_effect_tabs[1].dur = vibration_symbol_effect_tabs[2].dur = (int)((2.0f - bar) * (float)frames);
        for (i = 0; i < 3; i++) {
            vibration_symbol_effect_tabs[i].channel = i;
            vibration_symbol_effect_tabs[i].val = vibration_sets[n].ch[i][0].val;
            vibration_symbol_effect_tabs[i].count = 5;
            peri_act_queue_table(0, 0, &vibration_symbol_effect_tabs[i]);
        }
    }
}

/*
 * Editor listener: four-step ramp from a quarter between the set's first two strengths to the
 * second, over 0.2, 0.2, 0.1, 0.5 bars.
 */
/* 0x00177f48 */
static void vibration_symbol_effect_ramp(void)
{
    int n = vibration_assign_symbol_effect - 1;
    int i;
    int start;
    int diff;
    float bar_len;

    if (n >= 0) {
        bar_len = (float)(int)(240.0f / beat_clock_get_bpm() * FPS);
        for (i = 0; i < 3; i++) {
            vibration_symbol_effect_ramp_tabs[i].channel = i;
            vibration_symbol_effect_ramp_tabs[i].count = 5;
            /* ch[0][i * 5 + 1] is ch[i][1]: rows contiguous. */
            diff = vibration_sets[n].ch[0][i * 5 + 1].val;
            start = vibration_sets[n].ch[i][0].val;
            diff -= start;
            vibration_symbol_effect_ramp_tabs[i].pair[0].val = start + diff / 4;
            vibration_symbol_effect_ramp_tabs[i].pair[1].val = start + diff / 2;
            vibration_symbol_effect_ramp_tabs[i].pair[2].val = start + diff * 3 / 4;
            vibration_symbol_effect_ramp_tabs[i].pair[3].val = start + diff;
            vibration_symbol_effect_ramp_tabs[i].pair[0].dur = (int)(bar_len * 0.2f);
            vibration_symbol_effect_ramp_tabs[i].pair[1].dur = (int)(bar_len * 0.2f);
            vibration_symbol_effect_ramp_tabs[i].pair[2].dur = (int)(bar_len * 0.1f);
            vibration_symbol_effect_ramp_tabs[i].pair[3].dur = (int)(bar_len * 0.5f);
            peri_act_queue_table(0, 0, &vibration_symbol_effect_ramp_tabs[i]);
        }
    }
}

/* Editor listener: symbol end. */
/* 0x00178130 */
static void vibration_symbol_end(void)
{
    VibPair *src;
    VibTab *dst;
    int n = vibration_assign_symbol_end - 1;
    int i;

    if (n >= 0) {
        src = vibration_sets[n].ch[0];
        dst = vibration_symbol_end_tabs;
        for (i = 0; i < 3; i++) {
            dst->channel = i;
            dst->count = 5;
            dst->pair[0] = src[0];
            dst->pair[1] = src[1];
            dst->pair[2] = src[2];
            dst->pair[3] = src[3];
            dst->pair[4] = src[4];
            peri_act_queue_table(0, 0, dst);
            src += 5;
            dst++;
        }
    }
}

/* Editor listener: player death. */
/* 0x00178218 */
static void vibration_player_dead(void)
{
    VibPair *src;
    VibTab *dst;
    int n = vibration_assign_player_dead - 1;
    int i;

    if (n >= 0) {
        src = vibration_sets[n].ch[0];
        dst = vibration_player_dead_tabs;
        for (i = 0; i < 3; i++) {
            dst->channel = i;
            dst->count = 5;
            dst->pair[0] = src[0];
            dst->pair[1] = src[1];
            dst->pair[2] = src[2];
            dst->pair[3] = src[3];
            dst->pair[4] = src[4];
            peri_act_queue_table(0, 0, dst);
            src += 5;
            dst++;
        }
    }
}

/*
 * Variables: assign test, play, event assignments, nine sets with each step's duration and
 * strength.
 */
/* steer(cast) */
/* 0x00178300 */
static void vibration_editor_init(void)
{
    int i;
    int j;
    int n;
    int root;
    int val;

    vibration_symbol_hit_listener = ((int (*)(int, void *))listener_add)(11, vibration_symbol_hit);
    vibration_enemy_dead_listener = ((int (*)(int, void *))listener_add)(4, vibration_enemy_dead);
    vibration_player_hit_listener = ((int (*)(int, void *))listener_add)(19, vibration_player_hit);
    vibration_player_dead_listener = ((int (*)(int, void *))listener_add)(22, vibration_player_dead);
    vibration_symbol_effect_listener = ((int (*)(int, void *))listener_add)(7, vibration_symbol_effect);
    vibration_symbol_effect_ramp_listener = ((int (*)(int, void *))listener_add)(8, vibration_symbol_effect_ramp);
    vibration_symbol_end_listener = ((int (*)(int, void *))listener_add)(9, vibration_symbol_end);
    root = game_varedit_get();
    vibration_editor_vars_group = gw_varedit_add(gw_varedit_root, root, vibration_editor_vardef);
    gw_varedit_add(gw_varedit_root, vibration_editor_vars_group, vibration_assign_test_vardef);
    vibration_play_vars_group = ((int (*)(void *, int, void *))gw_varedit_add)(gw_varedit_root, vibration_editor_vars_group, vibration_play_vardef);
    gw_varedit_add_table(gw_varedit_root, vibration_play_vars_group, vibration_play_vardefs);
    vibration_assign_vars_group = ((int (*)(void *, int, void *))gw_varedit_add)(gw_varedit_root, vibration_editor_vars_group, vibration_assign_vardef);
    gw_varedit_add_table(gw_varedit_root, vibration_assign_vars_group, vibration_assign_vardefs);
    for (i = 0; i < 9; i++) {
        vibration_set_vars_groups[i] = gw_varedit_add(gw_varedit_root, vibration_editor_vars_group, vibration_set_vardef + i * 40);
        for (j = 0; j < 15; j++) {
            vibration_set_vardefs[j].dur = &vibration_sets[i].ch[0][j].dur;
            vibration_set_vardefs[j].val = &vibration_sets[i].ch[0][j].val;
        }
        gw_varedit_add_table(gw_varedit_root, vibration_set_vars_groups[i], vibration_set_vardefs);
    }
}

/* 0x00178558 */
static void vibration_editor_release(void)
{
    gw_varedit_remove(gw_varedit_root, game_varedit_get(), vibration_editor_vardef);
    listener_remove(11, vibration_symbol_hit_listener);
    listener_remove(4, vibration_enemy_dead_listener);
    listener_remove(19, vibration_player_hit_listener);
    listener_remove(22, vibration_player_dead_listener);
    listener_remove(7, vibration_symbol_effect_listener);
    listener_remove(8, vibration_symbol_effect_ramp_listener);
    listener_remove(9, vibration_symbol_end_listener);
}

/* Button and beat tests; button 0x200 plays set 0. */
/* 0x00178600 */
static void vibration_editor_exec(void)
{
    VibTab *dst;
    VibPair *src;
    int i;

    vibration_pad_press();
    vibration_editor_beat_pulse();
    if ((*(PeriActPadState * *)&pad_main)->buttons & PAD_Y) { /* steer(cast) */
        for (i = 0; i < 3; i++) {
            dst = &vibration_editor_tabs[i];
            src = vibration_sets[0].ch[i];
            dst->channel = i;
            dst->count = 5;
            dst->pair[0] = src[0];
            dst->pair[1] = src[1];
            dst->pair[2] = src[2];
            dst->pair[3] = src[3];
            dst->pair[4] = src[4];
            peri_act_queue_table(0, 0, dst);
        }
    }
}

/* 0x001786e8 */
static void vibration_editor_disp(void)
{
}

/* Returns the task, 0 once ended. */
/* 0x001786f0 */
static int vibration_editor_toggle(void)
{
    if (vibration_editor_task != 0) {
        TASK_OF(vibration_editor_task)->flags |= TASK_F_END;
        vibration_editor_task = 0;
    } else {
        int *w = task_create(system1_work, &vibration_editor_desc, 4);
        int flags = TASK_OF(w)->flags;

        vibration_editor_task = (int)w;
        TASK_OF(w)->flags = flags | TASK_F_RUN_PAUSED;
    }
    return vibration_editor_task;
}

/* Editor: the set chosen under "play". */
/* 0x00178758 */
static void vibration_play_no(void)
{
    VibPair *src;
    VibTab *dst;
    int n = vibration_play_no_value - 1;
    int i;

    if (n >= 0) {
        src = vibration_sets[n].ch[0];
        dst = vibration_play_no_tabs;
        for (i = 0; i < 3; i++) {
            dst->channel = i;
            dst->count = 5;
            dst->pair[0] = src[0];
            dst->pair[1] = src[1];
            dst->pair[2] = src[2];
            dst->pair[3] = src[3];
            dst->pair[4] = src[4];
            peri_act_queue_table(0, 0, dst);
            src += 5;
            dst++;
        }
    }
}
