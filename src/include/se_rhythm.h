/* Rhythm SE manager: MIDI ports, programs, note request queue played on the beat grid. */

#ifndef REZ_SE_RHYTHM_H
#define REZ_SE_RHYTHM_H

#define SE_PORTS 4
#define SE_CHANNELS 16
#define SE_QUEUE 48

typedef enum SeState {
    SE_REQ_NOTE = 0,     /* note on at next grid boundary, off after `timer` ticks */
    SE_REQ_START = 1,    /* note on at next boundary, no auto note off */
    SE_REQ_STOP = 2,     /* note off at next boundary */
    SE_REQ_PLAYING = 3   /* SE_REQ_NOTE after its note on, counting down */
} SeState;              /* unsigned: no negative enumerator */

typedef struct SePort {
    int handle;
    int program[SE_CHANNELS];
} SePort;                                   /* 0x44 bytes */

typedef struct SeRequest {
    SeState state;
    int port;
    int ch;                                 /* < 0: free slot */
    int prg;
    int note;
    int vel;
    int grid;                               /* 0..3: beat .. 1/8 beat */
    int unk_1c;                             /* cleared by se_request_clear */
    int timer;
} SeRequest;                                /* 0x24 bytes */

typedef struct SeRhythm {
    SePort port[SE_PORTS];                  /* 0x000 */
    SeRequest req[SE_QUEUE];                /* 0x110 */
    int pending[2];                         /* 0x7d0 */
    int playing[2];                         /* 0x7d8 */
    char pad_7e0[4];
    int start_count;                        /* 0x7e4 */
    int laser_listener;                     /* 0x7e8 listener_add(21, LaserBankLoad) handle */
    int laser_pack;                         /* 0x7ec laser bank pack file read by LaserBankLoad */
    int tick;                               /* 0x7f0 wraps at 3600 */
} SeRhythm;

#endif
