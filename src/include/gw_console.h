/* Debug GUI console. */

#ifndef REZ_GW_CONSOLE_H
#define REZ_GW_CONSOLE_H

struct GwConsole;
struct GwConLine;

void gw_console_init(int *cfg);
void gw_console_exit(void);
void gw_console_poll(void);
int gw_console_is_visible(void);
int gw_console_set_visible(int visible);
int gw_console_set_serial_out(int enable);
int gw_console_set_attr(int attr);
void gw_console_puts(char *s);
void gw_console_printf(char *fmt, ...);

#endif /* REZ_GW_CONSOLE_H */
