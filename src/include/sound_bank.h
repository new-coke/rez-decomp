/* Sound banks, queued sound DMA, sound debug windows. */

#ifndef REZ_SOUND_BANK_H
#define REZ_SOUND_BANK_H

struct SoundMpbLoadWork;
struct SoundDmaWork;

extern int sound_sd_err;

int sound_bank_debug_init(void);
void sound_mlt_load_file(void *path);
void sound_dma_exec(struct SoundDmaWork *w);
struct SoundDmaWork *sound_dma_create_bank(char *name, int buf, int size, int bank, int mode);
int sound_mpb_load_create_bpms(char *path, int mode);
int sound_mpb_load_create_bpfs(int path);
void sound_host_command_sample(void);

#endif /* REZ_SOUND_BANK_H */
