/*
 * Rhythm SE: "Sound MidiSe" plays sound effects as MIDI notes on four ports (0 system/menu; 1
 * lock-on, symbol change, stage; 2 enemies, bullets, bosses; 3 laser), queued to the next step of a
 * beat grid. Also the level's laser bank, "ChangeSymbolSe", "MidiSE Box" and SE test tools.
 */

#include "se_midi.h"
#include "se_rhythm.h"
#include "se_low.h"
#include "task.h"
#include "se_stage.h"
#include "se_symbol.h"
#include "debug.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_window.h"
#include "ulsnd.h"
#include "sound_midise.h"
#include "callback.h"
#include "debugmenu.h"
#include "file.h"
#include "perform.h"
#include "player_status.h"
#include "sound_bank.h"
#include "sound_beat.h"
#include "sound_main.h"
#include "sound_midiseq.h"
#include "sound_pcmstream.h"
#include "system.h"

/* Prints a failed sound driver call. */
#define SD_ERR(name, r) \
    do { \
        if (r) \
            ERROR_PRINTF("%s (E) : %08x\n", name, r); \
    } while (0)

extern int midi_se_vardef, midi_se_vardefs;
extern int midi_se_common_vardef, midi_se_common_vardefs;
extern int midi_se_stgcommon_vardef, midi_se_stgcommon_vardefs;
extern int midi_se_enemy_vardef, midi_se_enemy_vardefs;
extern int midi_se_boss_vardef, midi_se_boss_vardefs;
extern int midi_se_seq_vardef, midi_se_seq_vardefs;

extern int midi_se_common_test_no;
extern int midi_se_stgcommon_test_prg;
extern int midi_se_stgcommon_test_se;
extern int midi_se_enemy_test_reg;
extern int midi_se_enemy_test_se;
extern int midi_se_seq_test_prg;
extern int midi_se_seq_test_se;
extern int midi_se_boss_test_prg;
extern int midi_se_boss_test_se;

extern SeRhythm *se_rhythm_mgr;
extern SeChannelInit se_channels_port3[1];
extern SeChannelInit se_channels_default[10];
extern SeChannelInit se_channels_port1[7];
extern SeChannelInit se_channels_port2[16];

extern struct GwVarNode *gw_varedit_root;
extern int midi_se_vars_group;
/* note length before the auto note off, in eighth-beat steps (17) */
extern int auto_off_count;

/* velocity of the special sounds (se_special_trigger) */
extern int vol_kasu;

/* beat grid of the special sounds */
extern int div_kasu;

/* SE output enabled */
extern int se_midi_enabled;

/* laser SE output enabled */
extern int se_laser_enabled;

extern int debug_disp_level;

/* "Sound MidiSe" */
extern TaskDesc se_rhythm_desc;

/* how much of the grid step may remain for a special sound to play at once (0.85) */
extern float grv_kasu;

/* vol_* and div_* below: velocity and beat grid per SE kind. */
extern int vol_lockon;

extern int div_lockon;

extern int vol_chgsym;

extern int div_chgsym;

extern int change_symbol_se_ids;
extern int change_symbol_se_obj;
extern int change_symbol_se_pack;
extern int vol_laser;

extern int div_laser;

extern int system_flags;

extern SeStageEntry se_common_table[];

extern int vol_enemy;

extern int div_enemy;

/* depth beyond which enemy and bullet sounds play an octave higher (150) */
extern float se_far_z;

extern int vol_bullet;

extern int div_bullet;

extern int vol_boss;

/* "ChangeSymbolSe" */
extern TaskDesc change_symbol_se_desc;

extern int midise_box_win;
extern char *se_request_state_names[];

/* "MidiSE Box" */
extern char str_midise_box[];

extern struct {
    int pad[6];
    float x;                    /* 0x18 */
    int pad1c;
    float y;                    /* 0x20 */
} screen_layout;

#ifndef VERSION_US

/* Velocity and grid of each SE timing class (grid 3 = 1/8 beat, 1 = 1/2 beat, 0 = beat). */
SeTiming se_timings[4] = {
    { 127, 3 },
    { 127, 1 },
    { 127, 1 },
    { 127, 0 },
};

/* Stage SE table of SoundRhythmSe_StgCommon*. */
const SeStageEntry se_stg_common_table[9] = {
    { 5, 7, 60, 1 },
    { 6, 4, 14, 1 },
    { 6, 4, 15, 1 },
    { 6, 4, 12, 1 },
    { 6, 4, 13, 1 },
    { 7, 2, 12, 2 },
    { 7, 2, 13, 2 },
    { 9, 3, 12, 3 },
    { 9, 3, 13, 3 },
};

#endif /* !VERSION_US */

static void se_channel_init(void);
static void se_request_clear(SeRequest *req);
static void se_direct_note_off(int se);
static void midise_box_register(void);

/* Volume 0; errors printed. */
/* steer(cast) */
/* 0x001661d0 */
void se_midi_open(int *handle)
{
    int err;

    sound_sd_err = err = sdMidiOpen(handle);
    SD_ERR("sdMidiOpen", err);
    sound_sd_err = err = ((int (*)(int, char, int))sdMidiSetVol)(*handle, 0, 0);
    SD_ERR("sdMidiSetVol", err);
}

/* 0x00166258 */
void se_midi_close(int handle)
{
    sdMidiClose(handle);
}

/* 0x00166260 */
void se_midi_send3(int handle, unsigned char status, unsigned char data1, unsigned char data2)
{
    unsigned char buf[3];

    buf[0] = status;
    buf[1] = data1;
    buf[2] = data2;
    sound_sd_err = ulsnd_handle_send_midi(handle, 3, buf);
}

