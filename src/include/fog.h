#ifndef REZ_FOG_H
#define REZ_FOG_H
#include "types_game.h"

struct FogEnt;
struct FogAnimWork;
struct FogMapObj;
struct FogMapEase;
struct FogMapWork;

int fog_create(void *parent);
void fog_set(int color, int mode, float near, float far);
void *fog_set_enable(void *enable);
void fog_req_color(int color);
void BackColReq(int color);
int fog_get_current(int *color, int *back, float *near, float *far);
int fog_get_color(void);
int fog_get_back_color(void);
float fog_get_near(void);
float fog_get_far(void);
int fog_get_work(void);

extern FogSlot fog_slots[];
extern int fog_cur;
extern FogMapObj *fog_map_cur;

#endif /* REZ_FOG_H */
