// vmcs.h - vmcs fields/constants (skeleton)
#pragma once
#include <ntddk.h>

// vmcs field encodings (partial)
typedef enum elfhv_vmcs_field_t {
    VM_EXIT_REASON = 0x4402,
    VM_EXIT_QUALIFICATION = 0x6400,
    VM_INSTRUCTION_LENGTH = 0x440C
} elfhv_vmcs_field_t;

// exit reasons (partial)
typedef enum elfhv_vmexit_reason_t {
    EXIT_REASON_CPUID = 0x000A,
    EXIT_REASON_RDTSC = 0x0012,
    EXIT_REASON_RDMSR = 0x001C,
    EXIT_REASON_WRMSR = 0x001D,
    EXIT_REASON_EPT_VIOLATION = 0x0030
} elfhv_vmexit_reason_t;

// control fields (placeholders)
typedef enum elfhv_vmcs_control_t {
    PIN_BASED_VM_EXEC_CONTROL = 0x4000,
    CPU_BASED_VM_EXEC_CONTROL = 0x4002,
    VM_EXIT_CONTROLS = 0x400C,
    VM_ENTRY_CONTROLS = 0x4012
} elfhv_vmcs_control_t;
