// hook_more.h - additional hook skeletons
#pragma once
#include <ntddk.h>

typedef struct hv_vmcall_hook_t { int enabled; } hv_vmcall_hook_t;
typedef struct hv_pf_hook_t { int enabled; } hv_pf_hook_t;
typedef struct hv_int_hook_t { int vector; int enabled; } hv_int_hook_t;
typedef struct hv_nmi_hook_t { int enabled; } hv_nmi_hook_t;

typedef struct hv_syscall_table_hook_t { int enabled; } hv_syscall_table_hook_t;

typedef struct hv_guest_memory_hook_t { void* gpa; int enabled; } hv_guest_memory_hook_t;

int hv_vmcall_hook_enable(hv_vmcall_hook_t* h);
int hv_pf_hook_enable(hv_pf_hook_t* h);
int hv_int_hook_enable(hv_int_hook_t* h, int vector);
int hv_nmi_hook_enable(hv_nmi_hook_t* h);
int hv_syscall_table_hook_enable(hv_syscall_table_hook_t* h);
int hv_guest_memory_hook_enable(hv_guest_memory_hook_t* h, void* gpa);


typedef struct hv_hook_map_t {
    void* slots[64];
} hv_hook_map_t;

int hv_hook_map_set(hv_hook_map_t* m, int id, void* handler);
void* hv_hook_map_get(hv_hook_map_t* m, int id);
