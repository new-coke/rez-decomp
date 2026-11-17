/* DMA helpers: handles of the nine EE channels; sends wait for the channel's last transfer. */

#include "libc.h"
#include "ps_dma.h"
#include "libdma.h"
#include "libgraph.h"

typedef struct DmaChan {
    void *chan;             /* 0 from sceDmaGetChan */
    int idx;                /* 4 */
    char pad_08[4];
} DmaChan;

#ifndef VERSION_US

typedef struct PsDmaSendDmaChan {
    void *chan;             /* 0 from sceDmaGetChan */
    int idx;                /* 4 */
    int pad8;
} PsDmaSendDmaChan;

#endif /* !VERSION_US */

extern DmaChan psDmaChannels[];

#ifndef VERSION_US

#endif /* !VERSION_US */

/* 0x001c1c98 */
static int psDmaSync(int ch)
{
    return sceDmaSync(psDmaChannels[ch].chan, 0, 0);
}

#ifndef VERSION_US

/* Waits for the GS paths to go idle first. */
/* 0x001c1cd0 */
void psDmaSend(int ch, int tags)
{
    PsDmaSendDmaChan *entry = &psDmaChannels[ch];

    sceGsSyncPath(0, 0);
    sceDmaSend(entry->chan, tags);
}

#endif /* !VERSION_US */

#ifdef VERSION_US

/* 0x001c1cd0 */
void psDmaSend(int ch, int tags)
{
    DmaChan *entry = &psDmaChannels[ch];

    sceDmaSync(entry->chan, 0, 0);
    sceDmaSend(entry->chan, tags);
}

#endif /* VERSION_US */

/* Normal-mode send of n quadwords. */
/* 0x001c1d28 */
void psDmaSendN(int ch, void *addr, int n)
{
    sceDmaSync(psDmaChannels[ch].chan, 0, 0);
    sceDmaSendN(psDmaChannels[ch].chan, addr, n);
}

/* Channels 0..8, then resets the DMAC. */
/* 0x001c1d90 */
void psDmaInit(void)
{
    int i;

    memset(psDmaChannels, 0, 108);
    for (i = 0; i < 9; i++) {
        psDmaChannels[i].idx = i;
        psDmaChannels[i].chan = sceDmaGetChan(i);
    }
    sceDmaReset(1);
}