/* 0x001662a0 */
void se_midi_send_event(int handle, MidiEvent *ev)
{
    int *res = &sound_sd_err;
    unsigned char buf[3];

    buf[0] = ev->status;
    buf[1] = ev->data1;
    buf[2] = ev->data2;
    *res = ulsnd_handle_send_midi(handle, 3, buf);
}

/* 0x001662f0 */
void se_midi_bank_select(int handle, char ch, char bank)
{
    MidiEvent ev;

    ev.status = 0xb0 + ch;
    ev.data1 = 0;
    ev.data2 = bank;
    se_midi_send_event(handle, &ev);
}

/* 0x00166330 */
void se_midi_program_change(int handle, char ch, char prg)
{
    MidiEvent ev;

    ev.status = 0xc0 + ch;
    ev.data1 = prg;
    ev.data2 = 0;
    se_midi_send_event(handle, &ev);
}

/* 0x00166370 */
static void se_midi_note_on(int handle, char ch, char note, char vel)
{
    MidiEvent ev;

    ev.status = 0x90 + ch;
    ev.data1 = note;
    ev.data2 = vel;
    se_midi_send_event(handle, &ev);
}

/* 0x001663b8 */
static void se_midi_note_off(int handle, char ch, char note)
{
    MidiEvent ev;

    ev.status = 0x80 + ch;
    ev.data1 = note;
    ev.data2 = 0;
    se_midi_send_event(handle, &ev);
}

/* Control change 7. */
/* 0x001663f8 */
void se_midi_volume(int handle, char ch, char vol)
{
    MidiEvent ev;

    ev.status = 0xb0 + ch;
    ev.data1 = 7;
    ev.data2 = vol;
    se_midi_send_event(handle, &ev);
}

/* Control change 10. */
/* 0x00166438 */
static void se_midi_pan(int handle, char ch, char pan)
{
    MidiEvent ev;

    ev.status = 0xb0 + ch;
    ev.data1 = 10;
    ev.data2 = pan;
    se_midi_send_event(handle, &ev);
}

/*
 * Unused. Bend range 12 semitones (RPN 0/0, data entry 12), then the bend; its data bytes are the
 * low byte and bits 8..15, not two 7-bit halves.
 */
/* 0x00166478 */
static void se_midi_pitch_bend(int handle, char ch, short bend)
{
    MidiEvent ev;

    ev.status = 0xb0 + ch;
    ev.data1 = 98;
    ev.data2 = 0;
    se_midi_send_event(handle, &ev);
    ev.data1 = 99;
    ev.data2 = 0;
    se_midi_send_event(handle, &ev);
    ev.data1 = 6;
    ev.data2 = 12;
    se_midi_send_event(handle, &ev);
    ev.status = 0xe0 + ch;
    ev.data1 = bend;
    ev.data2 = (bend & 0xff00) >> 8;
    se_midi_send_event(handle, &ev);
}

/* SE test: selected common sound. */
/* 0x00166530 */
static void midi_se_common_test_on(void)
{
    SoundRhythmSe_Common(midi_se_common_test_no);
}

/* SE test: selected stage sound, bank 1, at once. */
/* 0x00166540 */
static void midi_se_stgcommon_test_on(void)
{
    se_direct_note_on(1, midi_se_stgcommon_test_prg, midi_se_stgcommon_test_se + 12);
}

/* SE test: selected enemy sound. */
/* 0x00166560 */
static void midi_se_enemy_test_on(void)
{
    SoundRhythmSe_Enemy(midi_se_enemy_test_reg, midi_se_enemy_test_se);
}

/* SE test: selected sequence sound, bank 3, at once. */
/* 0x00166578 */
static void midi_se_seq_test_on(void)
{
    se_direct_note_on(3, midi_se_seq_test_prg, midi_se_seq_test_se);
}

/* SE test: selected boss sound, quarter-beat grid. */
/* 0x00166590 */
static void midi_se_boss_test_on(void)
{
    SoundRhythmSe_BossStart(midi_se_boss_test_prg, midi_se_boss_test_se, 2);
}

/* SE test: stops the boss sound. */
/* 0x001665a8 */
static void midi_se_boss_test_off(void)
{
    SoundRhythmSe_BossStop(midi_se_boss_test_prg, midi_se_boss_test_se, 2);
}

/* Bank, program, pan per entry's port and channel; remembers the program. */
/* 0x001665c0 */
static void se_channel_setup(SeRhythm *mgr, SeChannelInit *tbl, int n)
{
    SePort *port;
    int handle;
    char ch;
    int idx;

    if (n <= 0)
        return;
    do {
        port = &mgr->port[tbl->port];
        ch = tbl->ch;
        idx = tbl->ch;
        handle = port->handle;
        se_midi_bank_select(handle, ch, tbl->bank);
        se_midi_program_change(handle, ch, tbl->prg);
        se_midi_pan(handle, ch, tbl->pan);
        port->program[idx] = tbl->prg;
        tbl++;
    } while (--n);
}

/* Once; prints the time taken. */
/* 0x00166690 */
static void se_midi_setup(void)
{
    int start;
    float ms;

    DEBUG_PRINT("[SOUND] MidiSE Channel Init\n");
    start = perform_timer_read();
    se_channel_init();
    ms = perform_elapsed(start) / 1000.0f;
    DEBUG_PRINTF("Initial Time = % 8.3fms\n", ms);
}

/* Port 3, bank 4; after its bank loads. */
/* 0x001666f8 */
void se_channel_setup_port3(void)
{
    if (se_rhythm_mgr)
        se_channel_setup(se_rhythm_mgr, se_channels_port3, 1);
}

/* Port 0, channels 0..9, program = channel. */
/* 0x00166720 */
static void se_channel_init(void)
{
    if (se_rhythm_mgr)
        se_channel_setup(se_rhythm_mgr, se_channels_default, 10);
}

