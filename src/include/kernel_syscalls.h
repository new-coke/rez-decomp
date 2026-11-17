/*
 * EE kernel system calls (hand-written assembly): threads, semaphores, cache, GS CRT, interrupts,
 * DMA, sceSif*.
 */

#ifndef REZ_KERNEL_SYSCALLS_H
#define REZ_KERNEL_SYSCALLS_H

int ChangeThreadPriority(int tid, int prio);
int Deci2Call(int func, void *param);
int DeleteSema(int sema);
void ExitDeleteThread(void);
void FlushCache(int mode);
int GetThreadId(void);
int GsGetIMR(void);
int GsPutIMR(int mask);
int PollSema(int sema);
int ReferSemaStatus(int, int *);
int ReferThreadStatus(int tid, void *status);
int RemoveDmacHandler(int ch, int id);
void RemoveIntcHandler(int n, int id);
int ResumeThread(int tid);
int RotateThreadReadyQueue(int prio);
void SetGsCrt(int inter, int omode, int ffmd);
void SetVSyncFlag(volatile int *flag, volatile long *data);
int SignalSema(int sema);
void SleepThread(void);
int SuspendThread(int tid);
int WaitSema(int sema);
int WakeupThread(int tid);
void _Exit(int);
int _StartThread(int tid, void *arg);
void iFlushCache(int);
int iGsGetIMR(void);
void iGsPutIMR(int v);
int iReferThreadStatus(int tid, void *status);
int iSignalSema(int sema);
int sceSifDmaStat(int sema);
int sceSifGetReg(unsigned int reg);
void sceSifSetDChain(void);
int sceSifSetDma(void *tag, int n);
int sceSifSetReg(unsigned int reg, int val);
void sceSifStopDma(void);

#endif /* REZ_KERNEL_SYSCALLS_H */
