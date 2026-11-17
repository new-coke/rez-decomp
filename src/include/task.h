/* Task = header + work area; callbacks get the work area, TASK_OF gets the header. */

#ifndef REZ_TASK_H
#define REZ_TASK_H

#include "types_common.h"

/* Task.flags */
#define TASK_F_END          0x1         /* end requested; freed on the next pass */
#define TASK_F_SUSPENDED    0x2         /* exec, disp and children skipped */
#define TASK_F_RELEASING    0x4         /* release pass: children ended, release called */
#define TASK_F_RUN_PAUSED   0x8         /* exec still runs while paused */

#define TASK_OF(work) ((Task *)(work) - 1)
#define FINISH(w) (TASK_OF(w)->flags |= 1)
#define FINISH_IF(w) do { if (w) FINISH(w); } while (0)

struct TaskHdr;
struct TaskDesc;
void task_system_init(void);
void task_set_dlist(void *w, int dlist);
void task_prof_reset(void);
void task_frame_begin(void);
void task_run_all(struct TaskHdr **root);
void task_run_all_exec(struct TaskHdr **root);
void *task_create_root(struct TaskHdr **root, struct TaskDesc *desc, int size);
void *task_create(void *parent, struct TaskDesc *desc, int size);
void *task_create_arg(void *w, struct TaskDesc *desc, int size, int prm);
void task_end_children(void *w);
void *task_create_sibling(void *w, struct TaskDesc *desc, int size);
void *task_create_sibling_arg(void *w, struct TaskDesc *desc, int size, int prm);
void *task_create_sibling_inherit(void *w, struct TaskDesc *desc, int size);
void *task_create_sibling_inherit_arg(void *w, struct TaskDesc *desc, int size, int prm);
void *task_create_inherit(void *w, struct TaskDesc *desc, int size);
void *task_create_inherit_arg(void *w, struct TaskDesc *desc, int size, int prm);

#endif
