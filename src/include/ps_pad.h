/* Pad records, per-frame poll, analog mode, stick conversion. */

#ifndef REZ_PS_PAD_H
#define REZ_PS_PAD_H

/* PsPadRec.held: scePadRead's button bytes 2 and 3, inverted so 1 is pressed. */
#define PS2_L2          0x1
#define PS2_R2          0x2
#define PS2_L1          0x4
#define PS2_R1          0x8
#define PS2_TRIANGLE    0x10
#define PS2_CIRCLE      0x20
#define PS2_CROSS       0x40
#define PS2_SQUARE      0x80
#define PS2_SELECT      0x100
#define PS2_L3          0x200
#define PS2_R3          0x400
#define PS2_START       0x800
#define PS2_UP          0x1000
#define PS2_RIGHT       0x2000
#define PS2_DOWN        0x4000
#define PS2_LEFT        0x8000

struct PsPadRec;

unsigned char psPadGetPort(struct PsPadRec *pad);
unsigned char psPadGetSlot(struct PsPadRec *pad);
unsigned char psPadGetFlags(struct PsPadRec *pad);
int psPadGetType(struct PsPadRec *pad);
int psPadGetState(struct PsPadRec *pad);
int psPadChangeAnalogMode(int port, int slot);
void psPadStickToFloat(unsigned char x, unsigned char y, int dz, float *ox, float *oy);
struct PsPadRec *psPadGet(int port);
void psPadUpdate(void);
void psPadInit(void);

#endif /* REZ_PS_PAD_H */
