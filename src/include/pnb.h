/* Model pack loader: chunk handlers, relocation, texture objects. */

#ifndef REZ_PNB_H
#define REZ_PNB_H

struct PnbHooks;
struct PnbHandler;
struct PnbEntry;
struct PnbHead;
struct PnbHandlerNode;
struct C17Nd;
struct RelocEntry;
struct RelocBlock;
struct TexObject;
struct TexObjectNode;
struct TexPtmObject;
struct TexSource;
struct TexObjectHead;

void pnb_install_hooks(struct PnbHooks *hooks);
void pnb_free_handlers(void);
void pnb_add_handler(struct PnbHandler *src);
void pnb_remove_handler(char *key);
void *pnb_register(void *data);
unsigned char pnb_get_version(unsigned char *hdr);
struct PnbEntry *pnb_get_entry(struct PnbHead *head, unsigned int idx);
void tex_object_release(void *obj);
void *tex_object_create_upload(void *src, int user);
void *tex_object_create(void *src, int user);
int tex_object_reload(int *obj);
int tex_object_update(int *obj, int x);
void screen_image_draw(int idx, int col, float z);
void mem_hooks_set(int *hooks);
void mem_hooks_init(void);

#endif /* REZ_PNB_H */
