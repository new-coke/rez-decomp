/* Debug GUI keyboard, line editor. */

#ifndef REZ_GW_KEYBOARD_H
#define REZ_GW_KEYBOARD_H

struct GwKbd;
struct GwLine;

int gw_keyboard_is_down(int key);
void gw_keyboard_poll(void);
void gw_line_clear(struct GwLine *line);
int gw_line_edit_key(struct GwLine *line);
int gw_keyboard_get_inject_ch(void);
int gw_keyboard_get_inject_key(void);

#endif /* REZ_GW_KEYBOARD_H */
