// translate.h - memory translation helpers (skeleton)
#pragma once
#include <ntddk.h>
#include "driver/arch/vmx.h"
#include "driver/arch/svm.h"
#include "driver/util/paging.h"

NTSTATUS hv_translate_hva_to_hpa(void* hva, PHYSICAL_ADDRESS* out_pa);
void* hv_translate_hpa_to_hva(PHYSICAL_ADDRESS pa);

NTSTATUS hv_cr3_map_pml4(CR3 cr3, PML4_Entry** out_pml4);
void hv_cr3_unmap_pml4(PML4_Entry* pml4);
int hv_cr3_validate_ranges(CR3 cr3);

NTSTATUS hv_translate_gva_to_gpa_vmx(vmx_state_t* st, UINT64 gva, UINT64* gpa);
NTSTATUS hv_translate_gva_to_gpa_svm(svm_state_t* st, UINT64 gva, UINT64* gpa);
