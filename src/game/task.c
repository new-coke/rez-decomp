/*
 * Task system: every object, effect, menu and sequence is a task in a tree, created from a
 * descriptor (name, init/exec/disp/release) as a header plus work area; ended tasks are released
 * and freed a frame later. Also a per-frame profile of running tasks.
 */

#include "system.h"
#include "task.h"
#include "heap.h"
#include "perform.h"
#include "render.h"

extern int system2_root;
extern int system1_root;
extern void soft_reset(const char *name);
extern TaskProf perform_recs[];
extern unsigned int system_flags;
extern int task_prof_enable;

/* "Task" soft reset; clears both trees and the live count. */
/* 0x0016f960 */
void task_system_init(void)
{
    soft_reset("Task");
    perf_record_cnt = 0;
    system1_root = 0;
    system2_root = 0;
    task_default_flags = 0;
}

/* Selected by the runners before the task's disp callback. */
/* 0x0016f9a0 */
void task_set_dlist(void *w, int dlist)
{
    TASK_HDR(w)->dlist = dlist;
}

/* First record becomes the end marker (depth 0, task -1). */
/* 0x0016f9a8 */
void task_prof_reset(void)
{
    TaskProf *prof = perform_recs;

    task_prof_cur = prof;
    prof->task = (void *)0xffffffff;
    perform_recs[0].depth = 0;
}

/* 0x0016f9d0 */
void task_frame_begin(void)
{
}

/* Times exec and disp per running task into its header and profile record (task_prof_enable). */
/* steer(cast) */
/* 0x0016f9d8 */
static void task_run_prof(TaskHdr **link)
{
    TaskHdr *task;
    TaskHdr *child;
    TaskProf *prof;
    void (*fn)(void *work);
    int start;
    int end;

    task_run_depth++;
    task = *link;
    do {
        if (!(task->flags & TASK_F_END)) {
            prof = task_prof_cur;
            prof->depth = task_run_depth;
            prof[1].depth = 0;
            prof[1].task = (void *)0xffffffff;
            prof->task = task;
            prof->exec_time = 0;
            prof->disp_time = 0;
            if (task->flags & TASK_F_SUSPENDED) {
                task_prof_cur = prof + 1;
                goto next;
            }
            if (!(task->flags & TASK_F_RUN_PAUSED)
                && (system_flags & (SYS_PAUSE_KBD | SYS_PAUSED))) {
                end = perform_timer_read();
            } else {
                start = perform_timer_read();
                fn = task->exec;
                if (fn)
                    fn(TASK_WORK(task));
                end = perform_timer_read();
                task->exec_time = perform_elapsed_cur(start, end);
            }
            fn = task->disp;
            if (fn) {
                RenderSetCurrentDisplayList(task->dlist);
                fn(TASK_WORK(task));
            }
            task->disp_time = perform_elapsed_cur(end, perform_timer_read());
            task_prof_cur->exec_time = task->exec_time;
            task_prof_cur->disp_time = task->disp_time;
            task_prof_cur++;
        }
        if (task->flags & TASK_F_END) {
            task->flags ^= TASK_F_RELEASING;
            if (task->flags & TASK_F_RELEASING) {
                for (child = task->child; child; child = child->next)
                    child->flags |= TASK_F_END;
            }
            if (task->child)
                task_run_prof(&task->child);
            if (task->flags & TASK_F_RELEASING) {
                if (task->release)
                    task->release(TASK_WORK(task));
                perf_record_cnt--;
            } else {
                *link = task->next;
                heap_free(task, 0);
                task = (TaskHdr *)(link - 1);
            }
        } else if (task->child) {
            task_run_prof(&task->child);
        }
    next:
        link = &task->next;
    } while ((task = task->next) != 0);
    task_run_depth--;
}

/*
 * One level plus children: as task_run_exec, also disp (after selecting the display list);
 * appends untimed profile records.
 */
