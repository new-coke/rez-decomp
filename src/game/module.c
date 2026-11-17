/*
 * Game modules: loadable code overlays (stages, menus, demos) and the task that runs one: load
 * into the 3 MB module heap, wait for files, change music, call the entry, wait for its task.
 */

#include "types_game.h"
#include "debug.h"
#include "module.h"
#include "camera.h"
#include "console.h"
#include "fade.h"
#include "file.h"
#include "game.h"
#include "heap.h"
#include "sound_midiseq.h"
#include "stage.h"
#include "task.h"

typedef struct ModuleEntry { char pad_00[4]; int (*fn)(int parent, int idx); int p[4]; } ModuleEntry;
typedef struct ModuleWork { char pad_00[4]; unsigned int state; } ModuleWork;
/* steer(alias): the code that matched reads ModuleWork through these member types; one view
 * shared by those sites (src/game/module.c). */
typedef struct ModuleWorkView {
    int idx;
    int state;
    int *file;
    int child;
    int end_task;
    int end_req;
    int music_no;
    int demo_task;
} ModuleWorkView;
/* Loaded overlay header; file_list: data files the module needs. */
typedef struct ModuleCodeObj { char pad_00[4]; int file_list; } ModuleCodeObj;
/* fov saved by module_demo_exec. */
typedef struct ModuleDemoWork { char pad_00[4]; int fov; } ModuleDemoWork;
typedef struct ModuleCode { void *(*fn)(void *); } ModuleCode;
/* steer(alias): the code that matched reads ModuleEntry through these member types; one view
 * shared by those sites (src/game/module.c). */
typedef struct ModuleEntryView {
    int p[2];
    ModuleCode *obj;
    char pad_0c[4];
    int file;
    char pad_14[4];
} ModuleEntryView;

typedef struct ModuleWorkIdxView {
    int idx;
    unsigned int state;
    int *file;
    void *child;
    int end_task;
    int end_req;
    int music_no;
} ModuleWorkIdxView;

/* steer(alias): the code that matched reads ModuleEntry through these member types; one view
 * shared by those sites (src/game/module.c). */
typedef struct ModuleEntryNameView {
    char *name;
    char pad_04[8];
    int end_req;
    int file;
    char pad_14[4];
} ModuleEntryNameView;

/* steer(alias): the code that matched reads ModuleWork through these member types; one view
 * shared by those sites (src/game/module.c). */
typedef struct ModuleWorkDemoTaskView {
    int idx;
    int state;
    int file;
    int child;
    void *end_task;
    char pad_14[4];
    int music_no;
    void *demo_task;
} ModuleWorkDemoTaskView;
typedef struct ModuleWorkChildView { char pad_00[4]; unsigned int state; char pad_08[4]; void *child; } ModuleWorkChildView;

typedef struct ModuleWorkWaitView { char pad_00[12]; void *child; char pad_10[4]; int end_req; } ModuleWorkWaitView;

typedef struct ModuleEntryLoadView { int p[4]; int file; char pad_14[4]; } ModuleEntryLoadView;

extern int module_heap_block;
extern int module_heap;
extern void soft_reset(char *);
extern char str_module[];
extern char str_module_heap[];
extern ModuleEntry module_entries[];
extern struct TaskDesc module_end_desc;
extern char str_module_start[];
extern int module_work;
extern struct TaskDesc module_desc;
extern struct TaskDesc module_stage_desc;
extern int scr_root;
extern struct TaskDesc module_demo_desc;
extern struct TaskDesc module_demo_end_desc;
extern int module_clear_flags[];

static int module_demo_task_create(void);
static int module_demo_end_task_create(int w);

/* 3 MB heap for overlays and their data. */
/* steer(cast) */
/* 0x0013e728 */
void module_heap_init(void)
{
    soft_reset(str_module);
    module_heap_block = ((int (*)(int, char *, int))heap_block_tag)(game_malloc_back(0x300000), str_module_heap, -1);
    module_heap = ((int (*)(int, int, char *))heap_create)(module_heap_block, 0x300000, str_module);
}

/* Dumps the block map, then selects heap 0. */
/* 0x0013e790 */
void module_heap_release(void)
{
    heap_set_current(module_heap);
    heap_map_dump(0, 0);
    heap_set_current(0);
}

