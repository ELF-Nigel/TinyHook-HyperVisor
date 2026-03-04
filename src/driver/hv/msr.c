// msr.c - msr monitoring policy (non-operational)
#include "driver/core/msr.h"
#include "driver/util/ringlog.h"
#include <ntddk.h>

static hv_msr_policy_t g_policy;
static LONG g_policy_init = 0;

static VOID hv_msr_policy_init_once(VOID) {
    if (InterlockedCompareExchange(&g_policy_init, 1, 0) == 0) {
        hv_msr_policy_defaults(&g_policy);
    }
}

VOID hv_msr_policy_defaults(hv_msr_policy_t* Policy) {
    if (!Policy) return;
    RtlZeroMemory(Policy, sizeof(*Policy));
    Policy->EnableBitmap = 1;
    Policy->TrackSyscallMsr = 1;
    Policy->TrackDebugMsr = 1;
    Policy->TrackEfer = 1;
    Policy->TrackPat = 1;
}

VOID hv_msr_policy_get(hv_msr_policy_t* OutPolicy) {
    if (!OutPolicy) return;
    hv_msr_policy_init_once();
    *OutPolicy = g_policy;
}

NTSTATUS hv_msr_policy_set(const hv_msr_policy_t* Policy) {
    if (!Policy) return STATUS_INVALID_PARAMETER;
    hv_msr_policy_init_once();
    g_policy = *Policy;
    return STATUS_SUCCESS;
}

VOID hv_msr_on_access(hv_msr_access_t Access, ULONG Msr, ULONGLONG Value) {
    hv_msr_policy_init_once();
    UNREFERENCED_PARAMETER(Value);

    // non-operational: record the access for telemetry/logging only
    if (g_policy.EnableBitmap) {
        hv_ringlog_write(HV_LOG_EVT_MSR, ((ULONGLONG)Access << 32) | Msr, Value);
    }
}
