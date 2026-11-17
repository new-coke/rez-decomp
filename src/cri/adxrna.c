/* ADX sound output (ADXRNA): ADXT's PCM output calls, passed through to PS2RNA (streams to IOP). */

#include "cri.h"
#include "libc.h"
#include "adxrna.h"
#include "ps2rna.h"


/* 0x001c7490 */
void adxrna_init(void)
{
    PS2RNA_Init();
}

/* 0x001c7498 */
void adxrna_finish(void)
{
    ps2rna_lib_finish();
}

/* 0x001c74a0 */
Ps2rnaObj *adxrna_create(struct SjObj **sj, int nch)
{
    return ps2rna_create(sj, nch);
}

/* Stops playback and transfer first. */
/* 0x001c74c0 */
void adxrna_destroy(Ps2rnaObj *rna)
{
    ps2rna_set_play_sw(rna, 0);
    ps2rna_set_transfer_sw(rna, 0);
    ps2rna_destroy(rna);
}

/* 0x001c74f8 */
static void adxrna_start(Ps2rnaObj *rna)
{
    PS2RNA_Start(rna);
}

/* 0x001c7500 */
static void adxrna_stop(Ps2rnaObj *rna)
{
    ps2rna_stop(rna);
}

/* 0x001c7508 */
void adxrna_set_transfer_sw(Ps2rnaObj *rna, int sw)
{
    ps2rna_set_transfer_sw(rna, sw);
}

/* 0x001c7510 */
void adxrna_set_play_sw(Ps2rnaObj *rna, int sw)
{
    ps2rna_set_play_sw(rna, sw);
}

/* 0x001c7518 */
void adxrna_get_time(Ps2rnaObj *rna, int *type, int *rate)
{
    ps2rna_get_time(rna, type, rate);
}

/* 0x001c7520 */
int adxrna_get_free_smpl(Ps2rnaObj *rna)
{
    return ps2rna_get_free_smpl(rna);
}

/* 0x001c7540 */
int adxrna_get_data_smpl(Ps2rnaObj *rna)
{
    return ps2rna_get_data_smpl(rna);
}

/* Unimplemented in the library: prints a message and hangs. */
/* 0x001c7560 */
static int ADXRNA_GetStat(void)
{
    printf("ADXRNA_GetStat: not implemented");
    for (;;)
        ;
}

/* 0x001c7598 */
void adxrna_exec_server(void)
{
    ps2rna_exec_server();
}

/* 0x001c75a0 */
static void adxrna_set_start_smpl(Ps2rnaObj *rna)
{
    PS2RNA_SetStartSmpl(rna);
}

/* 0x001c75a8 */
void adxrna_set_nch(Ps2rnaObj *rna, int n)
{
    ps2rna_set_nch(rna, n);
}

/* 0x001c75b0 */
void adxrna_set_sfreq(Ps2rnaObj *rna, int sfreq)
{
    ps2rna_set_sfreq(rna, sfreq);
}

/* 0x001c75b8 */
void adxrna_set_out_vol(Ps2rnaObj *rna, int vol)
{
    ps2rna_set_out_vol(rna, vol);
}

/* 0x001c75c0 */
void adxrna_set_out_pan(Ps2rnaObj *rna, int ch, int pan)
{
    ps2rna_set_out_pan(rna, ch, pan);
}

/* 0x001c75c8 */
void adxrna_set_bit_per_smpl(Ps2rnaObj *rna, int bps)
{
    PS2RNA_SetBitPerSmpl(rna, bps);
}

/* 0x001c75d0 */
static int adxrna_get_sfreq(Ps2rnaObj *rna)
{
    return ps2rna_get_sfreq(rna);
}

/* 0x001c75f0 */
static int adxrna_get_out_vol(Ps2rnaObj *rna)
{
    return ps2rna_get_out_vol(rna);
}

/* 0x001c7610 */
static int adxrna_get_out_pan(Ps2rnaObj *rna, int ch)
{
    return ps2rna_get_out_pan(rna, ch);
}

/* 0x001c7630 */
static int adxrna_get_bit_per_smpl(Ps2rnaObj *rna)
{
    return ps2rna_get_bit_per_smpl(rna);
}

/* 0x001c7650 */
int adxrna_discard_data(Ps2rnaObj *rna)
{
    return PS2RNA_DiscardData(rna);
}

/* 0x001c7670 */
static void adxrna_nop(void)
{
}

/* 0x001c7678 */
static void adxrna_nop_2(void)
{
}

/* Empty: the PS2 output does not need the total sample count. */
/* 0x001c7680 */
void adxrna_set_total_smpl(Ps2rnaObj *rna, int n)
{
}

/* 0x001c7688 */
static void adxrna_nop_3(void)
{
}

/* Returns 0. */
/* 0x001c7690 */
int adxrna_set_stm_hd_info(Ps2rnaObj *rna, void *hdr)
{
    PS2RNA_SetStmHdInfo(rna);
    return 0;
}
