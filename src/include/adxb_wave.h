/* RIFF WAVE input for the ADX basic decoder. */

#ifndef REZ_ADXB_WAVE_H
#define REZ_ADXB_WAVE_H

#include "types_cri.h"

extern char *adxb_wave_data_tag;
extern char *adxb_wave_fmt_tag;

int adxb_decode_header_wave(AdxbObj *adxb, void *hdr, int len);
int adxb_is_wave(void *hdr);
void adxb_exec_wave(AdxbObj *adxb);

#endif /* REZ_ADXB_WAVE_H */