/* Seven channels, port 1, bank 1; after their bank loads. */
/* 0x00166748 */
void se_channel_setup_port1(void)
{
    if (se_rhythm_mgr)
        se_channel_setup(se_rhythm_mgr, se_channels_port1, 7);
}

/* 16 enemy/bullet channels, port 2, banks 2 and 3; after their bank loads. */
/* 0x00166770 */
void se_channel_setup_port2(void)
{
    if (se_rhythm_mgr)
        se_channel_setup(se_rhythm_mgr, se_channels_port2, 16);
}

/* Queued notes dropped; playing or stopping notes get their note off first. */
/* 0x00166798 */
static void se_request_flush(void)
{
    SeRhythm *mgr = se_rhythm_mgr;
    SeRequest *req;
    int i;

    if (mgr == 0)
        return;
    req = mgr->req;
    for (i = 0; i < SE_QUEUE; i++, req++) {
        if (req->ch < 0)
            continue;
        switch (req->state) {
        case SE_REQ_NOTE:
        case SE_REQ_START:
            se_request_clear(req);
            break;
        case SE_REQ_STOP:
        case SE_REQ_PLAYING:
            se_midi_note_off(mgr->port[req->port].handle, req->ch, req->note);
            se_request_clear(req);
            break;
        }
    }
}

/* Control change 120 on ports 1..3; the channel loop runs to 16, one past the last channel. */
/* 0x00166848 */
void se_all_sound_off(void)
{
    SeRhythm *mgr = se_rhythm_mgr;
    MidiEvent ev;
    int port;
    int ch;
    int handle;

    if (mgr == 0)
        return;
    for (port = 1; port < SE_PORTS; port++) {
        ev.unk_00 = 0;
        handle = mgr->port[port].handle;
        ev.data1 = 120;
        ev.data2 = 0;
        for (ch = 0; ch <= 16; ch++) {
            ev.status = 0xb0 + ch;
            se_midi_send_event(handle, &ev);
        }
    }
}

/*
 * Port volume (vol - 1) * 127 under id; channel volume vol * 100 on all 16. The 127 cap tests only
 * the low byte, so vol 2.0 gives 200 truncated to a byte.
 */
/* steer(cast) */
/* 0x00166900 */
void se_port0_set_volume(float vol, int id)
{
    SeRhythm *mgr = se_rhythm_mgr;
    int level;
    char cc;
    int i;
    int low;

    if (mgr == 0)
        return;
    ((int (*)(int, char, int))sdMidiSetVol)(mgr->port[0].handle, (vol - 1.0f) * 127.0f, id);
    level = vol * 100.0f;
    low = *(char *)&level;
    if (low > 127)
        level = 127;
    cc = level;
    for (i = 0; i < SE_CHANNELS; i++)
        se_midi_volume(mgr->port[0].handle, i, cc);
}

/* Ports 1..3. */
/* steer(cast) */
/* 0x001669d8 */
void se_ports_set_volume(float vol, int id)
{
    SeRhythm *mgr = se_rhythm_mgr;
    char sv;
    int level;
    int i;
    int ch;
    int low;

    if (mgr == 0)
        return;
    sv = (vol - 1.0f) * 127.0f;
    for (i = 1; i < SE_PORTS; i++)
        ((int (*)(int, char, int))sdMidiSetVol)(mgr->port[i].handle, sv, id);
    level = vol * 100.0f;
    low = *(char *)&level;
    if (low > 127)
        level = 127;
    sv = level;
    for (i = 1; i < SE_PORTS; ) {
        id = i + 1;
        for (ch = 0; ch < SE_CHANNELS; ch++)
            se_midi_volume(mgr->port[i].handle, ch, sv);
        do {
            i = id;
        } while (0);
    }
}

/* 0x00166b08 */
static void se_request_clear(SeRequest *req)
{
    req->unk_1c = 0;
    req->port = 0;
    req->ch = -1;
}

/* 0x00166b20 */
static void se_channel_reset(int *prg)
{
    *prg = 0;
}

/*
 * From the stage's laser pack: entry for the level (capped) or the nearest lower existing one,
 * DMA'd into 'BPMS' for port 3. Also listener 21.
 */
/* 0x00166b28 */
static void LaserBankLoad(void)
{
    SeRhythm *mgr = se_rhythm_mgr;
    unsigned int level;
    unsigned int num;
    int addr;
    int size;

    if (mgr && mgr->laser_pack) {
        level = player_status_get_level();
        num = PackFileGetNum(mgr->laser_pack);
        if (level >= num)
            level = num - 1;
        while ((addr = PackFileGet(mgr->laser_pack, level, &size)) == 0)
            level--;
        DEBUG_PRINTF("LaserBankLoad : 0x%08x 0x%08x Level %2d\n", addr, size, level);
        sound_dma_create_bank("LASER", addr, size, 0x42504d53, 4);
    }
}

/*
 * "MidiSE Box" menu item, four MIDI ports, empty queue, LaserBankLoad as listener 21, variables
 * and tests.
 */
/* 0x00166bf8 */
static void se_rhythm_init(SeRhythm *w)
{
    int i;

    se_rhythm_mgr = w;
    midise_box_register();
    for (i = 0; i < SE_PORTS; i++)
        se_midi_open(&w->port[i].handle);
    {
        int j;

        for (j = 0; j < SE_QUEUE; j++)
            se_request_clear(&w->req[j]);
    }
    {
        int k;

        for (k = 0; k < 2; k++) {
            w->pending[k] = 0;
            w->playing[k] = 0;
        }
    }
    {
        int port;
        int ch;

        for (port = 0; port < SE_PORTS; port++) {
            SePort *sport = &w->port[port];

            for (ch = 0; ch < SE_CHANNELS; ch++)
                se_channel_reset(&sport->program[ch]);
        }
    }
    w->laser_listener = listener_add(21, LaserBankLoad);
    midi_se_vars_group = gw_varedit_add(&gw_varedit_root, sound_get_varedit(), &midi_se_vardef);
    gw_varedit_add_table(&gw_varedit_root, midi_se_vars_group, &midi_se_vardefs);
    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, midi_se_vars_group, &midi_se_common_vardef), &midi_se_common_vardefs);
    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, midi_se_vars_group, &midi_se_stgcommon_vardef), &midi_se_stgcommon_vardefs);
    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, midi_se_vars_group, &midi_se_enemy_vardef), &midi_se_enemy_vardefs);
    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, midi_se_vars_group, &midi_se_boss_vardef), &midi_se_boss_vardefs);
    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, midi_se_vars_group, &midi_se_seq_vardef), &midi_se_seq_vardefs);
}

