// diag.c - diagnostics tools (skeleton)
#include "driver/core/diag.h"
#include "driver/util/log.h"

void hv_dump_vmx(vmx_state_t* st) {
    if (!st) return;
    hv_log("vmx dump: vmxon=%p vmcs=%p\n", st->vmxon_region, st->vmcs_region);
}

void hv_dump_svm(svm_state_t* st) {
    if (!st) return;
    hv_log("svm dump: vmcb=%p\n", st->vmcb);
}
