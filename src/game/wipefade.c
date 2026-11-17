/*
 * Screen transitions: one front end over the types; callers start one, wait until covered,
 * switch scenes, then let it uncover.
 */

#include "tvset.h"
#include "wipefade.h"

extern WipeType wipe_types[];
extern WipeType *wipe_cur_type;
extern void *sound_sequence_work;

/*
 * type: 0 cross fade, 1 white-out, 2 camera angle zoom, 3 shape fade, 4 door wipe; task under the
 * sequence task. 0 when the type has no start function.
 */
/* 0x00173f50 */
void *wipe_start(int type, int len, int col)
{
    void *w;

    wipe_cur_type = &wipe_types[type];
    w = 0;
    if (wipe_cur_type->start) {
        w = wipe_cur_type->create(sound_sequence_work);
        wipe_cur_type->start(w, len, col);
    }
    return w;
}

/* 1 without a task or callback. */
/* 0x00173fe8 */
int wipe_is_covered(void *w)
{
    if (w && wipe_cur_type->is_covered)
        return wipe_cur_type->is_covered(w);
    return 1;
}

/* 0x00174028 */
void wipe_finish(void *w, int n)
{
    if (w && wipe_cur_type->finish)
        wipe_cur_type->finish(w, n);
}

/* 1 without a task or callback. */
/* 0x00174060 */
int wipe_is_done(void *w)
{
    if (w && wipe_cur_type->is_done)
        return wipe_cur_type->is_done(w);
    return 1;
}
