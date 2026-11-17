/* Area choose screen: beat-driven area marks. */

#ifndef REZ_AREACHO_AC_BEAT_H
#define REZ_AREACHO_AC_BEAT_H

struct AcBeat;

int ac_beat_show_cb(unsigned int *flags, int node_idx, int area);
void ac_beat_init(struct AcBeat *w);
void ac_beat_release(struct AcBeat *w);
void ac_beat_exec(struct AcBeat *w);
void ac_beat_disp(struct AcBeat *w);
void *ac_beat_create(void *parent);

#endif /* REZ_AREACHO_AC_BEAT_H */
