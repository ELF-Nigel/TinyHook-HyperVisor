// feature.c - hv feature registry (non-operational)
#include "driver/core/feature.h"
#include <string.h>

static hv_feature_t g_features[HV_FEATURE_CAPACITY];
static ULONG g_feature_count = 0;
static KSPIN_LOCK g_feature_lock;
static LONG g_feature_init = 0;

NTSTATUS hv_feature_registry_init(VOID) {
    if (InterlockedCompareExchange(&g_feature_init, 1, 0) == 0) {
        KeInitializeSpinLock(&g_feature_lock);
        RtlZeroMemory(g_features, sizeof(g_features));
        g_feature_count = 0;
    }
    return STATUS_SUCCESS;
}

NTSTATUS hv_feature_register(const hv_feature_t* Feature) {
    if (!Feature || Feature->Id == HV_FEAT_ID_INVALID) return STATUS_INVALID_PARAMETER;
    if (g_feature_init == 0) hv_feature_registry_init();

    KIRQL old_irql;
    KeAcquireSpinLock(&g_feature_lock, &old_irql);

    for (ULONG i = 0; i < g_feature_count; ++i) {
        if (g_features[i].Id == Feature->Id) {
            KeReleaseSpinLock(&g_feature_lock, old_irql);
            return STATUS_OBJECT_NAME_COLLISION;
        }
    }

    if (g_feature_count >= HV_FEATURE_CAPACITY) {
        KeReleaseSpinLock(&g_feature_lock, old_irql);
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    g_features[g_feature_count] = *Feature;
    g_features[g_feature_count].Name[HV_FEATURE_NAME_MAX] = '\0';
    g_feature_count++;

    KeReleaseSpinLock(&g_feature_lock, old_irql);
    return STATUS_SUCCESS;
}

NTSTATUS hv_feature_enable(ULONG FeatureId, BOOLEAN Enable) {
    if (FeatureId == HV_FEAT_ID_INVALID) return STATUS_INVALID_PARAMETER;
    if (g_feature_init == 0) hv_feature_registry_init();

    KIRQL old_irql;
    KeAcquireSpinLock(&g_feature_lock, &old_irql);
    for (ULONG i = 0; i < g_feature_count; ++i) {
        if (g_features[i].Id == FeatureId) {
            g_features[i].Enabled = Enable ? TRUE : FALSE;
            KeReleaseSpinLock(&g_feature_lock, old_irql);
            return STATUS_SUCCESS;
        }
    }
    KeReleaseSpinLock(&g_feature_lock, old_irql);
    return STATUS_NOT_FOUND;
}

ULONG hv_feature_count(VOID) {
    return g_feature_count;
}

const hv_feature_t* hv_feature_get(ULONG Index) {
    if (Index >= g_feature_count) return NULL;
    return &g_features[Index];
}

VOID hv_feature_snapshot(hv_feature_snapshot_t* Out) {
    if (!Out) return;
    RtlZeroMemory(Out, sizeof(*Out));
    KIRQL old_irql;
    KeAcquireSpinLock(&g_feature_lock, &old_irql);
    Out->Count = g_feature_count;
    for (ULONG i = 0; i < g_feature_count && i < HV_FEATURE_CAPACITY; ++i) {
        Out->Entries[i] = g_features[i];
    }
    KeReleaseSpinLock(&g_feature_lock, old_irql);
}

NTSTATUS hv_feature_register_defaults(VOID) {
    hv_feature_registry_init();

    const hv_feature_t defaults[] = {
        { HV_FEAT_ID_CPUID_MON, "cpuid monitor", FALSE, NULL, NULL },
        { HV_FEAT_ID_MSR_MON, "msr monitor", FALSE, NULL, NULL },
        { HV_FEAT_ID_CR_MON, "control register monitor", FALSE, NULL, NULL },
        { HV_FEAT_ID_DR_MON, "debug register monitor", FALSE, NULL, NULL },
        { HV_FEAT_ID_EPT_MON, "ept violation monitor", FALSE, NULL, NULL },
        { HV_FEAT_ID_MEM_TRACK, "memory access tracking", FALSE, NULL, NULL },
        { HV_FEAT_ID_EXEC_TRACK, "execution tracking", FALSE, NULL, NULL },
        { HV_FEAT_ID_CR3_TRACK, "cr3 tracking", FALSE, NULL, NULL },
        { HV_FEAT_ID_PROC_MON, "process monitor", FALSE, NULL, NULL },
        { HV_FEAT_ID_THREAD_MON, "thread monitor", FALSE, NULL, NULL },
        { HV_FEAT_ID_INTEGRITY_GUARD, "integrity guard", FALSE, NULL, NULL },
        { HV_FEAT_ID_EPT_HOOK, "ept hook", FALSE, NULL, NULL },
        { HV_FEAT_ID_BREAKPOINT_VIRT, "breakpoint virtualization", FALSE, NULL, NULL },
        { HV_FEAT_ID_MSR_REDIRECT, "msr redirection", FALSE, NULL, NULL },
        { HV_FEAT_ID_SYSCALL_MON, "syscall monitor", FALSE, NULL, NULL },
        { HV_FEAT_ID_IO_PORT_MON, "io port monitor", FALSE, NULL, NULL },
        { HV_FEAT_ID_VMCALL_MON, "vmcall monitor", FALSE, NULL, NULL },
        { HV_FEAT_ID_VMEXIT_LOG, "vmexit logging", FALSE, NULL, NULL },
        { HV_FEAT_ID_VMEXIT_STATS, "vmexit stats", FALSE, NULL, NULL },
        { HV_FEAT_ID_HEARTBEAT, "hypervisor heartbeat", FALSE, NULL, NULL }
    };

    NTSTATUS st = STATUS_SUCCESS;
    for (ULONG i = 0; i < (ULONG)(sizeof(defaults)/sizeof(defaults[0])); ++i) {
        NTSTATUS r = hv_feature_register(&defaults[i]);
        if (!NT_SUCCESS(r) && r != STATUS_OBJECT_NAME_COLLISION) st = r;
    }
    return st;
}
