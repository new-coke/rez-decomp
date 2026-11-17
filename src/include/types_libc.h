/* C library types: newlib stdio, malloc, reentrancy; libm word access; libgcc modes, soft-float. */

#ifndef REZ_TYPES_LIBC_H
#define REZ_TYPES_LIBC_H

#include "qword.h"
#include "types_main.h"
#include "types_common.h"

typedef void (*func_ptr)(void);

typedef int SItype_a __attribute__((mode(SI)));

typedef unsigned int USItype_a __attribute__((mode(SI)));

typedef int DItype_a __attribute__((mode(DI)));

typedef unsigned int UDItype_a __attribute__((mode(DI)));

typedef int word_type_a __attribute__((mode(__word__)));

struct DIstruct_a { SItype_a low, high; };

typedef union { struct DIstruct_a s; DItype_a ll; } DIunion_a;

typedef struct malloc_chunk_a {
    unsigned int prev_size;
    unsigned int size;
    struct malloc_chunk_a *fd;
    struct malloc_chunk_a *bk;
} *mchunkptr_a;

struct mallinfo_a {
    int arena;
    int ordblks;
    int smblks;
    int hblks;
    int hblkhd;
    int usmblks;
    int fsmblks;
    int uordblks;
    int fordblks;
    int keepcost;
};

typedef int qword_a REZ_QWORD;

union double_union_a {
    double d;
    unsigned int i[2];
};

struct _reent_init_a {
    char pad_00[56];
    int __sdidinit;
};

struct _glue_a {
    struct _glue_a *_next;
    int _niobs;
    FILE_a *_iobs;
};

struct _reent_sinit_a {
    int _errno;
    char pad_04[52];
    int __sdidinit;
    void (*__cleanup)(struct _reent_a *);
    char pad_40[408];
    struct _glue_a __sglue;
    FILE_a __sf[3];
};

typedef struct _Bigint_a {
    struct _Bigint_a *_next;
    int _k;
    int _maxwds;
    int _sign;
    int _wds;
    unsigned int _x[1];
} Bigint_a;

typedef struct malloc_chunk_a *mbinptr_a;

struct __siov_a {
    const void *iov_base;
    int iov_len;
};

struct __suio_a {
    struct __siov_a *uio_iov;
    int uio_iovcnt;
    int uio_resid;
};

#ifndef NULL
#define NULL ((void *)0)
#endif

/* ---- newlib libm ---- */
typedef union {
    float value;
    unsigned int word;
} ieee_float_shape_type;

#define GET_FLOAT_WORD(i, d) \
    do { ieee_float_shape_type gf_u; gf_u.value = (d); (i) = gf_u.word; } while (0)
#define SET_FLOAT_WORD(d, i) \
    do { ieee_float_shape_type sf_u; sf_u.word = (i); (d) = sf_u.value; } while (0)

typedef union {
    double value;
    struct {
        unsigned int lsw;
        unsigned int msw;
    } parts;
} ieee_double_shape_type;

#define EXTRACT_WORDS(ix0, ix1, d) \
    do { ieee_double_shape_type ew_u; ew_u.value = (d); (ix0) = ew_u.parts.msw; (ix1) = ew_u.parts.lsw; } while (0)
#define GET_HIGH_WORD(i, d) \
    do { ieee_double_shape_type gh_u; gh_u.value = (d); (i) = gh_u.parts.msw; } while (0)
#define GET_LOW_WORD(i, d) \
    do { ieee_double_shape_type gl_u; gl_u.value = (d); (i) = gl_u.parts.lsw; } while (0)
#define INSERT_WORDS(d, ix0, ix1) \
    do { ieee_double_shape_type iw_u; iw_u.parts.msw = (ix0); iw_u.parts.lsw = (ix1); (d) = iw_u.value; } while (0)

struct exception {                  /* newlib math exception record (matherr) */
    int type;
    char *name;
    double arg1;
    double arg2;
    double retval;
    int err;
};

typedef enum { _IEEE_ = -1, _SVID_, _XOPEN_, _POSIX_, _ISOC_ } _LIB_VERSION_TYPE;

/* ---- libgcc soft-float (config/fp-bit.c, double = DFmode) ---- */
typedef enum {
    CLASS_SNAN,
    CLASS_QNAN,
    CLASS_ZERO,
    CLASS_NUMBER,
    CLASS_INFINITY
} fp_class_type;

typedef unsigned long fractype;     /* 64-bit on the EE */

typedef struct {
    fp_class_type class;
    unsigned int sign;
    int normal_exp;
    union {
        fractype ll;
        unsigned int l[2];
    } fraction;
} fp_number_type;

typedef union {
    double value;
    fractype value_raw;
    struct {
        fractype fraction:52;
        unsigned int exp:11;
        unsigned int sign:1;
    } bits;
} FLO_union_type;

