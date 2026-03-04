// msr.h - msr monitoring policy (non-operational)
#pragma once
#include <ntddk.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum hv_msr_access_t {
    HV_MSR_READ = 0,
    HV_MSR_WRITE = 1
} hv_msr_access_t;

typedef struct hv_msr_policy_t {
    ULONG EnableBitmap;
    ULONG TrackSyscallMsr;
    ULONG TrackDebugMsr;
    ULONG TrackEfer;
    ULONG TrackPat;
} hv_msr_policy_t;

VOID hv_msr_policy_defaults(hv_msr_policy_t* Policy);
VOID hv_msr_policy_get(hv_msr_policy_t* OutPolicy);
NTSTATUS hv_msr_policy_set(const hv_msr_policy_t* Policy);
VOID hv_msr_on_access(hv_msr_access_t Access, ULONG Msr, ULONGLONG Value);

#ifdef __cplusplus
}
#endif
