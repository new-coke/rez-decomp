/* EE timer 0 at bus clock / 256; tick counts to microseconds and milliseconds. */

#include "ps_timer.h"

typedef struct TMode {
    unsigned clks:2;
    unsigned gate:1;
    unsigned gats:1;
    unsigned gatm:2;
    unsigned zret:1;
    unsigned cue:1;
    unsigned cmpe:1;
    unsigned ovfe:1;
    unsigned equf:1;
    unsigned ovff:1;
    unsigned pad:20;
} TMode;

/* 150 MHz bus / 256. */
/* 0x001b70e0 */
float psTimerToUsec(int ticks)
{
    return (float)ticks * 256.0f / 150000000.0f * 1000000.0f;
}

/* 150 MHz bus / 256. */
/* 0x001b7110 */
static float psTimerToMsec(int ticks)
{
    return (float)ticks * 256.0f / 150000000.0f * 1000.0f;
}

/* Allows one wrap of the 16-bit counter. */
/* 0x001b7148 */
int psTimerDiff(int now, int start)
{
    now -= start;
    if (now < 0)
        now += 0x10000;
    return now;
}

/* 0x001b7160 */
static void psTimerSetCount(int count)
{
    *(volatile int *)0x10000000 = count;
}

/* 0x001b7170 */
int psTimerGetCount(void)
{
    return *(volatile int *)0x10000000;
}

/* 0x001b7180 */
void psTimerExit(void)
{
}

/* Bus clock / 256, no gate/compare/overflow. Bug: uninitialised mode word, 20 high bits garbage. */
/* 0x001b7188 */
void psTimerInit(void)
{
    TMode mode;

    mode.clks = 2;
    mode.gate = 0;
    mode.gats = 0;
    mode.gatm = 0;
    mode.zret = 0;
    mode.cue = 1;
    mode.cmpe = 0;
    mode.ovfe = 0;
    mode.equf = 0;
    mode.ovff = 0;
    *(volatile unsigned int *)0x10000010 = *(unsigned int *)&mode;
}
