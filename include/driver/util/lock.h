// lock.h - simple spinlock wrapper
#pragma once
#include <ntddk.h>

typedef struct hv_lock_t {
    KSPIN_LOCK lock;
    KIRQL old_irql;
} hv_lock_t;

__forceinline void hv_lock_init(hv_lock_t* l) {
    if (!l) return;
    KeInitializeSpinLock(&l->lock);
    l->old_irql = PASSIVE_LEVEL;
}

__forceinline void hv_lock_acquire(hv_lock_t* l) {
    if (!l) return;
    KeAcquireSpinLock(&l->lock, &l->old_irql);
}

__forceinline void hv_lock_release(hv_lock_t* l) {
    if (!l) return;
    KeReleaseSpinLock(&l->lock, l->old_irql);
}
