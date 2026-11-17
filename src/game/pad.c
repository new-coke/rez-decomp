/*
 * Controller input: two pad ports sampled through a history ring, assigned to four logical ports,
 * decoded into game, menu and raw records (auto-repeat, swapped buttons); port owners, USB
 * keyboard, "Pad" debug window.
 */

#include "types_game.h"
#include "types_ps.h"
#include "debug.h"
#include "gw_draw.h"
#include "gw_keyboard.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_window.h"
#include "libpad.h"
#include "nj_system.h"
#include "ps_pad.h"
#include "pad.h"
#include "callback.h"
#include "debugmenu.h"
#include "heap.h"
#include "padslot.h"
#include "peri_act.h"
#include "replay.h"
#include "system.h"

typedef struct {
    short axis[6];
    int prev_on;
    int btn[5];
    int press2;
} PadButtons;

typedef struct {
    int id;
    int connected;
    PadButtons player;
    PadButtons menu;
    PadButtons raw;
    PadOwner *owners;
    char t1[32];
    char t2[32];
} PadData;

typedef struct {
    int w[25];
} PadSnap;

typedef struct {
    PadSnap snap[2];
} PadHistory;

typedef struct {
    char pad_00[37];
    unsigned char state;
    char pad_26[62];
} PadSnapView;

typedef struct {
    int id[4];
    int assign[4][2];
    int *pad[4];
    char recs[4][196];
    int input_on[4];
    int trig_on[4];
    int flip;
    unsigned char ready;
} PadSystemReadyView;

typedef struct {
    char pad_00[880];
    int flip;
} PadSystemView;

typedef struct PadOwnerView PadOwnerView;

struct PadOwnerView {
    char pad_00[4];
    int key;
    PadOwnerView *next;
};

#ifdef VERSION_US

/* Source line for heap_block_tag. */
#define SRC_LINE(early, us) (us)

#else

#define SRC_LINE(early, us) (early)

#endif /* !VERSION_US */

typedef struct {
    unsigned char b[4];
    unsigned char rep[2 + 6];
} PadKbdReport;

typedef struct {
    unsigned char mod;
    unsigned char r1;
    unsigned char keys[6];
} PadKbdKeys;

typedef struct {
    int id[4];
    int assign[4][2];
    PadData *pad[4];
    char recs[4][196];
    int input_on[4];
} PadSystemDebugView;

extern char pad_control_vardef[], pad_control_vardefs[], pad_shot_vardef[], pad_shot_vardefs[], pad_overdrive_vardef[], pad_overdrive_vardefs[];

extern int pad_info_changed;
extern int pad_info_ids;
extern int pad_info_status;
extern int pad_info_slots;
extern int pad_info_flags;
extern unsigned int pad_status_last[];
extern char *pad_status_names[];
extern void pad_record_clear(PadButtons *);
extern int pad_listener;
extern int pad_history_pos;
extern int pad_button_masks[2];
extern Pad pad_null_data;
extern void psPadSetCallback(void (*)());
extern void soft_reset(const char *name);
extern int pad_history_rate;
extern int pad_history_wait;
extern PadHistory pad_history[];
extern PadSystem pad_system;
extern PadSnapView pad_snaps[];
extern const int pad_id_mc_ports[];

extern PadKeyState kbd_state;

extern unsigned char pad_key_chars_shift[];
extern unsigned char pad_key_chars[];
extern unsigned char kbd_keys[6];
extern char *bits_to_string(unsigned int bits, int n, const char *on, const char *off);
extern int pad_debug_window;
extern float screen_layout[];
extern char gw_varedit_root[];
extern int pad_vars_group;
extern void pad_data_init(PadData *pd, int id);
extern void pad_data_merge(PadSystemView *sys, PadData *pd);
extern void pad_decode_packet(PadData *pd, PsPadRec *raw);

/* Port 0: records status, slot and flags for pad_update to report next frame. */
/* steer(cast) */
/* 0x00140670 */
void pad_info_callback(int id, int status)
{
    if (psPadGetPort((struct PsPadRec *)id) == 0) {
        pad_info_changed = 1;
        pad_info_ids = id;
        pad_info_status = status;
        pad_info_slots = ((int (*)(int))psPadGetSlot)(id);
        pad_info_flags = ((int (*)(int))psPadGetFlags)(id);
    }
}

