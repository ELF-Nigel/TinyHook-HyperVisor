// cpuid.h - cpuid virtualization policy (non-operational)
#pragma once
#include <ntddk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HV_CPUID_VENDOR_LEN 12

typedef struct hv_cpuid_regs_t {
    ULONG Eax;
    ULONG Ebx;
    ULONG Ecx;
    ULONG Edx;
} hv_cpuid_regs_t;

typedef struct hv_cpuid_policy_t {
    ULONG HideHypervisorBit;
    ULONG MaskVmxBit;
    ULONG MaskSvmBit;
    CHAR Vendor[HV_CPUID_VENDOR_LEN + 1];
    ULONG LeafMask[4];
} hv_cpuid_policy_t;

VOID hv_cpuid_policy_defaults(hv_cpuid_policy_t* Policy);
VOID hv_cpuid_policy_get(hv_cpuid_policy_t* OutPolicy);
NTSTATUS hv_cpuid_policy_set(const hv_cpuid_policy_t* Policy);
VOID hv_cpuid_apply_policy(ULONG Leaf, ULONG Subleaf, hv_cpuid_regs_t* Regs);

#ifdef __cplusplus
}
#endif
