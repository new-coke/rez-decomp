/* CRI types: ADX handles, stream joints, cvFs/ROFS/DVCI/HTCI/SRD file layers, EE/IOP transfer. */

#ifndef REZ_TYPES_CRI_H
#define REZ_TYPES_CRI_H

#include "types_main.h"

/* decode request block (AdxbObj.dec) */
typedef struct {
    void *buf; /* 00 (50) */
    int len; /* 04 (54) */
    int nch; /* 08 (58) */
    int blk; /* 0c (5c) */
    int spb; /* 10 (60): samples per block */
    char *obuf; /* 14 (64) */
    int obsize; /* 18 (68) */
    int obsize2; /* 1c (6c) */
    int wpos; /* 20 (70) */
    int nfree; /* 24 (74) */
    int nleft; /* 28 (78) */
    unsigned short unk_2c; /* 2c (7c): from AdxbObj.unk_48 (ADXB_DecodeHeaderAdx) */
    unsigned short unk_2e; /* 2e (7e): from AdxbObj.unk_4a (ADXB_DecodeHeaderAdx) */
    void *state; /* 30 (80) */
    int unk_34; /* 34 (84): cleared by adxb_set_input */
} AdxbDec;

/* ADXB handle, 184 bytes; 8 at adxb_obj_tbl */
typedef struct {
    short used; /* 00 */
    short hdrdec; /* 02 */
    int stat; /* 04 */
    void *adxd; /* 08: low level decoder */
    signed char type; /* 0c */
    signed char sample_bits; /* 0d */
    signed char nch; /* 0e */
    signed char blk; /* 0f */
    int spb; /* 10 */
    int total; /* 14 */
    int total_smpl; /* 18 */
    short cutoff; /* 1c */
    char pad_1e[2];
    int loop_ins_smpl; /* 20 */
    short loop_flag; /* 24 */
    short loop_type; /* 26 */
    int loop_start_smpl; /* 28 */
    int loop_start_ofst; /* 2c */
    int loop_end_smpl; /* 30 */
    int loop_end_ofst; /* 34 */
    void *obj; /* 38 */
    char *obuf; /* 3c */
    int obsize; /* 40 */
    int obsize2; /* 44 */
    unsigned short unk_48; /* 48: to dec.unk_2c by ADXB_DecodeHeaderAdx; 0 for other formats */
    unsigned short unk_4a; /* 4a: to dec.unk_2e by ADXB_DecodeHeaderAdx; 0 for other formats */
    int dec_state; /* 4c */
    AdxbDec dec; /* 50 */
    char *(*getwr)(void *obj, int *wpos, int *nfree, int *nleft); /* 88 */
    void *getwrobj; /* 8c */
    void (*addwr)(void *obj, int a, int n); /* 90 */
    void *addwrobj; /* 94 */
    int wr_nsmpl; /* 98 */
    int wpos; /* 9c */
    int dec_nsmpl; /* a0 */
    int dec_nbyte; /* a4 */
    short format; /* a8: 0 ADX, 10 AHX, 1/2 other formats */
    short pcm_fmt; /* aa */
    void *ahx; /* ac */
    int max_dec_smpl; /* b0 */
    int max_dec_blk; /* b4 */
} AdxbObj;

/* 52 bytes; 8 at adxd_obj_tbl */
typedef struct {
    int used; /* 00 */
    int id; /* 04 */
    int unk_08; /* 08: cleared by adxd_create; set by adxd_set_unk_08 (no caller) */
    int stat; /* 0c */
    int ndec; /* 10: blocks decoded */
    int mode; /* 14: 1 mono, 2 stereo */
    void *src; /* 18 */
    int nblk; /* 1c */
    short *dstl; /* 20 */
    short *dstr; /* 24 */
    short histl[2]; /* 28 */
    short histr[2]; /* 2c */
    short coef1; /* 30 */
    short coef2; /* 32 */
} AdxdObj;

/* ---- device interface registration ---- */
typedef struct CriDevIf CriDevIf;

