/* CRI library and stream joint locks. */

#ifndef REZ_CRI_LOCK_H
#define REZ_CRI_LOCK_H

extern int cri_lock_count;

void cri_lock_enter(void);
void cri_lock_leave(void);
void sj_lock(void);
void sj_unlock(void);

#endif /* REZ_CRI_LOCK_H */