/* Remembers statuses 0 and 2 on change; 6 (ready) sets locked analog mode, stops vibration. */
/* 0x001406e8 */
void pad_status_callback(int id)
{
    DEBUG_PRINTF("PS2 Peripheral StatusCallback [%08x] %d\n", ((unsigned int *)&pad_info_ids)[id], ((unsigned int *)&pad_info_status)[id]); /* steer(cast) */
    if (((unsigned int *)&pad_info_status)[id] < 9) { /* steer(cast) */
        DEBUG_PRINTF("    port   = %d\n", id);
        DEBUG_PRINTF("    slot   = %d\n", ((unsigned int *)&pad_info_slots)[id]); /* steer(cast) */
        DEBUG_PRINTF("    flag   = %d\n", ((unsigned int *)&pad_info_flags)[id]); /* steer(cast) */
        DEBUG_PRINTF("    status = %s\n", pad_status_names[((unsigned int *)&pad_info_status)[id]]); /* steer(cast) */
    }
    if (pad_status_last[id] != ((unsigned int *)&pad_info_status)[id]) { /* steer(cast) */
        switch (((unsigned int *)&pad_info_status)[id]) { /* steer(cast) */
#ifdef VERSION_US
        case 0:
        case 2:
            pad_status_last[id] = ((unsigned int *)&pad_info_status)[id]; /* steer(cast) */
            break;
        case 6:
            pad_status_last[id] = ((unsigned int *)&pad_info_status)[id]; /* steer(cast) */
            scePadSetMainMode(id, ((unsigned int *)&pad_info_slots)[id], 1, 3); /* steer(cast) */
            peri_act_close_all();
            break;
#else
        case 6:
            pad_status_last[id] = ((unsigned int *)&pad_info_status)[id]; /* steer(cast) */
            scePadSetMainMode(id, ((unsigned int *)&pad_info_slots)[id], 1, 3); /* steer(cast) */
            peri_act_close_all();
            break;
        case 0:
        case 2:
        case 7:
            pad_status_last[id] = ((unsigned int *)&pad_info_status)[id]; /* steer(cast) */
            break;
#endif
#ifdef VERSION_US
        case 7:
#endif
        case 1:
        case 3:
        case 4:
        case 5:
        default:
            break;
        }
    }
}

/* 0x00140878 */
void pad_data_init(PadData *pd, int id)
{
    char *tbl1;
    char *tbl2;
    int i;
    PadButtons *rec8;
    PadButtons *rec48;
    PadButtons *rec88;

    tbl1 = pd->t1;
    tbl2 = pd->t2;
    do { /* steer(barrier) */
        i = 32;
        rec8 = &pd->player;
        rec48 = &pd->menu;
        rec88 = &pd->raw;
        do {
            *tbl1++ = 0;
            *tbl2++ = 0;
        } while (--i);
    } while (0);
    pd->id = id;
    pd->connected = 0;
    pd->owners = 0;
    pad_record_clear(rec8);
    pad_record_clear(rec48);
    pad_record_clear(rec88);
}

/* Ports start empty (pad_null_data); input on ports 1..3, triggers on all; pad 0 -> port 0. */
/* steer(cast) */
/* 0x001408f8 */
void pad_init(void)
{
    PadSystem *ps = &pad_system;
    PadKeyState *st = &kbd_state;
    int i;
    char *rec = ps->recs[0];

    backup_setup();
    nj_pad_init();
    psPadSetCallback(pad_info_callback);
    pad_listener = ((int (*)(int, void (*)()))listener_add)(0, pad_history_push);
    ps->flip = 0;
    ps->ready = 0;
    for (i = 0; i < 4U; i++)
        ps->id[i] = -1;
    pad_button_masks[0] = 4;
    pad_button_masks[1] = 2;
    for (i = 0; i < 4; i++) {
        ps->assign[i][0] = 4;
        ps->assign[i][1] = 2;
    }
    pad_data_init(&pad_null_data, -1);
    for (i = 0; i < 4U; i++)
        ps->pad[i] = &pad_null_data;
    {
        char *pd = rec;
        for (i = 0; i < 4U; i++, pd += 196)
            pad_data_init(pd, i);
    }
    ps->input_on[0] = 0;
    {
        int *flag = &ps->input_on[1];
        for (i = 1; i < 4U; i++)
            *flag++ = 1;
    }
    for (i = 0; i < 4U; i++)
        ps->trig_on[i] = 1;
    st->id = -1;
    st->owner = 0;
    st->repeat_wait = 0;
    st->idx = 0;
    pad_state_record_clear(&st->cur);
    pad_state_record_clear(&st->saved);
    pad_history_pos = 0;
    ps->ready = 1;
    if (ps->id[0] != 0) {
        ps->pad[0] = (Pad *)rec;
        ps->id[0] = 0;
        mcslot_set_port(0, pad_id_mc_ports[0]);
        ps = 0;             /* steer(dead) */
    }
}

/* 0x00140b38 */
void pad_module_init(void)
{
    soft_reset("Pad");
    pad_driver_init();
    pad_debug_menu_init();
}

/* 0x00140b60 */
void pad_listener_remove(void)
{
    listener_remove(0, pad_listener);
}

