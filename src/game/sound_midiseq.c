/*
 * Music sequencer: "Sound MidiSeq" plays SMF songs locked to the beat clock, with game control
 * changes (loop point, PCM cues), note pulses and the "KMD0" mute table (tracks switched on a
 * beat). SUB sequencer for sub-sequences; "SeqLoad" loads songs and banks; MIDI debug windows.
 */

#include "se_midi.h"
#include "midiseq.h"
#include "types_game.h"
#include "task.h"
#include "debug.h"
#include "gw_choicebox.h"
#include "gw_draw.h"
#include "gw_system.h"
#include "gw_varedit.h"
#include "gw_window.h"
#include "ulsnd.h"
#include "sound_midiseq.h"
#include "debugmenu.h"
#include "file.h"
#include "map_manager.h"
#include "sound_bank.h"
#include "sound_beat.h"
#include "sound_main.h"
#include "sound_midise.h"
#include "sound_pcmstream.h"
#include "system.h"

extern int midi_seq_mute_no;
extern float midiseq_volume;

extern MidiseqWork *midiseq_work;
/* "NOTE OFF", "NOTE ON ", "KEY AT  ", "CTRL CNG", ... */
extern char *midi_event_names[];

/* "SYSTEM EXCLUSIVE" */
extern char str_midiseq_sysex[];

/* "EXCLUSIVE" */
extern char str_midiseq_exclusive[];

/* "META END TRACK" */
extern char str_midiseq_meta_end[];

extern char str_midiseq_meta_other[];

/* PCM stream table */
extern void *midiseq_pcm_table;

/* number of entries */
extern int midiseq_pcm_cnt;

/* "Midiseq (E) : PcmStream ..." (no such file) */
extern char str_midiseq_pcm_error[];

extern char str_midiseq_pcm_play[];

/* note pulses (index 1..8 used here) */
extern unsigned char midiseq_pulse_levels[];

/* "Midi CtrlChange Loop : Track[%2d]\n" */
extern char str_midiseq_ctrl_loop[];

extern char str_midiseq_ctrl_unknown[];

/* "Mute Do! [%2d:%08x] %s %s\n" */
extern char str_midiseq_mute_do[];

/* "Mute Do! [%2d] is Nothing\n" */
extern char str_midiseq_mute_nothing[];

/* "Start" */
extern char str_midiseq_start[];

/* "     " */
extern char str_midiseq_start_blank[];

/* "PcmStop" */
extern char str_midiseq_pcm_stop[];

extern char str_midiseq_pcm_stop_blank[];

/* "MuteTable Update Tick %10.2f" */
extern char str_midiseq_mute_tick[];

/* " Phrase Just !\n" */
extern char str_midiseq_phrase_just[];

/* " Phrase Slipped ! Haku %6d Gap %4d\n" */
extern char str_midiseq_phrase_slipped[];

/* "All Note Off\n" */
extern char str_midiseq_all_note_off[];

/* "Midi Sequence End : %10.2f \n" */
extern char str_midiseq_end[];

extern int midiseq_bgm_handle;

/* BGM track mask (variable editor) */
extern unsigned int midi_seq_track_flag;

/* "Sound MidiSeq" */
extern TaskDesc midiseq_desc;

extern char str_midiseq_sub_set[];

/* list type: 0 BGM, 1 SUB */
extern int midiseq_list_type;

/* "Track[%2d] Tick %10.2f, TickL %10.2f" */
extern char str_midiseq_track_tick[];

/* "%2d%s %6d ch%2d %s %s\n" */
extern char str_midiseq_track_row[];

/* "*" */
extern char str_midiseq_track_on[];

extern char str_midiseq_track_off[];

extern void *midiseq_seqwin;
/* debug menu */

/* "MIDI_Sequence" */
extern char str_midiseq_seqwin[];

/* "List Type" */
extern char str_midiseq_list_type[];

/* "BGM" */
extern char str_midiseq_bgm[];

/* "SUB" */
extern char str_midiseq_sub[];

extern struct {
    int pad[6];
    float x;                    /* 0x18 */
    int pad1c[2];
    float bottom;               /* 0x24 */
} screen_layout;

/* "Channel %s" */
extern char str_midiseq_channel[];

/* "" */
extern char str_mute[];

/* "no:prg vol" */
extern char str_midiseq_ch_header[];

/* "%2d:% 3d %3d %s" */
extern char str_midiseq_ch_row[];

/* "ON" */
extern char str_midiseq_on[];

extern char str_midiseq_off[];

extern void *midiseq_chwin;

/* "MIDI_Channel" */
extern char str_midiseq_chwin[];

extern struct GwVarNode *gw_varedit_root;

extern int midi_seq_vardef;
extern int midi_seq_vardefs;
extern int midi_seq_track_flag_vardef;
extern int midi_seq_track_flag_vardefs;
extern int midi_seq_vars_group;
/* loader of the current song */
extern SeqLoad *seqload_cur;

/* running loader task */
extern SeqLoad *seqload_task;

/* loaded mid file */
extern unsigned char *seqload_mid;

extern MidiseqSong midiseq_songs[];

/* parent task */
extern void *system1_work;

/* "SeqLoad" */
extern TaskDesc seqload_desc;

extern int music_id;

extern char str_music_change[];

extern char str_midiseq_kmd0[];

static void midiseq_mute_apply(void);
static void midiseq_debug_register(void);
static void midiseq_debug_unregister(void);
static MidiseqMute *midiseq_find_kmd0(unsigned char *data, int size);

