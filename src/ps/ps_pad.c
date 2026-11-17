/* Controller input: two pad ports polled per frame, edge and auto-repeat buttons, analog sticks. */

#include "types_ps.h"
#include "libc.h"
#include "ps_pad.h"
#include "libpad.h"

extern int psPadRepeatDelay;
extern int psPadRepeatInterval;
typedef void (*PsPadCallback)(PsPadRec *rec, int state);

extern PsPadCallback psPadCallbackFunc;
extern PsPadRec psPadRecords[];
extern char psPadDmaBuffers[];
extern PadInfo psPadInfo[];

/* Ignored if delay < interval. */
/* 0x001b6088 */
static void psPadSetRepeat(int delay, int interval)
{
    if (!(delay < interval)) {
        psPadRepeatDelay = delay;
        psPadRepeatInterval = interval;
    }
}

/* Returns the previous one. */
/* 0x001b60a8 */
PsPadCallback psPadSetCallback(PsPadCallback cb)
{
    PsPadCallback old;

    old = psPadCallbackFunc;
    psPadCallbackFunc = cb;
    return old;
}

/* 0x001b60b8 */
unsigned char psPadGetPort(PsPadRec *pad)
{
    return pad->port;
}

/* 0x001b60c0 */
unsigned char psPadGetSlot(PsPadRec *pad)
{
    return pad->slot;
}

/* 1 pressure-sensitive buttons, 2 actuators. */
/* 0x001b60c8 */
unsigned char psPadGetFlags(PsPadRec *pad)
{
    return pad->flags;
}

/* Controller id; -1 when pad is 0. */
/* 0x001b60d0 */
int psPadGetType(PsPadRec *pad)
{
    int ret;

    ret = -1;
    if (pad)
        ret = pad->id;
    return ret;
}

/* 0 when pad is 0. */
/* 0x001b60e8 */
int psPadGetState(PsPadRec *pad)
{
    int ret;

    ret = 0;
    if (pad)
        ret = pad->state;
    return ret;
}

/* Locked analog mode, only in stable state 6. 1 ok, 0 failed (reason printed). */
/* 0x001b6100 */
int psPadChangeAnalogMode(int port, int slot)
{
    char buf[16];
    int st;
    int ret;

    printf("psPadChangeAnalogMode(%d, %d)\n", port, slot);
    do { } while (0);
    st = scePadGetState(port, slot);
    if (st == 6) {
        if (scePadSetMainMode(port, slot, 1, 3) == 1) {
            ret = 1;
        } else {
            printf("psPadChangeAnalogMode: scePadSetMainMode returned error\n");
            ret = 0;
        }
    } else {
        scePadStateIntToStr(st, buf);
        printf("psPadChangeAnalogMode: cannot change mode[%s]\n", buf);
        ret = 0;
    }
    return ret;
}

/* -1..1; within dz of centre 128 -> 0, else shifted by dz, scaled 1/(127 - dz). ox/oy may be 0. */
/* 0x001b61b0 */
void psPadStickToFloat(unsigned char x, unsigned char y, int dz, float *ox, float *oy)
{
    float scale;
    int axis;
    int adj;
    int mag;

    scale = 1.0f / (127.0f - (float)dz);
    axis = x - 128;
    if (axis <= -128)
        axis = -127;
    if (axis > 0) {
        mag = axis < 0 ? -axis : axis;
        adj = axis - dz;
    } else {
        adj = axis + dz;
        mag = axis < 0 ? -axis : axis;
    }
    axis = 0;
    if (!(mag < dz))
        axis = adj;
    if (ox)
        *ox = (float)axis * scale;
    axis = y - 128;
    if (axis <= -128)
        axis = -127;
    if (axis > 0) {
        adj = axis - dz;
        mag = axis < 0 ? -axis : axis;
    } else {
        adj = axis + dz;
        mag = axis < 0 ? -axis : axis;
    }
    axis = 0;
    if (!(mag < dz))
        axis = adj;
    if (oy)
        *oy = (float)axis * scale;
}

/* 0 when port >= 3. */
/* 0x001b62a8 */
PsPadRec *psPadGet(int port)
{
    if (port >= 3)
        return 0;
    return &psPadRecords[port];
}

/* 0x001b62d0 */
static void psPadCallCallback(PsPadRec *rec, int state)
{
    if (psPadCallbackFunc)
        psPadCallbackFunc(rec, state);
}

/* No buttons, sticks centred. */
/* 0x001b62f8 */
static void psPadClearData(PsPadRec *pad)
{
    pad->up = 0xffffffff;
    pad->held = 0;
    pad->repeat = 0;
    pad->released = 0;
    pad->pressed = 0;
    pad->stick[1] = 128;
    pad->stick[0] = 128;
    pad->stick[3] = 128;
    pad->stick[2] = 128;
}

/* 0x001b6330 */
static void psPadStub(void)
{
}

/* 0x001b6338 */
static void psPadClearInfo(PsPadRec *pad)
{
    memset(pad->info, 0, sizeof(PadInfo));
}