struct CriDevIf {
    char pad_00[12];
    int (*init)(void *arg, CriDevIf *dev); /* 0c */
    void (*finish)(void); /* 10 */
};

/* cvFs device interface: function table from a device's get-interface function. */
typedef int (*CvFsFn)();

typedef struct {
    CvFsFn exec_server;
    CvFsFn entry_err_func; /* (fn, obj) */
    CvFsFn get_file_size;
    CvFsFn get_free_size;
    CvFsFn open;
    CvFsFn close;
    CvFsFn seek;
    CvFsFn tell;
    CvFsFn req_rd;
    CvFsFn req_wr;
    CvFsFn stop_tr;
    CvFsFn get_stat;
    CvFsFn get_sct_len;
    CvFsFn set_sct_len;
    CvFsFn get_num_tr;
    CvFsFn change_dir;
    CvFsFn is_exist_file;
    CvFsFn get_num_files;
    CvFsFn load_dir_info;
    CvFsFn get_max_byte_rate;
    CvFsFn make_dir;
    CvFsFn remove_dir;
    CvFsFn delete_file;
    CvFsFn get_file_size_ex;
    CvFsFn opt_fn1;
    CvFsFn opt_fn2;
    char pad_68[8];
} CvFsDevIf;

/* cvFs device table entry (32 at cvfs_dev_tbl) */
typedef struct {
    CvFsDevIf *dev;
    char name[12];
} CvFsDev;

/* cvFs file handle: device interface + device handle (40 at cvfs_hn_tbl) */
typedef struct {
    CvFsDevIf *dev;
    void *fd;
} CvFsHn;

/*
 * opt_fn1 argument block. Volume commands: 1 set current, 2 add, 3 delete, 4 query, 5 volume
 * info, 100 "has volumes".
 */
typedef struct {
    void *img; /* 00: image file handle */
    char *name; /* 04: volume (or device) name */
    void *info; /* 08 */
    char pad_0c[8];
} CvFsOptArg;

/* DTX (EE/IOP data transfer) object: 68 bytes, 8 at dtx_obj_tbl */
typedef struct {
    signed char used; /* 00 */
    signed char busy; /* 01 */
    signed char send_on; /* 02 */
    char pad_03[1];
    int svr; /* 04: server-side handle */
    int seq;
    char *eewk; /* 0c */
    int len; /* 10: work length - 64 */
    int *ucwk; /* 14: uncached address of the work tail ([15]: IOP count) */
    char *iopwk; /* 18 */
    int wklen; /* 1c */
    void (*fn20)(); /* 20 */
    void *obj24; /* 24 */
    void (*fn28)(); /* 28 */
    void *obj2c; /* 2c */
    struct { int data; char *addr; int size; int mode; } dma;
    int dmaid; /* 40 */
} DtxObj;

/*
 * DVCI file cache directory (dvci_file_cache): nmax 8-byte {lsn, size} entries, then nmax names
 * of namelen + 1 bytes.
 */
typedef struct {
    char *buf; /* 00 */
    int nfile; /* 04 */
    int nmax; /* 08 */
    int namelen; /* 0c */
} DvciCache;

/* file entry (sceCdSearchFile result plus one word), 36 bytes */
typedef struct {
    unsigned int lsn; /* 00 */
    unsigned int size; /* 04 */
    char name[16]; /* 08 */
    unsigned char date[8]; /* 18 */
    char pad_20[4];
} DvciFile;

/* DVCI (CD/DVD-ROM cvFs device) file handle: 64 bytes, 40 at dvci_obj_tbl */
typedef struct {
    unsigned char trycount;
    unsigned char spindlctrl;
    unsigned char datapattern;
    unsigned char pad;
} DvciMode;

typedef struct {
    signed char used; /* 00 */
    signed char cached; /* 01: found in the file cache */
    signed char stat; /* 02: 0 idle, 1 complete, 2 reading, 3 error */
    char pad_03[1];
    int fsize; /* 04: bytes */
    int fsize_sct; /* 08 */
    int cursct; /* 0c */
    int rqsct; /* 10 */
    char *buf; /* 14 */
    DvciFile file; /* 18 */
    DvciMode mode; /* 3c */
} DvciHn;

