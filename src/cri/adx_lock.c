/* ADX lock: nesting count over the SVM lock, so nested ADX calls do not release it early. */

#include "types_cri.h"
#include "adx_lock.h"
#include "svm.h"

/* 0x001d61c0 */
void adx_lock_init(void)
{
    adx_lock_count = 0;
}

/* Outermost level takes the SVM lock. */
/* 0x001d61c8 */
void adx_lock(void)
{
    if (adx_lock_count == 0)
        svm_lock();
    adx_lock_count++;
}

/* Outermost level releases the SVM lock. */
/* 0x001d61f8 */
void adx_unlock(void)
{
    if (--adx_lock_count == 0)
        svm_unlock();
}