/*
 * Listener: samples both ports into the 8-deep history ring (drained by pad_update). With
 * pad_history_rate set, the ring restarts every that many calls.
 */
/* steer(cast) */
/* 0x00140b70 */
void pad_history_push(void)
{
    PadSnap *dst;
    unsigned int i;

    if (pad_history_rate > 0) {
        if (pad_history_wait-- <= 0) {
            pad_history_wait = pad_history_rate;
            pad_history_pos = 0;
        }
    }
    psPadUpdate();
    dst = pad_history[pad_history_pos].snap;
    for (i = 0; i < 2; i++, dst++)
        *dst = *((PadSnap *(*)(int))psPadGet)(pad_id_mc_ports[i]);
    if (pad_history_pos < 7)
        pad_history_pos++;
}

/* 0x00140d00 */
void pad_record_clear(PadButtons *rec)
{
    rec->prev_on = 0;
    rec->axis[0] = 0;
    rec->axis[1] = 0;
    rec->axis[2] = 0;
    rec->axis[3] = 0;
    rec->axis[4] = 0;
    rec->axis[5] = 0;
    rec->btn[0] = 0;
    rec->btn[1] = 0;
    rec->btn[2] = 0;
    rec->btn[3] = 0;
    rec->btn[4] = 0;
    rec->press2 = 0;
}

/* 0x00140d38 */
void pad_state_record_clear(PadKeyRecView *rec)
{
    int i;

    for (i = 5; i >= 0; i--)
        rec->keys[i] = 0;
    rec->key_hold = 0;
    rec->key_trig = 0;
    rec->key = 0;
    rec->mods = 0;
    rec->chr = 0;
}

/* Driver port of the pad on logical port port, or -1. */
/* 0x00140d78 */
int pad_get_type_class(int port)
{
    int type = pad_system.pad[port]->id;

    if (type < 0)
        return -1;
    return pad_id_mc_ports[type];
}

/* 0x00140db8 */
int pad_is_connected(int port)
{
    PadSystem *ps = &pad_system;

    if (ps->id[port] >= 0 && ps->pad[port] != &pad_null_data)
        return 1;
    return 0;
}

/* Usable: latest sample in state 4 or 7. */
/* 0x00140df8 */
int pad_id_is_ready(int i)
{
    PadSnapView *snap = (PadSnapView *)(i * 100 + (long)pad_snaps);

    if (snap->state != 4 && snap->state != 7)
        return 0;
    return 1;
}

/* Fixed assignment: a pad that becomes usable is reattached, a dead one -> pad_null_data. */
/* 0x00140e30 */
void pad_ports_update(void)
{
    PadSystem *ps = &pad_system;
    int i;
    int id;

    for (i = 0; i < 4; i++) {
        id = ps->id[i];
        if (id >= 0) {
            if (pad_id_is_ready(id) == 0) {
                if (ps->pad[i] != &pad_null_data) {
                    mcslot_reset_port(i, pad_id_mc_ports[id]);
                    ps->pad[i] = &pad_null_data;
                }
            } else if (ps->pad[i] == &pad_null_data) {
                mcslot_set_port(i, pad_id_mc_ports[id]);
                ps->pad[i] = (Pad *)((char *)ps + (id * 196 + 64));
            }
        }
    }
}

/* Packs usable pads onto ports in order; a pad taken is removed from later ports. */
/* 0x00140f40 */
void pad_ports_assign(void)
{
    PadSystem *ps = &pad_system;
    int port;
    int id;
    int next;
    int k;

    for (port = 0; port < 4; port++) {
        if (ps->id[port] >= 0) {
            if (pad_id_is_ready(ps->id[port]) == 0) {
                mcslot_reset_port(port, pad_id_mc_ports[ps->id[port]]);
                ps->id[port] = -1;
                ps->pad[port] = &pad_null_data;
            }
        }
    }
    port = 0;
    id = 0;
    for (; port < 4 && id < 4; port = next) {
        next = port + 1;
        do {
            if (pad_id_is_ready(id) != 0) {
                if (ps->id[port] != id) {
                    for (k = port + 1; k < 4; k++) {
                        if (ps->id[k] == id) {
                            mcslot_reset_port(k, pad_id_mc_ports[id]);
                            ps->id[k] = -1;
                            ps->pad[k] = &pad_null_data;
                        }
                    }
                    ps->id[port] = id;
                    ps->pad[port] = (Pad *)((char *)ps + (id * 196 + 64));
                    mcslot_set_port(port, pad_id_mc_ports[id]);
                }
                id++;
                break;
            }
            id++;
        } while (id < 4);
    }
}

/* Keyboard disabled. */
/* 0x00141148 */
int pad_keyboard_enabled(void)
{
    return 0;
}