/* Removes the listener from list 20, not 21 where it was added. */
/* steer(cast) */
/* 0x00166e10 */
static void se_rhythm_release(SeRhythm *w)
{
    int i;
    int x;

    x = ((int (*)())sound_get_varedit)(w);
    gw_varedit_remove(&gw_varedit_root, x, midi_se_vars_group);
    listener_remove(20, w->laser_listener);
    for (i = 0; i < SE_PORTS; i++)
        se_midi_close(w->port[i].handle);
    se_rhythm_mgr = 0;
}

/*
 * Special grid step: ends playing specials, plays pending ones. Eighth beat: expires notes, then
 * plays queued requests whose grid steps now (note for auto_off_count eighths, start, or stop).
 */
/* 0x00166e98 */
static void se_rhythm_tick(SeRhythm *w)
{
    SeRequest *req;
    int *prog;
    int handle;
    int i;
    int note;

    if (++w->tick >= 3600)
        w->tick = 0;

    if (beat_grid_hit(div_kasu)) {
        int kasu_handle = w->port[1].handle;
        int j;

        for (j = 0; j < 2; j++) {
            note = j + 12;
            if (w->playing[j]) {
                w->playing[j] = 0;
                se_midi_note_off(kasu_handle, 4, note);
            }
            if (w->pending[j]) {
                w->pending[j] = 0;
                se_midi_note_on(kasu_handle, 4, note, vol_kasu);
                w->playing[j] = 1;
            }
        }
    }

    if (beat_grid_hit(3)) {
        for (i = 0; i < SE_QUEUE; i++) {
            req = &w->req[i];
            if (req->ch >= 0 && req->state == SE_REQ_PLAYING) {
                int port_handle = w->port[req->port].handle;

                if (req->timer > 0) {
                    req->timer--;
                } else {
                    se_midi_note_off(port_handle, req->ch, req->note);
                    if (debug_flags_test(2))
                        DEBUG_PRINTF("%4d[%2d]midise Note Off p%2d c%2d prg%2d note%2d Auto\n",
                                     w->tick, i, req->port, req->ch, req->prg, req->note);
                    se_request_clear(req);
                }
            }
        }
        for (i = 0; i < SE_QUEUE; i++) {
            req = &w->req[i];
            if (req->ch >= 0 && beat_grid_hit(req->grid)) {
                prog = &w->port[req->port].program[req->ch];
                handle = w->port[req->port].handle;
                if (*prog != req->prg) {
                    se_midi_program_change(handle, req->ch, req->prg);
                    *prog = req->prg;
                }
                switch (req->state) {
                case SE_REQ_NOTE:
                    se_midi_note_on(handle, req->ch, req->note, req->vel);
                    if (debug_flags_test(2))
                        DEBUG_PRINTF("%4d[%2d]midise Note On  p%2d c%2d prg%2d note%2d\n",
                                     w->tick, i, req->port, req->ch, req->prg, req->note);
                    req->state = SE_REQ_PLAYING;
                    req->timer = auto_off_count;
                    break;
                case SE_REQ_START:
                    se_midi_note_on(handle, req->ch, req->note, req->vel);
                    if (debug_flags_test(2))
                        DEBUG_PRINTF("%4d[%2d]midise Note On  p%2d c%2d prg%2d note%2d\n",
                                     w->tick, i, req->port, req->ch, req->prg, req->note);
                    se_request_clear(req);
                    break;
                case SE_REQ_STOP:
                    se_midi_note_off(handle, req->ch, req->note);
                    if (debug_flags_test(2))
                        DEBUG_PRINTF("%4d[%2d]midise Note Off p%2d c%2d prg%2d note%2d\n",
                                     w->tick, i, req->port, req->ch, req->prg, req->note);
                    se_request_clear(req);
                    break;
                case SE_REQ_PLAYING:
                    break;
                default:
                    se_request_clear(req);
                    break;
                }
            }
        }
    }
}

/* Second frame: sets up the channels, hands over to se_rhythm_tick. */
/* 0x001671e8 */
static void se_rhythm_start_tick(SeRhythm *w)
{
    if (w->start_count++ > 0) {
        se_midi_setup();
        TASK_OF(w)->exec = (void (*)(void *))se_rhythm_tick;
    }
}

/*
 * Debug flag 2: port 0/1 channel programs; debug level 0: "MidiSe Laser Disable" / "MidiSe
 * Disable" while off.
 */
/* 0x00167230 */
static void se_rhythm_disp(SeRhythm *w)
{
    int i;
    int ch;
    SePort *port;

    if (debug_flags_test(2)) {
        debug_set_origin(-10, 19);
        debug_puts("MidiSe");
        for (i = 0; i < 2; i++) {
            port = &w->port[i];
            debug_set_origin(-((i + 1) * 10), 20);
            debug_printf("Port %d", i);
            debug_newline();
            for (ch = 0; ch < SE_CHANNELS; ch++) {
                debug_printf("ch%2d:%3d", ch, port->program[ch]);
                debug_newline();
            }
        }
    }
    if (debug_disp_level == 0) {
        if (se_laser_enabled == 0) {
            debug_set_origin(25, -2);
            debug_printf("MidiSe Laser Disable");
        }
        if (se_midi_enabled == 0) {
            debug_set_origin(25, -1);
            debug_printf("MidiSe Disable");
        }
    }
}

