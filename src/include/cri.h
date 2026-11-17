/* CRI ADX internals shared by ADXF, ADXT, ADXSJD, ADXSTM and ADXM. */

#ifndef CRI_H
#define CRI_H

#include "types_cri.h"

/* ADXF handle, 312 bytes; 16 in adxf_obj_tbl */
typedef struct {
    signed char used;       /* 00 */
    signed char stat;       /* 01 */
    signed char sjflag;     /* 02: sj supplied by the caller */
    char pad_03[1];
    void *stm;              /* 04: ADXSTM handle */
    SjObj *sj;              /* 08 */
    int fsize_sct;          /* 0c */
    int fsize_byte;         /* 10 */
    int cursct;             /* 14 */
    int rqsct;              /* 18 */
    int rqnsct;             /* 1c */
    int nreadsct;           /* 20 */
    void *buf;              /* 24 */
    int bufsize;            /* 28 */
    int maxsct;             /* 2c */
    int ofst;               /* 30 */
    char pad_34[260];
} AdxfObj;

/* cvFsGetVolumeInfo record (16 bytes) */
typedef struct {
    const char *name;
    void *img;          /* image file handle */
    char pad_08[8];
} CvFsVolInfo;

/* ADXF globals */
extern volatile int adxf_ver_str;
extern int adxf_init_count;
extern AdxfObj adxf_obj_tbl[];
extern char adxf_pt_info_tbl[];       /* 1024 bytes */
extern int adxf_read_dcache_flag;
extern char adxf_history_ring[];       /* 256 x 16 bytes */
extern unsigned short adxf_pt_load_count[]; /* per partition */
extern int adxf_history_idx;
extern int adxf_ptload_stat;
extern void *adxf_ptload_handle;
extern int adxf_ptload_ptid;          /* -1 = none */

#define LE32(p) ((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24))

/* ADXF partition info (280 bytes + variable sector table) */
typedef struct {
    int next;
    int size;               /* 04: record size, bytes */
    int nfile;              /* 08 */
    unsigned short nfile16; /* 0c */
    unsigned char added;
    char pad_0f[1];
    char fname[256];        /* 10 */
    void *dir;              /* 110 */
    short fsct[2];          /* 114: first sector, then each file's length in sectors */
} AdxfPtInfo;

extern unsigned char adxf_work_buf[];  /* 64-byte aligned */
extern unsigned char *adxf_work_ptr;
extern unsigned char *adxf_work_ptr2;

/* ADXF_AddPartition file entry (8 bytes), read from AdxfPtInfo.fsct */
typedef struct {
    unsigned short flid;    /* 00: file index in the AFS */
    unsigned short nsct;    /* 02 */
    int ofst;               /* 04: first sector */
} AdxfPtEnt;

/* ADXRNA handle (partial) */
typedef struct {
    char pad_00[8];
    int *chobj[2];          /* 08: per channel; element 3 is the joint passed to sjx_reset */
} AdxrnaObj;

/* ADXT globals */
extern int adxt_init_count;
extern int adxt_svm_server_id;      /* from SVM_SetCbSvr */
extern int adxt_server_flag;      /* 1: adxt_lib_init installs adxt_svm_server as SVM server */
extern int adxt_exec_count;

/* ADXSJD handle (156 bytes; 8 of them at adxsjd_obj_tbl) */
typedef struct {
    signed char used;       /* 00 */
    signed char stat;       /* 01 */
    signed char nch;        /* 02 */
    signed char eos_flag;
    void *adxb;             /* 04: decoder handle */
    SjObj *sji;             /* 08: input joint */
    SjObj *sjo[2];          /* 0c: output joints */
    SjChunk cki;            /* 14: input chunk */
    SjChunk ck[2];          /* 1c: output chunks */
    int decoded_smpl;
    int decoded_bytes;
    int smpl_pos;
    int max_dec_smpl;              /* 38 */
    int smpl_limit;              /* 3c */
    int limit_count;
    int byte_count;
    int limit_fn;
    int limit_obj;
    int filter_fn;
    int filter_obj;
    unsigned char hdr[64];  /* 58: stream header copy */
    int hdr_size;              /* 98 */
} AdxsjdObj;