/*
 * Assignment fixed once the system is ready, packed before. Disabled ports get cleared records
 * (only the game record when only triggers are off). Keyboard read last.
 */
/* 0x00141150 */
void pad_update(void)
{
    PadSystemReadyView *ps = &pad_system;
    int *flags = ps->input_on;
    char *recs = ps->recs[0];
    int *trig_on = ps->trig_on;
    int i;
    unsigned int j, k;
    int n;
    PadSnap *snap, *p, *next;
    char *pd;
    int *input_on;

    for (i = 0; i < 2; i++) {
        if ((*(int (*)[2])&pad_info_changed)[i]) { /* steer(cast) */
            (*(int (*)[2])&pad_info_changed)[i] = 0; /* steer(cast) */
            pad_status_callback(i);
        }
    }
    if (pad_history_pos > 0) {
        snap = pad_snaps;
        p = pad_history[0].snap;
        for (k = 0; k < 2; k++, p++, snap++)
            *snap = *p;
        n = --pad_history_pos;
        if (n > 0) {
            p = pad_history[0].snap;
            next = pad_history[1].snap;
            do {
                for (k = 0; k < 2; k++, p++, next++)
                    *p = *next;
            } while (--n);
        }
    }
    ps->assign[0][0] = pad_button_masks[0];
    ps->assign[0][1] = pad_button_masks[1];
    if (ps->ready)
        pad_ports_update();
    else
        pad_ports_assign();
    pd = recs;
    snap = pad_snaps;
    input_on = flags;
    for (j = 0; j < 4; j++) {
        ((void (*)(void *, void *, int))pad_decode_packet)(pd, snap, j); /* steer(cast) */
        pad_button_repeat_update(pd + 132, pd + 88);
        pad_button_repeat_update_alt(pd + 164, pd + 88);
        pad_port_update(ps, pd);
        if (*input_on == 0) {
            pad_record_clear(pd + 8);
            pad_record_clear(pd + 48);
        } else if (trig_on[j] == 0) {
            pad_record_clear(pd + 8);
        }
        pd += 196;
        snap++;
        input_on++;
    }
    if (pad_keyboard_enabled()) {
        pad_keyboard_update();
        pad_keyboard_translate();
        pad_state_restore();
    }
}

/* 0x00141520 */
void peripherals_update(void)
{
    mcslot_update_all();
    peri_act_update_all();
}

/*
 * Connected in states 4 and 7: sticks centred on 0, buttons mapped to game bits. Pressed and
 * released always updated.
 */
/* 0x00141540 */
void pad_decode_packet(PadData *pd, PsPadRec *raw)
{
    PadButtons *rec = &pd->raw;
    unsigned int prev;
    unsigned int cur;

    cur = rec->btn[0];
    rec->prev_on = cur;
    if (raw->id == 4 || raw->id == 7)
        pd->connected = 1;
    else
        pd->connected = 0;
    if (pd->connected == 1) {
        rec->axis[0] = raw->stick[2] - 128;
        rec->axis[1] = raw->stick[3] - 128;
        rec->axis[2] = raw->stick[0] - 128;
        rec->axis[3] = raw->stick[1] - 128;
        rec->btn[0] = 0;
        rec->axis[4] = 0;
        rec->axis[5] = 0;
        if (raw->held & PS2_UP)
            rec->btn[0] = PAD_UP;
        if (raw->held & PS2_DOWN)
            rec->btn[0] |= PAD_DOWN;
        if (raw->held & PS2_LEFT)
            rec->btn[0] |= PAD_LEFT;
        if (raw->held & PS2_RIGHT)
            rec->btn[0] |= PAD_RIGHT;
        if (raw->held & PS2_SELECT)
            rec->btn[0] |= PAD_Z;
        if (raw->held & PS2_START)
            rec->btn[0] |= PAD_START;
        if (raw->held & PS2_CROSS)
            rec->btn[0] |= PAD_A;
        if (raw->held & PS2_CIRCLE)
            rec->btn[0] |= PAD_B;
        if (raw->held & PS2_SQUARE)
            rec->btn[0] |= PAD_X;
        if (raw->held & PS2_TRIANGLE)
            rec->btn[0] |= PAD_Y;
        if (raw->held & PS2_L1)
            rec->btn[0] |= PAD_L1;
        if (raw->held & PS2_R1)
            rec->btn[0] |= PAD_R1;
        if (raw->held & PS2_L2)
            rec->btn[0] |= PAD_L2;
        if (raw->held & PS2_L3)
            rec->btn[0] |= PAD_L3;
        if (raw->held & PS2_R2)
            rec->btn[0] |= PAD_R2;
        if (raw->held & PS2_R3)
            cur = rec->btn[0] |= PAD_R3;
        else
            cur = rec->btn[0];
    }
    prev = rec->prev_on;
    rec->btn[1] = ~prev & cur;
    rec->btn[2] = prev & ~cur;
}

