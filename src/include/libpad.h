/* Controller library. */

#ifndef REZ_LIBPAD_H
#define REZ_LIBPAD_H

struct PadFrame;

int scePadInit(int mode);
int scePadEnd(void);
int scePadPortOpen(int port, int slot, int addr);
int scePadPortClose(int port, int slot);
int scePadGetFrameCount(int port, int slot);
int scePadRead(int port, int slot, unsigned char *data);
int scePadGetState(int port, int slot);
void scePadStateIntToStr(unsigned int state, char *str);
int scePadSetReqState(int port, int slot, int state);
int scePadGetReqState(int port, int slot);
int scePadInfoComb(int port, int slot, int offs, int term);
int scePadInfoAct(int port, int slot, int offs, int term);
int scePadInfoMode(int port, int slot, int term, int offs);
int scePadSetMainMode(int port, int slot, int offs, int lock);
int scePadSetActDirect(int port, int slot, unsigned char *data);
int scePadSetActAlign(int port, int slot, unsigned char *data);
int scePadGetButtonMask(int port, int slot);
int scePadSetButtonInfo(int port, int slot, int mode);
int scePadInfoPressMode(int port, int slot);
int scePadEnterPressMode(int port, int slot);
int scePadExitPressMode(int port, int slot);
void libpad_bzero(unsigned char *p, int n);

#endif /* REZ_LIBPAD_H */
