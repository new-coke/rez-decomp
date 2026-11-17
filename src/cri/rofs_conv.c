/* ROFS header conversion: big-endian "CVMH"/"ZONE" sectors, or built for plain ISO 9660. */

#include "types_cri.h"
#include "rofs_mai.h"
#include "libc.h"
#include "rofs_conv.h"

static int rofs_make_iso_header(char *src, char *dst);
static int rofs_make_iso_zone_header(char *src, char *dst);

/* Both may be unaligned. */
/* 0x001dcdf0 */
static void rofs_swap_2words(char *src, char *dst)
{
    int x;
    unsigned int y;

    x = ROFS_U32(src);
    ROFS_U32(dst) = ROFS_SW32(x);
    y = ROFS_U32(src + 4);
    ROFS_U32(dst + 4) = ROFS_SW32(y);
}

/* 0x001dce68 */
static void rofs_copy4(char *src, char *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
}

/* 0x001dce90 */
static void rofs_copy7(char *src, char *dst)
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
    dst[3] = src[3];
    dst[4] = src[4];
    dst[5] = src[5];
    dst[6] = src[6];
}

/* Big-endian: size, date, version, id, two 32-byte names, flags, sector table size and index. */
/* 0x001dced0 */
static int rofs_conv_volume_header(char *src, char *dst)
{
    RofsVd *vd;

    vd = (RofsVd *)dst;
    rofs_swap_2words(src + 16, dst);
    rofs_copy7(src + 24, vd->date);
    rofs_copy4(src + 32, vd->ver);
    vd->unk_14 = ROFS_SW32M(*(unsigned int *)(src + 36));
    vd->magic = ROFS_SW32M(*(unsigned int *)(src + 40));
    vd->name1 = *(RofsName *)(src + 44);
    vd->name2 = *(RofsName *)(src + 76);
    vd->name2.c[31] = 0;
    rofs_copy4(src + 108, vd->unk_5c);
    vd->unk_60 = src[112];
    vd->unk_61 = src[113];
    vd->ntbl = ROFS_SW32M(*(int *)(src + 116));
    vd->idx = ROFS_SW32M(*(int *)(src + 120));
    vd->nsct = ROFS_SW32M(*(int *)(src + 124));
    return 0;
}

/* Plus its sector table; 0 or the error. */
/* 0x001dd0d8 */
static int rofs_conv_cvmh_header(char *src, char *dst)
{
    RofsVd *vd;
    int ret;
    int *p;
    int i;

    vd = (RofsVd *)dst;
    ret = rofs_conv_volume_header(src + 12, dst);
    if (ret != 0)
        return ret;
    p = (int *)(src + 256);
    for (i = 0; i < vd->ntbl; i++, p++)
        vd->tbl[i] = ROFS_SW32M(*p);
    return 0;
}

/* Both may be unaligned. */
/* 0x001dd188 */
static void rofs_swap_3words(char *src, char *dst)
{
    ROFS_U32(dst) = ROFS_SW32((int)ROFS_U32(src));
    src += 4;
    ROFS_U32(dst + 4) = ROFS_SW32((int)ROFS_U32(src));
    ROFS_U32(dst + 8) = ROFS_SW32(ROFS_U32(src + 4));
}

/* Sector type and size, area starts and sizes. */
/* 0x001dd238 */
static int rofs_conv_zone_header(char *src, char *dst)
{
    RofsZone *z;

    z = (RofsZone *)dst;
    z->id = ROFS_SW32M(*(unsigned int *)(src + 12));
    z->mode[0] = src[16];
    z->mode[1] = src[17];
    z->mode[2] = src[18];
    z->mode[3] = src[19];
    z->mode[4] = src[20];
    z->sct_size = ROFS_SW32M(*(int *)(src + 24));
    z->unk_10 = ROFS_SW32M(*(int *)(src + 28));
    rofs_swap_3words(src + 32, (char *)z->start);
    rofs_swap_3words(src + 44, (char *)z->size);
    return 0;
}

/* Plain ISO 9660 or "CVMH" into dst (1904 bytes); 0, or -2 unknown. */
/* 0x001dd350 */
int rofs_conv_vol_sector(char *src, char *dst)
{
    unsigned int x;
    unsigned int magic;

    memset(dst, 0, 1904);
    x = ROFS_U32(src);
    magic = ROFS_SW32(x);
    if (magic == 0x43494d46)
        return rofs_make_iso_header(src, dst);
    if (magic == 0x43564d48)
        return rofs_conv_cvmh_header(src, dst);
    return -2;
}