/* HTCI (host cvFs device) file cache entry */
typedef struct {
    int fd;
    int size;
} HtciFile;

/* HTCI handle, 36 bytes; 13 at htci_obj_tbl */
typedef struct {
    signed char used; /* 00 */
    signed char cache; /* 01: opened from the file cache */
    signed char stat; /* 02 */
    signed char req; /* 03: request step */
    char pad_04[4];
    int st; /* 08: host status */
    int fd; /* 0c */
    int fsize; /* 10: bytes */
    int fsct; /* 14: sectors */
    int pos; /* 18: sector position */
    void *buf; /* 1c */
    int nsct; /* 20 */
} HtciObj;

/* LSC stream entry: 64 bytes */
typedef struct {
    int id; /* 00 */
    char fname[40]; /* 04 */
    void *dir; /* 2c */
    int ofst; /* 30 */
    int nsct; /* 34 */
    int stat; /* 38 */
    int rdsct; /* 3c */
} LscEnt;

/* stream entry, 64 bytes */
typedef struct {
    char s[39];
} LscFname;

typedef struct {
    int id; /* 00 */
    LscFname fname; /* 04 */
    char pad_2b[1];
    int ofst; /* 2c */
    int nsct; /* 30 */
    int range_nsct; /* 34 */
    int stat; /* 38 */
    int rdsct; /* 3c */
} LscStm;

/* command record sent to the IOP, 16 bytes */
typedef struct {
    short cmd; /* 00 */
    char pad_02[2];
    void *id; /* 04 */
    int val; /* 08 */
    int val2; /* 0c */
} Ps2rnaCmd;

typedef struct {
    int nhn; /* 00 */
    int nvol; /* 04 */
    int ndir; /* 08 */
    void *work; /* 0c */
} RofsCfg;

/* low level device interface */
typedef struct {
    int (*init)(void); /* 00 */
    void (*finish)(void); /* 04 */
    void *seek;
    void *req_rd;
    void *stop_tr;
    void (*exec_server)(void);
    void *get_stat;
    void *get_sct_len;
    void *get_num_tr;
    void *req_rd_blk;
    int (*read)(void *h, int lsn, int nsct, void *buf); /* 28 */
} RofsDev;

/* Directory record buffer: 24-byte RofsDir header (with volume name), then 48-byte entries. */
typedef struct {
    int lsn; /* 00 */
    int size; /* 04 */
    unsigned char flags; /* 08: 2 = directory */
    char pad_09[1];
    char name[38]; /* 0a */
} RofsDirEnt;

/* ROFS (CD-ROM file system) */
typedef int (*RofsFn)();

/* ROFS interface table (rofs_if) */
typedef struct {
    char pad_00[12];
    RofsFn init;
    RofsFn finish;
    RofsFn set_pfs_func_tbl;
    RofsFn add_volume;
    RofsFn del_volume;
    RofsFn set_def_volume;
    RofsFn get_def_volume;
    RofsFn open;
    RofsFn open_range;
    RofsFn exec_server;
    RofsFn get_file_siz;
    RofsFn get_volume_inf;
    RofsFn load_dir;
    RofsFn set_dir;
    RofsFn is_exist_file;
    RofsFn get_num_files;
    RofsFn get_dir_inf;
    RofsFn entry_err_func;
} RofsIf;

typedef struct {
    char c[32];
} RofsName;

/* PFS device function table used by ROFS, cvFs device interface order */
typedef struct {
    RofsFn exec_server;
    RofsFn entry_err_func;
    RofsFn get_file_size;
    RofsFn get_free_size;
    RofsFn open;
    RofsFn close;
    RofsFn seek;
    RofsFn tell;
    RofsFn req_rd;
    RofsFn req_wr;
    RofsFn stop_tr;
    RofsFn get_stat;
    RofsFn get_sct_len;
    RofsFn set_sct_len;
    RofsFn get_num_tr;
    RofsFn change_dir;
} RofsPfs;