/* 0x00168908 */
static void midiseq_vol_select(void)
{
}

/* Requests mute table entry midi_seq_mute_no. */
/* 0x00168910 */
static void midiseq_mute_set_select(void)
{
    midiseq_mute_request(midi_seq_mute_no);
}

/* CC 123 on every channel (loop runs to 16, one past the last), then the driver ends every note. */
/* 0x00168920 */
static void midiseq_all_notes_off_flush(int handle)
{
    MidiEvent ev;
    int i;

    ev.unk_00 = 0;
    ev.data1 = 123;
    ev.data2 = 0;
    for (i = 0; i < 17; i++) {
        ev.status = 0xb0 + i;
        se_midi_send_event(handle, &ev);
    }
    ulsnd_handle_all_off(handle);
}

/* CC 123 on every channel, as above. */
/* 0x00168990 */
static void midiseq_all_notes_off(int handle)
{
    MidiEvent ev;
    int i;

    ev.unk_00 = 0;
    ev.data1 = 123;
    ev.data2 = 0;
    for (i = 0; i < 17; i++) {
        ev.status = 0xb0 + i;
        se_midi_send_event(handle, &ev);
    }
}

/* Pointer after it -> *end. Loops while byte > 0x80, so a lone 0x80 ends it. */
/* 0x001689f8 */
static unsigned int midi_read_varlen(unsigned char *src, unsigned char **end)
{
    unsigned int val;
    unsigned char byte;

    val = 0;
    do {
        val <<= 7;
        byte = *src++;
        val |= byte & 0x7f;
    } while (byte > 0x80);
    *end = src;
    return val;
}

/* Big-endian (SMF chunk length); pointer after it -> *end. */
/* 0x00168a30 */
static unsigned int midi_read32(unsigned char *src, unsigned char **end)
{
    unsigned int val;

    val = *src++;
    val <<= 8;
    val |= *src++;
    val <<= 8;
    val |= *src++;
    val <<= 8;
    val |= *src++;
    *end = src;
    return val;
}

/* Big-endian (SMF header field); pointer after it -> *end. */
/* 0x00168a70 */
static unsigned int midi_read16(unsigned char *src, unsigned char **end)
{
    unsigned int hi;
    unsigned int lo;

    hi = *src++;
    lo = *src++;
    *end = src;
    return lo | (hi << 8);
}

/* Stored, then sent as CC 7 scaled by the music volume (0 while muted, at most 127). */
/* 0x00168a90 */
static void midiseq_set_channel_volume(Midiseq *seq, int ch, int vol)
{
    int handle;
    MidiseqChannel *chan = &seq->ch[ch];

    chan->volume = vol;
    handle = (int)*(void **)&seq->handle; /* steer(alias) */
    if (seq->mute)
        vol = 0;
    vol = vol * midiseq_volume;
    if (vol > 127)
        vol = 127;
    se_midi_volume(handle, ch, vol);
}

/* Before scaling. */
/* 0x00168af0 */
static int midiseq_get_volume(Midiseq *seq, int ch)
{
    MidiseqChannel *chan = &seq->ch[ch];

    return chan->volume;
}

/* Both sequencers; every channel volume re-sent as 0. */
/* 0x00168b08 */
static void midiseq_mute_all(void)
{
    MidiseqWork *w = midiseq_work;
    Midiseq *seq;
    int i;

    if (w == 0)
        return;
    seq = &w->seq[0];
    seq->mute = 1;
    for (i = 0; i < 16; i++)
        midiseq_set_channel_volume(seq, i, midiseq_get_volume(seq, i));
    seq = &w->seq[1];
    seq->mute = 1;
    for (i = 0; i < 16; i++)
        midiseq_set_channel_volume(seq, i, midiseq_get_volume(seq, i));
}

/* Both sequencers; channel volumes re-sent. */
/* 0x00168bb8 */
static void midiseq_unmute_all(void)
{
    MidiseqWork *w = midiseq_work;
    Midiseq *seq;
    int i;

    if (w == 0)
        return;
    seq = &w->seq[0];
    seq->mute = 0;
    for (i = 0; i < 16; i++)
        midiseq_set_channel_volume(seq, i, midiseq_get_volume(seq, i));
    seq = &w->seq[1];
    seq->mute = 0;
    for (i = 0; i < 16; i++)
        midiseq_set_channel_volume(seq, i, midiseq_get_volume(seq, i));
}

/*
 * Under id: port volume (vol - 1) * 127 of both, channel volumes scaled by vol. Not while the
 * task is ending.
 */
/* steer(cast) */
/* 0x00168c68 */
void midiseq_set_volume(int id, float vol)
{
    MidiseqWork *w = midiseq_work;
    Midiseq *seq;
    int i;

    if (w == 0 || (MIDISEQ_TASK_FLAGS(w) & TASK_F_END))
        return;
    seq = &w->seq[0];
    ((int (*)(int, char, int))sdMidiSetVol)(seq->handle, (vol - 1.0f) * 127.0f, id);
    midiseq_volume = vol;
    for (i = 0; i < 16; i++)
        midiseq_set_channel_volume(seq, i, midiseq_get_volume(seq, i));
    seq = &w->seq[1];
    ((int (*)(int, char, int))sdMidiSetVol)(seq->handle, (vol - 1.0f) * 127.0f, id);
    for (i = 0; i < 16; i++)
        midiseq_set_channel_volume(seq, i, midiseq_get_volume(seq, i));
}