/* Plain ISO 9660 or "ZONE" into dst (44 bytes); 0, or -2 unknown. */
/* 0x001dd400 */
int rofs_conv_zone_sector(char *src, char *dst)
{
    unsigned int x;
    unsigned int magic;

    memset(dst, 0, 44);
    x = ROFS_U32(src);
    magic = ROFS_SW32(x);
    if (magic == 0x43494d46)
        return rofs_make_iso_zone_header(src, dst);
    if (magic == 0x5a4f4e45)
        return rofs_conv_zone_header(src, dst);
    return -2;
}

/* Id "ROFS", creator "CRI CD CRAFT", date, one-entry sector table, volume sectors. */
/* 0x001dd4b0 */
static int rofs_make_iso_volume_header(char *src, char *dst)
{
    RofsVd *vd;
    char *field;
    char *date;

    vd = (RofsVd *)dst;
    vd->size = ROFS_SW32M(*(unsigned int *)(src + 4));
    vd->size += 8;
    field = vd->ver;
    field[0] = 0;
    field[1] = 16;
    field[2] = 0;
    field[3] = 0;
    vd->unk_14 = 1;
    vd->magic = 0x524f4653;
    memcpy(&vd->name1, "CRI CD CRAFT", 13);
    field = vd->unk_5c;
    date = src + 36;
    field[0] = date[1];
    field[1] = date[2];
    field[2] = date[3];
    field[3] = 0;
    vd->unk_60 = 1;
    vd->unk_61 = 4;
    vd->ntbl = 1;
    vd->idx = 0;
    vd->nsct = (ROFS_SW32M(*(int *)(src + 32)) + 0x56230) / 2048;
    return 0;
}

/* Sector mapping cleared. */
/* 0x001dd618 */
static int rofs_make_iso_header(char *src, char *dst)
{
    int ret;

    rofs_iso_hdr0 = 0;
    rofs_iso_hdr1 = 0;
    rofs_iso_hdr2 = 0;
    rofs_iso_hdr3 = 0;
    ret = rofs_make_iso_volume_header(src, dst);
    if (ret != 0)
        return ret;
    ROFS_U32(dst + 112) = 0;
    return 0;
}

/* Type 1 (2048-byte sectors) only, 2 and 4 -> -3; 2352-byte raw sectors; two-area mapping. */
/* 0x001dd670 */
static int rofs_make_iso_zone_header(char *src, char *dst)
{
    RofsZone *z;
    int ret;
    int mode;
    int sz;
    int n;
    int quo;

    z = (RofsZone *)dst;
    z->id = 3;
    z->mode[0] = 0;
    switch (ROFS_SW32M(*(unsigned int *)(src + 16))) {
    case 1:
        mode = 0;
        ret = 0;
        break;
    case 2:
        mode = 1;
        ret = -3;
        break;
    case 4:
        mode = 2;
        ret = -3;
        break;
    default:
        return -4;
    }
    z->mode[1] = mode;
    z->mode[2] = 4;
    z->mode[3] = 248;
    z->mode[4] = 1;
    z->sct_size = 2352;
    z->unk_10 = 2450;
    sz = ROFS_SW32M(*(int *)(src + 28)) + 16;
    n = quo = sz / 2048;
    z->start[0] = quo;
    z->start[2] = 0;
    quo = sz - (quo << 11);
    rofs_iso_hdr0 = n;
    rofs_iso_hdr1 = quo;
    sz = ROFS_SW32M(*(int *)(src + 32)) + 0x56230;
    n = quo = sz / 2048;
    z->size[0] = quo;
    z->size[2] = 0;
    quo = sz - (quo << 11);
    rofs_iso_hdr2 = n;
    rofs_iso_hdr3 = quo;
    return ret;
}

/* 2048-byte sectors up to the first area, 2352-byte raw sectors after each area start. */
/* 0x001dd840 */
static int rofs_sct_to_pos(int sct)
{
    if (sct >= rofs_iso_hdr2)
        return (rofs_iso_hdr2 << 11) + rofs_iso_hdr3 + (sct - rofs_iso_hdr2) * 2352;
    if (sct >= rofs_iso_hdr0)
        return (rofs_iso_hdr0 << 11) + rofs_iso_hdr1 + (sct - rofs_iso_hdr0) * 2352;
    return sct << 11;
}
