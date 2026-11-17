/* Memory card detection, mount, format, load, save. */

#ifndef REZ_LIBULMC_H
#define REZ_LIBULMC_H

struct McCardInfo;
struct McFileStat;

void ulMcConnectEvent(int op, int res);
void ulMcInit(int save_desc);
void ulMcPoll(void);
int ulMcGetFileBlocks(unsigned int which, char *path);
int ulMcFindFile(unsigned int which, char *path);
int ulMcLoadFile(unsigned int which, char *path, int buf, int size);
int ulMcLoadFileAt(unsigned int which, char *path, int buf, int blocks, int size);
int ulMcProbe(unsigned int which);
int ulMcGetFree(unsigned int card);
int ulMcCardToPort(unsigned int card);
int ulMcCardToSlot(unsigned int card);
int ulMcSetup(int port_mask, int slot_mask, int auto_mount, void (*cb)(void));
int ulMcDrainMessages(void);
int ulMcSaveFile(unsigned int card, int path, int buf, int blocks, void *x, int attr);
void ulMcSetCallback(int cb);
void ulMcSetSubCallback(int cb);
int mc_drain_events(void);
int mc_mount_request(unsigned int card);
int mc_calc_save_blocks(int kind, int reserved, unsigned int len);
int mc_unmount(void);
int mc_format(unsigned int card);
void mc_slot_mark_used(int port, int slot);
void mc_poll(void);
int mc_scan_step(void);
int mc_is_connected(int port, int slot);

#endif /* REZ_LIBULMC_H */