/* 0x001673c8 */
void *se_rhythm_create(void *parent)
{
    return task_create(parent, &se_rhythm_desc, sizeof(SeRhythm));
}

/* Auto note off. Not queued twice; dropped when the queue is full. */
/* 0x001673f0 */
static void rhythm_request(int port, int ch, int prg, int note, int vel, int grid)
{
    SeRhythm *mgr = se_rhythm_mgr;
    SeRequest *base;
    SeRequest *it;
    int i;

    if (mgr == 0)
        return;
    if (port > 3) {
        ERROR_PRINTF("[E]rhythm_request() port_no is too BIG! : %d\n", port);
        return;
    }
    it = mgr->req;
    if (ch > 15) {
        ERROR_PRINTF("[E]rhythm_request() ch_no is too BIG! : %d\n", ch);
        return;
    }
    for (i = 0, base = it; i < SE_QUEUE; i++, it++) {
        if (it->ch >= 0 && it->state == SE_REQ_NOTE && it->port == port && it->ch == ch
            && it->prg == prg && it->note == note) {
            if (debug_flags_test(2))
                DEBUG_PRINTF("%4d[%2d]midise Note On  p%2d c%2d prg%2d note%2d Cancel\n",
                             mgr->tick, i, port, ch, prg, note);
            return;
        }
    }
    for (i = 0, it = base; i < SE_QUEUE; i++, it++) {
        if (it->ch < 0) {
            it->grid = grid;
            it->port = port;
            it->ch = ch;
            it->prg = prg;
            it->note = note;
            it->vel = vel;
            it->state = SE_REQ_NOTE;
            return;
        }
    }
}

/* No auto note off. The channel error message lacks its number. */
/* 0x001675a8 */
static void rhythm_request_start(int port, int ch, int prg, int note, int vel, int grid)
{
    SeRequest *base;
    SeRequest *it;
    int i;

    if (se_rhythm_mgr == 0)
        return;
    if (port > 3) {
        ERROR_PRINTF("[E]rhythm_request_start() port_no is too BIG! : %d\n", port);
        return;
    }
    it = se_rhythm_mgr->req;
    if (ch > 15) {
        ERROR_PRINTF("[E]rhythm_request_start() ch_no is too BIG! : %d\n");
        return;
    }
    for (i = 0, base = it; i < SE_QUEUE; i++, it++) {
        if (it->ch >= 0 && it->state == SE_REQ_START && it->port == port && it->ch == ch
            && it->prg == prg && it->note == note)
            return;
    }
    for (i = 0, it = base; i < SE_QUEUE; i++, it++) {
        if (it->ch < 0) {
            it->grid = grid;
            it->port = port;
            it->ch = ch;
            it->prg = prg;
            it->note = note;
            it->vel = vel;
            it->state = SE_REQ_START;
            return;
        }
    }
}

/* 0x00167690 */
static void rhythm_request_stop(int port, int ch, int note, int grid)
{
    SeRhythm *mgr = se_rhythm_mgr;
    SeRequest *base;
    SeRequest *it;
    int i;

    if (mgr == 0)
        return;
    if (port > 3) {
        ERROR_PRINTF("[E]rhythm_request_stop() port_no is too BIG! : %d\n", port);
        return;
    }
    it = mgr->req;
    if (ch > 15) {
        ERROR_PRINTF("[E]rhythm_request_stop() ch_no is too BIG! : %d\n");
        return;
    }
    for (i = 0, base = it; i < SE_QUEUE; i++, it++) {
        if (it->ch >= 0 && it->state == SE_REQ_STOP && it->port == port && it->ch == ch
            && it->note == note)
            return;
    }
    it = base;
    i = 0;
    if (it->ch < 0) {
        mgr->req[0].state = SE_REQ_STOP;
        it->port = port;
        it->ch = ch;
        it->note = note;
        it->grid = grid;
    } else {
        do {
            if (++i >= SE_QUEUE)
                return;
            it++;
        } while (it->ch >= 0);
        it->state = SE_REQ_STOP;
        it->port = port;
        it->ch = ch;
        it->note = note;
        it->grid = grid;
    }
}

/*
 * k 0..2: note k + 12, port 1 channel 4, grid div_kasu; at once within the first 15% of a step,
 * else the next step.
 */
/* 0x00167788 */
void se_special_trigger(int k)
{
    SeRhythm *mgr = se_rhythm_mgr;
    float remain;
    int handle;
    int note;
    int prg;

    if (k >= 3 || mgr == 0)
        return;
    prg = 1;
    remain = beat_grid_get_remain(div_kasu);
    note = k + 12;
    if (!beat_grid_hit(div_kasu) && grv_kasu <= remain) {
        if (se_midi_enabled == 0)
            return;
        mgr = se_rhythm_mgr;
        handle = mgr->port[1].handle;
        if (mgr->port[1].program[4] != prg) {
            se_midi_program_change(handle, 4, prg);
            mgr->port[1].program[4] = prg;
        }
        if (mgr->playing[k])
            se_midi_note_off(handle, 4, note);
        mgr->pending[k] = 0;
        se_midi_note_on(handle, 4, note, vol_kasu);
        mgr->playing[k] = 1;
    } else if (se_midi_enabled != 0) {
        mgr->pending[k] = 1;
    }
}

/* Note n + 12, port 1 channel 1; not with player flag 0x10000. */
/* 0x001678d0 */
void SoundRhythmSe_Lockon(int n)
{
    if (player_status_test_flags(0x10000))
        return;
    if (se_midi_enabled)
        rhythm_request(1, 1, 0, n + 12, vol_lockon, div_lockon);
}

