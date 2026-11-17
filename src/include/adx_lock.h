#ifndef REZ_ADX_LOCK_H
#define REZ_ADX_LOCK_H

extern int adx_lock_count;

void adx_lock_init(void);
void adx_lock(void);
void adx_unlock(void);

#endif /* REZ_ADX_LOCK_H */
