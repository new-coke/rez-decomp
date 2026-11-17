/* EE timer 0; counts to microseconds. */

#ifndef REZ_PS_TIMER_H
#define REZ_PS_TIMER_H

float psTimerToUsec(int ticks);
int psTimerDiff(int now, int start);
int psTimerGetCount(void);
void psTimerExit(void);
void psTimerInit(void);

#endif /* REZ_PS_TIMER_H */
