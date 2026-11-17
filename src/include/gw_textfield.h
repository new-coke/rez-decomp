/* Debug GUI text fields. */

#ifndef REZ_GW_TEXTFIELD_H
#define REZ_GW_TEXTFIELD_H

#include "types_gw.h"

struct GwTextFieldView;
struct GwTextField;
struct GwWin;

void gwTextFieldSetEnterCallback(struct GwTextFieldView *tf, void (*cb)(void *));
int gwTextFieldIsDone(struct GwTextField *tf);
void gwTextFieldSetText(struct GwTextField *tf, char *s);
char *gwTextFieldGetText(struct GwTextField *tf);
struct GwTextField *gwTextFieldCreate(char *text, GwTextFieldFn on_enter, int x, int y, int width, int height, struct GwWin *parent);

#endif /* REZ_GW_TEXTFIELD_H */
