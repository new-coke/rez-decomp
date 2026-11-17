/* Warning, select and info message boxes. */

#ifndef REZ_WARNING_H
#define REZ_WARNING_H

struct Warning;

int warning_request(unsigned int id, int sel);
void warning_cancel(void);
int warning_is_done(void);
int warning_get_result(void);

#endif /* REZ_WARNING_H */
