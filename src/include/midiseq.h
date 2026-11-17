/* MIDI sequencer ("Sound MidiSeq"): BGM and SUB state in midiseq_work, song table, "SeqLoad". */

#ifndef REZ_MIDISEQ_H
#define REZ_MIDISEQ_H

typedef struct MidiseqEvent {   /* 0x0c: current event of a track */
    int ch;                     /* 0x00 status & 0x0f */
    float time;                 /* 0x04 absolute position in ticks (480 per beat) */
    unsigned char status;       /* 0x08 (running status kept) */
    unsigned char data1;        /* 0x09 */
    unsigned char data2;        /* 0x0a */
    unsigned char meta;         /* 0x0b meta type of an FF event */
} MidiseqEvent;

typedef struct MidiseqTrack {   /* 0x38 */
    int ch;                     /* 0x00 channel of the last handled event */
    int program;                /* 0x04 program change (-1: none) */
    int bank;                   /* 0x08 bank LSB, CC 32 (-1: none) */
    float loop_pos;             /* 0x0c position saved at the loop marker */
    MidiseqEvent ev;            /* 0x10 */
    MidiseqEvent loop_ev;       /* 0x1c event saved at the loop marker */
    unsigned char *ptr;         /* 0x28 read pointer */
    unsigned char *loop_ptr;    /* 0x2c read pointer saved at the loop marker */
    unsigned char *start;       /* 0x30 track data (after the MTrk header) */
    char *name;                 /* 0x34 meta 03 (track name) text, "" by default */
} MidiseqTrack;

typedef struct MidiseqChannel { /* 0x10: per MIDI channel output state */
    int note_on;                /* 0x00 a note is on */
    int program;                /* 0x04 last program sent (-1) */
    int volume;                 /* 0x08 CC 7 value before scaling (127) */
    int bank;                   /* 0x0c last bank sent (-1) */
} MidiseqChannel;

typedef struct MidiseqMute {    /* 0x0c: mute-table entry; table ends with restart == -1 */
    int restart;
    int pcm_stop;
    unsigned int mask;
} MidiseqMute;

typedef struct Midiseq {        /* 0x83c */
    int handle;                 /* 0x000 MIDI port */
    int mute;                   /* 0x004 all channel volumes forced to 0 */
    MidiseqChannel ch[16];      /* 0x008 */
    MidiseqTrack track[32];     /* 0x108 */
    float base;                 /* 0x808 position at the last beat */
    float loop_pos;             /* 0x80c */
    float frac;                 /* 0x810 ticks since the last beat */
    int ntracks;                /* 0x814 */
    unsigned int mask;          /* 0x818 current track mask */
    unsigned int new_mask;      /* 0x81c pending track mask */
    int sub_switch;             /* 0x820 */
    int flags;                  /* 0x824 bit 1: loop marker */
    int mute_cur;               /* 0x828 applied mute-table index */
    int mute_req;               /* 0x82c requested mute-table index */
    MidiseqMute *mute_table;    /* 0x830 */
    int mute_count;             /* 0x834 */
    unsigned char *data;        /* 0x838 SMF data, 0 when inactive */
} Midiseq;

typedef struct MidiseqWork {    /* 0x107c: work area of the "Sound MidiSeq" task */
    Midiseq seq[2];             /* 0x000 BGM, 0x83c SUB */
    int state;                  /* 0x1078 */
} MidiseqWork;

typedef struct MidiseqFile {    /* 0x08 */
    int dev;
    char *name;
} MidiseqFile;

typedef struct MidiseqSong {    /* 0x18: entry of midiseq_songs, indexed by song id */
    MidiseqFile pfp;            /* 0x00 "s_mNN.pfp" (loaded by sound_mpb_load_create_bpfs) */
    MidiseqFile pmp;            /* 0x08 "s_mNN.pmp" (loaded by sound_mpb_load_create_bpms) */
    MidiseqFile mid;            /* 0x10 "mNN.mid" (SMF + mute table, KMD0 chunk) */
} MidiseqSong;

typedef struct SeqLoad {        /* 0x14: work area of the "SeqLoad" task */
    int id;                     /* 0x00 song id */
    int state;                  /* 0x04 */
    void *pfp_task;             /* 0x08 */
    void *pmp_task;             /* 0x0c */
    int loading;                /* 0x10 mid file read in progress */
} SeqLoad;

/* Task header flags before a work area; bit 0 ends the task. */
#define MIDISEQ_TASK_FLAGS(work) (*(int *)((char *)(work) - 0x24))
#define MIDISEQ_TASK_END(work) do { MIDISEQ_TASK_FLAGS(work) |= 1; } while (0)

#endif