/* Fires on press, then every 8 frames after a delay of 10, until release. */
/* 0x00141798 */
void pad_button_repeat_update(char *cnt, PadButtonsView *rec)
{
    unsigned int bits = rec->trig;
    unsigned int bit;

    for (bit = 1; bit != 0; bit <<= 1, cnt++) {
        if (*cnt == 0) {
            if (rec->trig & bit)
                *cnt = 10;
            else if (rec->cur & bit) {
                *cnt = 8;
                bits |= bit;
            }
        }
        if (*cnt != 0) {
            if (rec->release & bit)
                *cnt = 0;
            else
                (*cnt)--;
        }
    }
    rec->repeat = bits;
}

/* Second press within 12 frames: dclick for that frame, dclick_on until release. */
/* 0x00141820 */
void pad_button_repeat_update_alt(char *cnt, PadButtonsView *rec)
{
    unsigned int held = 0;
    unsigned int fired = 0;
    unsigned int bit;

    for (bit = 1; bit != 0; cnt++, bit <<= 1) {
        if (*cnt == 0) {
            if (rec->trig & bit)
                *cnt = 12;
        } else if (*cnt > 0) {
            if (rec->trig & bit) {
                held |= bit;
                fired |= bit;
                *cnt = -1;
            } else {
                (*cnt)--;
            }
        } else {
            if (rec->release & bit)
                *cnt = 0;
            else
                held |= bit;
        }
    }
    rec->dclick_on = held;
    rec->dclick = fired;
}

/* 0x001418b8 */
void pad_input_enable(int port)
{
    (*(struct { char pad[0x350] ; int flags[1] ; } *)&pad_system).flags[port] = 1; /* steer(cast) */
}

/* 0x001418d8 */
void pad_input_disable(int port)
{
    (*(struct { char pad[0x350] ; int flags[1] ; } *)&pad_system).flags[port] = 0; /* steer(cast) */
}

/* 0x001418f0 */
void pad_trigger_enable(int port)
{
    pad_system.trig_on[port] = 1;
}

/* 0x00141910 */
void pad_trigger_disable(int port)
{
    pad_system.trig_on[port] = 0;
}

/* Keeps val's 0xf0 bits; +4 if a mask button is held, +0x400 if one was newly pressed. */
/* 0x00141928 */
int pad_button_combine(PadButtonsView *rec, int val, unsigned int mask)
{
    val &= 0xf0;
    if (mask & rec->cur)
        val |= 4;
    if (mask & rec->trig)
        val |= 0x400;
    return val;
}

/* sys->flip: game record swaps buttons 0x10/0x20 and inverts both sticks' vertical axes. */
/* 0x00141950 */
void pad_data_merge(PadSystemView *sys, PadData *pd)
{
    PadButtons *dst;
    PadButtons *raw;

    raw = &pd->raw;
    pd->menu = pd->raw;
    pd->player = pd->raw;
    dst = &pd->player;
    if (sys->flip) {
        dst->btn[0] &= ~0x30U;
        if (raw->btn[0] & PAD_UP)
            dst->btn[0] |= PAD_DOWN;
        if (raw->btn[0] & PAD_DOWN)
            dst->btn[0] |= PAD_UP;
        dst->axis[1] = -dst->axis[1];
        dst->axis[3] = -dst->axis[3];
    }
    dst->btn[0] = pad_button_combine(sys, dst->btn[0], raw->btn[0]);
    dst->btn[1] = pad_button_combine(sys, dst->btn[1], raw->btn[1]);
    dst->btn[2] = pad_button_combine(sys, dst->btn[2], raw->btn[2]);
    dst->btn[3] = pad_button_combine(sys, dst->btn[3], raw->btn[3]);
    dst->btn[4] = pad_button_combine(sys, dst->btn[4], raw->btn[4]);
    dst->press2 = pad_button_combine(sys, dst->press2, raw->press2);
}

/* Skipped during replay. Owned pad: empty records (owner reads via pad_port_merge_owned). */
/* 0x00141b08 */
void pad_port_update(int sys, int *data)
{
    if (replay_is_playing() == 0) {
        if (data[32] == 0) {
            pad_data_merge(sys, data);
            return;
        }
        pad_record_clear((char *)data + 8);
        pad_record_clear((char *)data + 48);
    }
}

/* 0x00141b78 */
PadOwnerView *pad_port_owner_find(PadOwnerView *owner, int key)
{
    while (owner) {
        if (owner->key == key)
            return owner;
        owner = owner->next;
    }
    return 0;
}

