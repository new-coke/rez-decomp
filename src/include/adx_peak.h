/* Peak level meter. */

#ifndef REZ_ADX_PEAK_H
#define REZ_ADX_PEAK_H

#include "types_cri.h"

struct SjObj;

extern int adx_peak_init_count;
extern PkmObj adx_peak_obj_tbl[];

void adx_peak_destroy(PkmObj *obj);
void adx_peak_set_sfreq(PkmObj *obj, int rate);

#endif /* REZ_ADX_PEAK_H */
