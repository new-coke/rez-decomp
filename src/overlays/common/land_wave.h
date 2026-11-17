/* Land waves: music-driven line waves. */

#ifndef REZ_COMMON_LAND_WAVE_H
#define REZ_COMMON_LAND_WAVE_H

#include "types_overlays.h"
#include "types_common.h"

void land_wave_heights(float *out);
void land_wave_init(LandWaveWork *w, LandWaveArg *prm);
void land_wave_release(LandWaveWork *w);
void land_wave_exec(LandWaveWork *w);
void land_wave_disp(LandWaveWork *w);
void *land_wave_create(void *parent, void *rec);
void land_wave_mng_count_nodes(unsigned int *node, int *count);
int land_wave_mng_record_nodes(unsigned int *node, int **out, int idx);
int land_wave_mng_next_node(float *recs, int n, Vec3 *pl, float rsq);
void land_wave_mng_load_path(LandWaveMngWork *w);
void land_wave_mng_init(LandWaveMngWork *w);
void land_wave_mng_release(LandWaveMngWork *w);
void land_wave_mng_exec(LandWaveMngWork *w);
void land_wave_mng_disp(void);
void *land_wave_mng_create(void *parent);
void land_wave_mng_set_variant(int variant);
void land_wave_rotate_zyx(int *ang);

#endif /* REZ_COMMON_LAND_WAVE_H */
