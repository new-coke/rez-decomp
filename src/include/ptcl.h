/* Particle tasks, ptcl.dat tables. */

#ifndef REZ_PTCL_H
#define REZ_PTCL_H

struct PtclWorkMaxView;
struct PtclArg;
struct PtclWorkPartView;
struct PtclWork;
struct PtclWorkMaxView;
struct PtclWorkView;
struct PtclWorkPartView;
struct Vec3;
struct IVec3;
struct PtclWorkView;
struct PtclWorkView;
struct PtclWorkMaxView;
struct PtclWorkMaxView;
struct PtclWorkMaxView;
struct PtclWorkMaxView;
struct PtclTask;
struct PtclData;
struct PtclEntData;

struct PtclWorkMaxView *ptcl_task_create(void *parent, int cfg, void (*cb)(int), int cbarg);
struct PtclWorkMaxView *ptcl_task_create_loop(void *parent, int cfg, void (*cb)(int), int cbarg);
struct PtclWorkMaxView *ptcl_task_create_world(void *parent, int cfg, int drift, void (*cb)(int), int cbarg);
void ptcl_task_stop(struct PtclWorkView *w);
void ptcl_task_set_src(struct PtclWorkMaxView *w, int eye_src, int rot_src);
int ptcl_data_load_default(void);
void ptcl_data_free(struct PtclData *data);
struct PtclEntData *ptcl_data_get_entry(struct PtclData *data, int idx);

#endif /* REZ_PTCL_H */
