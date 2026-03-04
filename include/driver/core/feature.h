// feature.h - hv feature registry (non-operational)
#pragma once
#include <ntddk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HV_FEATURE_NAME_MAX 31
#define HV_FEATURE_CAPACITY 32

typedef enum hv_feature_id_t {
    HV_FEAT_ID_INVALID = 0,
    HV_FEAT_ID_CPUID_MON = 1,
    HV_FEAT_ID_MSR_MON = 2,
    HV_FEAT_ID_CR_MON = 3,
    HV_FEAT_ID_DR_MON = 4,
    HV_FEAT_ID_EPT_MON = 5,
    HV_FEAT_ID_MEM_TRACK = 6,
    HV_FEAT_ID_EXEC_TRACK = 7,
    HV_FEAT_ID_CR3_TRACK = 8,
    HV_FEAT_ID_PROC_MON = 9,
    HV_FEAT_ID_THREAD_MON = 10,
    HV_FEAT_ID_INTEGRITY_GUARD = 11,
    HV_FEAT_ID_EPT_HOOK = 12,
    HV_FEAT_ID_BREAKPOINT_VIRT = 13,
    HV_FEAT_ID_MSR_REDIRECT = 14,
    HV_FEAT_ID_SYSCALL_MON = 15,
    HV_FEAT_ID_IO_PORT_MON = 16,
    HV_FEAT_ID_VMCALL_MON = 17,
    HV_FEAT_ID_VMEXIT_LOG = 18,
    HV_FEAT_ID_VMEXIT_STATS = 19,
    HV_FEAT_ID_HEARTBEAT = 20
} hv_feature_id_t;

typedef struct hv_feature_t {
    ULONG Id;
    CHAR Name[HV_FEATURE_NAME_MAX + 1];
    BOOLEAN Enabled;
    NTSTATUS (*Initialize)(VOID* Context);
    VOID (*HandleEvent)(VOID* Context);
} hv_feature_t;

typedef struct hv_feature_snapshot_t {
    ULONG Count;
    hv_feature_t Entries[HV_FEATURE_CAPACITY];
} hv_feature_snapshot_t;

NTSTATUS hv_feature_registry_init(VOID);
NTSTATUS hv_feature_register(const hv_feature_t* Feature);
NTSTATUS hv_feature_enable(ULONG FeatureId, BOOLEAN Enable);
ULONG hv_feature_count(VOID);
const hv_feature_t* hv_feature_get(ULONG Index);
VOID hv_feature_snapshot(hv_feature_snapshot_t* Out);
NTSTATUS hv_feature_register_defaults(VOID);

#ifdef __cplusplus
}
#endif
