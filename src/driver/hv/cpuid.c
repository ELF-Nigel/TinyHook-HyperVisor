// cpuid.c - cpuid policy (non-operational)
#include "driver/core/cpuid.h"
#include "driver/util/ringlog.h"
#include <ntddk.h>
#include <ntstrsafe.h>

static hv_cpuid_policy_t g_policy;
static LONG g_policy_init = 0;

static VOID hv_cpuid_policy_init_once(VOID) {
    if (InterlockedCompareExchange(&g_policy_init, 1, 0) == 0) {
        hv_cpuid_policy_defaults(&g_policy);
    }
}

VOID hv_cpuid_policy_defaults(hv_cpuid_policy_t* Policy) {
    if (!Policy) return;
    RtlZeroMemory(Policy, sizeof(*Policy));
    Policy->HideHypervisorBit = 1;
    Policy->MaskVmxBit = 1;
    Policy->MaskSvmBit = 1;
    RtlStringCchCopyA(Policy->Vendor, HV_CPUID_VENDOR_LEN + 1, "elfhypervisr");
}

VOID hv_cpuid_policy_get(hv_cpuid_policy_t* OutPolicy) {
    if (!OutPolicy) return;
    hv_cpuid_policy_init_once();
    *OutPolicy = g_policy;
}

NTSTATUS hv_cpuid_policy_set(const hv_cpuid_policy_t* Policy) {
    if (!Policy) return STATUS_INVALID_PARAMETER;
    hv_cpuid_policy_init_once();
    g_policy = *Policy;
    g_policy.Vendor[HV_CPUID_VENDOR_LEN] = '\0';
    return STATUS_SUCCESS;
}

VOID hv_cpuid_apply_policy(ULONG Leaf, ULONG Subleaf, hv_cpuid_regs_t* Regs) {
    if (!Regs) return;
    hv_cpuid_policy_init_once();

    // non-operational policy transform: record intent only
    if (g_policy.HideHypervisorBit) {
        Regs->Ecx &= ~(1u << 31);
    }
    if (g_policy.MaskVmxBit) {
        Regs->Ecx &= ~(1u << 5);
    }
    if (g_policy.MaskSvmBit) {
        Regs->Ecx &= ~(1u << 2);
    }
    if (Leaf == 0 && g_policy.Vendor[0] != '\0') {
        // write a synthetic vendor signature into the returned regs
        const CHAR* v = g_policy.Vendor;
        Regs->Ebx = *(const ULONG*)(v + 0);
        Regs->Edx = *(const ULONG*)(v + 4);
        Regs->Ecx = *(const ULONG*)(v + 8);
    }

    hv_ringlog_write(HV_LOG_EVT_CPUID, Leaf, Subleaf);
}