/* 0x0016fc40 */
static void task_run(TaskHdr **link)
{
    TaskHdr *task;
    TaskHdr *child;
    TaskProf *prof;
    void (*fn)(void *work);

    task_run_depth++;
    task = *link;
    do {
        if (!(task->flags & TASK_F_END)) {
            prof = task_prof_cur;
            prof->depth = task_run_depth;
            prof[1].depth = 0;
            prof[1].task = (void *)0xffffffff;
            prof->task = task;
            prof->exec_time = 0;
            prof->disp_time = 0;
            if (task->flags & TASK_F_SUSPENDED) {
                task_prof_cur = prof + 1;
                goto next;
            }
            if ((task->flags & TASK_F_RUN_PAUSED)
                || !(system_flags & (SYS_PAUSE_KBD | SYS_PAUSED))) {
                fn = task->exec;
                if (fn)
                    fn(TASK_WORK(task));
            }
            fn = task->disp;
            if (fn) {
                RenderSetCurrentDisplayList(task->dlist);
                fn(TASK_WORK(task));
            }
            task_prof_cur++;
        }
        if (task->flags & TASK_F_END) {
            task->flags ^= TASK_F_RELEASING;
            if (task->flags & TASK_F_RELEASING) {
                for (child = task->child; child; child = child->next)
                    child->flags |= TASK_F_END;
            }
            if (task->child)
                task_run(&task->child);
            if (task->flags & TASK_F_RELEASING) {
                if (task->release)
                    task->release(TASK_WORK(task));
                perf_record_cnt--;
            } else {
                *link = task->next;
                heap_free(task, 0);
                task = (TaskHdr *)(link - 1);
            }
        } else if (task->child) {
            task_run(&task->child);
        }
    next:
        link = &task->next;
    } while ((task = task->next) != 0);
    task_run_depth--;
}

/* Sets system flag 0x10000 (draw frame); profiling runner while task_prof_enable. */
/* 0x0016fe40 */
void task_run_all(TaskHdr **root)
{
    system_flags |= SYS_DISPLAY_PASS;
    task_run_depth = -1;
    if (*root) {
        if (task_prof_enable)
            task_run_prof(root);
        else
            task_run(root);
    }
}

/*
 * Exec only. Sleeping (flag 2) skipped; exec skipped while paused (system_flags & 3) without flag
 * 8. Ended (flag 1): flag 4, children ended, release; unlinked and freed a frame later.
 */
/* 0x0016fe98 */
static void task_run_exec(TaskHdr **link)
{
    TaskHdr *task;
    TaskHdr *child;

    task_run_depth++;
    task = *link;
    do {
        if (!(task->flags & TASK_F_END)) {
            if (task->flags & TASK_F_SUSPENDED)
                goto next;
            if (task->exec && ((task->flags & TASK_F_RUN_PAUSED)
                               || !(system_flags & (SYS_PAUSE_KBD | SYS_PAUSED))))
                task->exec(TASK_WORK(task));
        }
        if (task->flags & TASK_F_END) {
            task->flags ^= TASK_F_RELEASING;
            if (task->flags & TASK_F_RELEASING) {
                for (child = task->child; child; child = child->next)
                    child->flags |= TASK_F_END;
            }
            if (task->child)
                task_run_exec(&task->child);
            if (task->flags & TASK_F_RELEASING) {
                if (task->release)
                    task->release(TASK_WORK(task));
                perf_record_cnt--;
            } else {
                *link = task->next;
                heap_free(task, 0);
                task = (TaskHdr *)(link - 1);
            }
        } else if (task->child) {
            task_run_exec(&task->child);
        }
    next:
        link = &task->next;
    } while ((task = task->next) != 0);
    task_run_depth--;
}

/* Clears system flag 0x10000 (draw frame). */
/* 0x00170018 */
void task_run_all_exec(TaskHdr **root)
{
    system_flags &= ~SYS_DISPLAY_PASS;
    task_run_depth = -1;
    if (*root)
        task_run_exec(root);
}

/* Returns the previous value. */
/* 0x00170058 */
static int task_set_default_flags(int flags)
{
    int old = task_default_flags;

    task_default_flags = flags;
    return old;
}

/* Replaces *root; no parent. */
/* 0x00170068 */
void *task_create_root(TaskHdr **root, TaskDesc *desc, int size)
{
    TaskHdr *task;

    task = heap_block_tag(game_malloc(size + 64), desc->name, -1);
    if (task) {
        perf_record_cnt++;
        *root = task;
        task->magic = TASK_MAGIC;
        task->next = 0;
        task->child = 0;
        task->parent = 0;
        task->exec = desc->exec;
        task->disp = desc->disp;
        task->release = desc->release;
        task->name = desc->name;
        task->flags = task_default_flags;
        task->dlist = 5;
        task++;
        heap_bzero(task, size);
        if (desc->init)
            desc->init(task);
    }
    return task;
}

