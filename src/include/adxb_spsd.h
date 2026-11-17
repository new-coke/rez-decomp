/* SPSD input for the ADX basic decoder. */

#ifndef REZ_ADXB_SPSD_H
#define REZ_ADXB_SPSD_H

#include "types_cri.h"

int adxb_decode_header_spsd(AdxbObj *adxb, void *hdr, int len);
void adxb_exec_spsd(AdxbObj *adxb);
int adxb_is_spsd(void *hdr);

#endif /* REZ_ADXB_SPSD_H */