/* ---- ROFS (CD file system) on-disc records ---- */
typedef struct {
    unsigned int v;
} __attribute__((packed)) RofsU32;

/* Volume header, unpacked from the disc; 1904 bytes, unaligned */
typedef struct {
    int id; /* 00 */
    unsigned int size; /* 04 */
    char date[7]; /* 08 */
    char pad_0f[1];
    char ver[4]; /* 10 */
    unsigned int unk_14; /* 14 */
    unsigned int magic; /* 18 */
    RofsName name1; /* 1c */
    RofsName name2; /* 3c */
    char unk_5c[4]; /* 5c */
    char unk_60; /* 60 */
    char unk_61; /* 61 */
    char pad_62[2];
    int ntbl; /* 64 */
    int idx; /* 68 */
    int nsct; /* 6c */
    int tbl[448]; /* 70 */
} __attribute__((packed)) RofsVd;

/* Zone header, unpacked; 44 bytes, unaligned */
typedef struct {
    unsigned int id; /* 00 */
    unsigned char mode[5]; /* 04 */
    char pad_09[3];
    int sct_size; /* 0c */
    int unk_10; /* 10 */
    int start[3]; /* 14 */
    int size[3]; /* 20 */
} __attribute__((packed)) RofsZone;

/* SJ chunk */
typedef struct {
    char *data;
    int len;
} SjCk;

/* SJ object; first word is the interface table */
typedef struct SjObj SjObj;

typedef struct {
    char pad_00[12];            /* 00: unused here (0 in sjrbf_vtbl) */
    void (*destroy)(SjObj *sj); /* 0c */
    void *(*get_uuid)(SjObj *sj); /* 10 */
    void (*reset)(SjObj *sj); /* 14 */
    void (*getchunk)(SjObj *sj, int id, int nbyte, void *ck); /* 18 */
    void (*ungetchunk)(SjObj *sj, int id, void *ck); /* 1c: sjrbf_unget_chunk in sjrbf_vtbl */
    void (*putchunk)(SjObj *sj, int id, void *ck); /* 20: sjrbf_put_chunk in sjrbf_vtbl */
    int (*getnum)(SjObj *sj, int id); /* 24 */
    int (*is_get_chunk)(SjObj *sj, int id, int nbyte, int *rbyte); /* 28 */
    void (*set_error_cb)(SjObj *sj, void *fn, void *obj); /* 2c */
} SjIf;

struct SjObj {
    SjIf *vtbl;
};

/* ---- SRD (CD / host read requests) ---- */
typedef struct {
    unsigned char b[4];
} SrdMode;

typedef struct {
    int req; /* 00: 1 CD, 2 host file */
    int stat; /* 04: 0 idle, 1 requested, 2 reading, 3 done, 9 error */
    int lsn; /* 08 */
    int nsct; /* 0c */
    void *buf; /* 10 */
    SrdMode mode; /* 14 */
    int fd; /* 18 */
    void *hbuf; /* 1c */
    int hsize; /* 20 */
    int err; /* 24 */
} SrdObj;

/* ---- SVM (server/callback manager) ---- */
typedef int (*SvmFn)(void *obj);

typedef struct {
    SvmFn fn;
    void *obj;
} SvmCb;

typedef void (*HtciErrFn)(void *obj, const char *msg, void *hn);

typedef void (*SjErrFn)(void *obj, int err);

/*
 * Peak meter, 48 bytes; 8 at adx_peak_obj_tbl. Reads 16-bit samples from its input joints, writes
 * one 16-byte record (peak, position, rate, samples) per period to its output joints.
 */
