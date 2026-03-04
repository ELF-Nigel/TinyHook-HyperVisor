// hypercall.c - hypercall interface (skeleton)
#include "driver/core/hypercall.h"

NTSTATUS hv_hypercall_dispatch(ULONG id, void* in, ULONG in_len, void* out, ULONG out_len) {
    UNREFERENCED_PARAMETER(in);
    UNREFERENCED_PARAMETER(in_len);
    UNREFERENCED_PARAMETER(out);
    UNREFERENCED_PARAMETER(out_len);
    switch (id) {
        case HV_HC_NOP: return STATUS_SUCCESS;
        case HV_HC_PING: return STATUS_SUCCESS;
        case HV_HC_QUERY: return STATUS_NOT_IMPLEMENTED;
        default: return STATUS_INVALID_PARAMETER;
    }
}
