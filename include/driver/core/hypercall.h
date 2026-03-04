// hypercall.h - hypercall interface (skeleton)
#pragma once
#include <ntddk.h>

typedef enum hv_hypercall_id {
    HV_HC_NOP = 0,
    HV_HC_PING = 1,
    HV_HC_QUERY = 2
} hv_hypercall_id;

NTSTATUS hv_hypercall_dispatch(ULONG id, void* in, ULONG in_len, void* out, ULONG out_len);
