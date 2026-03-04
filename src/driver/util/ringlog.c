// ringlog.c - hypervisor ring log buffer (non-operational)
#include "driver/util/ringlog.h"
#include <ntddk.h>

static hv_ringlog_entry_t g_ring[HV_RINGLOG_CAPACITY];
static ULONGLONG g_head = 0;
static ULONGLONG g_seq = 0;
static ULONG g_dropped = 0;
static KSPIN_LOCK g_lock;
static LONG g_init = 0;

VOID hv_ringlog_init(VOID) {
    if (InterlockedCompareExchange(&g_init, 1, 0) == 0) {
        KeInitializeSpinLock(&g_lock);
        RtlZeroMemory(g_ring, sizeof(g_ring));
        g_head = 0;
        g_seq = 0;
        g_dropped = 0;
    }
}

VOID hv_ringlog_write(ULONG EventType, ULONGLONG Data1, ULONGLONG Data2) {
    hv_ringlog_init();
    KIRQL irql;
    KeAcquireSpinLock(&g_lock, &irql);
    hv_ringlog_entry_t* e = &g_ring[g_head % HV_RINGLOG_CAPACITY];
    e->Seq = ++g_seq;
    KeQuerySystemTimePrecise((PLARGE_INTEGER)&e->Timestamp100ns);
    e->EventType = EventType;
    e->Reserved = 0;
    e->Data1 = Data1;
    e->Data2 = Data2;
    g_head++;
    if (g_seq > HV_RINGLOG_CAPACITY) g_dropped++;
    KeReleaseSpinLock(&g_lock, irql);
}

VOID hv_ringlog_read(ULONGLONG Cursor, hv_ringlog_chunk_t* Out) {
    if (!Out) return;
    hv_ringlog_init();
    RtlZeroMemory(Out, sizeof(*Out));

    KIRQL irql;
    KeAcquireSpinLock(&g_lock, &irql);
    ULONG count = 0;
    for (ULONG i = 0; i < HV_RINGLOG_CAPACITY && count < RTL_NUMBER_OF(Out->Entries); ++i) {
        hv_ringlog_entry_t* e = &g_ring[i];
        if (e->Seq > Cursor && e->Seq != 0) {
            Out->Entries[count++] = *e;
            Out->Cursor = e->Seq;
        }
    }
    Out->Count = count;
    Out->Dropped = g_dropped;
    KeReleaseSpinLock(&g_lock, irql);
}
