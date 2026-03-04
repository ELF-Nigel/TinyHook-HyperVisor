// log.h - per-cpu logging (skeleton)
#pragma once
#include <ntddk.h>

static __forceinline void hv_log(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vDbgPrintExWithPrefix("[elfhv] ", DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, fmt, ap);
    va_end(ap);
}
