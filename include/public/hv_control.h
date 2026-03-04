// hv_control.h - usermode/kernel control interface (non-operational hv actions)
#pragma once

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#include <winioctl.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define HV_CONTROL_VERSION 1

#define HV_CONTROL_DEVICE_NT L"\\Device\\hvcontrol"
#define HV_CONTROL_DEVICE_DOS L"\\DosDevices\\hvcontrol"

#define HVCTL_IOCTL_BASE 0x801
#define IOCTL_HV_GET_STATUS      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_HV_GET_CONFIG      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x802, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_HV_SET_CONFIG      CTL_CODE(FILE_DEVICE_UNKNOWN, 0x803, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_HV_ENABLE_FEATURE  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x804, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_HV_DISABLE_FEATURE CTL_CODE(FILE_DEVICE_UNKNOWN, 0x805, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define IOCTL_HV_QUERY_CPU       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x806, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_HV_GET_LOG         CTL_CODE(FILE_DEVICE_UNKNOWN, 0x807, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_HV_LIST_FEATURES   CTL_CODE(FILE_DEVICE_UNKNOWN, 0x808, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_HV_GET_EXIT_STATS  CTL_CODE(FILE_DEVICE_UNKNOWN, 0x809, METHOD_BUFFERED, FILE_READ_ACCESS)
#define IOCTL_HV_GET_HVLOG       CTL_CODE(FILE_DEVICE_UNKNOWN, 0x80A, METHOD_BUFFERED, FILE_READ_ACCESS)

typedef enum hvctl_result_t {
    HVCTL_RES_OK = 0,
    HVCTL_RES_NOT_SUPPORTED = 1,
    HVCTL_RES_INVALID = 2,
    HVCTL_RES_BUSY = 3,
    HVCTL_RES_ERROR = 4
} hvctl_result_t;

typedef enum hvctl_driver_state_t {
    HVCTL_DRIVER_STOPPED = 0,
    HVCTL_DRIVER_STARTING = 1,
    HVCTL_DRIVER_ACTIVE = 2,
    HVCTL_DRIVER_ERROR = 3
} hvctl_driver_state_t;

typedef enum hvctl_hv_state_t {
    HVCTL_HV_OFF = 0,
    HVCTL_HV_INITIALIZED = 1,
    HVCTL_HV_RUNNING = 2
} hvctl_hv_state_t;

typedef enum hvctl_feature_id_t {
    HVCTL_FEAT_VMX = 1,
    HVCTL_FEAT_EPT = 2,
    HVCTL_FEAT_VPID = 3,
    HVCTL_FEAT_SVM = 4,
    HVCTL_FEAT_NPT = 5
} hvctl_feature_id_t;

#define HVCTL_CFG_VMX  (1u << 0)
#define HVCTL_CFG_EPT  (1u << 1)
#define HVCTL_CFG_VPID (1u << 2)
#define HVCTL_CFG_SVM  (1u << 3)
#define HVCTL_CFG_NPT  (1u << 4)

#define HVCTL_STATUS_STAGED_ONLY (1u << 0)

typedef struct hvctl_header_t {
    ULONG Version;
    ULONG Command;
    ULONG Size;
    ULONG Flags;
    ULONGLONG CorrelationId;
} hvctl_header_t;

typedef struct hvctl_status_t {
    ULONG Version;
    ULONG DriverState;
    ULONG HvState;
    ULONG FeatureFlags;
    ULONG ConfigFlags;
    ULONG LastError;
    ULONG PolicyFlags;
    ULONGLONG LastUpdate100ns;
    ULONGLONG ConfigRevision;
    ULONGLONG LogSeq;
} hvctl_status_t;

typedef struct hvctl_config_t {
    ULONG Flags;
    ULONG LogLevel;
    ULONG VmexitSample;
    ULONG Reserved;
} hvctl_config_t;

typedef struct hvctl_feature_t {
    ULONG FeatureId;
    ULONG Enable;
} hvctl_feature_t;

#define HVCTL_LOG_ENTRY_LEN 96
#define HVCTL_LOG_MAX_ENTRIES 4

typedef struct hvctl_log_chunk_t {
    ULONGLONG Cursor;
    ULONG Count;
    CHAR Entries[HVCTL_LOG_ENTRY_LEN * HVCTL_LOG_MAX_ENTRIES];
} hvctl_log_chunk_t;

typedef struct hvctl_feature_entry_t {
    ULONG Id;
    ULONG Enabled;
    CHAR Name[HVCTL_LOG_ENTRY_LEN];
} hvctl_feature_entry_t;

typedef struct hvctl_feature_list_t {
    ULONG Count;
    hvctl_feature_entry_t Entries[32];
} hvctl_feature_list_t;

typedef struct hvctl_exit_stats_t {
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
} hvctl_exit_stats_t;

typedef struct hvctl_hvlog_entry_t {
    ULONGLONG Timestamp100ns;
    ULONG EventType;
    ULONG Reserved;
    ULONGLONG Data1;
    ULONGLONG Data2;
} hvctl_hvlog_entry_t;

#define HVCTL_HVLOG_MAX_ENTRIES 8

typedef struct hvctl_hvlog_chunk_t {
    ULONGLONG Cursor;
    ULONG Count;
    ULONG Dropped;
    hvctl_hvlog_entry_t Entries[HVCTL_HVLOG_MAX_ENTRIES];
} hvctl_hvlog_chunk_t;

typedef struct hvctl_request_t {
    hvctl_header_t Header;
    union {
        hvctl_config_t Config;
        hvctl_feature_t Feature;
        ULONGLONG LogCursor;
        ULONGLONG HvLogCursor;
    } Payload;
} hvctl_request_t;

typedef struct hvctl_response_t {
    hvctl_header_t Header;
    ULONG Result;
    union {
        hvctl_status_t Status;
        hvctl_config_t Config;
        hvctl_log_chunk_t Log;
        hvctl_feature_list_t Features;
        hvctl_exit_stats_t ExitStats;
        hvctl_hvlog_chunk_t HvLog;
    } Payload;
} hvctl_response_t;

#ifdef __cplusplus
}
#endif