/* Once per key; then only owners receive the pad's input. */
/* 0x00141ba8 */
void pad_owner_add(int port, int key)
{
    PadData *pd = pad_system.pad[port];
    PadOwner *owner;

    if (pad_port_owner_find(pd->owners, key) == 0) {
        owner = heap_block_tag(game_malloc(12), "pad.c", SRC_LINE(1399, 1400));
        if (owner) {
            owner->key = key;
            owner->next = pd->owners;
            pd->owners = owner;
            owner->unk_00 = 0;
        }
    }
}

/* 0x00141c30 */
void pad_port_owner_remove(int port, int key)
{
    PadOwner **link = &(*(struct { int id[12] ; PadData *pad[12] ; } *)&pad_system).pad[port]->owners; /* steer(cast) */
    PadOwner *owner;

    if (*link != 0) {
        do {
            owner = *link;
            if (owner->key == key) {
                *link = owner->next;
                heap_free(owner, 0);
                return;
            }
            link = &owner->next;
        } while (owner->next);
    }
}

/* key 0: any owner. Skipped during replay. */
/* 0x00141c90 */
void pad_port_merge_owned(int port, int key)
{
    PadData *pd = pad_system.pad[port];

    if (replay_is_playing() == 0) {
        if (pd->owners) {
            if (key == 0 || pd->owners->key == key)
                pad_data_merge(&pad_system, pd);
        }
    }
}

/* Same key rule. */
/* 0x00141d20 */
void pad_port_clear_owned(int port, int key)
{
    PadData *pd = pad_system.pad[port];

    if (replay_is_playing() == 0) {
        if (pd->owners) {
            if (key == 0 || pd->owners->key == key) {
                pad_record_clear(&pd->player);
                pad_record_clear(&pd->menu);
            }
        }
    }
}

/* Empty while the keyboard is owned. */
/* 0x00141da8 */
void pad_state_restore(void)
{
    if (kbd_state.owner)
        pad_state_record_clear(&kbd_state.cur);
    else
        kbd_state.cur = kbd_state.saved;
}

/* 0x00141df8 */
PadOwnerView *pad_state_owner_find(PadOwnerView *owner, int key)
{
    while (owner) {
        if (owner->key == key)
            return owner;
        owner = owner->next;
    }
    return 0;
}

/* Once per key. */
/* 0x00141e28 */
void pad_state_owner_add(int key)
{
    PadKeyState *st = &kbd_state;
    PadOwner *owner;

    if (pad_state_owner_find(st->owner, key) == 0) {
        owner = heap_block_tag(game_malloc(12), "pad.c", SRC_LINE(1531, 1532));
        if (owner) {
            owner->key = key;
            owner->next = st->owner;
            st->owner = owner;
        }
    }
}

/* 0x00141ea0 */
void pad_owner_remove(int key)
{
    PadKeyState *st = &kbd_state;
    PadOwner **link = &st->owner;
    PadOwner *owner;

    if (*link != 0) {
        do {
            owner = *link;
            if (owner->key == key) {
                *link = owner->next;
                heap_free(owner, 0);
                return;
            }
            link = &owner->next;
        } while (owner->next);
    }
}

/* key 0: any owner. */
/* 0x00141ef8 */
void pad_state_restore_owned(int key)
{
    if (kbd_state.owner) {
        if (key == 0 || kbd_state.owner->key == key)
            kbd_state.cur = kbd_state.saved;
    }
}

/* Same key rule. */
/* 0x00141f58 */
void pad_state_clear_owned(int key)
{
    if (kbd_state.owner) {
        if (key == 0 || kbd_state.owner->key == key)
            pad_state_record_clear(&kbd_state.cur);
    }
}

/*
 * Into kbd_state.saved. Repeat: first after 10 frames, then every frame. Missing keyboard dropped;
 * error codes 1..3 clear the record.
 */
