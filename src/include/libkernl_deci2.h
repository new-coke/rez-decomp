/* DECI2 debug sockets, debug console print. */

#ifndef REZ_LIBKERNL_DECI2_H
#define REZ_LIBKERNL_DECI2_H

int sceDeci2Open(unsigned short proto, void *opt, void (*handler)(int, int, void *));
int sceDeci2ReqSend(int sock, char dest);
int sceDeci2Poll(int sock);
int sceDeci2ExRecv(int sock, void *buf, unsigned short len);
int sceDeci2ExSend(int sock, void *buf, unsigned short len);
int libkernl_dbg_puts(char *str);

#endif /* REZ_LIBKERNL_DECI2_H */
