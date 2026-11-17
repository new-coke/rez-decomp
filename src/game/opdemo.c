/* Opening demo: ends on its first frame and hands over to stage 0 via "Module Stage". */

#include "opdemo.h"
#include "fade.h"
#include "model.h"
#include "module.h"
#include "stage.h"
#include "task.h"

extern struct TaskDesc opdemo_desc;

/* 0x001405b0 */
void opdemo_reset(void)
{
}

/* 0x001405b8 */
static void opdemo_task_init(void)
{
    fade_start(0, 1, 0);
}

/* 0x001405c8 */
static void opdemo_task_release(int *w)
{
    model_unload_timed(w[1]);
    model_pnb_unload(w[0]);
    w[1] = 0;
    w[0] = 0;
}

/* Stage 0 starts once this task has gone. */
/* 0x00140600 */
static void opdemo_task_exec(void *w)
{
    if (w)
        TASK_OF(w)->flags |= TASK_F_END;
    stage_next_set(0);
    module_stage_setup(w);
}

/* 0x00140640 */
static void opdemo_task_disp(void)
{
}

/* 0x00140648 */
static void *opdemo_task_create(void *parent)
{
    return task_create(parent, &opdemo_desc, 28);
}
