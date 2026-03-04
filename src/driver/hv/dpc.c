// dpc.c - per-cpu dpc helpers
#include "driver/core/dpc.h"

static VOID hv_dpc_routine(KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2) {
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);
    hv_dpc_t* d = (hv_dpc_t*)DeferredContext;
    if (d) KeSetEvent(&d->done, IO_NO_INCREMENT, FALSE);
}

int hv_dpc_init(hv_dpc_t* dpcs, ULONG cpu_count) {
    if (!dpcs || cpu_count == 0) return STATUS_INVALID_PARAMETER;
    for (ULONG i = 0; i < cpu_count; ++i) {
        dpcs[i].cpu = i;
        KeInitializeEvent(&dpcs[i].done, NotificationEvent, FALSE);
        KeInitializeDpc(&dpcs[i].dpc, hv_dpc_routine, &dpcs[i]);
        KeSetTargetProcessorDpc(&dpcs[i].dpc, (CCHAR)i);
    }
    return STATUS_SUCCESS;
}

void hv_dpc_shutdown(hv_dpc_t* dpcs, ULONG cpu_count) {
    if (!dpcs) return;
    for (ULONG i = 0; i < cpu_count; ++i) {
        KeResetEvent(&dpcs[i].done);
    }
}

int hv_dpc_ping_cpu(hv_dpc_t* dpc, ULONG timeout_ms) {
    if (!dpc) return STATUS_INVALID_PARAMETER;
    KeResetEvent(&dpc->done);
    if (!KeInsertQueueDpc(&dpc->dpc, NULL, NULL)) return STATUS_UNSUCCESSFUL;
    LARGE_INTEGER timeout;
    timeout.QuadPart = -(LONGLONG)timeout_ms * 10000LL;
    NTSTATUS st = KeWaitForSingleObject(&dpc->done, Executive, KernelMode, FALSE, &timeout);
    return (st == STATUS_SUCCESS) ? STATUS_SUCCESS : STATUS_TIMEOUT;
}
