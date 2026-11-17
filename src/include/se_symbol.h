/* "ChangeSymbolSe" task work (12 bytes), its task header view. */

#ifndef REZ_SE_SYMBOL_H
#define REZ_SE_SYMBOL_H

typedef struct ChangeSymbolSe {
    int state;                  /* 0x00: exec step 0..3 */
    int obj;                    /* 0x04: change_symbol_se_obj at init; 0 kills the task */
    int id;                     /* 0x08: change_symbol_se_ids[arg] at init */
} ChangeSymbolSe;

/* 0x40-byte task header (task.h), used part. */
typedef struct SymTaskHdr {
    char pad_00[28];
    int flags;                  /* 0x1c: bit 0 = end the task */
    char pad_20[32];
} SymTaskHdr;

#define SYM_TASK(work) ((SymTaskHdr *)(work) - 1)

#endif
