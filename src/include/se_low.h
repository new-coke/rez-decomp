/* Sound low level: MIDI channel set-up entry, sound task header view. */

#ifndef REZ_SE_LOW_H
#define REZ_SE_LOW_H

/* Channel set-up table entry (se_channels_default, _port1.._port3), read by se_channel_setup. */
typedef struct SeChannelInit {
    int port;                   /* index into SeRhythm.port */
    int bank;
    int ch;
    int prg;
    int pan;                    /* always 0x40 */
} SeChannelInit;                /* 0x14 bytes */

/* 0x40-byte task header (task.h) as the sound tasks use it. */
typedef struct SndTask {
    char pad_00[4];
    struct SndTask *next;       /* 0x04: next sibling */
    struct SndTask *child;      /* 0x08: first child */
    char pad_0c[16];
    int flags;                  /* 0x1c: bit 0 stop walk, bit 1 frozen, bit 3 set on create */
    char pad_20[32];
} SndTask;                      /* 0x40 bytes; the work area follows */

#define SND_TASK(work) ((SndTask *)(work) - 1)
#define SND_WORK(task) ((task) ? (void *)((task) + 1) : (void *)0)

#endif