/* steer(cast) */
/* 0x00141f90 */
void pad_keyboard_update(void)
{
    PadKeyState *st = &kbd_state;
    unsigned char *sr = (unsigned char *)&st->saved;
    PadKbdKeys *rep;
    unsigned int old;
    int n;
    int prevn;
    int key;
    unsigned char *keys;

    if (st->id < 0)
        return;
    rep = (PadKbdKeys *)((PadKbdReport *(*)(int))nj_keyboard_report)(pad_id_mc_ports[st->id]);
    if (rep == 0) {
        st->id = -1;
        pad_state_record_clear(sr);
        return;
    }
    old = sr[3];
    n = st->idx;
    sr[6] = old;
    prevn = n;
    sr[3] = rep->mod;
    sr[4] = rep->mod & ~old;
    sr[5] = old & ~rep->mod;
    if ((unsigned int)n < 6 && rep->keys[n] != 0) {
        do {
            n++;
        } while ((unsigned int)n < 6 && rep->keys[n] != 0);
    }
    keys = rep->keys;
    if (n > 0)
        n--;
    if (n < prevn) {
        n = 0;
        if (rep->keys[n] != 0) {
            do {
                n++;
            } while ((unsigned int)n < 5 && keys[n] != 0);
        }
        st->idx = n;
    } else {
        st->idx = n;
    }
    *(unsigned short *)(sr + 8) = *(unsigned short *)&rep->keys[0];
    *(unsigned short *)(sr + 10) = *(unsigned short *)&rep->keys[2];
    *(unsigned short *)(sr + 12) = *(unsigned short *)&rep->keys[4];
    key = rep->keys[n];
    if (key == 0) {
        do { sr[0] = 0; } while (0);
        sr[2] = 0;
        sr[1] = 0;
    } else if (key < 4 && key != 0) {
        pad_state_record_clear(sr);
    } else {
        if (sr[0] == key) {
            sr[1] = 0;
            if (st->repeat_wait == 0) {
                sr[2] = key;
            } else {
                sr[2] = 0;
                st->repeat_wait--;
            }
        } else {
            sr[2] = key;
            st->repeat_wait = 10;
            sr[1] = key;
        }
        sr[0] = key;
    }
}

/*
 * Shifted when either Shift is held. No key: takes a character injected by the debug on-screen
 * keyboard.
 */
/* 0x00142170 */
void pad_keyboard_translate(void)
{
    PadKeyState *st = &kbd_state;
    unsigned char *rec = (unsigned char *)&st->saved;
    int key;

    if (rec[3] & 0x22)
        rec[7] = pad_key_chars_shift[rec[2]];
    else
        rec[7] = pad_key_chars[rec[2]];
    if (rec[7] == 0) {
        rec[7] = gw_keyboard_get_inject_ch();
        if (rec[0] == 0) {
            key = gw_keyboard_get_inject_key();
            rec[1] = key;
            rec[2] = key;
            rec[0] = key;
        }
    }
}

/* 0x00142200 */
int pad_keyboard_key_down(int key)
{
    unsigned char *p = kbd_keys;
    int n;

    for (n = 6; n != 0; n--) {
        if (*p++ == key)
            return 1;
    }
    return 0;
}

/* Exact match; a left/right pair (0x44, 0x22, 0x11) matches either side. */
/* 0x00142238 */
int pad_keyboard_modifiers_match(int mask)
{
    unsigned char mods = (*(struct { char pad[7] ; unsigned char b7 ; } *)&kbd_state).b7; /* steer(cast) */
    int match;

    if (!(!(mods & ~mask)
        && (!(mask & 0x44) || ((mods & 0x44) & mask))
        && (!(mask & 0x22) || ((mods & 0x22) & mask))
        && (!(mask & 0x11) || ((mods & 0x11) & mask))
        && (!(mask & 0x80) || ((mods & 0x80) & mask))
        && (!(mask & 0x08) || ((mods & 0x08) & mask))))
        return 0;
    match = 1;
    if (mask & 0x08)
        match = ((mods & 0x08) & mask) != 0;
    return match;
}

/* 0x001422e8 */
int pad_button_assign_get(int port, int slot)
{
    return (*(struct { char pad[16] ; int t[1][2] ; } *)&pad_system).t[port][slot]; /* steer(cast) */
}

/* Also kept in pad_button_masks, which pad_update copies to port 0 every frame. */
/* 0x00142308 */
void pad_button_assign_set(int port, int slot, int mask)
{
    (*(struct { char pad[16] ; int t[1][2] ; } *)&pad_system).t[port][slot] = mask; /* steer(cast) */
    pad_button_masks[slot] = mask;
}

