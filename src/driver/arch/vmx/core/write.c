// write.c - write options (annotated skeleton)
#include "driver/core/write.h"

// NOTE: this is a non-functional skeleton. it outlines where EPT/NPT trapping would be wired.

static NTSTATUS hv_write_direct(void* gpa, const void* data, SIZE_T len) {
    if (!gpa || !data || !len) return STATUS_INVALID_PARAMETER;
    // layout:
    // 1) translate gpa->hpa->hva
    // 2) validate mapping and bounds
    // 3) memcpy to target
    // 4) flush if required
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS hv_write_ept(void* gpa, const void* data, SIZE_T len) {
    if (!gpa || !data || !len) return STATUS_INVALID_PARAMETER;
    // layout:
    // 1) locate ept entry for gpa
    // 2) set r/w bits
    // 3) invept
    // 4) write to target
    return STATUS_NOT_IMPLEMENTED;
}

static NTSTATUS hv_write_npt(void* gpa, const void* data, SIZE_T len) {
    if (!gpa || !data || !len) return STATUS_INVALID_PARAMETER;
    // layout:
    // 1) locate npt entry for gpa
    // 2) set r/w bits
    // 3) invlpga
    // 4) write to target
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS hv_write_guest(void* gpa, const void* data, SIZE_T len, hv_write_mode mode) {
    if (!gpa || !data || !len) return STATUS_INVALID_PARAMETER;
    switch (mode) {
        case HV_WRITE_DIRECT: return hv_write_direct(gpa, data, len);
        case HV_WRITE_EPT:    return hv_write_ept(gpa, data, len);
        case HV_WRITE_NPT:    return hv_write_npt(gpa, data, len);
        default: return STATUS_INVALID_PARAMETER;
    }
}
