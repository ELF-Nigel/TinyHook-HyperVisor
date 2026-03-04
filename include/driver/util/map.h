// map.h - per-cpu permission maps (skeleton)
#pragma once
#include <ntddk.h>

typedef struct hv_perm_entry_t {
    void* gpa;
    int r, w, x;
} hv_perm_entry_t;

typedef struct hv_perm_map_t {
    hv_perm_entry_t entries[128];
    ULONG count;
} hv_perm_map_t;

int hv_perm_map_set(hv_perm_map_t* m, void* gpa, int r, int w, int x);