/* Negative idx: nothing. */
/* 0x0013e7c8 */
int module_call_entry(int parent, int idx)
{
    if (idx < 0)
        return 0;
    return module_entries[idx].fn(parent, idx);
}

/* State 3 or later. */
/* 0x0013e808 */
static int module_is_running(ModuleWork *w)
{
    return w->state >= 3;
}

/* 0x0013e818 */
static void module_request_end(ModuleWorkView *w)
{
    w->end_req = 1;
}

/* 0x0013e828 */
int module_get_heap(void)
{
    return module_heap;
}

/* Requests the data files the module lists. */
/* 0x0013e838 */
void module_request_pairs(ModuleCodeObj *module)
{
    file_list_request(module->file_list);
}

/* Selects the module heap. */
/* 0x0013e840 */
static void model_heap_select_saved(void)
{
    heap_set_current(module_heap);
}

/* Selects heap 0. */
/* 0x0013e850 */
static void model_heap_reset_current(void)
{
    heap_set_current(0);
}

/* 0x0013e858 */
static void module_end_exec(void)
{
    model_heap_reset_current();
}

/* 0x0013e860 */
static void *module_end_task_create(int w)
{
    void *task = task_create_sibling(w, &module_end_desc, 0);

    TASK_OF(task)->flags |= TASK_F_RUN_PAUSED;
    return task;
}

/*
 * Module heap current. state: 0 start load, 1 wait overlay, request files, 2 wait files, music,
 * 3 wait end_req, call entry (task -> child), 4 clear child once it ends.
 */
/* 0x0013e898 */
static void module_update(ModuleWorkIdxView *w)
{
    ModuleEntryView *ent = &module_entries[w->idx];

    model_heap_select_saved();
    switch (w->state) {
    case 0:
        file_request_simple(w->file, 0);
        w->state = 1;
        break;
    case 1:
        if (file_request_status(w->file)) {
            file_load_sync(w->file, ent->obj, 0);
            module_request_pairs(ent->obj);
            w->state = 2;
        }
        break;
    case 2:
        if (file_request_all_done()) {
            if (w->music_no >= 0) {
                music_change(w->music_no);
                w->music_no = -1;
            }
            w->state = 3;
        }
        break;
    case 3:
        if (w->end_req) {
            w->child = ent->obj->fn(w);
            w->state = 4;
        }
        break;
    case 4:
        if (w->child == 0 || (C09_TASK(w->child)->flags & 1))
            w->child = 0;
        break;
    }
}

/* Overlay file and start flag from module table entry idx. */
/* 0x0013e9d0 */
static void module_task_init(ModuleWorkView *w, int idx)
{
    ModuleEntryNameView *ent = &module_entries[idx];
    int end_req;

    *(void **)&w->child = 0;
    *(void **)&w->state = 0;
    (*(ModuleWorkView * *)&module_work) = w; /* steer(cast) */
    w->file = &ent->file;
    end_req = ent->end_req;
    w->idx = idx;
    w->end_req = end_req;
    w->music_no = -1;
    w->end_task = module_end_task_create(w);
    w->demo_task = module_demo_task_create();
    DEBUG_PRINTF(str_module_start, ent->name);
}

/* Shared by both module tasks. */
/* 0x0013ea58 */
static void module_release(ModuleWorkDemoTaskView *w)
{
    do {
        if (w->demo_task)
            C09_TASK(w->demo_task)->flags |= 1;
    } while (0);
    if (w->end_task)
        C09_TASK(w->end_task)->flags |= 1;
    module_work = 0;
}

/* 0x0013ea98 */
static void module_exec(ModuleWorkChildView *w)
{
    module_update(w);
    if (w->state >= 4 && w->child == 0) {
        if (w)
            C09_TASK(w)->flags |= 1;
    }
}

/* 0x0013eaf0 */
static void *module_start_task_create(int parent, int prm)
{
    void *w = task_create_arg(parent, &module_desc, 32, prm);

    C09_TASK(w)->flags |= 8;
    return w;
}

/* "Module Stage" task: runs the stages in sequence. */
/* 0x0013eb28 */
static void module_stage_init(ModuleWorkView *w, int prm)
{
    module_work = w;
    w->idx = prm;
    w->music_no = -1;
    w->end_task = module_end_task_create(w);
    w->demo_task = module_demo_task_create();
    fade_start_out(0);
    console_set_disp_level(0);
    game_task_request(w);
    module_stage_setup(0);
}

