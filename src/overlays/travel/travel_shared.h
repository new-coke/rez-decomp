/* Travelling mode area select: task and colour macros. */

#ifndef REZ_TRAVEL_SHARED_H
#define REZ_TRAVEL_SHARED_H

#include "types_common.h"
#include "fog.h"
#include "task.h"


/* suspend (flag 2) a task that has not finished */
#define TASK_SUSPEND(w) do { void *t_ = (w); \
    if (t_ && !(TASK_OF(t_)->flags & 1)) TASK_OF(t_)->flags |= 2; } while (0)

#define TASK_RESUME(w) do { void *t_ = (w); \
    if (t_ && !(TASK_OF(t_)->flags & 1)) TASK_OF(t_)->flags &= ~2; } while (0)

#include "color.h"
extern void *ulMatrixCur;

#endif /* REZ_TRAVEL_SHARED_H */
