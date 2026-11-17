/* CRI library and stream joint locks; nesting counters only. */

#include "types_cri.h"
#include "cri_lock.h"
#include "lsc.h"

/* 0x001d1d48 */
static void cri_lock_init(void)
{
    cri_lock_count = 0;
}

/* 0x001d1d50 */
static void cri_lock_finish(void)
{
    cri_lock_count = 0;
}

/* 0x001d1d58 */
void cri_lock_enter(void)
{
    cri_lock_count++;
}

/* 0x001d1d68 */
void cri_lock_leave(void)
{
    cri_lock_count--;
}

/* 0x001d1d78 */
static void cri_lock_clear_flag(void)
{
    sj_lock_count = 0;
}

/* 0x001d1d88 */
void sj_lock(void)
{
    sj_lock_count;
    sj_lock_count++;
}

/* 0x001d1da0 */
void sj_unlock(void)
{
    sj_lock_count--;
    sj_lock_count;
}