/* Flag 1; nothing for null w. */
/* 0x00170150 */
void task_end_children(void *w)
{
    TaskHdr *child;

    if (w) {
        for (child = TASK_HDR(w)->child; child; child = child->next) {
            if (TASK_WORK(child))
                child->flags |= TASK_F_END;
        }
    }
}

/* Appended to the siblings of w's task. */
/* 0x00170190 */
void *task_create_sibling(void *w, TaskDesc *desc, int size)
{
    TaskHdr *task;
    TaskHdr **link;
    TaskHdr *sib;
    TaskHdr *hdr;
    TaskHdr *par;

    task = heap_block_tag(game_malloc(size + 64), desc->name, -1);
    if (task) {
        perf_record_cnt++;
        hdr = TASK_HDR(w);
        par = hdr->parent;
        link = &hdr->next;
        for (sib = *link; sib; sib = sib->next)
            link = &sib->next;
        *link = task;
        task->magic = TASK_MAGIC;
        task->next = 0;
        task->child = 0;
        task->parent = par;
        task->exec = desc->exec;
        task->disp = desc->disp;
        task->release = desc->release;
        task->name = desc->name;
        task->flags = task_default_flags;
        task->dlist = 5;
        task++;
        heap_bzero(task, size);
        if (desc->init)
            desc->init(task);
    }
    return task;
}

/* prm passed to init. */
/* 0x001702c8 */
void *task_create_sibling_arg(void *w, TaskDesc *desc, int size, int prm)
{
    TaskHdr *task;
    TaskHdr **link;
    TaskHdr *sib;
    TaskHdr *hdr;
    TaskHdr *par;

    task = heap_block_tag(game_malloc(size + 64), desc->name, -1);
    if (task) {
        perf_record_cnt++;
        hdr = TASK_HDR(w);
        par = hdr->parent;
        link = &hdr->next;
        for (sib = *link; sib; sib = sib->next)
            link = &sib->next;
        *link = task;
        task->magic = TASK_MAGIC;
        task->next = 0;
        task->child = 0;
        task->parent = par;
        task->exec = desc->exec;
        task->disp = desc->disp;
        task->release = desc->release;
        task->name = desc->name;
        task->flags = task_default_flags;
        task->dlist = 5;
        task++;
        heap_bzero(task, size);
        if (desc->init)
            ((void (*)(void *, int))desc->init)(task, prm);
    }
    return task;
}

/*
 * Header plus size-byte work, heap-tagged with the descriptor's name; default flags, display list
 * 5; work cleared; init called. 0 when out of memory.
 */
/* 0x00170408 */
void *task_create(void *parent, TaskDesc *desc, int size)
{
    TaskHdr *task;
    TaskHdr **link;
    TaskHdr *child;
    TaskHdr *hdr;

    task = heap_block_tag(game_malloc(size + 64), desc->name, -1);
    if (task) {
        perf_record_cnt++;
        hdr = TASK_HDR(parent);
        link = &hdr->child;
        for (child = *link; child; child = child->next)
            link = &child->next;
        *link = task;
        task->magic = TASK_MAGIC;
        task->next = 0;
        task->child = 0;
        task->parent = hdr;
        task->exec = desc->exec;
        task->disp = desc->disp;
        task->release = desc->release;
        task->name = desc->name;
        task->flags = task_default_flags;
        task->dlist = 5;
        task++;
        heap_bzero(task, size);
        if (desc->init)
            desc->init(task);
    }
    return task;
}

/* prm passed to init. */
/* 0x00170540 */
void *task_create_arg(void *w, TaskDesc *desc, int size, int prm)
{
    TaskHdr *task;
    TaskHdr **link;
    TaskHdr *child;
    TaskHdr *hdr;

    task = heap_block_tag(game_malloc(size + 64), desc->name, -1);
    if (task) {
        perf_record_cnt++;
        hdr = TASK_HDR(w);
        link = &hdr->child;
        for (child = *link; child; child = child->next)
            link = &child->next;
        *link = task;
        task->magic = TASK_MAGIC;
        task->next = 0;
        task->child = 0;
        task->parent = hdr;
        task->exec = desc->exec;
        task->disp = desc->disp;
        task->release = desc->release;
        task->name = desc->name;
        task->flags = task_default_flags;
        task->dlist = 5;
        task++;
        heap_bzero(task, size);
        if (desc->init)
            ((void (*)(void *, int))desc->init)(task, prm);
    }
    return task;
}