extern AdxsjdObj adxsjd_obj_tbl[];

/* ADXSTM handle (64 bytes; 40 of them at adxstm_obj_tbl) */
typedef struct {
    signed char used;       /* 00 */
    signed char stat;       /* 01 */
    signed char rdflg;
    signed char rtycnt;
    int sj;
    void *fd;               /* 08: cvFs file */
    int start_sct;
    int ofst;               /* 10: first sector in the file */
    int fsize;              /* 14: bytes */
    int rq_size;              /* 18: chunk size requested */
    int fill_thr;              /* 1c */
    int nrd;                /* 20: sectors of the read in progress */
    SjChunk ck;             /* 24: chunk being filled */
    int maxsct;             /* 2c */
    int fsct;               /* 30: sectors */
    int total_rd;              /* 34 */
    int end_cb;              /* 38 */
    int end_obj;              /* 3c */
} AdxstmObj;

extern AdxstmObj adxstm_obj_tbl[];
extern int adxstm_rt_first;      /* first handle of the first ADXSTM range */
extern int adxstm_rt_count;      /* number of handles in it */
extern int adxstm_nrt_first;      /* first handle of the second range */
extern int adxstm_nrt_count;      /* number of handles in it */
extern int adxstm_retry_limit;      /* negative = retry forever */

/* ADXM / ADX thread layer globals */
extern int adxm_setup_count;
extern volatile int adxm_border_wait;         /* set while waiting for the server border */
extern volatile int adxm_svt2_running;
extern volatile long adxm_idle_loop_cnt;        /* loop counters of the three threads */
extern volatile long adxm_svt1_loop_cnt;
extern volatile long adxm_svt2_loop_cnt;
extern volatile int adxm_setup_tid;
extern volatile int adxm_idle_tid;
extern volatile int adxm_svt5_tid;
extern volatile int adxm_svt6_tid;
extern volatile long adxm_idle_exit_req;        /* exit requests / exited flags, three threads */
extern volatile long adxm_idle_exited;
extern volatile long adxm_svt1_exit_req;
extern volatile long adxm_svt1_exited;
extern volatile long adxm_svt2_exit_req;
extern volatile long adxm_svt2_exited;
extern volatile long adxm_svt5_loop_cnt;
extern volatile long adxm_svt6_loop_cnt;
extern volatile long adxm_svt5_exit_req;
extern volatile long adxm_svt5_exited;
extern volatile long adxm_svt6_exit_req;
extern volatile long adxm_svt6_exited;
extern int adxm_setup_prio;                  /* saved priority of the setup thread */
extern int adxm_setup_thread;                  /* thread id */
extern int adxm_thread_prio[];

/* kernel thread creation record (48 bytes) */
typedef struct {
    int status;             /* 00 */
    void (*entry)(void);    /* 04 */
    void *stack;            /* 08 */
    int stack_size;         /* 0c */
    void *gp;               /* 10 */
    int prio;               /* 14 */
    int cur_prio;           /* 18 */
    char pad_1c[20];
} AdxThrdParam;

extern volatile int adxm_svt1_tid;
extern volatile int adxm_svt2_tid;
extern int adxm_init_prio;                  /* for created threads */
extern volatile int adxps2_ver_str;         /* version word, read once by ADXM setup */
extern int adxm_init_count;                  /* setup count */
extern volatile long adxm_vsync_cnt;
extern int (*adxm_vsync_cb)(int arg);
extern volatile int adxm_adxt_svr_busy;                /* busy flag of adxt_svm_server */
extern volatile int adxm_vsync_busy;
extern int CreateThread(AdxThrdParam *p);

#endif
