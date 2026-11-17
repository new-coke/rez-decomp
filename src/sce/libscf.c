/* System configuration library: console settings and RTC to GMT/local time conversion. */

#include "types_libc.h"
#include "types_cri.h"
#include "types_sce.h"
#include "libc.h"
#include "libscf.h"
#include "libkernl_fileio.h"

extern char libscf_romver[16];
extern ScfT10kConfig libscf_t10k_config;
extern void GetOsdConfigParam(ScfOsdConfig *c);
extern int GetOsdConfigParam2(ScfOsdConfig2 *c, int size, int offs);

/* Reads "rom0:ROMVER" on first use. */
/* 0x001e4d10 */
static char *libscf_get_romver(void)
{
    int fd;

    if (libscf_romver[0] == 0) {
        fd = sceOpen("rom0:ROMVER", 1);
        if (fd == -1)
            printf("Can't open rom0:ROMVER\n");
        if (sceRead(fd, libscf_romver, 14) == -1)
            printf("Can't read rom error\n");
        sceClose(fd);
    }
    return libscf_romver;
}

/* 'T' in the ROM version; settings then come from libscf_t10k_config. */
/* 0x001e4db0 */
static int libscf_is_trom(void)
{
    if (libscf_romver[0] == 0)
        libscf_get_romver();
    return libscf_romver[4] == 'T';
}

/* Version 0 configurations only have a Japanese/English bit. */
/* 0x001e4df0 */
int sceScfGetLanguage(void)
{
    ScfOsdConfig osd;
    int ret;

    GetOsdConfigParam(&osd);
    if (libscf_is_trom()) {
        ret = libscf_t10k_config.language;
    } else {
        GetOsdConfigParam(&osd);
        if (osd.version == 0)
            ret = osd.japLanguage;
        else
            ret = osd.language;
    }
    return ret;
}

/* What a development tool ROM reports. */
/* 0x001e4e50 */
static void sceScfSetT10kConfig(ScfT10kConfig *config)
{
    libscf_t10k_config = *config;
}

/* 4:3, full or 16:9. */
/* 0x001e4e70 */
static int sceScfGetAspect(void)
{
    ScfOsdConfig osd;

    if (libscf_is_trom())
        return libscf_t10k_config.screenType;
    GetOsdConfigParam(&osd);
    return osd.screenType;
}

/* S/PDIF output. */
/* 0x001e4eb0 */
static int sceScfGetSpdif(void)
{
    ScfOsdConfig osd;

    if (libscf_is_trom())
        return libscf_t10k_config.spdifMode;
    GetOsdConfigParam(&osd);
    return osd.spdifMode;
}

/* Minutes from GMT; version 0 configurations: Japan (540). */
/* 0x001e4ef0 */
static int sceScfGetTimeZone(void)
{
    ScfOsdConfig osd;
    int ret;

    if (libscf_is_trom()) {
        ret = libscf_t10k_config.timezone;
    } else {
        ret = 540;
        GetOsdConfigParam(&osd);
        if (osd.version) {
            ret = osd.timezoneOffset;
            printf("Timezone=%d\n", ret);
        }
    }
    return ret;
}

/* 0 on version 0 configurations. */
/* 0x001e4f58 */
static int sceScfGetDateNotation(void)
{
    ScfOsdConfig2 osd2;
    ScfOsdConfig osd;
    int ret;

    if (libscf_is_trom()) {
        ret = libscf_t10k_config.dateNotation;
    } else {
        GetOsdConfigParam(&osd);
        if (osd.version == 0) {
            ret = 0;
        } else {
            GetOsdConfigParam2(&osd2, 1, 1);
            ret = osd2.dateNotation;
        printf("DateNotation=%d\n", ret);
        }
    }
    return ret;
}

/* 0 on version 0 configurations. */
/* 0x001e4fd8 */
static int sceScfGetSummerTime(void)
{
    ScfOsdConfig2 osd2;
    ScfOsdConfig osd;
    int ret;

    if (libscf_is_trom()) {
        ret = libscf_t10k_config.summerTime;
    } else {
        GetOsdConfigParam(&osd);
        if (osd.version == 0) {
            ret = 0;
        } else {
            GetOsdConfigParam2(&osd2, 1, 1);
            ret = osd2.summerTime;
        printf("SummerTime=%d\n", ret);
        }
    }
    return ret;
}

/* 12 or 24 hour; 0 on version 0 configurations. */
/* 0x001e5058 */
static int sceScfGetTimeNotation(void)
{
    ScfOsdConfig2 osd2;
    ScfOsdConfig osd;
    int ret;

    if (libscf_is_trom()) {
        ret = libscf_t10k_config.timeNotation;
    } else {
        GetOsdConfigParam(&osd);
        if (osd.version == 0) {
            ret = 0;
        } else {
            GetOsdConfigParam2(&osd2, 1, 1);
            ret = osd2.timeNotation;
        printf("TimeNotation=%d\n", ret);
        }
    }
    return ret;
}

/* 0..99. */
/* 0x001e50d8 */
static unsigned char libscf_itob(unsigned char c)
{
    SCF_ASSERT(c <= 99, 281, "c <=99");
    return c + (c / 10) * 6;
}

