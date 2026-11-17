/* Sound Beat clock: tempo, beat counters, position in the 1, 1/2, 1/4, 1/8 beat grids. */

#ifndef REZ_BEAT_CLOCK_H
#define REZ_BEAT_CLOCK_H

typedef struct BeatClock {
    float bpm;          /* 0x00 current tempo */
    int div;            /* 0x04 eighth-of-beat counter within the current beat */
    int haku;           /* 0x08 beat counter (haku = beat) */
    float interval[4];  /* 0x0c ticks per beat, 1/2, 1/4, 1/8 beat */
    float rest[4];      /* 0x1c ticks since each grid's last boundary */
    float ratio[4];     /* 0x2c rest / interval */
    float bar;          /* 0x3c position within a 4-beat bar, 0..1 */
} BeatClock;

typedef struct BeatGrid {
    int hit;            /* boundary reached this tick */
    float remain;       /* 1 - rest / interval */
} BeatGrid;

#endif
