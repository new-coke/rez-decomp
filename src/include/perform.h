/* Performance timers and window. */

#ifndef REZ_PERFORM_H
#define REZ_PERFORM_H

void perform_hard_reset(void);
void perform_soft_reset(void);
int perform_timer_read(void);
float perform_elapsed(int start);
float perform_elapsed_cur(int start, int end);
void perform_window_toggle(void);

#endif /* REZ_PERFORM_H */