/* Port 1, channel 8, program 5: n 0 starts held note 19, else plays note 19 - n. */
/* 0x00167930 */
void SoundRhythmSe_ChangeSymbol(int n)
{
    if (se_midi_enabled) {
        if (n == 0)
            rhythm_request_start(1, 8, 5, 19, vol_chgsym, div_chgsym);
        else
            rhythm_request(1, 8, 5, 19 - n, vol_chgsym, div_chgsym);
    }
}

/* Stops the note of SoundRhythmSe_ChangeSymbol(0). */
/* 0x00167998 */
void SoundRhythmSe_ChangeSymbolStop(void)
{
    if (se_midi_enabled)
        rhythm_request_stop(1, 8, 19, div_chgsym);
}

/* Stream index table, stream file, sub-sequence pack. */
/* 0x001679c8 */
void change_symbol_se_set_data(int ids, int obj, int pack)
{
    change_symbol_se_ids = ids;
    change_symbol_se_obj = obj;
    change_symbol_se_pack = pack;
}

/* 0x001679e8 */
void change_symbol_se_clear_data(void)
{
    change_symbol_se_ids = 0;
    change_symbol_se_obj = 0;
    change_symbol_se_pack = 0;
}

/* Note base + idx * 8 + 4, port 3; laser SE on and player flag 0x10000 clear. */
/* 0x00167a08 */
void SoundRhythmSe_Laser(int base, int idx)
{
    if (player_status_test_flags(0x10000))
        return;
    if (se_laser_enabled)
        rhythm_request(3, 0, 0, base + ((idx << 3) | 4), vol_laser, div_laser);
}

/* Also loads the bank for the player's level. */
/* 0x00167a80 */
void se_laser_bank_set(int pack)
{
    SeRhythm *mgr = se_rhythm_mgr;

    if (mgr) {
        mgr->laser_pack = pack;
        LaserBankLoad();
    }
}

/* 0x00167aa0 */
void se_laser_bank_clear(void)
{
    SeRhythm *mgr = se_rhythm_mgr;

    if (mgr)
        mgr->laser_pack = 0;
}

/* 0x00167ab8 */
void se_stg_common_clear(void)
{
}

/* se_stg_common_table entry on port 1, in its timing class. */
/* 0x00167ac0 */
void SoundRhythmSe_StgCommon(int id)
{
    SeStageEntry *ent;

    if (se_midi_enabled) {
        ent = &se_stg_common_table[id];
        rhythm_request(1, ent->ch, ent->prg, ent->note, se_timings[ent->timing].vel,
                       se_timings[ent->timing].grid);
    } else {
        DEBUG_PRINTF("SoundRhythmSe_StgCommon() : Cancel %d\n", id);
    }
}

/* Held note. */
/* 0x00167b20 */
void SoundRhythmSe_StgCommonStart(int id)
{
    SeStageEntry *ent;

    if (se_midi_enabled) {
        ent = &se_stg_common_table[id];
        rhythm_request_start(1, ent->ch, ent->prg, ent->note, se_timings[ent->timing].vel,
                       se_timings[ent->timing].grid);
    } else {
        DEBUG_PRINTF("SoundRhythmSe_StgCommonStart() : Cancel %d\n", id);
    }
}

/* 0x00167b80 */
void SoundRhythmSe_StgCommonStop(int id)
{
    SeStageEntry *ent;

    if (se_midi_enabled) {
        ent = &se_stg_common_table[id];
        rhythm_request_stop(1, ent->ch, ent->note, se_timings[ent->timing].grid);
    } else {
        DEBUG_PRINTF("SoundRhythmSe_StgCommonStop() : Cancel %d\n", id);
    }
}

/*
 * se_common_table entry on port 0. Paused (beat clock stopped): at once with a queued note off;
 * else queued in its timing class.
 */
/* 0x00167bd8 */
void SoundRhythmSe_Common(int id)
{
    SeRhythm *mgr = se_rhythm_mgr;
    SeStageEntry *ent;
    int ch, prg, note, vel, grid;
    int handle;

    if (mgr) {
        ent = &se_common_table[id];
        ch = ent->ch;
        prg = ent->prg;
        note = ent->note;
        vel = se_timings[ent->timing].vel;
        grid = se_timings[ent->timing].grid;
        if (system_flags & (SYS_PAUSE_KBD | SYS_PAUSED)) {
            handle = mgr->port[0].handle;
            if (se_midi_enabled) {
                se_midi_bank_select(handle, ch, 0);
                se_midi_program_change(handle, ch, prg);
                se_midi_note_on(handle, ch, note, vel);
                rhythm_request_stop(0, ch, note, grid);
            }
        } else {
            rhythm_request(0, ch, prg, note, vel, grid);
        }
    }
}

/* Unused. Held note. */
/* 0x00167d50 */
static void SoundRhythmSe_CommonStart(int id)
{
    SeStageEntry *ent;
    int ch, prg, note, vel, grid;

    if (se_rhythm_mgr) {
        ent = &se_common_table[id];
        ch = ent->ch;
        prg = ent->prg;
        note = ent->note;
        vel = se_timings[ent->timing].vel;
        grid = se_timings[ent->timing].grid;
        if (se_midi_enabled)
            rhythm_request_start(0, ch, prg, note, vel, grid);
    }
}

/* Unused. */
/* 0x00167dc0 */
static void SoundRhythmSe_CommonStop(int id)
{
    SeStageEntry *ent;
    int ch, note, grid;

    if (se_rhythm_mgr) {
        ent = &se_common_table[id];
        ch = ent->ch;
        note = ent->note;
        grid = se_timings[ent->timing].grid;
        if (se_midi_enabled)
            rhythm_request_stop(0, ch, note, grid);
    }
}