static __inline__ int fp_isnan(fp_number_type *x)
{
    return x->class == CLASS_SNAN || x->class == CLASS_QNAN;
}

static __inline__ int fp_isinf(fp_number_type *x)
{
    return x->class == CLASS_INFINITY;
}

static __inline__ int fp_iszero(fp_number_type *x)
{
    return x->class == CLASS_ZERO;
}

typedef long intfrac;               /* signed 64-bit */
#define FP_LSHIFT(X) ((X) = ((X) & 1) | ((X) >> 1))

/* ---- libgcc soft-float (config/fp-bit.c, float = SFmode) ---- */
typedef unsigned int fractype_f;

typedef struct {
    fp_class_type class;
    unsigned int sign;
    int normal_exp;
    union {
        fractype_f ll;
        unsigned short l[2];
    } fraction;
} fp_number_type_f;

typedef union {
    float value;
    fractype_f value_raw;
    struct {
        fractype_f fraction:23;
        unsigned int exp:8;
        unsigned int sign:1;
    } bits;
} FLO_union_type_f;

static __inline__ int fpf_isnan(fp_number_type_f *x)
{
    return x->class == CLASS_SNAN || x->class == CLASS_QNAN;
}

static __inline__ int fpf_isinf(fp_number_type_f *x)
{
    return x->class == CLASS_INFINITY;
}

static __inline__ int fpf_iszero(fp_number_type_f *x)
{
    return x->class == CLASS_ZERO;
}

/* ---- newlib libc ---- */
struct _reent_a {                   /* used parts of newlib struct _reent */
    int _errno;
    void *_stdin;
    void *_stdout;
    void *_stderr;
};

/* ---- newlib libm: word store macros ---- */
#define SET_HIGH_WORD(d, v) \
    do { ieee_double_shape_type sh_u; sh_u.value = (d); sh_u.parts.msw = (v); (d) = sh_u.value; } while (0)
#define SET_LOW_WORD(d, v) \
    do { ieee_double_shape_type sl_u; sl_u.value = (d); sl_u.parts.lsw = (v); (d) = sl_u.value; } while (0)

/* Bigint of the number conversion code; _Balloc allocates, RtCtx.freelist caches per size. */
typedef struct Big {
    struct Big *next;       /* 0x00 free-list link */
    int k;                  /* 0x04 size class */
    int maxwds;             /* 0x08 capacity in words */
    int sign;               /* 0x0c */
    int wds;                /* 0x10 words in use */
    unsigned int x[1];      /* 0x14 */
} Big;

typedef void (*SigFunc)(int sig);

/* C library reentrancy state (used fields); default one via _impure_ptr. */
typedef struct RtCtx {
    int err;                /* 0x000 error code of the last failed call */
    char pad_04[68];
    Big *p5s;               /* 0x048 cached powers 5^(4*2^n) */
    Big **freelist;         /* 0x04c free Bigs per size class */
    char pad_50[388];
    SigFunc *sig_func;      /* 0x1d4 table of 32 signal handlers (0 default, 1 ignore, -1 error) */
} RtCtx;

typedef struct Buf {
    unsigned char *base;
    int size;
} Buf;

typedef struct Stream {
    unsigned char *p;       /* 0x00 current position */
    int r;                  /* 0x04 read space left */
    int w;                  /* 0x08 write space left */
    short flags;            /* 0x0c */
    short file;             /* 0x0e descriptor */
    Buf bf;                 /* 0x10 buffer */
    int lbfsize;            /* 0x18 */
    void *cookie;           /* 0x1c */
    int (*read)(void *cookie, char *buf, int n);            /* 0x20 */
    int (*write)(void *cookie, const char *buf, int n);     /* 0x24 */
    long (*seek)(void *cookie, long offset, int whence);    /* 0x28 */
    int (*close)(void *cookie);                             /* 0x2c */
    Buf ub;                 /* 0x30 ungetc buffer */
    unsigned char *up;      /* 0x38 */
    int ur;                 /* 0x3c */
    unsigned char ubuf[3];  /* 0x40 */
    unsigned char nbuf[1];  /* 0x43 */
    Buf lb;                 /* 0x44 line buffer */
    int blksize;            /* 0x4c */
    int offset;             /* 0x50 */
    RtCtx *ctx;             /* 0x54 */
} Stream;

#define DW_HI(u) ((u).w.hi)
#define DW_LO(u) ((u).w.lo)

/* Double as two words, struct form (libm). */
typedef union DShape {
    double value;
    struct {
        unsigned int lsw;
        unsigned int msw;
    } parts;
} DShape;

#endif /* REZ_TYPES_LIBC_H */