/* History fill, pad 0 owner, port 0 menu/game records as button letters, keyboard. */
/* 0x00142338 */
void pad_debug_window_draw(int win)
{
    int cw;
    int ch;
    int y;
    PadData *pd;
    PadButtons *rec;
    PadKeyState *st;
    PadSystemDebugView *ps;
    const char *on;
    const char *off;
    char *str;

    gw_get_cell_size(&cw, &ch);
    st = &kbd_state;
    ps = &pad_system;
    gw_win_printf(win, 4, 0, "%1d", pad_history_pos);
    if (ps->pad[0]->owners)
        gw_win_printf(win, cw * 4 + 4, 0, "1P Lock %08x", ps->pad[0]->owners->key);
    y = ch;
    str = str_empty;
    gw_win_printf(win, 4, y, "%s", str);
    pd = ps->pad[0];
    y += ch;
    gw_win_printf(win, 4, y, "1P[%4d]", pd->id);
    rec = &pd->menu;
    if (ps->input_on[0] == 0)
        gw_win_printf(win, cw * 10 + 4, y, "Disable");
    y += ch;
    gw_win_printf(win, 4, y, "Analog       X %4d Y %4d", pd->menu.axis[0], rec->axis[1]);
    on = "lrrlduDXYZRLDUSABC";
    off = "------------------";
    y += ch;
    gw_win_printf(win, 4, y, "ON      %s",
                 bits_to_string(rec->btn[0], 18, on, off));
    y += ch;
    gw_win_printf(win, 4, y, "PRESS   %s",
                 bits_to_string(rec->btn[1], 18, on, off));
    y += ch;
    gw_win_printf(win, 4, y, "RELEASE %s",
                 bits_to_string(rec->btn[2], 18, on, off));
    y += ch;
    gw_win_printf(win, 4, y, "REPEAT  %s",
                 bits_to_string(rec->btn[3], 18, on, off));
    y += ch;
    gw_win_printf(win, 4, y, "ON2     %s",
                 bits_to_string(rec->btn[4], 18, on, off));
    y += ch;
    gw_win_printf(win, 4, y, "PRESS2  %s",
                 bits_to_string(rec->press2, 18, on, off));
    y += ch;
    pd = ps->pad[0];
    gw_win_printf(win, 4, y, "1P Player");
    rec = &pd->player;
    if (ps->input_on[0] == 0)
        gw_win_printf(win, cw * 10 + 4, y, "Disable");
    y += ch;
    gw_win_printf(win, 4, y, "Analog       X %4d Y %4d", pd->player.axis[0], rec->axis[1]);
    y += ch;
    gw_win_printf(win, 4, y, "ON      %s",
                 bits_to_string(rec->btn[0], 18, on, off));
    y += ch;
    gw_win_printf(win, 4, y, "PRESS   %s",
                 bits_to_string(rec->btn[1], 18, on, off));
    y += ch;
    gw_win_printf(win, 4, y, "RELEASE %s",
                 bits_to_string(rec->btn[2], 18, on, off));
    y += ch;
    gw_win_printf(win, 4, y, "REPEAT  %s",
                 bits_to_string(rec->btn[3], 18, on, off));
    y += ch;
    gw_win_printf(win, 4, y, "ON2     %s",
                 bits_to_string(rec->btn[4], 18, on, off));
    y += ch;
    gw_win_printf(win, 4, y, "PRESS2  %s",
                 bits_to_string(rec->press2, 18, on, off));
    y += ch;
    gw_win_printf(win, 4, y, "Kbd Port  %d", st->id);
    y += ch;
    gw_win_printf(win, 4, y, " on       %2d", ((unsigned char *)st)[18]);
    y += ch;
    gw_win_printf(win, 4, y, " press    %2d", ((unsigned char *)st)[19]);
    y += ch;
    gw_win_printf(win, 4, y, " repeat   %2d", ((unsigned char *)st)[20]);
}

/* 0x00142868 */
void pad_debug_enable(int win)
{
    pad_debug_window = win;
    menu_item_set_value(info_menu_name, "Pad", 1);
}

/* 0x00142888 */
void pad_debug_disable(void)
{
    pad_debug_window = 0;
    menu_item_set_value(info_menu_name, "Pad", 0);
}

/* 27 x 22 characters, bottom left. */
/* 0x001428a8 */
void pad_debug_window_toggle(void)
{
    int cw;
    int ch;
    int win;

    if (pad_debug_window) {
        gw_window_close(pad_debug_window);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    win = gw_window_open("Pad", 95, (int)(screen_layout[6] + 4.0f),
                     (int)(screen_layout[9] - (float)(ch * 22 + 4) - 4.0f), cw * 27 + 4,
                     ch * 22 + 4, 0);
    if (win != 0) {
        pad_debug_enable(win);
        gw_window_set_handlers(win, 0, pad_debug_window_draw, pad_debug_disable);
    }
}

/* Variable editor: control group with shot and overdrive sub-groups. */
/* 0x00142990 */
void pad_debug_menu_init(void)
{
    int n;

    menu_item_add(info_menu_name, "Pad", pad_debug_window_toggle, 0, 0);
    n = gw_varedit_add(gw_varedit_root, 0, pad_control_vardef);
    pad_vars_group = n;
    gw_varedit_add_table(gw_varedit_root, n, pad_control_vardefs);
    n = gw_varedit_add(gw_varedit_root, pad_vars_group, pad_shot_vardef);
    gw_varedit_add_table(gw_varedit_root, n, pad_shot_vardefs);
    n = gw_varedit_add(gw_varedit_root, pad_vars_group, pad_overdrive_vardef);
    gw_varedit_add_table(gw_varedit_root, n, pad_overdrive_vardefs);
}