typedef struct {
    signed char used; /* 00 */
    signed char stat; /* 01: 2 = running */
    signed char nch; /* 02 */
    char pad_03[1];
    SjObj *in[2]; /* 04 */
    SjObj *out[2]; /* 0c */
    int pos[2]; /* 14: samples consumed per channel */
    int nchi; /* 1c */
    int rate; /* 20 */
    float period; /* 24: seconds per record */
    float unk_28; /* 28: 0.1 at adx_peak_create; accessors only */
    int nrec; /* 2c */
} PkmObj;

/* PSJ: per-channel stream joints, 24 bytes; 8 at ps2rna_psj_tbl */
typedef struct {
    signed char used; /* 00 */
    char pad_01[3];
    void *sjrmt;
    SjObj *sjuni;
    void *sjx;
    char *ckdata; /* 10 */
    int cklen; /* 14 */
} PsjObj;

/* 36 bytes; 32 at sjmem_obj_tbl */
typedef struct {
    SjIf *vtbl; /* 00 */
    int used; /* 04 */
    void *uuid; /* 08 */
    int len; /* 0c: data left */
    int pos; /* 10: read position */
    char *buf; /* 14 */
    int size; /* 18 */
    SjErrFn errfn; /* 1c */
    void *errobj; /* 20 */
} SjMem;

/* 48 bytes; 64 at sjrbf_obj_tbl */
typedef struct {
    SjIf *vtbl; /* 00 */
    int used; /* 04 */
    void *uuid; /* 08 */
    int ndata; /* 0c: data bytes (id 1) */
    int nfree; /* 10: free bytes (id 0) */
    int wpos; /* 14: free/write position */
    int rpos; /* 18: data/read position */
    char *buf; /* 1c */
    int bsize; /* 20 */
    int xsize; /* 24: extra area after the ring */
    SjErrFn errfn; /* 28 */
    void *errobj; /* 2c */
} SjRbf;

/* 20 bytes; 16 at sjx_obj_tbl */
typedef struct {
    signed char used; /* 00 */
    char pad_01[1];
    short uid; /* 02: generation, bumped by reset */
    SjObj *sj; /* 04: EE side */
    void *sjrmt; /* 08: IOP side */
    int id; /* 0c: chunk id used */
    int iopid; /* 10: IOP-side handle */
} SjxObj;

/* Stream joint chunk (8 bytes). */
typedef struct SjChunk {
    char *data;
    int len;
} SjChunk;

/* Uni stream joint chunk list cell (16 bytes). */
typedef struct SjCell SjCell;
struct SjCell {
    SjCell *next;       /* 00 */
    char pad_04[4];
    SjChunk ck;         /* 08 */
};

/* 48 bytes; 64 at sjuni_obj_tbl */
typedef struct {
    SjIf *vtbl;         /* 00 */
    signed char used;   /* 04 */
    signed char mode;   /* 05: 1 = merge contiguous chunks */
    void *uuid;         /* 08 */
    SjCell *cells;      /* 0c: work area */
    int ncell;          /* 10 */
    SjCell *free;       /* 14: free cell list */
    SjCell *list[4];    /* 18: chunk lists per id */
    SjErrFn errfn;      /* 28 */
    void *errobj;       /* 2c */
} SjUni;

/* DTX record, 16 bytes */
typedef struct {
    signed char flag;   /* 00 */
    signed char id;     /* 01 */
    short uid;          /* 02 */
    SjxObj *sjx;        /* 04 */
    SjChunk ck;         /* 08 */
} SjxRec;

/* DTR handle, 64 bytes; 16 at dtr_obj_tbl */
typedef struct {
    signed char used;   /* 00 */
    signed char stat;   /* 01 */
    signed char busy;   /* 02 */
    SjObj *sjin;        /* 04 */
    SjObj *sjout;       /* 08 */
    SjChunk ckin;       /* 0c */
    SjChunk ckout;      /* 14 */
    int dma_data;
    int dma_addr;
    int dma_size;
    int dma_mode;
    int sema;           /* 2c */
    int unk_30; /* 30: 1 in dtr_create; never read */
    int unk_34; /* 34: 0 in dtr_create; never read */
    int unit;           /* 38 */
    int xfer_total;          /* 3c */
} DtrObj;

