/* On-screen debug text console. */

#ifndef REZ_UL_CONSOLE_H
#define REZ_UL_CONSOLE_H

struct UlConsole;

void ulConsoleDraw(void);
void ulConsoleSetCellSize(int width, int height);
void ulConsolePuts(char *str);
void ulConsolePrintf(const char *fmt, ...);
void ulConsoleSetCursor(int x, int y);
void ulConsoleSetScale(float scale);
void ulConsoleClear(void);
void ulConsoleExit(void);
void ulConsoleInit(struct UlConsole *k, int cols, int rows);

#endif /* REZ_UL_CONSOLE_H */