/*
 * se 0..39, id 16..31: port 2, channel/program (id - 16) / 2. From 12 and from 24 each skip an
 * octave; odd ids +64 semitones (two enemies per program).
 */
/* 0x00167e20 */
void SoundRhythmSe_Enemy(int id, unsigned int se)
{
    int note;

    id -= 16;
    if (id < 0)
        return;
    if (id >= 16) {
        ERROR_PRINTF("[E]SoundRhythmSe_Enemy() RegistID is too BIG! : %d\n", id + 16);
        return;
    }
    if (se >= 40) {
        ERROR_PRINTF("[E]SoundRhythmSe_Enemy() se is too BIG! : Reg[%d] %d\n", id + 16, se);
        return;
    }
    if (se >= 12) {
        se += 12;
        if (se >= 36)
            se += 12;
    }
    if (se_midi_enabled) {
        note = (id & 1) ? se + 76 : se + 12;
        id >>= 1;
        rhythm_request(2, id, id, note, vol_enemy, div_enemy);
    } else {
        DEBUG_PRINTF("SoundRhythmSe_Enemy() : Cancel %d\n", se);
    }
}

/* An octave higher beyond se_far_z. */
/* 0x00167ef8 */
void SoundRhythmSe_EnemyPos(int id, unsigned int se, float *pos)
{
    int note;

    id -= 16;
    if (id < 0)
        return;
    if (id >= 16) {
        ERROR_PRINTF("[E]SoundRhythmSe_EnemyPos() RegistID is too BIG! : %d\n", id + 16);
        return;
    }
    if (se >= 40) {
        ERROR_PRINTF("[E]SoundRhythmSe_EnemyPos() se is too BIG! : Reg[%d] %d\n", id + 16, se);
        return;
    }
    if (se >= 12) {
        se += 12;
        if (se >= 36)
            se += 12;
    }
    if (se_midi_enabled) {
        int hi = se + 76;
        int lo = se + 12;

        note = !(id & 1) ? lo : hi;
        if (!(pos[2] <= se_far_z))
            note += 12;
        id >>= 1;
        rhythm_request(2, id, id, note, vol_enemy, div_enemy);
    } else {
        DEBUG_PRINTF("SoundRhythmSe_EnemyPos() : Cancel %d\n", se);
    }
}

/* id 32..45: port 2, channel/program 8 + (id - 32) / 2, note se + 12, odd ids +64 semitones. */
/* 0x00167fe8 */
void SoundRhythmSe_Bullet(int id, int se)
{
    int note;

    id -= 32;
    if (id < 0)
        return;
    if (id >= 14) {
        ERROR_PRINTF("[E]SoundRhythmSe_Bullet() RegistID is too BIG! : %d\n", id + 32);
        return;
    }
    if (se_midi_enabled) {
        note = (id & 1) ? se + 76 : se + 12;
        id >>= 1;
        id += 8;
        rhythm_request(2, id, id, note, vol_bullet, div_bullet);
    } else {
        DEBUG_PRINTF("SoundRhythmSe_Bullet() : Cancel %d\n", se);
    }
}

/* An octave higher beyond se_far_z. Its range error prints SoundRhythmSe_Bullet's message. */
/* 0x00168080 */
void SoundRhythmSe_BulletPos(int id, int se, float *pos)
{
    int note;

    id -= 32;
    if (id < 0)
        return;
    if (id >= 14) {
        ERROR_PRINTF("[E]SoundRhythmSe_Bullet() RegistID is too BIG! : %d\n", id + 32);
        return;
    }
    if (se_midi_enabled) {
        int hi = se + 76;
        int lo = se + 12;

        note = !(id & 1) ? lo : hi;
        if (!(pos[2] <= se_far_z))
            note += 12;
        id >>= 1;
        rhythm_request(2, id + 8, id + 8, note, vol_bullet, div_bullet);
    } else {
        DEBUG_PRINTF("SoundRhythmSe_BulletPos() : Cancel %d\n", se);
    }
}

/* Bank 3, at once (area screen menu sounds). */
/* 0x00168138 */
void se_note_on(int prg, int note)
{
    if (se_midi_enabled)
        se_direct_note_on(3, prg, note);
}

/* Every note on port 0; se unused. */
/* 0x00168160 */
void se_note_off(int se)
{
    if (se_midi_enabled)
        se_direct_note_off(se);
}

/* Note + 12, port 2, channel/program ch, on grid `grid`. */
/* 0x00168180 */
void SoundRhythmSe_Boss(int ch, int note, int grid)
{
    if (se_midi_enabled)
        rhythm_request(2, ch, ch, note + 12, vol_boss, grid);
}

/* Held note. */
/* 0x001681b8 */
void SoundRhythmSe_BossStart(int ch, int note, int grid)
{
    if (se_midi_enabled)
        rhythm_request_start(2, ch, ch, note + 12, vol_boss, grid);
}

/* 0x001681f0 */
void SoundRhythmSe_BossStop(int ch, int note, int grid)
{
    if (se_midi_enabled)
        rhythm_request_stop(2, ch, note + 12, grid);
}

/*
 * Stream index idx of the stage's table (0 without one); sub-sequence to the MIDI sequencer; the
 * stage's stream file.
 */
/* 0x00168220 */
static void change_symbol_se_init(ChangeSymbolSe *w, int idx)
{
    int data;

    if (change_symbol_se_ids)
        w->id = (*(int * *)&change_symbol_se_ids)[idx]; /* steer(cast) */
    else
        w->id = 0;
    if (change_symbol_se_pack) {
        data = PackFileGet(change_symbol_se_pack, w->id, 0);
        if (data)
            midiseq_set_sub(data);
    }
    w->obj = change_symbol_se_obj;
}

