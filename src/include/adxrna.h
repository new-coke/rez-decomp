/* ADX sound output over PS2RNA. */

#ifndef REZ_ADXRNA_H
#define REZ_ADXRNA_H

#include "types_cri.h"

struct SjObj;

void adxrna_init(void);
void adxrna_finish(void);
Ps2rnaObj *adxrna_create(struct SjObj **sj, int nch);
void adxrna_destroy(Ps2rnaObj *rna);
void adxrna_set_transfer_sw(Ps2rnaObj *rna, int sw);
void adxrna_set_play_sw(Ps2rnaObj *rna, int sw);
void adxrna_get_time(Ps2rnaObj *rna, int *type, int *rate);
int adxrna_get_free_smpl(Ps2rnaObj *rna);
int adxrna_get_data_smpl(Ps2rnaObj *rna);
void adxrna_exec_server(void);
void adxrna_set_nch(Ps2rnaObj *rna, int n);
void adxrna_set_sfreq(Ps2rnaObj *rna, int sfreq);
void adxrna_set_out_vol(Ps2rnaObj *rna, int vol);
void adxrna_set_out_pan(Ps2rnaObj *rna, int ch, int pan);
void adxrna_set_bit_per_smpl(Ps2rnaObj *rna, int bps);
int adxrna_discard_data(Ps2rnaObj *rna);
void adxrna_set_total_smpl(Ps2rnaObj *rna, int n);
int adxrna_set_stm_hd_info(Ps2rnaObj *rna, void *hdr);

#endif /* REZ_ADXRNA_H */
