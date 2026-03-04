// stats.h - hv stats
#pragma once
#if defined(_KERNEL_MODE)
#include <ntddk.h>
#else
#include <stdint.h>
#endif

#if defined(_KERNEL_MODE)
typedef LONG64 elfhv_i64;
#else
typedef int64_t elfhv_i64;
#endif

typedef struct hv_stats_t {
    volatile elfhv_i64 vmexit_count;
    volatile elfhv_i64 cpuid_count;
    volatile elfhv_i64 rdtsc_count;
    volatile elfhv_i64 rdmsr_count;
    volatile elfhv_i64 wrmsr_count;
    volatile elfhv_i64 cr_access_count;
    volatile elfhv_i64 io_count;
    volatile elfhv_i64 ept_count;
    volatile elfhv_i64 vmcall_count;
    volatile elfhv_i64 last_reason;
    volatile elfhv_i64 reason_counts[256];
} hv_stats_t;

typedef struct hv_exit_stats_t {
    ULONGLONG TotalExits;
    ULONGLONG CpuidExits;
    ULONGLONG RdtscExits;
    ULONGLONG MsrReads;
    ULONGLONG MsrWrites;
    ULONGLONG CrAccessExits;
    ULONGLONG IoExits;
    ULONGLONG EptExits;
    ULONGLONG VmcallExits;
    ULONGLONG LastReason;
    ULONGLONG ReasonCounts[64];
} hv_exit_stats_t;

VOID hv_stats_init(VOID);
VOID hv_stats_on_vmexit(ULONG64 Reason);
VOID hv_stats_on_cpuid(VOID);
VOID hv_stats_on_rdtsc(VOID);
VOID hv_stats_on_rdmsr(VOID);
VOID hv_stats_on_wrmsr(VOID);
VOID hv_stats_on_cr_access(VOID);
VOID hv_stats_on_io(VOID);
VOID hv_stats_on_ept(VOID);
VOID hv_stats_on_vmcall(VOID);
VOID hv_exit_stats_snapshot(hv_exit_stats_t* Out);
