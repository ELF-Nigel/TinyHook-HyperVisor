// control_reg.c - control register monitoring policy (non-operational)
#include "driver/core/control_reg.h"
#include "driver/util/ringlog.h"
#include <ntddk.h>

static hv_cr_policy_t g_policy;
static LONG g_policy_init = 0;

static VOID hv_cr_policy_init_once(VOID) {
    if (InterlockedCompareExchange(&g_policy_init, 1, 0) == 0) {
        hv_cr_policy_defaults(&g_policy);
    }
}

VOID hv_cr_policy_defaults(hv_cr_policy_t* Policy) {
    if (!Policy) return;
    RtlZeroMemory(Policy, sizeof(*Policy));
    Policy->TrackCr0 = 1;
    Policy->TrackCr3 = 1;
    Policy->TrackCr4 = 1;
    Policy->ProtectSmep = 1;
    Policy->ProtectSmap = 1;
    Policy->ProtectVmx = 1;
}

VOID hv_cr_policy_get(hv_cr_policy_t* OutPolicy) {
    if (!OutPolicy) return;
    hv_cr_policy_init_once();
    *OutPolicy = g_policy;
}

NTSTATUS hv_cr_policy_set(const hv_cr_policy_t* Policy) {
    if (!Policy) return STATUS_INVALID_PARAMETER;
    hv_cr_policy_init_once();
    g_policy = *Policy;
    return STATUS_SUCCESS;
}

VOID hv_cr_on_access(ULONG CrNumber, ULONGLONG Value, ULONG IsWrite) {
    hv_cr_policy_init_once();
    UNREFERENCED_PARAMETER(Value);

    // non-operational: log intent only
    if ((CrNumber == 0 && g_policy.TrackCr0) ||
        (CrNumber == 3 && g_policy.TrackCr3) ||
        (CrNumber == 4 && g_policy.TrackCr4)) {
        hv_ringlog_write(HV_LOG_EVT_CR, ((ULONGLONG)CrNumber << 32) | IsWrite, Value);
    }
}