/* 0x0013eb88 */
static void module_stage_exec(ModuleWorkChildView *w)
{
    module_update(w);
    if (w->state >= 4 && w->child == 0) {
        if (w)
            C09_TASK(w)->flags |= 1;
    }
}

/* Loads the next stage while the previous one (child) runs; sets end_req once it ends. */
/* 0x0013ebe0 */
static void module_stage_wait_exec(ModuleWorkWaitView *w)
{
    if (w->child != 0 && (C09_TASK(w->child)->flags & 1) == 0) {
        w->end_req = 0;
    } else {
        w->end_req = 1;
        C09_TASK(w)->exec = module_stage_exec;
    }
    module_update(w);
}

/* 0x0013ec20 */
static void *module_stage_task_create(int parent, int prm)
{
    void *w = task_create_arg(parent, &module_stage_desc, 32, prm);

    C09_TASK(w)->flags |= 8;
    return w;
}

/* 0x0013ec58 */
static void module_demo_init(int *w)
{
    TASK_OF(w)->flags |= TASK_F_RUN_PAUSED;
    *w = module_demo_end_task_create(w);
}

/* 0x0013ec90 */
static void module_demo_release(int **w)
{
    int *blk = *w;

    if (blk)
        TASK_OF(blk)->flags |= TASK_F_END;
}

/* Fixed fov 13653 (about 75 degrees) for the frame; "ModuleDemoEnd" restores it. */
/* 0x0013ecb0 */
static void module_demo_exec(ModuleDemoWork *w)
{
    w->fov = camera_get_fov();
    camera_set_fov_direct(13653);
    camera_update();
}

/* 0x0013ece0 */
static void module_demo_disp(void)
{
}

/* 0x0013ece8 */
static int module_demo_task_create(void)
{
    return task_create(scr_root, &module_demo_desc, 8);
}

/* 0x0013ed18 */
static void module_demo_end_init(int *w, int demo)
{
    unsigned int *fl = (unsigned int *)((char *)w - 36);
    unsigned int flags = *fl;

    *w = demo;
    *fl = flags | 8;
}

/* 0x0013ed30 */
static void module_demo_end_release(void)
{
}

/* 0x0013ed38 */
static void module_demo_end_exec(ModuleDemoWork **w)
{
    camera_set_fov_direct((*w)->fov);
    camera_update();
}

/* 0x0013ed58 */
static void module_demo_end_disp(void)
{
}

/* 0x0013ed60 */
static int module_demo_end_task_create(int w)
{
    return task_create_sibling_arg(w, &module_demo_end_desc, 4, w);
}

/* 0 when no module runs. */
/* 0x0013ed88 */
static int module_get_demo_task(void)
{
    if (module_work == 0)
        return 0;
    return (*(ModuleWorkView * *)&module_work)->demo_task; /* steer(cast) */
}

/* 0x0013eda8 */
int module_get_current(void)
{
    return module_work;
}

/* Points "Module Stage" at the next stage; start held until child has ended. */
/* 0x0013edb8 */
void module_stage_setup(int child)
{
    ModuleWorkDemoTaskView *w = module_work;
    int stage = stage_get_next();

    stage_set(stage);
    w->idx = 12;
    w->file = stage_info(stage);
    w->state = 0;
    w->music_no = stage_get_setup_arg(stage);
    w->child = child;
    C09_TASK(w)->exec = module_stage_wait_exec;
}

/* 0x0013ee38 */
void module_set_clear_flag(int idx, int flag)
{
    module_clear_flags[idx] = flag;
}

/* 0x0013ee50 */
int module_get_clear_flag(int idx)
{
    return module_clear_flags[idx];
}

/* 0x0013ee68 */
void module_entry_start_load(int idx)
{
    ModuleEntryLoadView *ent = &module_entries[idx];

    file_request_simple(&ent->file, 0);
}

/* 0x0013ee88 */
int module_file_status(int idx)
{
    ModuleEntryLoadView *ent = &module_entries[idx];

    return file_request_status(&ent->file);
}

/* Loads (waiting), requests the data files, returns the entry's result for prm. */
/* 0x0013eeb8 */
int module_start(int idx, int prm)
{
    ModuleEntryView *ent;

    ent = &module_entries[idx];

    file_load_sync(&ent->file, ent->obj, 0);
    module_request_pairs(ent->obj);
    return ent->obj->fn(prm);
}