/* Copies src's first two words, clears the rest (rounded up to words); nothing below two words. */
/* 0x00170680 */
static void task_work_inherit(int *dst, int *src, unsigned int size)
{
    int first;
    int second;

    if (size >= 8) {
        size -= 8;
        first = src[0];
        second = src[1];
        dst[0] = first;
        dst[1] = second;
        dst += 2;
        if (size) {
            size += 3;
            size >>= 2;
            do {
                *dst++ = 0;
            } while (--size);
        }
    }
}

/* Work starts with w's first two words instead of cleared. */
/* 0x001706d8 */
void *task_create_sibling_inherit(void *w, TaskDesc *desc, int size)
{
    TaskHdr *task;
    TaskHdr **link;
    TaskHdr *sib;
    TaskHdr *hdr;
    TaskHdr *par;

    task = heap_block_tag(game_malloc(size + 64), desc->name, -1);
    if (task) {
        perf_record_cnt++;
        hdr = TASK_HDR(w);
        par = hdr->parent;
        link = &hdr->next;
        for (sib = *link; sib; sib = sib->next)
            link = &sib->next;
        *link = task;
        task->magic = TASK_MAGIC;
        task->next = 0;
        task->child = 0;
        task->parent = par;
        task->exec = desc->exec;
        task->disp = desc->disp;
        task->release = desc->release;
        task->name = desc->name;
        task->flags = task_default_flags;
        task->dlist = 5;
        task++;
        task_work_inherit(task, w, size);
        if (desc->init)
            desc->init(task);
    }
    return task;
}

/* prm passed to init. */
/* 0x00170810 */
void *task_create_sibling_inherit_arg(void *w, TaskDesc *desc, int size, int prm)
{
    TaskHdr *task;
    TaskHdr **link;
    TaskHdr *sib;
    TaskHdr *hdr;
    TaskHdr *par;

    task = heap_block_tag(game_malloc(size + 64), desc->name, -1);
    if (task) {
        perf_record_cnt++;
        hdr = TASK_HDR(w);
        par = hdr->parent;
        link = &hdr->next;
        for (sib = *link; sib; sib = sib->next)
            link = &sib->next;
        *link = task;
        task->magic = TASK_MAGIC;
        task->next = 0;
        task->child = 0;
        task->parent = par;
        task->exec = desc->exec;
        task->disp = desc->disp;
        task->release = desc->release;
        task->name = desc->name;
        task->flags = task_default_flags;
        task->dlist = 5;
        task++;
        task_work_inherit(task, w, size);
        if (desc->init)
            ((void (*)(void *, int))desc->init)(task, prm);
    }
    return task;
}

/* Work starts with the parent work's first two words instead of cleared. */
/* 0x00170958 */
void *task_create_inherit(void *w, TaskDesc *desc, int size)
{
    TaskHdr *task;
    TaskHdr **link;
    TaskHdr *child;
    TaskHdr *hdr;

    task = heap_block_tag(game_malloc(size + 64), desc->name, -1);
    if (task) {
        perf_record_cnt++;
        hdr = TASK_HDR(w);
        link = &hdr->child;
        for (child = *link; child; child = child->next)
            link = &child->next;
        *link = task;
        task->magic = TASK_MAGIC;
        task->next = 0;
        task->child = 0;
        task->parent = hdr;
        task->exec = desc->exec;
        task->disp = desc->disp;
        task->release = desc->release;
        task->name = desc->name;
        task->flags = task_default_flags;
        task->dlist = 5;
        task++;
        task_work_inherit(task, w, size);
        if (desc->init)
            desc->init(task);
    }
    return task;
}

/* prm passed to init. */
/* 0x00170a90 */
void *task_create_inherit_arg(void *w, TaskDesc *desc, int size, int prm)
{
    TaskHdr *task;
    TaskHdr **link;
    TaskHdr *child;
    TaskHdr *hdr;

    task = heap_block_tag(game_malloc(size + 64), desc->name, -1);
    if (task) {
        perf_record_cnt++;
        hdr = TASK_HDR(w);
        link = &hdr->child;
        for (child = *link; child; child = child->next)
            link = &child->next;
        *link = task;
        task->magic = TASK_MAGIC;
        task->next = 0;
        task->child = 0;
        task->parent = hdr;
        task->exec = desc->exec;
        task->disp = desc->disp;
        task->release = desc->release;
        task->name = desc->name;
        task->flags = task_default_flags;
        task->dlist = 5;
        task++;
        task_work_inherit(task, w, size);
        if (desc->init)
            ((void (*)(void *, int))desc->init)(task, prm);
    }
    return task;
}
