// vmexit.h - vmexit routing (skeleton)
#pragma once
#include <ntddk.h>
#include "driver/arch/vmcs.h"

void vmexit_route(ULONG64 reason);

typedef void (*hv_vmexit_handler_t)(ULONG64 reason);

int hv_vmexit_register(ULONG16 reason, hv_vmexit_handler_t handler);
int hv_vmexit_unregister(ULONG16 reason);
void hv_vmexit_dispatch(ULONG64 reason);
int hv_vmexit_dispatch_if_registered(ULONG64 reason);
