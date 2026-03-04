// hook_ext.h - extended hook skeletons
#pragma once
#include <ntddk.h>

typedef struct hv_ept_hook_t { void* gpa; int enabled; } hv_ept_hook_t;
typedef struct hv_npt_hook_t { void* gpa; int enabled; } hv_npt_hook_t;
typedef struct hv_msr_hook_t { ULONG msr; int enabled; } hv_msr_hook_t;
typedef struct hv_io_hook_t { USHORT port; int enabled; } hv_io_hook_t;
typedef struct hv_syscall_hook_t { ULONG id; int enabled; } hv_syscall_hook_t;
typedef struct hv_cpuid_hook_t { ULONG leaf; int enabled; } hv_cpuid_hook_t;

int hv_ept_hook_register(hv_ept_hook_t* h, void* gpa);
int hv_ept_hook_enable(hv_ept_hook_t* h);
int hv_ept_hook_disable(hv_ept_hook_t* h);

int hv_npt_hook_register(hv_npt_hook_t* h, void* gpa);
int hv_npt_hook_enable(hv_npt_hook_t* h);
int hv_npt_hook_disable(hv_npt_hook_t* h);

int hv_msr_hook_register(hv_msr_hook_t* h, ULONG msr);
int hv_msr_hook_enable(hv_msr_hook_t* h);
int hv_msr_hook_disable(hv_msr_hook_t* h);

int hv_io_hook_register(hv_io_hook_t* h, USHORT port);
int hv_io_hook_enable(hv_io_hook_t* h);
int hv_io_hook_disable(hv_io_hook_t* h);

int hv_syscall_hook_register(hv_syscall_hook_t* h, ULONG id);
int hv_syscall_hook_enable(hv_syscall_hook_t* h);
int hv_syscall_hook_disable(hv_syscall_hook_t* h);

int hv_cpuid_hook_register(hv_cpuid_hook_t* h, ULONG leaf);
int hv_cpuid_hook_enable(hv_cpuid_hook_t* h);
int hv_cpuid_hook_disable(hv_cpuid_hook_t* h);