/* 0x001e5140 */
static unsigned char libscf_btoi(unsigned char c)
{
    unsigned char ret;

    SCF_ASSERT(c <= 0x99, 294, "c <= 0x99");
    ret = (c >> 4) * 6;
    return c - ret;
}

/* In place. */
/* 0x001e5198 */
static void libscf_rtc_btoi(ScfClock *prtc)
{
    SCF_ASSERT(prtc != NULL, 306, "prtc != NULL");
    prtc->year = libscf_btoi(prtc->year);
    prtc->month = libscf_btoi(prtc->month);
    prtc->day = libscf_btoi(prtc->day);
    prtc->hour = libscf_btoi(prtc->hour);
    prtc->minute = libscf_btoi(prtc->minute);
    prtc->second = libscf_btoi(prtc->second);
}

/* In place. */
/* 0x001e5220 */
static void libscf_rtc_itob(ScfClock *prtc)
{
    SCF_ASSERT(prtc != NULL, 321, "prtc != NULL");
    prtc->year = libscf_itob(prtc->year);
    prtc->month = libscf_itob(prtc->month);
    prtc->day = libscf_itob(prtc->day);
    prtc->hour = libscf_itob(prtc->hour);
    prtc->minute = libscf_itob(prtc->minute);
    prtc->second = libscf_itob(prtc->second);
}

/* Binary. Every year divisible by four is leap; 99 wraps to 0. */
/* 0x001e52a8 */
static void libscf_rtc_add_day(ScfClock *prtc)
{
    char days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    SCF_ASSERT(prtc != NULL, 336, "prtc != NULL");
    prtc->day++;
    if ((((volatile ScfClock *)prtc)->year & 3) == 0)
        days[1] = 29;
    if (days[prtc->month - 1] < prtc->day) {
        prtc->day = 1;
        prtc->month++;
        if (prtc->month == 13) {
            if (prtc->year == 99)
                prtc->year = 0;
            else
                prtc->year++;
            prtc->month = 1;
        }
    }
}

/* Binary; 0 wraps to 99. The leap test uses the year before the wrap. */
/* 0x001e5390 */
static void libscf_rtc_sub_day(ScfClock *prtc)
{
    char days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

    SCF_ASSERT(prtc != NULL, 360, "prtc != NULL");
    prtc->day--;
    if ((((volatile ScfClock *)prtc)->year & 3) == 0)
        days[1] = 29;
    if (prtc->day == 0) {
        prtc->month--;
        if (prtc->month == 0) {
            if (prtc->year == 0)
                prtc->year = 99;
            else
                prtc->year--;
            prtc->month = 12;
        }
        prtc->day = days[prtc->month - 1];
    }
}

/* Binary; carries into the day. */
/* 0x001e5468 */
static void libscf_rtc_add_hour(ScfClock *prtc)
{
    SCF_ASSERT(prtc != NULL, 385, "prtc != NULL");
    prtc->hour++;
    if (prtc->hour == 24) {
        prtc->hour = 0;
        libscf_rtc_add_day(prtc);
    }
}

/* Binary; borrows from the day. */
/* 0x001e54d8 */
static void libscf_rtc_sub_hour(ScfClock *prtc)
{
    SCF_ASSERT(prtc != NULL, 398, "prtc != NULL");
    if (prtc->hour == 0) {
        prtc->hour = 23;
        libscf_rtc_sub_day(prtc);
    } else {
        prtc->hour--;
    }
}

/* BCD, at most a day either way. Bug: carry test is "more than 60", so 60 minutes stays. */
/* 0x001e5540 */
static void libscf_rtc_add_minutes(ScfClock *prtc, int diff)
{
    int minute;

    SCF_ASSERT(prtc != NULL, 416, "prtc != NULL");
    SCF_ASSERT(-60*24<=diff && diff <= 60*24, 417, "-60*24<=diff && diff <= 60*24");
    libscf_rtc_btoi(prtc);
    minute = prtc->minute + diff;
    if (minute >= 0) {
        while (minute > 60) {
            minute -= 60;
            libscf_rtc_add_hour(prtc);
        }
    } else {
        while (minute < 0) {
            minute += 60;
            libscf_rtc_sub_hour(prtc);
        }
    }
    prtc->minute = minute;
    libscf_rtc_itob(prtc);
}

/* The RTC keeps Japan time (GMT+9). */
/* 0x001e5610 */
static void sceScfGetGMTfromRTC(ScfClock *prtc)
{
    SCF_ASSERT(prtc != NULL, 444, "prtc != NULL");
    libscf_rtc_add_minutes(prtc, -540);
}

/* With the configured time zone and summer time; the RTC keeps Japan time. */
/* 0x001e5658 */
static void sceScfGetLocalTimefromRTC(ScfClock *prtc)
{
    int diff;
    int st;

    diff = sceScfGetTimeZone();
    st = sceScfGetSummerTime() * 60 - 540;
    diff += st;
    SCF_ASSERT(prtc != NULL, 455, "prtc != NULL");
    libscf_rtc_add_minutes(prtc, diff);
}
