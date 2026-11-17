/* Screen transitions: start, cover, uncover, done. */

#ifndef REZ_WIPEFADE_H
#define REZ_WIPEFADE_H

void *wipe_start(int type, int len, int col);
int wipe_is_covered(void *w);
void wipe_finish(void *w, int n);
int wipe_is_done(void *w);

#endif /* REZ_WIPEFADE_H */