/* State 6: id, mode, mode list, pressure, actuators. State 2: id only. Other states clear it. */
/* 0x001b6348 */
static void psPadUpdateInfo(PsPadRec *pad)
{
    PadInfo *info;
    int i;
    int j;

    info = pad->info;
    switch (pad->state) {
    case 6:
        if ((info->nmodes = scePadInfoMode(pad->port, pad->slot, 4, -1)) != 0) {
            info->cur = scePadInfoMode(pad->port, pad->slot, 3, 0);
            info->id = scePadInfoMode(pad->port, pad->slot, 4, info->cur);
            for (i = 0; i < info->nmodes; i++)
                info->modes[i] = scePadInfoMode(pad->port, pad->slot, 4, i);
        }
        if (scePadInfoPressMode(pad->port, pad->slot) == 1)
            pad->flags |= 1;
        else
            pad->flags &= 0xfe;
        if (scePadInfoMode(pad->port, pad->slot, 2, 0)) {
            pad->flags |= 2;
            info->nact = scePadInfoComb(pad->port, pad->slot, -1, 0);
            for (j = 0; j < info->nact; j++) {
                info->act[j].w[0] = scePadInfoComb(pad->port, pad->slot, j, 1);
                info->act[j].w[1] = scePadInfoComb(pad->port, pad->slot, j, 2);
                info->act[j].w[2] = scePadInfoComb(pad->port, pad->slot, j, 3);
                info->act[j].w[3] = scePadInfoComb(pad->port, pad->slot, j, 4);
            }
        } else {
            pad->flags &= 0xfd;
            info->nact = 0;
            memset(info->act, 0, 64);
        }
        break;
    case 2:
        info->id = scePadInfoMode(pad->port, pad->slot, 1, 0);
        info->cur = 0;
        info->nmodes = 0;
        pad->flags &= 0xfc;
        memset(info->modes, 255, 4);
        break;
    default:
        memset(info, 0, sizeof(PadInfo));
        pad->flags = 0;
        memset(info->modes, 255, 4);
        break;
    }
}

/*
 * Ids 4 and 7 give buttons (active-low) and sticks; others or failed reads reset input.
 * On a state change refreshes info and calls the callback.
 */
/* 0x001b6658 */
void psPadUpdate(void)
{
    int port;
    PsPadRec *pad;
    int state;
    unsigned char state8;
    unsigned char prev;
    unsigned char id;
    unsigned int old_held;
    unsigned int old_up;
    unsigned int nw;
    unsigned int cur;
    int n;

    for (port = 0; port < 2; port++) {
        pad = &psPadRecords[port];
        state = scePadGetState(port, 0);
        prev = pad->state;
        state8 = state;
        pad->state = state;
        if (state8 != 2 && state8 != 6) {
            psPadClearData(pad);
            psPadClearInfo(pad);
        } else {
            n = scePadRead(port, 0, pad->data);
            if (n > 0 && pad->data[0] == 0) {
                pad->id = pad->data[1] >> 4;
                id = pad->data[1] >> 4;
                old_held = pad->held;
                old_up = pad->up;
                switch (id) {
                case 4:
                    nw = ((pad->data[2] << 8) ^ 0xff00) | (~pad->data[3] & 0xff);
                    pad->stick[1] = 128;
                    pad->stick[0] = 128;
                    pad->stick[3] = 128;
                    pad->held = nw;
                    pad->up = ~nw;
                    pad->stick[2] = 128;
                    break;
                case 7:
                    nw = ((pad->data[2] << 8) ^ 0xff00) | (~pad->data[3] & 0xff);
                    pad->up = ~nw;
                    pad->stick[0] = pad->data[4];
                    pad->stick[1] = pad->data[5];
                    pad->stick[2] = pad->data[6];
                    pad->stick[3] = pad->data[7];
                    pad->held = nw;
                    break;
                default:
                    psPadClearData(pad);
                    break;
                }
                cur = pad->held;
                pad->pressed = cur & ~old_held;
                pad->released = pad->up & ~old_up;
                if (pad->pressed) {
                    pad->repeat = pad->pressed;
                    pad->repeat_cnt = 0;
                } else if (cur && cur == old_held) {
                    pad->repeat_cnt += 1;
                    if (pad->repeat_cnt > psPadRepeatDelay) {
                        pad->repeat = old_held;
                        pad->repeat_cnt -= psPadRepeatInterval;
                    } else {
                        pad->repeat = 0;
                    }
                } else {
                    pad->repeat = 0;
                    pad->repeat_cnt = 0;
                }
            } else {
                psPadClearData(pad);
            }
        }
        if (prev != pad->state) {
            psPadUpdateInfo(pad);
            psPadCallCallback(pad, state);
        }
    }
}

/* 0x001b68a0 */
static void psPadExit(void)
{
    int i;

    for (i = 0; i < 2; i++)
        scePadPortClose(i, 0);
    scePadEnd();
}

/* Ports 0 and 1, slot 0, info in psPadInfo. Repeat after 30 frames, then every 2. */
/* 0x001b68e0 */
void psPadInit(void)
{
    int port;
    int rec_port;
    char *dmabuf;
    PsPadRec *pad;

    if (scePadInit(0) != 1)
        printf("ERROR: scePadInit is faild!\n");
    dmabuf = psPadDmaBuffers;
    for (port = 0; port < 2; port++) {
        scePadPortOpen(port, 0, dmabuf);
        dmabuf += 256;
    }
    pad = psPadRecords;
    for (rec_port = 0; rec_port < 2; rec_port++) {
        memset(pad, 0, sizeof(PsPadRec));
        pad->port = rec_port;
        pad->up = 0xffffffff;
        pad->info = &psPadInfo[rec_port];
        pad->slot = 0;
        pad++;
    }
    psPadRepeatDelay = 30;
    psPadRepeatInterval = 2;
}
