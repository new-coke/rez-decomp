/*
 * PC files: queue of files for the host PC to fetch over the dev link (debug editors' saves);
 * each keeps a copy of its data until fetched.
 */

#include "types_game.h"
#include "color.h"
#include "debug.h"
#include "pcfile.h"
#include "heap.h"

extern char str_pcfile[];
extern PcFileEnt pcfile_entries[];
extern PcFileEnt *pcfile_cur;
extern unsigned int pcfile_count;
extern void soft_reset(char *name);
extern char str_pcfile_save_failed[];
extern char str_pcfile_save_failed_jp[];
extern char str_pcfile_save_set[];
extern char str_pcfile_file_save[];
extern char str_pcfile_c[];
extern char str_pcfile_pc_get[];

/* 0x0014e2f8 */
void pcfile_init(void)
{
    soft_reset(str_pcfile);
    pcfile_cur = pcfile_entries;
    pcfile_count = 0;
    do {
        pcfile_entries[0].name[0] = 0;
        pcfile_entries[0].done = 0;
        pcfile_entries[0].buf = 0;
        pcfile_entries[0].size = 0;
    } while (0);
}

/* Copies data; with 512 queued only reports the failure. */
/* 0x0014e340 */
void pcfile_save_set(char *name, char *data, int size)
{
    int i;
    int n;
    char *dst;
    char *src;
    char *buf;
    PcFileEnt *ent;

    if (pcfile_count >= 512) {
        ERROR_PRINTF(str_pcfile_save_failed, name);
        DEBUG_PRINT(str_pcfile_save_failed_jp);
        return;
    }
    DEBUG_PRINTF(str_pcfile_save_set, name, size);
    debug_message_window(1, 1, 120, RGBA(0, 0, 0x80, 0x80), str_pcfile_file_save, name);
    i = 0;
    while (*name != 0) {
        pcfile_cur->name[i] = *name++;
        i++;
    }
    pcfile_cur->name[i] = 0;
    pcfile_cur->done = 0;
    buf = heap_block_tag(game_malloc(size), str_pcfile_c, 98);
    pcfile_cur->size = size;
    pcfile_cur->buf = buf;
    src = data;
    dst = buf;
    do {
        *dst++ = *src++;
    } while (--size != 0);
    ent = pcfile_cur;
    pcfile_cur = ent + 1;
    pcfile_count = pcfile_count + 1;
    do { ent[1].name[0] = 0; } while (0);
    pcfile_cur->done = 0;
    pcfile_cur->buf = 0;
    pcfile_cur->size = 0;
}

/* Frees fetched files' data (ent->done); queue restarts once none is pending. */
/* 0x0014e500 */
void pcfile_update(void)
{
    int n = pcfile_count;
    int pending = 0;
    PcFileEnt *ent;

    if (n > 0) {
        ent = pcfile_entries;
        do {
            if (ent->buf != 0) {
                if (ent->done != 0) {
                    DEBUG_PRINTF(str_pcfile_pc_get, ent);
                    heap_free(ent->buf, 0);
                    ent->name[0] = 0;
                    ent->done = 0;
                    ent->buf = 0;
                    ent->size = 0;
                } else {
                    pending++;
                }
            }
            n--;
            ent++;
        } while (n > 0);
    }
    if (pending == 0) {
        pcfile_count = 0;
        pcfile_cur = pcfile_entries;
    }
}
