/* Stage SE tables for SoundRhythmSe_StgCommon/Start/Stop: note, velocity, grid. */

#ifndef REZ_SE_STAGE_H
#define REZ_SE_STAGE_H

typedef struct SeStageEntry {
    int ch;
    int prg;
    int note;
    int timing;         /* index into se_timings */
} SeStageEntry;

typedef struct SeTiming {
    int vel;
    int grid;
} SeTiming;

extern const SeStageEntry se_stg_common_table[];   /* 9 entries */
extern SeTiming se_timings[];

#endif