/* PS2RNA handle, 84 bytes; 8 at ps2rna_obj_tbl */
typedef struct {
    signed char used;   /* 00 */
    int nch;            /* 04 */
    PsjObj *psj[2];     /* 08 */
    DtrObj *dtr[2];
    SjObj *sj[2];       /* 18: input joints */
    void *id;           /* 20: IOP-side handle */
    signed char play;   /* 24 */
    signed char play_sent;  /* 25 */
    signed char nch_cur;    /* 26 */
    signed char nch_sent;   /* 27 */
    int sfreq;          /* 28 */
    int sfreq_sent;     /* 2c */
    int vol;            /* 30 */
    int vol_sent;       /* 34 */
    int outpan[2];       /* 38 */
    int unk_40[2]; /* 40: cleared per channel by ps2rna_create; never read */
    signed char transfer_sw;  /* 48 */
    signed char unk_49; /* 49: 1 in ps2rna_create / ps2rna_set_transfer_sw; never read */
    int unk_4c; /* 4c: cleared by ps2rna_create; never read */
    int bufsize;        /* 50 */
} Ps2rnaObj;

/* ADXT handle (164 bytes; 8 at adxt_obj_tbl) */
typedef struct {
    signed char used;       /* 00 */
    signed char stat;       /* 01 */
    signed char pmode;
    signed char nch;        /* 03: output joints */
    void *sjd;              /* 04 */
    int fstm;
    int rna;
    SjObj *sjf;             /* 10 */
    SjObj *sji;             /* 14: input joint in use */
    SjObj *sjo[2];          /* 18 */
    unsigned char *wbuf;    /* 20: input buffer in the work area */
    int wsize;
    int unk_28; /* 28: 36 in adxt_create; never read */
    unsigned char *work;    /* 2c: 64-byte aligned work area */
    int obuf_size;              /* 30 */
    int obuf_dist;              /* 34 */
    int svr_freq;              /* 38 */
    short maxsct;            /* 3c: input buffer size in sectors */
    short minsct;            /* 3e: sectors */
    short outvol;            /* 40 */
    short outpan[2];         /* 42: per channel */
    char pad_46[6];
    int lpcnt;              /* 4c */
    char pad_50[4];
    int unk_54; /* 54: cleared by adxt_create; never read */
    int unk_58; /* 58: cleared by adxt_create; never read */
    int unk_5c; /* 5c: cleared by adxt_create; never read */
    short ercode;            /* 60 */
    char pad_62[2];
    int unk_64; /* 64: cleared with ercode (adxt_create, adxt_clear_error); never read */
    short unk_68; /* 68: cleared with ercode (adxt_create, adxt_clear_error); never read */
    short unk_6a; /* 6a: cleared with ercode (adxt_create, adxt_clear_error); never read */
    signed char lpflg;      /* 6c */
    signed char rcvr_mode;      /* 6d */
    char pad_6e[2];
    signed char start_hold;      /* 70 */
    signed char ready;      /* 71 */
    signed char pause;      /* 72 */
    char pad_73[1];
    void *pkm;            /* 74 */
    SjObj *sjx[2];          /* 78 */
    SjObj *sjy[2];          /* 80 */
    int time_ofst;              /* 88 */
    int unk_8c; /* 8c: INT_MAX at adxt_start_decode; never read */
    int unk_90; /* 90: -1 at adxt_start_decode; never read */
    void *stm;
    signed char unk_98; /* 98: 1 at adxt_create / adxt_start_sj / adxt_start_lsc; never read */
    char pad_99[3];
    int vsync_time;              /* 9c */
    int vsync_base;              /* a0 */
} AdxtObj;

typedef struct {
    int n;                      /* 00 */
    int max;                    /* 04 */
    int unk_08; /* 08: cleared by RFHN_CreateDirhn */
    char name[12];              /* 0c: volume name */
    RofsDirEnt ent[1];          /* 18 */
} RofsDir;

