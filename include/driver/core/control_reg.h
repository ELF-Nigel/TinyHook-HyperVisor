// control_reg.h - control register monitoring policy (non-operational)
#pragma once
#include <ntddk.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hv_cr_policy_t {
    ULONG TrackCr0;
    ULONG TrackCr3;
    ULONG TrackCr4;
    ULONG ProtectSmep;
    ULONG ProtectSmap;
    ULONG ProtectVmx;
} hv_cr_policy_t;

VOID hv_cr_policy_defaults(hv_cr_policy_t* Policy);
VOID hv_cr_policy_get(hv_cr_policy_t* OutPolicy);
NTSTATUS hv_cr_policy_set(const hv_cr_policy_t* Policy);
VOID hv_cr_on_access(ULONG CrNumber, ULONGLONG Value, ULONG IsWrite);

#ifdef __cplusplus
}
#endif
