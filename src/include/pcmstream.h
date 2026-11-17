/* "Sound Pcm" ADX stream player (sound_pcmstream.c): file loader, task work. */

#ifndef REZ_PCMSTREAM_H
#define REZ_PCMSTREAM_H

/* ADXT handle. Only sj is touched: the player calls slot 5 of its function table. */
typedef struct AdxtSjVtbl {
    void (*fn[8])(void *self);
} AdxtSjVtbl;

typedef struct AdxtSj {
    AdxtSjVtbl *vtbl;
} AdxtSj;

typedef struct Adxt {
    char pad_00[16];
    AdxtSj *sj;                     /* 0x10 */
} Adxt;

/* Opens the stream file, reads its header (".adx" or "STMP" index). */
typedef struct PcmFile {
    int state;                      /* 0x00: NOP OPEN CLOSE WAIT HEADER_START HEADER_WAIT ? READ */
    char pad_04[4];
    void *buf;                      /* 0x08: 2 KB header buffer */
    void *fh;                       /* 0x0c: open file handle */
    int size;                       /* 0x10: file size in bytes */
    char pad_14[12];
    char name[1024];                /* 0x20 */
    int path;                       /* 0x420: path object passed to the file functions */
    char *name_p;                   /* 0x424: points at name */
} PcmFile;                          /* 0x428 bytes */

typedef struct PcmStream {
    void *work_buf;                 /* 0x000: ADXT work buffer (pcm_adxt_work) */
    Adxt *adxt;                     /* 0x004 */
    int adxt_stat;                  /* 0x008 */
    unsigned int mode;              /* 0x00c: WAIT .. PLAYWAIT (0-6), names in pcm_mode_names */
    PcmFile file;                   /* 0x010 */
    char pad_438[1040];
    int arg;                        /* 0x848 */
    int start;                      /* 0x84c: start playing once READY */
    int request;                    /* 0x850 */
    int stop;                       /* 0x854 */
    int pause;                      /* 0x858 */
    int frames;                     /* 0x85c */
    int time_err;                   /* 0x860 */
    int time;                       /* 0x864 */
    int index;                      /* 0x868 */
    int length;                     /* 0x86c: stream length in sectors */
    int unk_870; /* 0x870: cleared with stop/dec_err/time in pcm_mode_exec case 0; never read */
    int dec_err;                    /* 0x874 */
    int volume;                     /* 0x878: target volume */
    int cur_volume;                 /* 0x87c: volume at the start of a fade */
    int fade;                       /* 0x880: fade frames left */
    int fade_len;                   /* 0x884 */
    int unk_888; /* 0x888: cleared with stop/dec_err/time in pcm_mode_exec case 0; never read */
    int count;                      /* 0x88c: start/stop counter (mod 60) */
    int running;                    /* 0x890: set each frame the stream is not paused */
    int fade_out;                   /* 0x894: fade-out frames elapsed */
    int fade_in;                    /* 0x898: fade-in frames left */
    int fade_in_on;                 /* 0x89c: set for ".adx" streams */
} PcmStream;                        /* 0x8a0 bytes (task work size 2208) */

#endif
