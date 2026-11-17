/* Module heap, module task, stage sequencing. */

#ifndef REZ_MODULE_H
#define REZ_MODULE_H

struct ModuleWork;
struct ModuleWorkView;
struct ModuleCodeObj;
struct ModuleWorkIdxView;
struct ModuleWorkView;
struct ModuleWorkDemoTaskView;
struct ModuleWorkChildView;
struct ModuleWorkView;
struct ModuleWorkWaitView;
struct ModuleDemoWork;

void module_heap_init(void);
void module_heap_release(void);
int module_call_entry(int parent, int idx);
int module_get_heap(void);
void module_request_pairs(struct ModuleCodeObj *module);
int module_get_current(void);
void module_stage_setup(int child);
void module_set_clear_flag(int idx, int flag);
int module_get_clear_flag(int idx);
void module_entry_start_load(int idx);
int module_file_status(int idx);
int module_start(int idx, int prm);

#endif /* REZ_MODULE_H */
