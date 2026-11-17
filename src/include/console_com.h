/* Message console: cells, lists, text, event messages. */

#ifndef REZ_CONSOLE_COM_H
#define REZ_CONSOLE_COM_H

struct ConsoleCell;
struct ConsoleCom;
struct ConsoleSys;
struct SecObj;

void console_cell_clear(short *cell);
void console_cell_set(short *cell, short ch, short age);
void console_cell_age(struct ConsoleCell *cell);
void *console_draw_cell(struct ConsoleCell *cell, void *buf, float x, float y, float sx);
void console_clear(struct ConsoleCom *con);
void console_clear_line(struct ConsoleCom *con);
void console_com_init(struct ConsoleCom *con, int cols, int rows, int mode);
void console_free(struct ConsoleCom *con);
void console_scroll(struct ConsoleCom *con);
void console_update_cells(struct ConsoleCom *con);
void console_follow(struct ConsoleCom *parent, struct ConsoleCom *con);
void console_draw(struct ConsoleCom *con, float sx);
struct ConsoleCom *console_create(struct ConsoleCom **head, int cols, int rows, int mode);
void console_destroy(struct ConsoleCom **head, struct ConsoleCom *con);
struct ConsoleCom *console_create_follow(int cols, int rows);
struct ConsoleCom *console_create_fixed(int cols, int rows);
void console_destroy_follow(struct ConsoleCom *con);
void console_destroy_fixed(struct ConsoleCom *con);
void console_set_font_mode(struct ConsoleCom *con, int mode);
void console_set_mode(struct ConsoleCom *con, int mode);
void console_set_pos(struct ConsoleCom *con, float x, float y);
void console_get_pos(struct ConsoleCom *con, float *px, float *py);
void console_set_window(struct ConsoleCom *con, int width, int height);
void console_get_size(struct ConsoleCom *con, int *px, int *py);
void console_get_pixel_size(struct ConsoleCom *con, int *pw, int *ph);
void console_set_panel(struct ConsoleCom *con, int on);
void console_clear_home(struct ConsoleCom *con);
void console_clear_eol(struct ConsoleCom *con);
void console_set_cursor(struct ConsoleCom *con, int x, int y);
void console_get_cursor(struct ConsoleCom *con, int *px, int *py);
void console_newline(struct ConsoleCom *con);
void console_set_color(struct ConsoleCom *con, int col);
void console_set_panel_colors(struct ConsoleCom *con, int col_a, int col_b);
void console_reset_cursor_x(struct ConsoleCom *con);
int console_putc(struct ConsoleCom *con, int ch, unsigned flag);
int console_puts(struct ConsoleCom *con, const char *str, unsigned flag);
void console_com_task_init(struct ConsoleSys *work);
void console_com_task_release(struct ConsoleSys *work);
void console_com_task_exec(struct ConsoleSys *work);
void console_com_task_disp(struct ConsoleSys *work);
void *console_com_task_create(void *parent);
void console_msg_name(int **pp);
void console_msg_overload(void);
void console_msg_security_attack(struct SecObj **pp);
void console_msg_analyzing(void);
void console_msg_node(int *node);
void console_msg_level(int *p);
void console_msg_fire(int *p);
void console_msg_bonus(int *p);
void console_events_register(void);
void console_events_unregister(void);

#endif /* REZ_CONSOLE_COM_H */
