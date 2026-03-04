// dpc.h - per-cpu dpc helpers
#pragma once
#include <ntddk.h>

typedef struct hv_dpc_t {
    KDPC dpc;
    KEVENT done;
    ULONG cpu;
} hv_dpc_t;

int hv_dpc_init(hv_dpc_t* dpcs, ULONG cpu_count);
void hv_dpc_shutdown(hv_dpc_t* dpcs, ULONG cpu_count);
int hv_dpc_ping_cpu(hv_dpc_t* dpc, ULONG timeout_ms);