/* 16 channels: note off, volume 127, no bank or program sent (-1). */
/* 0x00168da0 */
static void midiseq_channels_reset(Midiseq *seq)
{
    MidiseqChannel *chan;
    int i;

    chan = seq->ch;
    for (i = 0; i < 16; i++, chan++) {
        chan->note_on = 0;
        chan->volume = 127;
        chan->program = -1;
        chan->bank = -1;
    }
}

/* For "MIDI_Sequence"; "" when unknown. */
/* 0x00168dd8 */
static char *midi_event_name(MidiseqEvent *ev)
{
    int type = ev->status & 0xf0;

    switch (type) {
    case 0x80:
    case 0x90:
    case 0xa0:
    case 0xb0:
    case 0xc0:
    case 0xd0:
    case 0xe0:
        return midi_event_names[(ev->status >> 4) - 8];
    case 0xf0:
        switch (ev->status) {
        case 0xf0:
            return str_midiseq_sysex;
        case 0xf7:
            return str_midiseq_exclusive;
        case 0xff:
            if (ev->meta == 0x2f)
                return str_midiseq_meta_end;
            return str_midiseq_meta_other;
        }
        break;
    }
    return str_empty;
}

/* 0x00168f10 */
static void midiseq_send_event(Midiseq *seq, MidiseqEvent *ev)
{
    se_midi_send3(seq->handle, ev->status, ev->data1, ev->data2);
}

/* Streams the song cues with control change 30. */
/* 0x00168f28 */
static void midiseq_pcm_table_set(void *table)
{
    midiseq_pcm_table = table;
    midiseq_pcm_cnt = file_list_count(table);
}

/* 0x00168f50 */
void midiseq_pcm_table_clear(void)
{
    midiseq_pcm_table = 0;
    midiseq_pcm_cnt = 0;
}

/* CC 30, when on: below 100 loads that stream ready; 100 starts it. */
/* 0x00168f68 */
static void midiseq_pcm_play(unsigned char id, int on)
{
    if (on) {
        if (id < 100) {
            if (id < midiseq_pcm_cnt)
                pcm_load(&(*(PcmEntry * *)&midiseq_pcm_table)[id], 1); /* steer(cast) */
            else
                ERROR_PRINTF(str_midiseq_pcm_error);
        } else if (id == 100) {
            pcm_start();
            DEBUG_PRINTF(str_midiseq_pcm_play);
        }
    }
}

/* 0x00168ff8 */
static unsigned int midiseq_track_read_varlen(MidiseqTrack *trk)
{
    return midi_read_varlen(trk->ptr, &trk->ptr);
}

/* F0/F7: reads the length, skips the body. */
/* 0x00169020 */
static void midiseq_track_skip_sysex(MidiseqTrack *trk)
{
    unsigned int len = midiseq_track_read_varlen(trk);

    trk->ptr += len;
}

/* Type -> ev.meta; keeps a pointer to a track name (type 3); skips the body. */
/* 0x00169050 */
static void midiseq_track_read_meta(MidiseqTrack *trk)
{
    unsigned char type;
    unsigned int len;

    type = *trk->ptr++;
    trk->ev.meta = type;
    len = midiseq_track_read_varlen(trk);
    if (type == 3)
        trk->name = (char *)trk->ptr;
    trk->ptr += len;
}

/* Two data bytes (one for program change and channel pressure); ev.ch = channel. */
/* 0x001690b0 */
static void midiseq_track_read_data(MidiseqTrack *trk)
{
    MidiseqEvent *ev = &trk->ev;

    switch (ev->status & 0xf0) {
    case 0x80:
    case 0x90:
    case 0xa0:
    case 0xb0:
    case 0xe0:
        ev->data1 = *trk->ptr++;
        ev->data2 = *trk->ptr++;
        break;
    case 0xc0:
    case 0xd0:
        ev->data1 = *trk->ptr++;
        ev->data2 = 0;
        break;
    }
    ev->ch = ev->status & 0x0f;
}

/* Delta time added to ev.time, status with running status, then the body by type. */
/* 0x00169188 */
static void midiseq_track_next_event(MidiseqTrack *trk)
{
    MidiseqEvent *ev = &trk->ev;
    unsigned int delta;
    int status;

    delta = midiseq_track_read_varlen(trk);
    status = *trk->ptr;
    if ((signed char)*trk->ptr >= 0)
        status = ev->status;
    else
        trk->ptr++;
    ev->time += delta;
    ev->status = status;
    switch (status) {
    case 0xf0:
    case 0xf7:
        midiseq_track_skip_sysex(trk);
        break;
    case 0xff:
        midiseq_track_read_meta(trk);
        break;
    default:
        midiseq_track_read_data(trk);
        break;
    }
}

/*
 * First time saves read pointer, event and position; later restores them and returns the saved
 * position.
 */
/* 0x00169278 */
static float midiseq_track_loop(MidiseqTrack *trk, float pos)
{
    if (trk->loop_ptr == 0) {
        trk->loop_ptr = trk->ptr;
        trk->loop_ev = trk->ev;
        trk->loop_pos = pos;
    } else {
        trk->ptr = trk->loop_ptr;
        trk->ev = trk->loop_ev;
        pos = trk->loop_pos;
    }
    return pos;
}

/* Clears the loop point and position, reads the first event. */
/* 0x001692d0 */
static void midiseq_track_rewind(MidiseqTrack *trk)
{
    trk->loop_ptr = 0;
    trk->ptr = trk->start;
    trk->ev.time = 0;
    if (trk->ptr)
        midiseq_track_next_event(trk);
}

