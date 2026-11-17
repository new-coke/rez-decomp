/* Sound driver: command queue, IOP buffers, wave banks, MIDI handles. */

#ifndef REZ_ULSND_H
#define REZ_ULSND_H

#include "types_ul.h"
#include "types_gw.h"

struct UlHandle;
struct UlRec;

int ulsnd_set_fx_prg(unsigned int ch, int val);
int ulsnd_buffer_commands(void);
int ulsnd_is_busy(void);
int ulsnd_handle_send_midi(struct UlHandle *hn, int n, unsigned char *data);
int ulsnd_handle_all_off(struct UlHandle *hn);
int ulsnd_configure(int bufsize, int spu_area_size, int aux_size);
int ulsnd_alloc_buffers(int heap);
int ulsnd_connect(void);
int ulsnd_set_debug(int dbg);
int ulsnd_set_stereo(int stereo);
int sdSndClearFxPrg(void);
int sdMemBlkCreate(void * blk);
int sdMemBlkDestroy(void * blk);
int sdMemBlkSetPrm(int rec, int data, int size, unsigned int mode, int reserved);
int sdMemBlkGetStat(int rec, int *out);
int sdMltDownload(void * rec);
int sdBankDownload(int rec, unsigned int id, char idx);
int sdMemBlkSetTransferMode(void);
int sdMidiOpen(void *out);
int sdMidiClose(void *hn);
int sdMidiSetVol(void *hn, char level);
int ulsnd_update(void);
int ulsnd_build_channel_map(int *p);

extern UlSnd ulsnd_work;

#endif /* REZ_ULSND_H */
