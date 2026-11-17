/* Trance Vibrator (USB), EE side: RPC to its IOP module for start, device list and level. */

#include "ul_iop_rpc.h"
#include "peri_vib.h"

typedef struct EntryTab {
    int dev_cnt;
    int count;                  /* 4 */
} EntryTab;

extern int peri_vib_pads;
extern int peri_vib_pad_count;

/* 0x001792f0 */
static int peri_vib_return_zero(void)
{
    return 0;
}

/* 0x001792f8 */
static int peri_vib_return_zero_2(void)
{
    return 0;
}

/* printf-style; does nothing. */
/* 0x00179300 */
static int peri_vib_return_zero_3(int a, int b, ...)
{
    return 0;
}

/* 0x00179328 */
static int peri_vib_return_zero_4(void)
{
    return 0;
}

/* 0x00179330 */
static int peri_vib_return_zero_5(void)
{
    return 0;
}

/* 0x00179338 */
static int peri_vib_return_zero_6(void)
{
    return 0;
}

/* 0x00179340 */
static int peri_vib_return_zero_7(void)
{
    return 0;
}

/* 0x00179348 */
static int peri_vib_return_zero_8(void)
{
    return 0;
}

/* steer(cast) */
/* 0x00179350 */
int peri_vib_init(int mode)
{
    return ((int (*)(int))iop_rpc_init)(mode);
}

/* Skipped while the driver is busy. */
/* 0x00179370 */
void peri_vib_refresh(void)
{
    int buf[4];

    if (iop_rpc_status(1, buf) == 0)
        iop_rpc_get_info(&peri_vib_pads);
}

/* 0x001793a8 */
static int peri_vib_status(int mode, int * buf)
{
    return iop_rpc_status(mode, buf);
}

/* Waits for the driver first. */
/* 0x001793c8 */
static void peri_vib_set_act(int idx, int act)
{
    int buf[4];

    do { } while (0);
    iop_rpc_status(0, buf);
    iop_rpc_set_act(idx, act);
}

/* Waits for the driver after each accepted request. Returns 0. */
/* 0x00179410 */
int peri_vib_set_act_all(int act)
{
    int buf[4];
    int i;

    i = (*(EntryTab *)&peri_vib_pads).dev_cnt; /* steer(cast) */
    if ((*(EntryTab *)&peri_vib_pads).count > 0) { /* steer(cast) */
        iop_rpc_status(0, buf);
        while (--i >= 0) {
            if (iop_rpc_set_act(i, act) == 0)
                iop_rpc_status(0, buf);
        }
    }
    return 0;
}

/* Waits for the driver first. */
/* 0x00179490 */
static void peri_vib_get_info(int info)
{
    int buf[4];

    iop_rpc_status(0, buf);
    iop_rpc_get_info(info);
}

/* Waits for the driver first. */
/* 0x001794c8 */
static void peri_vib_send_word(int word, int reply)
{
    int buf[4];

    do { } while (0);
    iop_rpc_status(0, buf);
    iop_rpc_send_word(word, reply);
}

/* 0x00179510 */
int peri_vib_is_connected(void)
{
    return peri_vib_pad_count > 0;
}

/* 0x00179520 */
int peri_vib_get_count(void)
{
    return peri_vib_pad_count;
}