/* LSC handle: 1080 bytes, 8 at lsc_obj_tbl. */
typedef struct {
    signed char used;           /* 00 */
    signed char stat;           /* 01: 0 stop, 1 waiting, 2 running */
    signed char reading;        /* 02 */
    signed char flag;
    signed char unk_04; /* set by lsc_set_flag04 (0 when cleared); never read */
    char pad_05[3];
    SjObj *sj;                  /* 08 */
    char pad_0c[8];
    int min;                    /* 14 */
    int bufsize;                /* 18 */
    int wr;                     /* 1c */
    int rd;                     /* 20 */
    int nstm;                   /* 24 */
    void *stm;
    int fsize;                  /* 2c: sectors */
    char pad_30[4];
    int unk_34; /* cleared by lsc_stop; never read */
    LscEnt ent[16];             /* 38 */
} LscObj;

/* ROFS volume (32 bytes). */
typedef struct {
    char name[12];              /* 00 */
    void *fh;                   /* 0c: device handle */
    int unk_10; /* getCvmfInfo info[27] (mountVolume); first arg of pfs get_stat */
    int ptloc;                  /* 14: path table location */
    int ptsize;                 /* 18: path table size */
    char *dirbuf;               /* 1c: current directory record */
} RofsVol;

/* ROFS file handle (56 bytes; rofs_work->hns). */
typedef struct {
    struct RofsWork *work;      /* 00 */
    int idx;                    /* 04 */
    int lsn;                    /* 08 */
    int sct_ofs;
    int fsize;                  /* 10: bytes */
    int fsct;                   /* 14: 2048-byte sectors */
    RofsVol *vol;               /* 18 */
    int nsct;                   /* 1c */
    int pos;                    /* 20 */
    int ntr;                    /* 24 */
    int rqsct;                  /* 28 */
    void *rdbuf;                /* 2c */
    short used;                 /* 30 */
    short stat;                 /* 32 */
    short trstat;               /* 34 */
    short err;                  /* 36 */
} RofsHn;

typedef struct RofsWork RofsWork;

/* ROFS work area (rofs_work), supplied by the application. */
struct RofsWork {
    int stat;                   /* 00 */
    int nhn;                    /* 04: handles (+1) */
    int nvol;                   /* 08 */
    int ndir;                   /* 0c */
    int count;                  /* 10: server calls */
    char *hn_area;              /* 14: handles, 56 bytes each */
    RofsHn *hn_tbl;             /* 18 */
    RofsVol *vol;
    RofsVol *cur;               /* 20: current volume */
    char *dirarea;              /* 24 */
    int pfs_tbl;
    union {
        RofsPfs *pfs;           /* cvFs-style interface (rofs_pfs_tbl) */
        RofsDev *dev;
    } io;                       /* 2c */
    void *devarg;               /* 30 */
    void (*errfn)(void *obj, const char *msg, int err); /* 34 */
    void *errobj;               /* 38 */
    char *sctbuf;               /* 3c: 64-byte aligned sector buffer */
    char pad_40[4224];
    RofsHn hns[1];              /* 10c0 */
};

#define CRI_MIN(a, b) ((a) < (b) ? (a) : (b))

/* 16- and 32-bit byte swaps. */
#define CRI_SW16(x) (((x) >> 8 & 0xff) | ((x) << 8 & 0xff00))
#define CRI_SW32(x) (((x) >> 24 & 0xff) | ((x) >> 8 & 0xff00) | ((x) << 8 & 0xff0000) | (x) << 24)

#define ROFS_U32(p) (((RofsU32 *)(p))->v)
#define ROFS_SW32(x) (((x) & 0xff) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))
/* Same swap on a value in memory (low byte read as a byte). */
#define ROFS_SW32M(x) ((unsigned char)(x) << 24 | ((x) >> 8 & 0xff) << 16 | ((x) >> 16 & 0xff) << 8 | ((x) >> 24 & 0xff))


#endif /* REZ_TYPES_CRI_H */
