// hook.h - inline hook interfaces (skeleton)
#pragma once
#include <ntddk.h>

typedef struct hv_inline_hook_t {
    void* target;
    void* detour;
    void* trampoline;
    int enabled;
} hv_inline_hook_t;

int hv_inline_hook_register(hv_inline_hook_t* h, void* target, void* detour);
int hv_inline_hook_enable(hv_inline_hook_t* h);
int hv_inline_hook_disable(hv_inline_hook_t* h);