/* No channel, program or bank, empty name; reads the first event. */
/* 0x001692f8 */
static void midiseq_track_init(MidiseqTrack *trk, unsigned char *data)
{
    trk->name = str_empty;
    trk->ch = -1;
    trk->program = -1;
    trk->bank = -1;
    trk->ptr = data;
    trk->loop_ptr = 0;
    trk->start = data;
    trk->ev.time = 0;
    if (data)
        midiseq_track_next_event(trk);
}

/*
 * on: track mask bit (off: no notes); 1 at track end. Notes pulse levels[1 + (note & 7)]; bank and
 * program sent lazily. CC 7 volume, 30 PCM cue, 31 loop (track 0), 121 reset; 1/10/74/120 sent.
 */
/* 0x00169338 */
static int midiseq_track_events(Midiseq *seq, MidiseqTrack *trk, unsigned int on, float pos, int *flags, int n)
{
    MidiseqEvent *ev;
    int ended;
    int send;

    ended = 0;
    if (trk->ptr == 0)
        return 1;
    if (*flags & 2)
        pos = ((float (*)(MidiseqTrack *, int, float))midiseq_track_loop)(trk, n, pos); /* steer(cast) */
    for (ev = &trk->ev; !(pos < ev->time); ev = &trk->ev) {
        trk->ch = ev->ch;
        if (ev->status == 0xff) {
            if (ev->meta == 0x2f) {
                ended = 1;
                ev->time = pos + 1.0f;
            } else {
                midiseq_track_next_event(trk);
            }
            continue;
        }
        send = 1;
        switch (ev->status & 0xf0) {
        case 0xb0:
            switch (ev->data1) {
            case 1:
            case 10:
            case 74:
            case 120:
                break;
            case 7:
                midiseq_set_channel_volume(seq, ev->ch, ev->data2);
                send = 0;
                break;
            case 30:
                midiseq_pcm_play(ev->data2, on);
                send = 0;
                break;
            case 31:
                if (n == 0) {
                    pos = ((float (*)(MidiseqTrack *, int, float))midiseq_track_loop)(trk, 0, pos); /* steer(cast) */
                    *flags |= 2;
                } else {
                    ERROR_PRINTF(str_midiseq_ctrl_loop, n);
                }
                send = 0;
                break;
            case 32:
                trk->bank = ev->data2;
                send = 0;
                break;
            case 121:
                midiseq_send_event(seq, ev);
                send = 0;
                midiseq_set_channel_volume(seq, ev->ch, midiseq_get_volume(seq, ev->ch));
                break;
            default:
                DEBUG_PRINTF(str_midiseq_ctrl_unknown, ev->data1, ev->data2, trk->ch);
                send = 0;
                break;
            }
            break;
        case 0xc0:
            trk->program = ev->data1;
            send = 0;
            break;
        case 0x80:
            if (!on)
                send = 0;
            seq->ch[ev->ch].note_on = 0;
            break;
        case 0x90:
            if (ev->data2 == 0) {
                seq->ch[ev->ch].note_on = 0;
            } else if (!on) {
                send = 0;
                seq->ch[ev->ch].note_on = 0;
            } else {
                seq->ch[ev->ch].note_on = send;
                midiseq_pulse_levels[(ev->data1 & 7) + 1] = 0xff;
            }
            break;
        }
        if (on) {
            if (seq->ch[trk->ch].bank != trk->bank && trk->bank >= 0) {
                se_midi_bank_select(seq->handle, trk->ch, trk->bank);
                seq->ch[trk->ch].bank = trk->bank;
            }
            if (seq->ch[trk->ch].program != trk->program && trk->program >= 0) {
                se_midi_program_change(seq->handle, trk->ch, trk->program);
                seq->ch[trk->ch].program = trk->program;
            }
        }
        if (send)
            midiseq_send_event(seq, ev);
        midiseq_track_next_event(trk);
    }
    return ended;
}

/* Position -480 (one beat before the start), no loop point, tracks rewound. */
/* 0x00169730 */
static void midiseq_restart(Midiseq *seq)
{
    int i;

    seq->frac = 0;
    seq->loop_pos = seq->base = -480.0f;
    for (i = 0; i < 32; i++)
        midiseq_track_rewind(&seq->track[i]);
}

/* No song, tracks, mask, flags, mute table or applied entry; position -480; channels reset. */
/* 0x00169788 */
static void midiseq_reset(Midiseq *seq)
{
    int i;

    seq->mute = 0;
    seq->loop_pos = seq->base = -480.0f;
    seq->frac = 0;
    seq->ntracks = 0;
    seq->mask = 0;
    seq->new_mask = 0;
    seq->sub_switch = 0;
    seq->flags = 0;
    for (i = 0; i < 32; i++)
        midiseq_track_init(&seq->track[i], 0);
    seq->mute_table = 0;
    seq->mute_count = 0;
    seq->data = 0;
    seq->mute_cur = -1;
    midiseq_channels_reset(seq);
}

/*
 * On change: its mask applies next beat; "Start" restarts (or jumps to track 0's loop point); notes
 * end next beat; "PcmStop" stops PCM. Past the table: dropped; no table: all tracks play.
 */
