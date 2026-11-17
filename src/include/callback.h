/* Event listener lists keyed by event number. */

#ifndef REZ_CALLBACK_H
#define REZ_CALLBACK_H

struct Listener;

void listener_table_clear(void);
struct Listener *listener_add(unsigned int idx, void (*fn)(int));
void listener_remove(unsigned int idx, struct Listener *target);
void listener_dispatch(int idx, int val);

#endif /* REZ_CALLBACK_H */
