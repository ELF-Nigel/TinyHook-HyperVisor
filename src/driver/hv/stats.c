// stats.c - vmexit telemetry (non-operational)
#include "driver/core/stats.h"
#include <ntddk.h>

extern hv_stats_t g_hv_stats;

VOID hv_stats_init(VOID) {
    RtlZeroMemory(&g_hv_stats, sizeof(g_hv_stats));
}

static __forceinline VOID hv_stats_inc(volatile elfhv_i64* counter) {
    InterlockedIncrement64((volatile LONG64*)counter);
}

VOID hv_stats_on_vmexit(ULONG64 Reason) {
    hv_stats_inc(&g_hv_stats.vmexit_count);
    g_hv_stats.last_reason = (elfhv_i64)Reason;
    ULONG idx = (ULONG)(Reason & 0xFF);
    hv_stats_inc(&g_hv_stats.reason_counts[idx]);
}

VOID hv_stats_on_cpuid(VOID) { hv_stats_inc(&g_hv_stats.cpuid_count); }
VOID hv_stats_on_rdtsc(VOID) { hv_stats_inc(&g_hv_stats.rdtsc_count); }
VOID hv_stats_on_rdmsr(VOID) { hv_stats_inc(&g_hv_stats.rdmsr_count); }
VOID hv_stats_on_wrmsr(VOID) { hv_stats_inc(&g_hv_stats.wrmsr_count); }
VOID hv_stats_on_cr_access(VOID) { hv_stats_inc(&g_hv_stats.cr_access_count); }
VOID hv_stats_on_io(VOID) { hv_stats_inc(&g_hv_stats.io_count); }
VOID hv_stats_on_ept(VOID) { hv_stats_inc(&g_hv_stats.ept_count); }
VOID hv_stats_on_vmcall(VOID) { hv_stats_inc(&g_hv_stats.vmcall_count); }

VOID hv_exit_stats_snapshot(hv_exit_stats_t* Out) {
    if (!Out) return;
    RtlZeroMemory(Out, sizeof(*Out));
    Out->TotalExits = (ULONGLONG)g_hv_stats.vmexit_count;
    Out->CpuidExits = (ULONGLONG)g_hv_stats.cpuid_count;
    Out->RdtscExits = (ULONGLONG)g_hv_stats.rdtsc_count;
    Out->MsrReads = (ULONGLONG)g_hv_stats.rdmsr_count;
    Out->MsrWrites = (ULONGLONG)g_hv_stats.wrmsr_count;
    Out->CrAccessExits = (ULONGLONG)g_hv_stats.cr_access_count;
    Out->IoExits = (ULONGLONG)g_hv_stats.io_count;
    Out->EptExits = (ULONGLONG)g_hv_stats.ept_count;
    Out->VmcallExits = (ULONGLONG)g_hv_stats.vmcall_count;
    Out->LastReason = (ULONGLONG)g_hv_stats.last_reason;
    for (ULONG i = 0; i < 64; ++i) {
        Out->ReasonCounts[i] = (ULONGLONG)g_hv_stats.reason_counts[i];
    }
}
