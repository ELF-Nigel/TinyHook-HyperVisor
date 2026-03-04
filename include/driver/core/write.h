// write.h - write options (skeleton)
#pragma once
#include <ntddk.h>

typedef enum hv_write_mode {
    HV_WRITE_DIRECT = 0,
    HV_WRITE_EPT = 1,
    HV_WRITE_NPT = 2
} hv_write_mode;

NTSTATUS hv_write_guest(void* gpa, const void* data, SIZE_T len, hv_write_mode mode);