/* 0x00169820 */
void midiseq_mute_update(Midiseq *seq)
{
    MidiseqMute *ent;
    int req;
    char *start_str;
    char *fmt;
    int n;

    if (seq->mute_count == 0) {
        seq->new_mask = 0xffffffff;
        return;
    }
    if (seq->mute_cur == seq->mute_req)
        return;
    req = seq->mute_req;
    if (req >= seq->mute_count)
        goto err;
    seq->mute_cur = req;
    ent = &seq->mute_table[req];
    seq->new_mask = ent->mask;
    fmt = str_midiseq_mute_do;
    n = req - 1;
    if (!ent->restart)
        goto norestart;
    start_str = str_midiseq_start;
    goto have_a;
err:
    DEBUG_PRINTF(str_midiseq_mute_nothing, req - 1);
    seq->mute_req = seq->mute_cur;
    return;
norestart:
    start_str = str_midiseq_start_blank;
have_a:
    DEBUG_PRINTF(fmt, n, ent->mask, start_str, ent->pcm_stop ? str_midiseq_pcm_stop : str_midiseq_pcm_stop_blank);
    if (ent->restart) {
        if (seq->track[0].loop_ptr == 0)
            midiseq_restart(seq);
        else
            seq->flags |= 2;
    }
    seq->sub_switch = 1;
    if (ent->pcm_stop)
        pcm_stop();
}

/*
 * 480 ticks/beat, snapped on each beat, where a new mask applies ("Phrase Just" on a bar's first
 * beat, else "Phrase Slipped"); between beats += 480 / frames per beat. 1 when all 32 ended.
 */
/* 0x00169948 */
static int midiseq_update(Midiseq *seq)
{
    int flags;
    int ended;
    int haku;
    int i;
    unsigned int bit;
    MidiseqTrack *trk;
    float pos;

    if (seq->data == 0)
        return 0;
    midiseq_mute_update(seq);
    if (beat_grid_hit(0)) {
        flags = seq->flags;
        seq->base += 480.0f;
        seq->frac = 0;
        seq->flags = 0;
        if (seq->mask != seq->new_mask) {
            seq->mask = seq->new_mask;
            DEBUG_PRINTF(str_midiseq_mute_tick, seq->base);
            if (beat_grid_hit(0) && (beat_clock_get_haku() & 3) == 0) {
                DEBUG_PRINT(str_midiseq_phrase_just);
            } else {
                haku = beat_clock_get_haku();
                DEBUG_PRINTF(str_midiseq_phrase_slipped, haku, haku % 4);
            }
        }
        if (seq->sub_switch) {
            seq->sub_switch = 0;
            midiseq_all_notes_off(seq->handle);
            midiseq_channels_reset(seq);
            DEBUG_PRINT(str_midiseq_all_note_off);
        }
    } else {
        seq->frac += 480.0f / beat_clock_get_interval();
        flags = 0;
    }
    ended = 0;
    bit = 1;
    pos = seq->base + seq->frac;
    for (i = 0; i < 32; i++, bit <<= 1)
        ended += midiseq_track_events(seq, &seq->track[i], seq->mask & bit, pos, &flags, i);
    if (ended >= 32)
        flags |= 1;
    if (flags & 2) {
        if (seq->loop_pos < 0.0f)
            seq->loop_pos = seq->base + seq->frac;
        else
            seq->base = seq->loop_pos;
    }
    if (flags & 1) {
        DEBUG_PRINTF(str_midiseq_end, seq->base);
        seq->loop_pos = seq->base = -480.0f;
        return 1;
    }
    return 0;
}

/* Track count from the header (time division ignored); the first (conductor) track skipped. */
/* 0x00169ba8 */
static void midiseq_load_smf(Midiseq *seq, unsigned char *data)
{
    unsigned char *buf;
    unsigned short n;
    unsigned int len;
    MidiseqTrack *trk;

    seq->data = data;
    buf = data + 4;
    midi_read32(buf, &buf);
    midi_read16(buf, &buf);
    n = midi_read16(buf, &buf) - 1;
    midi_read16(buf, &buf);
    seq->ntracks = n;
    if (n != 0) {
        buf += 4;
        len = midi_read32(buf, &buf);
        buf += len;
        trk = seq->track;
        do {
            buf += 4;
            len = midi_read32(buf, &buf);
            midiseq_track_init(trk, buf);
            trk++;
            buf += len;
        } while (--n != 0);
    }
}

/* Debug windows; a MIDI port each for BGM and SUB, both cleared; pulses cleared; state 0 (stop). */
/* 0x00169c98 */
static void midiseq_task_init(MidiseqWork *w)
{
    unsigned char *pulse;
    int i;

    midiseq_work = w;
    midiseq_debug_register();
    se_midi_open(&w->seq[0].handle);
    se_midi_open(&w->seq[1].handle);
    midiseq_reset(&w->seq[0]);
    midiseq_reset(&w->seq[1]);
    midiseq_bgm_handle = w->seq[0].handle;
    midiseq_unmute_all();
    w->state = 0;
    pulse = midiseq_pulse_levels;
    for (i = 9, pulse += 9; i >= 0; i--)
        *pulse-- = 0;
}

/* 0x00169d30 */
static void midiseq_task_release(MidiseqWork *w)
{
    midiseq_debug_unregister();
    se_midi_close(w->seq[1].handle);
    se_midi_close(w->seq[0].handle);
    midiseq_bgm_handle = 0;
    midiseq_work = 0;
}

/*
 * Pulses x 0.8 a frame; BGM with mask midi_seq_track_flag in state 1 (0 rewinds and stops); SUB,
 * cleared when it ends.
 */