/* Drops the sub-sequence, stops the stream. */
/* 0x001682a0 */
static void change_symbol_se_release(void)
{
    midiseq_clear_sub();
    pcm_stop();
}

/*
 * Loads the stream, starts it on a bar's first beat once ready, ends when finished; at once
 * without a stream file.
 */
/* steer(cast) */
/* 0x001682c0 */
static void change_symbol_se_exec(ChangeSymbolSe *w)
{
    if (w->obj == 0) {
        do {
            if (w)
                SYM_TASK(w)->flags |= 1;
        } while (0);
        return;
    }
    switch (w->state) {
    case 0:
        pcm_load_index(w->obj, 1, w->id);
        w->state++;
        break;
    case 1:
        if (((int (*)(int))pcm_is_ready)(w->obj))
            w->state++;
        break;
    case 2:
        if (beat_grid_hit(0) && (beat_clock_get_haku() & 3) == 0) {
            pcm_start();
            w->state++;
        }
        break;
    case 3:
        if (!((int (*)(int))pcm_is_playing)(w->obj))
            SYM_TASK(w)->flags |= 1;
        break;
    }
}

/* 0x001683c0 */
void *change_symbol_se_create(void *parent, int idx)
{
    return task_create_arg(parent, &change_symbol_se_desc, 12, idx);
}

/* Outside the beat queue: port 0 channel 15, full velocity. */
/* 0x001683e8 */
void se_direct_note_on(int bank, int prg, int note)
{
    SeRhythm *mgr = se_rhythm_mgr;

    if (mgr && se_midi_enabled) {
        se_midi_bank_select(mgr->port[0].handle, 15, bank);
        se_midi_program_change(mgr->port[0].handle, 15, prg);
        se_midi_note_on(mgr->port[0].handle, 15, note, 127);
    }
}

/* 0x00168490 */
static void se_direct_note_off(int se)
{
    SeRhythm *mgr = se_rhythm_mgr;

    if (mgr && se_midi_enabled)
        ulsnd_handle_all_off(mgr->port[0].handle);
}

/* Off silences every channel of ports 1 and 2 and empties the queue. */
/* 0x001684c0 */
void SoundMidiSeEnable(int enable)
{
    SeRhythm *mgr;
    SePort *port;
    int i;
    int ch;
    unsigned char status;

    if (!enable && se_midi_enabled) {
        mgr = se_rhythm_mgr;
        if (mgr) {
            for (i = 0; i < SE_PORTS; i++) {
                port = &mgr->port[i];
                for (ch = 0; ch < SE_CHANNELS; ch++) {
                    status = 0xb0 + ch;
                    if (i != 0 && i != 3) {
                        se_midi_send3(port->handle, status, 120, 0);
                        se_midi_send3(port->handle, status, 123, 0);
                        ulsnd_handle_all_off(port->handle);
                    }
                }
            }
            se_request_flush();
        }
        DEBUG_PRINT("SoundMidiSeEnable() : Disable\n");
    } else {
        DEBUG_PRINT("SoundMidiSeEnable() : Enable\n");
    }
    se_midi_enabled = enable;
}

/* Off silences the laser channel and empties the queue. */
/* 0x001685f0 */
void SoundMidiSeEnableLaser(int enable)
{
    SeRhythm *mgr;

    if (!enable && se_laser_enabled) {
        mgr = se_rhythm_mgr;
        if (mgr) {
            se_midi_send3(mgr->port[3].handle, 0xb0, 120, 0);
            se_midi_send3(mgr->port[3].handle, 0xb0, 123, 0);
            ulsnd_handle_all_off(mgr->port[3].handle);
        }
        se_request_flush();
        DEBUG_PRINT("SoundMidiSeEnableLaser() : Disable\n");
    } else {
        DEBUG_PRINT("SoundMidiSeEnableLaser() : Enable\n");
    }
    se_laser_enabled = enable;
}

/* 0x001686a0 */
static void midise_box_open(int win)
{
    midise_box_win = win;
    menu_item_set_value(info_menu_name, "MidiSE Box", 1);
}

/* 0x001686c0 */
static void midise_box_close(void)
{
    midise_box_win = 0;
    menu_item_set_value(info_menu_name, "MidiSE Box", 0);
}

/* 48 slots: state, port, channel, note, grid, timer. */
/* 0x001686e0 */
static void midise_box_draw(void *w)
{
    SeRhythm *mgr = se_rhythm_mgr;
    SeRequest *req;
    int cw;
    int ch;
    int y;
    int i;

    if (mgr == 0)
        return;
    gw_get_cell_size(&cw, &ch);
    req = mgr->req;
    gw_win_printf(w, 4, 0, "        po ch not d o");
    y = ch;
    for (i = 0; i < SE_QUEUE; i++, req++) {
        gw_win_printf(w, 4, y, "%2d", i);
        if (req->ch >= 0)
            gw_win_printf(w, cw * 3 + 4, y, "%s %2d %2d %3d %1d %1d", se_request_state_names[req->state],
                         req->port, req->ch, req->note, req->grid, req->timer);
        y += ch;
    }
}

/* 0x00168808 */
static void midise_box_toggle(void)
{
    int cw;
    int ch;
    void *w;

    if (midise_box_win != 0) {
        gw_window_close(midise_box_win);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    w = gw_window_open(str_midise_box, 95, screen_layout.x + 4.0f, screen_layout.y + 4.0f,
                     cw * 23, ch * 49 + 4, 0);
    if (w != 0) {
        midise_box_open(w);
        gw_window_set_handlers(w, 0, midise_box_draw, midise_box_close);
    }
}

/* 0x001688e0 */
static void midise_box_register(void)
{
    menu_item_add(info_menu_name, str_midise_box, midise_box_toggle, 0, 0);
}
