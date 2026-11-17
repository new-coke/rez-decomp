/* AIFF input for the ADX basic decoder. */

#ifndef REZ_ADXB_AIFF_H
#define REZ_ADXB_AIFF_H

#include "types_cri.h"

int adxb_is_aiff(void *hdr);
int adxb_decode_header_aiff(AdxbObj *adxb, void *hdr, int len);
void adxb_exec_aiff(AdxbObj *adxb);

#endif /* REZ_ADXB_AIFF_H */