/* 0x00169d78 */
static void midiseq_task_exec(MidiseqWork *w)
{
    int i;

    for (i = 0; i < 10; i++)
        midiseq_pulse_levels[i] = midiseq_pulse_levels[i] * 0.8f;
    w->seq[0].new_mask = midi_seq_track_flag;
    if (w->state != 1) {
        if (w->state == 0) {
            midiseq_restart(&w->seq[0]);
            w->state = 2;
        }
    } else {
        midiseq_update(&w->seq[0]);
    }
    midi_seq_track_flag = w->seq[0].new_mask;
    if (midiseq_update(&w->seq[1]))
        midiseq_reset(&w->seq[1]);
}

/* 0x00169e60 */
static void midiseq_task_disp(MidiseqWork *w)
{
}

/* 0x00169e68 */
void *midiseq_task_create(void *parent)
{
    return task_create(parent, &midiseq_desc, sizeof(MidiseqWork));
}

/* Unless playing: restarts the beat clock with it, unmutes. */
/* 0x00169e90 */
void midiseq_play(void)
{
    MidiseqWork *w = midiseq_work;

    if (w != 0 && w->state != 1) {
        beat_clock_reset();
        midiseq_unmute_all();
        w->state = 1;
    }
}

/* Mutes both sequencers. */
/* 0x00169ee8 */
static void midiseq_pause(void)
{
    MidiseqWork *w = midiseq_work;

    if (w != 0) {
        midiseq_mute_all();
        w->state = 2;
    }
}

/* Rewound next frame; notes ended. */
/* 0x00169f20 */
void midiseq_stop(void)
{
    MidiseqWork *w = midiseq_work;

    if (w != 0) {
        w->state = 0;
        midiseq_all_notes_off_flush(w->seq[0].handle);
    }
}

/* Also restarts the beat clock. */
/* 0x00169f48 */
static void midiseq_set_bgm(unsigned char *data)
{
    Midiseq *seq = &midiseq_work->seq[0];

    if (seq != 0) {
        midiseq_reset(seq);
        midiseq_load_smf(seq, data);
        beat_clock_reset();
    }
}

/* 0x00169fa8 */
static void midiseq_clear_bgm(void)
{
    Midiseq *seq = &midiseq_work->seq[0];

    if (seq != 0)
        midiseq_reset(seq);
}

/* Starts playing it, all tracks on. */
/* 0x00169fc8 */
void midiseq_set_sub(unsigned char *data)
{
    MidiseqWork *w = midiseq_work;
    Midiseq *seq;

    if (w != 0) {
        DEBUG_PRINTF(str_midiseq_sub_set, data);
        seq = &w->seq[1];
        midiseq_reset(seq);
        midiseq_load_smf(seq, data);
        midiseq_mute_update(seq);
    }
}

/* Also ends its notes. */
/* 0x0016a040 */
void midiseq_clear_sub(void)
{
    MidiseqWork *w = midiseq_work;

    if (w != 0) {
        midiseq_reset(&w->seq[1]);
        midiseq_all_notes_off_flush(w->seq[1].handle);
    }
}

/* Entries up to one with restart -1; counts them, applies the requested one. */
/* 0x0016a088 */
static void midiseq_set_mute_table(MidiseqMute *table)
{
    Midiseq *seq = &midiseq_work->seq[0];
    int n;

    if (seq != 0) {
        seq->mute_table = table;
        seq->mute_count = 0;
        if (table != 0) {
            n = 0;
            do {
                if (table->restart == -1)
                    break;
                table++;
                n++;
            } while (1);
            seq->mute_count = n;
            midiseq_mute_apply();
        }
    }
}

/*
 * With a table: applies the requested entry, its mask -> midi_seq_track_flag. Contains an empty
 * loop of mute_count iterations.
 */
/* 0x0016a0d8 */
static void midiseq_mute_apply(void)
{
    Midiseq *seq = &midiseq_work->seq[0];
    int i;

    if (seq == 0)
        return;
    i = seq->mute_count;
    if (i <= 0)
        return;
    while (--i != 0)
        ;
    midiseq_mute_update(seq);
    midi_seq_track_flag = seq->new_mask;
}

/* Kept as idx + 1, so -1 asks for "none". */
/* 0x0016a140 */
void midiseq_mute_request(int idx)
{
    MidiseqWork *w = midiseq_work;

    if (w != 0)
        w->seq[0].mute_req = ++idx;
}

/* At once; returns the previous state. */
/* 0x0016a160 */
int midiseq_track_enable(int track, int on)
{
    Midiseq *seq = &midiseq_work->seq[0];
    unsigned int bit;
    int old;

    if (seq == 0)
        return 0;
    bit = 1 << track;
    old = seq->new_mask & bit;
    if (on) {
        seq->mask |= bit;
        seq->new_mask |= bit;
        midi_seq_track_flag |= bit;
    } else {
        seq->mask &= ~bit;
        seq->new_mask &= ~bit;
        midi_seq_track_flag &= ~bit;
    }
    return old;
}

/* Of the track's channel. */
/* 0x0016a1e0 */
void midiseq_track_volume(int track, int vol)
{
    Midiseq *seq = &midiseq_work->seq[0];

    if (seq != 0)
        midiseq_set_channel_volume(seq, seq->track[track].ch, vol);
}

/* Unused; as midiseq_get_pulse_levels. */
/* 0x0016a218 */
static unsigned char *midiseq_note_pulses(void)
{
    return midiseq_pulse_levels;
}

