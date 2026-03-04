// manager.h - api manager (skeleton)
#pragma once
#include <ntddk.h>
#include "driver/hooks/hook.h"
#include "driver/hooks/hook_ext.h"
#include "driver/hooks/hook_more.h"
#include "driver/util/lock.h"

typedef struct hv_manager_t {
    int initialized;
    int policy_strict;
    hv_lock_t lock;
    struct {
        USHORT port;
        UCHAR read;
        UCHAR write;
        UCHAR enabled;
    } io_table[256];
    ULONG io_count;
    struct {
        ULONG leaf;
        ULONG subleaf;
        UCHAR enabled;
    } cpuid_table[128];
    ULONG cpuid_count;
} hv_manager_t;

void hv_manager_init(hv_manager_t* m);
void hv_manager_shutdown(hv_manager_t* m);
int hv_manager_io_set(hv_manager_t* m, USHORT port, int read, int write);
int hv_manager_io_clear(hv_manager_t* m, USHORT port);
int hv_manager_cpuid_set(hv_manager_t* m, ULONG leaf, ULONG subleaf);
int hv_manager_cpuid_clear(hv_manager_t* m, ULONG leaf, ULONG subleaf);
int hv_manager_io_get(hv_manager_t* m, USHORT port, int* read, int* write);
int hv_manager_cpuid_get(hv_manager_t* m, ULONG leaf, ULONG subleaf);
ULONG hv_manager_io_count(hv_manager_t* m);
ULONG hv_manager_cpuid_count(hv_manager_t* m);
void hv_manager_io_clear_all(hv_manager_t* m);
void hv_manager_cpuid_clear_all(hv_manager_t* m);
