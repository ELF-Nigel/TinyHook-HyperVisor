// ringlog.h - hypervisor ring log buffer (non-operational)
#pragma once
#include <ntddk.h>

#ifdef __cplusplus
extern "C" {
#endif

#define HV_RINGLOG_CAPACITY 256

typedef enum hv_log_event_t {
    HV_LOG_EVT_NONE = 0,
    HV_LOG_EVT_VMEXIT = 1,
    HV_LOG_EVT_CPUID = 2,
    HV_LOG_EVT_MSR = 3,
    HV_LOG_EVT_CR = 4,
    HV_LOG_EVT_IO = 5
} hv_log_event_t;

typedef struct hv_ringlog_entry_t {
    ULONGLONG Seq;
    ULONGLONG Timestamp100ns;
    ULONG EventType;
    ULONG Reserved;
    ULONGLONG Data1;
    ULONGLONG Data2;
} hv_ringlog_entry_t;

typedef struct hv_ringlog_chunk_t {
    ULONGLONG Cursor;
    ULONG Count;
    ULONG Dropped;
    hv_ringlog_entry_t Entries[8];
} hv_ringlog_chunk_t;

VOID hv_ringlog_init(VOID);
VOID hv_ringlog_write(ULONG EventType, ULONGLONG Data1, ULONGLONG Data2);
VOID hv_ringlog_read(ULONGLONG Cursor, hv_ringlog_chunk_t* Out);

#ifdef __cplusplus
}
#endif
