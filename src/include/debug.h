/*
 * Debug output, view-cone helpers, string parsing, debug HUD and flags. Release keeps every debug
 * print call and format string; the functions behind the macros are empty.
 */

#ifndef REZ_DEBUG_H
#define REZ_DEBUG_H

void debug_trace(const char *s);
void debug_tracef(const char *fmt, ...);
void debug_error_tracef(const char *fmt, ...);

#define DEBUG_PRINT(s)          debug_trace(s)
#define DEBUG_PRINTF(args...)   debug_tracef(args)
#define ERROR_PRINTF(args...)   debug_error_tracef(args)

struct Vec3;
struct DebugCommand;

extern int perf_record_cnt;
extern int view_cone_ang;
void set_view_params(float *eye, float *dir, int n);
int ray_hits(float *eye, float *dir, int angle, float *center, float radius);
int view_sphere_visible(struct Vec3 *pos, float r);
int view_sphere_visible_local(struct Vec3 *pos, float r);
float project_point(struct Vec3 *p, struct Vec3 *out);
float project_point_abs(float *p, float *out);
float project_xyz_size(float x, float y, float z, float *out, float *sx, float *sy);
float ratio_guarded(float dist, float size);
float view_unproject_size(float *pos, float size);
float view_project_size(float size, float *pos);
float safe_powf(float base, float power);
void matrix_window_draw(int win);
int matrix_window_create(int title, int x, int y, int mtx);
int line_length(const char *str);
char *copy_line(char *dst, const char *src, int n);
int token_compare(const char *str, const char *line);
int parse_hex_prefix(const char *str);
float str_to_float(const char *str);
int str_to_int(const char *str);
void format_number_commas(char *dst, int val);
void debug_init(void);
void debug_soft_init(void);
void debug_hud(void);
void debug_set_origin(int x, int y);
void debug_get_origin(int *px, int *py);
void debug_move_x(int n);
void debug_move_y(int n);
void debug_newline(void);
void debug_printf(const char *fmt, ...);
void debug_puts(char *str);
void debug_message_window_tick(int win);
void debug_message_window_draw(int win);
void debug_message_window_closed(int win);
void debug_message_window(int ax, int ay, int timer, int col, const char *fmt, ...);
void debug_error(const char *fmt, ...);
void debug_flag_toggle(int item, int val, int idx);
void debug_menu_init(void);
void debug_menu_refresh(void);
void debug_flags_set(unsigned mask);
void debug_flags_clear(unsigned mask);
int debug_flags_test(int mask);
void debug_command_add(int name, int help, int run);
void debug_command_remove(int name);
int debug_command_add_table(struct DebugCommand *cmd, int n);
void debug_command_remove_table(struct DebugCommand *cmd, int n);
void hard_reset(const char *name);
int debug_color_set(int attr);

#endif /* REZ_DEBUG_H */
