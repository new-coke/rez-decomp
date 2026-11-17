/* USB Trance Vibrator. */

#ifndef REZ_PERI_VIB_H
#define REZ_PERI_VIB_H

int peri_vib_init(int mode);
void peri_vib_refresh(void);
int peri_vib_set_act_all(int act);
int peri_vib_is_connected(void);
int peri_vib_get_count(void);

#endif /* REZ_PERI_VIB_H */
