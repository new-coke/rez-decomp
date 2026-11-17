/* Area 1 light trails. */

#ifndef REZ_STG01_LINE_H
#define REZ_STG01_LINE_H

struct TrailPt;
struct Trail;
struct LineWork;
struct LineMngWork;

typedef struct TrailPt TrailPt;
typedef struct Trail Trail;
typedef struct LineWork LineWork;
typedef struct LineMngWork LineMngWork;

void line_draw_band(float *p0, float *p1, float alpha0, float r0, float g0, float b0, float alpha1, float r1, float g1, float b1);
void line_draw_segment(float alpha0, float alpha1, TrailPt *pa, TrailPt *pb);
void line_draw_points(float alpha, TrailPt *p, int n);
int line_extract(float from, float to, Trail *trl, TrailPt *out);
void line_init(LineWork *w, Trail *trl);
void line_release(LineWork *w);
void line_exec(LineWork *w);
void line_disp(LineWork *w);
void *line_create(void *parent, void *trl);
int line_count_nodes(unsigned int *nd, int cnt);
TrailPt *line_walk_nodes(unsigned int *nd, TrailPt *out, float *acc);
float line_walk_root(unsigned int *nd, TrailPt *out);
void line_trail_build(unsigned int *root, Trail *trl);
void line_trail_rebuild(unsigned int *root, Trail *trl);
void line_trail_free(Trail *trl);
void line_mng_init(LineMngWork *w);
void line_mng_release(LineMngWork *w);
void line_mng_exec(LineMngWork *w);
void line_mng_disp(void);
void *line_mng_create(int *w);

#endif /* REZ_STG01_LINE_H */
