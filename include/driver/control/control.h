// control.h - driver control plane (non-operational hv actions)
#pragma once
#include <ntddk.h>
#include "public/hv_control.h"
#include "driver/core/hv.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct hvctl_state_t {
    volatile LONG Initialized;
    KSPIN_LOCK Lock;
    PDEVICE_OBJECT Device;
    UNICODE_STRING DeviceName;
    UNICODE_STRING SymbolicName;
    hvctl_config_t StagedConfig;
    ULONGLONG ConfigRevision;
    ULONGLONG LogSeq;
    ULONG DriverState;
    ULONG LastError;
    hv_state_t* Backend;
} hvctl_state_t;

NTSTATUS hvctl_attach_driver(PDRIVER_OBJECT DriverObject);
VOID hvctl_detach_driver(VOID);
VOID hvctl_set_backend(hv_state_t* Hv);

#ifdef __cplusplus
}
#endif
