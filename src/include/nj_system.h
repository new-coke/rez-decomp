/* Timers, heap, memory fill/copy, empty hardware calls. */

#ifndef REZ_NJ_SYSTEM_H
#define REZ_NJ_SYSTEM_H

int nj_timer_get_count(void);
int nj_timer_diff(int start, int end);
int nj_timer_to_usec(int ticks);
int nj_clock_read(void);
void nj_heap_create(void *mem, int size);
void *nj_malloc(int size);
void *nj_pad_slot_info(void);
int nj_lcd_get_orientation(void);
int nj_lcd_write(void);
int nj_lcd_is_ready(void);
void nj_pad_init(void);
void *nj_keyboard_report(void);
int *nj_keyboard_get_status(void);
short nj_to_short(int x);
int nj_sound_get_info(void);
int nj_sound_apply(void);
int nj_sound_set_mono(void);
int nj_sound_return_one(void);
int nj_sound_return_one_2(void);
int nj_video_get_broadcast(void);
int nj_video_get_cable(void);
int nj_video_return_one(void);
void nj_video_clear_outputs(int ignored_a, int ignored_b, unsigned long *out_a, unsigned long *out_b);
void nj_fill8(void *dst, int val, int n);
void nj_fill16(unsigned short *dst, unsigned short val, int n);
void nj_fill32(unsigned int *dst, unsigned int val, int n);
void nj_fill64(unsigned int *dst, unsigned int val, int n);
void nj_fill128(unsigned int *dst, unsigned int val, int n);
void nj_fill256(unsigned int *dst, unsigned int val, int n);
void nj_fill256_thunk(unsigned int *dst, unsigned int val, int n);
void nj_copy8(void *dst, const void *src, int n);
void nj_copy16(unsigned short *dst, unsigned short *src, int n);
void nj_copy32(unsigned int *dst, unsigned int *src, int n);
void nj_copy64(unsigned long *dst, unsigned long *src, int n);

#endif /* REZ_NJ_SYSTEM_H */
