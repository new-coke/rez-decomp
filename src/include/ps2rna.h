/* PS2 sound output handles, channel buffers. */

#ifndef REZ_PS2RNA_H
#define REZ_PS2RNA_H

#include "types_cri.h"

struct SjObj;

extern void *ps2rna_dtx;
extern int ps2rna_dtx_bsize;
extern char ps2rna_dtx_buf[];
extern char *ps2rna_dtx_sbuf;
extern int ps2rna_init_count;
extern char *ps2rna_iop_buf;
extern char *ps2rna_iop_buf_aligned;
extern int ps2rna_npsj;
extern Ps2rnaObj ps2rna_obj_tbl[];
extern PsjObj ps2rna_psj_tbl[];
extern int ps2rna_psj_wksize;
extern char *ps2rna_psj_work;
extern char *ps2rna_psj_work_aligned;
extern signed char ps2rna_psj_work_own;
extern char ps2rna_sjuni_buf[];
extern volatile int ps2rna_ver_str;
extern int ps2rna_vol_tbl[];

void PS2RNA_Init(void);
void ps2rna_lib_finish(void);
Ps2rnaObj *ps2rna_create(struct SjObj **sj, int nch);
void ps2rna_destroy(Ps2rnaObj *rna);
void PS2RNA_Start(Ps2rnaObj *rna);
void ps2rna_stop(Ps2rnaObj *rna);
void ps2rna_set_transfer_sw(Ps2rnaObj *rna, int sw);
void ps2rna_set_play_sw(Ps2rnaObj *rna, int sw);
void ps2rna_get_time(Ps2rnaObj *rna, int *type, int *rate);
unsigned int ps2rna_get_free_smpl(Ps2rnaObj *rna);
unsigned int ps2rna_get_data_smpl(Ps2rnaObj *rna);
void ps2rna_exec_server(void);
void PS2RNA_SetStartSmpl(Ps2rnaObj *rna);
void ps2rna_set_nch(Ps2rnaObj *rna, int n);
void ps2rna_set_sfreq(Ps2rnaObj *rna, int sfreq);
void ps2rna_set_out_vol(Ps2rnaObj *rna, int vol);
void ps2rna_set_out_pan(Ps2rnaObj *rna, int ch, int pan);
void PS2RNA_SetBitPerSmpl(Ps2rnaObj *rna, int bps);
int ps2rna_get_sfreq(Ps2rnaObj *rna);
int ps2rna_get_out_vol(Ps2rnaObj *rna);
int ps2rna_get_out_pan(Ps2rnaObj *rna, int ch);
int ps2rna_get_bit_per_smpl(Ps2rnaObj *rna);
int PS2RNA_DiscardData(Ps2rnaObj *rna);
void PS2RNA_SetStmHdInfo(Ps2rnaObj *rna);
struct SjObj *ps2rna_get_sjuni(Ps2rnaObj *rna, int ch);
void *ps2rna_get_sjrmt(Ps2rnaObj *rna, int ch);

#endif /* REZ_PS2RNA_H */
