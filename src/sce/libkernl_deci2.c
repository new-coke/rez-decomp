/* DECI2 debug protocol, EE side, via the kernel's Deci2Call. Development hardware only. */

#include "types_libc.h"
#include "types_sce.h"
#include "libkernl_deci2.h"
#include "kernel_syscalls.h"

extern char libkernl_deci2_work[];

/* Work area: uncached view of libkernl_deci2_work. Returns the socket. */
/* 0x001fdd60 */
int sceDeci2Open(unsigned short proto, void *opt, void (*handler)(int, int, void *))
{
    int param[4];

    param[0] = proto;
    param[1] = (int)opt;
    param[2] = (int)handler;
    param[3] = (unsigned int)libkernl_deci2_work | 0x20000000;
    return Deci2Call(1, param);
}

/* 0x001fdda8 */
static int sceDeci2Close(int sock)
{
    int param[1];

    param[0] = sock;
    return Deci2Call(2, param);
}

/* The handler is called when the send can start. */
/* 0x001fddd0 */
int sceDeci2ReqSend(int sock, char dest)
{
    int param[2];

    param[0] = sock;
    param[1] = dest;
    return Deci2Call(3, param);
}

/* 0x001fde00 */
int sceDeci2Poll(int sock)
{
    int param[1];

    param[0] = sock;
    return Deci2Call(4, param);
}

/* 0x001fde28 */
int sceDeci2ExRecv(int sock, void *buf, unsigned short len)
{
    int param[3];

    param[0] = sock;
    param[1] = (int)buf;
    param[2] = len;
    return Deci2Call(-5, param);
}

/* 0x001fde60 */
int sceDeci2ExSend(int sock, void *buf, unsigned short len)
{
    int param[3];

    param[0] = sock;
    param[1] = (int)buf;
    param[2] = len;
    return Deci2Call(-6, param);
}

/* 0x001fde98 */
static int sceDeci2ExReqSend(int sock, char dest)
{
    int param[2];

    param[0] = sock;
    param[1] = dest;
    return Deci2Call(-7, param);
}

/* 0x001fdec8 */
static int sceDeci2ExLock(int sock)
{
    int param[1];

    param[0] = sock;
    return Deci2Call(-8, param);
}

/* 0x001fdef0 */
static int sceDeci2ExUnLock(int sock)
{
    int param[1];

    param[0] = sock;
    return Deci2Call(-9, param);
}

/* On the development tool's console. */
/* 0x001fdf18 */
int libkernl_dbg_puts(char *str)
{
    char *param[1];

    param[0] = str;
    return Deci2Call(16, param);
}