/* A byte per note class: 255 when a note of it plays, then fades. */
/* 0x0016a228 */
unsigned char *midiseq_get_pulse_levels(void)
{
    return midiseq_pulse_levels;
}

/*
 * BGM or SUB: track count, position, loop point; per track on/off, ticks to the next event,
 * channel, event, name.
 */
/* 0x0016a238 */
static void midiseq_seqwin_draw(void *win)
{
    MidiseqWork *w = midiseq_work;
    Midiseq *seq;
    MidiseqTrack *trk;
    MidiseqEvent *ev;
    unsigned int bit;
    char *name;
    int cw;
    int ch;
    int y;
    int i;
    int delta;

    if (w == 0)
        return;
    seq = &w->seq[1];
    if (midiseq_list_type == 0)
        seq = &w->seq[0];
    bit = 1;
    gw_get_cell_size(&cw, &ch);
    trk = seq->track;
    y = ch + 6;
    gw_win_printf(win, 4, y, str_midiseq_track_tick, seq->ntracks, seq->base, seq->loop_pos);
    y += ch;
    for (i = 0; i < seq->ntracks; i++) {
        ev = &trk->ev;
        name = midi_event_name(ev);
        delta = ev->time - (seq->base + seq->frac);
        gw_win_printf(win, 4, y, str_midiseq_track_row, i + 1, (midi_seq_track_flag & bit) ? str_midiseq_track_on : str_midiseq_track_off,
                     delta, ev->ch + 1, name, trk->name);
        trk++;
        bit <<= 1;
        y += ch;
    }
}

/* 0x0016a3c8 */
static void midiseq_seqwin_open(void *w)
{
    midiseq_seqwin = w;
    menu_item_set_value(info_menu_name, str_midiseq_seqwin, 1);
}

/* 0x0016a3e8 */
static void midiseq_seqwin_close(void)
{
    midiseq_seqwin = 0;
    menu_item_set_value(info_menu_name, str_midiseq_seqwin, 0);
}

/* "List Type": BGM or SUB, for both debug windows. */
/* 0x0016a408 */
static void midiseq_seqwin_select(void *list, int sel)
{
    midiseq_list_type = sel;
}

