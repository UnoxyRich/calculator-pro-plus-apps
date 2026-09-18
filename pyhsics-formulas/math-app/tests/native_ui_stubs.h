#ifndef MATHAPP_NATIVE_UI_STUBS_H
#define MATHAPP_NATIVE_UI_STUBS_H

#include <stddef.h>
#include <stdint.h>

enum {
    sk_Up = 1,
    sk_Down = 2,
    sk_Enter = 3,
    sk_Clear = 4
};

uint16_t os_GetKey(void);
void os_ClrHomeFull(void);
void os_NewLine(void);
void os_PutStrFull(const char *text);
void os_GetStringInput(const char *prompt, char *buffer, size_t capacity);

#endif
