/* Event listeners: 25 callback lists, one per event number. */

#include "types_game.h"
#include "callback.h"
#include "heap.h"

extern Listener *listener_lists[25];

/* 0x00104b00 */
void listener_table_clear(void)
{
    int *p = listener_lists;
    int n = 25;
    do {
        *p++ = 0;
    } while (--n);
}

/* 0 for a bad list number. Node allocated with 4 bytes, less than its two fields. */
/* 0x00104b38 */
Listener *listener_add(unsigned int idx, void (*fn)(int))
{
    Listener **slot;
    Listener *listener;
    Listener *last;

    if (idx >= 25)
        return 0;
    slot = &listener_lists[idx];
    listener = heap_block_tag(game_malloc(4), "callback.c", 95);
    do { listener->next = 0; } while (0);
    listener->fn = fn;
    last = *slot;
    if (last == 0) {
        *slot = listener;
    } else {
        while (last->next)
            last = *(Listener *volatile *)&last->next;
        last->next = listener;
    }
    return listener;
}

/* 0x00104be0 */
void listener_remove(unsigned int idx, Listener *target)
{
    Listener *cur;
    Listener *prev;
    Listener *head;

    if (target == 0 || idx >= 25)
        return;
    cur = listener_lists[idx];
    head = cur;
    prev = cur;
    for (; cur; prev = cur, cur = cur->next) {
        if (cur == target) {
            if (cur == head)
                listener_lists[idx] = cur->next;
            else
                prev->next = cur->next;
            heap_free(cur, 0);
            return;
        }
    }
}

/* In the order added. */
/* 0x00104c60 */
void listener_dispatch(int idx, int val)
{
    Listener *listener;

    for (listener = listener_lists[idx]; listener != 0; listener = listener->next) {
        if (listener->fn)
            listener->fn(val);
    }
}
