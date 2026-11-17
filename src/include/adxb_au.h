/* Sun AU input for the ADX basic decoder. */

#ifndef REZ_ADXB_AU_H
#define REZ_ADXB_AU_H

#include "types_cri.h"

int adxb_is_au(void *hdr);
int adxb_decode_header_au(AdxbObj *adxb, void *hdr, int len);
void adxb_exec_au(AdxbObj *adxb);

#endif /* REZ_ADXB_AU_H */