/* Opens at the bottom left with its list. */
/* 0x0016a418 */
static void midiseq_seqwin_toggle(void)
{
    int cw;
    int ch;
    int height;
    void *w;
    void *list;

    if (midiseq_seqwin != 0) {
        gw_window_close(midiseq_seqwin);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    height = ch * 34 + ch + 6;
    w = gw_window_open(str_midiseq_seqwin, 95, screen_layout.x + 4.0f, screen_layout.bottom - height - 4.0f,
                     cw * 43, height, 0);
    if (w != 0) {
        midiseq_seqwin_open(w);
        gw_window_set_handlers(w, 0, midiseq_seqwin_draw, midiseq_seqwin_close);
        list = gw_choicebox_create(str_midiseq_list_type, 2, 2, cw * 10, ch + 2, w);
        gw_choicebox_add_item(list, str_midiseq_bgm);
        gw_choicebox_add_item(list, str_midiseq_sub);
        gw_choicebox_set_sel(list, 0);
        gw_choicebox_set_callback(list, midiseq_seqwin_select);
        midiseq_seqwin_select(list, 0);
    }
}

/* Muted or not; per channel last program, volume, note on. */
/* 0x0016a580 */
static void midiseq_chwin_draw(void *win)
{
    MidiseqWork *w = midiseq_work;
    Midiseq *seq;
    MidiseqChannel *chan;
    int cw;
    int ch;
    int y;
    int i;

    if (w == 0)
        return;
    seq = &w->seq[1];
    if (midiseq_list_type == 0)
        seq = &w->seq[0];
    gw_get_cell_size(&cw, &ch);
    chan = seq->ch;
    gw_win_printf(win, 4, 0, str_midiseq_channel, seq->mute ? str_mute : str_empty);
    y = ch;
    gw_win_printf(win, 4, y, str_midiseq_ch_header);
    y += ch;
    for (i = 0; i < 16; i++) {
        gw_win_printf(win, 4, y, str_midiseq_ch_row, i + 1, chan->program, chan->volume,
                     chan->note_on ? str_midiseq_on : str_midiseq_off);
        chan++;
        y += ch;
    }
}

/* 0x0016a6b8 */
static void midiseq_chwin_open(void *w)
{
    midiseq_chwin = w;
    menu_item_set_value(info_menu_name, str_midiseq_chwin, 1);
}

/* 0x0016a6d8 */
static void midiseq_chwin_close(void)
{
    midiseq_chwin = 0;
    menu_item_set_value(info_menu_name, str_midiseq_chwin, 0);
}

/* 0x0016a6f8 */
static void midiseq_chwin_toggle(void)
{
    int cw;
    int ch;
    int height;
    void *w;

    if (midiseq_chwin != 0) {
        gw_window_close(midiseq_chwin);
        return;
    }
    gw_get_cell_size(&cw, &ch);
    height = ch * 18;
    w = gw_window_open(str_midiseq_chwin, 95, screen_layout.x + 4.0f, screen_layout.bottom - height - 4.0f,
                     cw * 16, height, 0);
    if (w != 0) {
        midiseq_chwin_open(w);
        gw_window_set_handlers(w, 0, midiseq_chwin_draw, midiseq_chwin_close);
    }
}

/* Info menu items; variables and track mask in the sound entry. */
/* 0x0016a7d0 */
static void midiseq_debug_register(void)
{
    int parent;

    menu_item_add(info_menu_name, str_midiseq_seqwin, midiseq_seqwin_toggle, 0, 0);
    menu_item_add(info_menu_name, str_midiseq_chwin, midiseq_chwin_toggle, 0, 0);
    parent = sound_get_varedit();
    midi_seq_vars_group = gw_varedit_add(&gw_varedit_root, parent, &midi_seq_vardef);
    gw_varedit_add_table(&gw_varedit_root, midi_seq_vars_group, &midi_seq_vardefs);
    gw_varedit_add_table(&gw_varedit_root, gw_varedit_add(&gw_varedit_root, midi_seq_vars_group, &midi_seq_track_flag_vardef), &midi_seq_track_flag_vardefs);
}

/* 0x0016a898 */
static void midiseq_debug_unregister(void)
{
    int parent;

    menu_item_remove(info_menu_name, str_midiseq_seqwin);
    menu_item_remove(info_menu_name, str_midiseq_chwin);
    parent = sound_get_varedit();
    gw_varedit_remove(&gw_varedit_root, parent, midi_seq_vars_group);
}

/*
 * Loader of song id: stops the BGM, frees the previous song, ends older loaders, resets the mute
 * request.
 */
/* 0x0016a8f8 */
static void seqload_init(SeqLoad *w, int id)
{
    seqload_cur = w;
    w->id = id;
    midiseq_mute_request(-1);
    if (seqload_mid != 0) {
        midiseq_stop();
        midiseq_clear_bgm();
        file_heap_free(seqload_mid);
        seqload_mid = 0;
    }
    w->loading = 0;
    if (seqload_task != 0)
        MIDISEQ_TASK_END(seqload_task);
    midiseq_mute_request(0);
    seqload_task = w;
}

/* Cancels a running song file read. */
/* 0x0016a990 */
static void seqload_release(SeqLoad *w)
{
    MidiseqSong *song;

    if (seqload_cur == w)
        seqload_cur = 0;
    song = &midiseq_songs[w->id];
    if (seqload_task == w)
        seqload_task = 0;
    if (w->loading)
        file_request_cancel(&song->mid);
}

/*
 * Loads the .pfp and .pmp banks in the background, reads the .mid and makes it the BGM with its
 * "KMD0" mute table. A read error ends it at once.
 */
/* steer(cast) */
/* 0x0016a9e8 */
static void seqload_exec(SeqLoad *w)
{
    MidiseqSong *song = &midiseq_songs[w->id];
    MidiseqMute *table;
    int err;

    switch (w->state) {
    case 0:
        w->state = 1;
        break;
    case 1:
        w->pfp_task = sound_mpb_load_create_bpfs(&song->pfp);
        w->pmp_task = sound_mpb_load_create_bpms(&song->pmp, 3);
        w->state++;
        break;
    case 2:
        if (w->pfp_task == 0 || (MIDISEQ_TASK_FLAGS(w->pfp_task) & TASK_F_END))
            w->pfp_task = 0;
        if (w->pmp_task == 0 || (MIDISEQ_TASK_FLAGS(w->pmp_task) & TASK_F_END))
            w->pmp_task = 0;
        if (w->pfp_task == 0 && w->pmp_task == 0) {
            file_request_simple(&song->mid, 0);
            err = w->state;
            w->loading = 1;
            w->state = err + 1;
        }
        break;
    case 3:
        err = file_request_status(&song->mid);
        if (err < 0)
            MIDISEQ_TASK_FLAGS(w) |= TASK_F_END;
        if (err > 0) {
            seqload_mid = ((unsigned char *(*)(MidiseqFile *, int, int))file_load_sync)(&song->mid, 0, 0);
            w->loading = 0;
            table = midiseq_find_kmd0(seqload_mid, file_get_size(&song->mid));
            midiseq_set_bgm(seqload_mid);
            midiseq_set_mute_table(table);
            MIDISEQ_TASK_END(w);
        }
        break;
    }
}

/* Under the system task. */
/* 0x0016ab90 */
static void *seqload_create(int id)
{
    return task_create_arg(system1_work, &seqload_desc, 20, id);
}

/* Unless already current: 0 stops the BGM and frees the song, else loads that song. */
/* 0x0016abc0 */
void music_change(int id)
{
    if (id != music_id) {
        music_id = id;
        DEBUG_PRINTF(str_music_change, id);
        if (id == 0) {
            midiseq_stop();
            midiseq_clear_bgm();
            file_heap_free(seqload_mid);
            seqload_mid = 0;
        } else {
            do {
                seqload_create(id);
            } while (0);
        }
    }
}

/* 0x0016ac30 */
int seqload_idle(void)
{
    return seqload_cur == 0;
}

/* Big-endian (chunk length). */
/* 0x0016ac40 */
static unsigned int read_be32(unsigned char *src)
{
    unsigned int val;

    val = src[0] << 8;
    val += src[1];
    val <<= 8;
    val += src[2];
    val <<= 8;
    val += src[3];
    return val;
}

/* The "KMD0" chunk's mute table (data 4-byte aligned), or 0. */
/* 0x0016ac70 */
static MidiseqMute *midiseq_find_kmd0(unsigned char *data, int size)
{
    unsigned char *end = data + size;
    unsigned int len;

    do {
        len = read_be32(data + 4);
        if (fourcc_equal(data, str_midiseq_kmd0))
            return (MidiseqMute *)((unsigned int)(data + 11) & ~3);
        data += 8;
        data += len;
    } while (data < end);
    return 0;
}
