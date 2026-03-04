// npt.h - nested page tables helpers (skeleton)
#pragma once
#include <ntddk.h>
#include "driver/util/paging.h"

typedef enum svm_paging_struct_t {
    PS_PML4 = 0,
    PS_PDP,
    PS_PDT,
    PS_PTE
} svm_paging_struct_t;

typedef struct svm_pt_range_t {
    UINT64 start_pfn;
    UINT64 end_pfn;
    PPT_Entry pt;
    UINT64 num_ptes;
} svm_pt_range_t;

typedef struct svm_npt_tables_t {
    PML4_Entry* pml4;
    PPDP_Entry pdp;
    PPD_Entry pd[512];
    PPT_Entry pt[512][512];
    svm_pt_range_t pt_ranges[128];
    ULONG pt_range_count;
    PHYSICAL_ADDRESS pml4_pa;
    PHYSICAL_ADDRESS pdp_pa;
} svm_npt_tables_t;

USHORT svm_get_npt_index(svm_paging_struct_t level, UINT64 gva);
PPT_Entry svm_get_npt_pte_for_gpa(svm_npt_tables_t* npt, UINT64 gpa);
UINT64 svm_get_hpa_for_gpa(svm_npt_tables_t* npt, UINT64 gpa);
void* svm_get_hva_for_gpa(svm_npt_tables_t* npt, UINT64 gpa);
PPD_Entry svm_get_npt_pde_for_gpa(svm_npt_tables_t* npt, UINT64 gpa);
PPDP_Entry svm_get_npt_pdp_for_gpa(svm_npt_tables_t* npt, UINT64 gpa);

int svm_npt_init(svm_npt_tables_t* npt);
void svm_npt_shutdown(svm_npt_tables_t* npt);
int svm_npt_setup_ranges(svm_npt_tables_t* npt);
int svm_npt_build_tables(svm_npt_tables_t* npt);
int svm_npt_sync_physical_ranges(svm_npt_tables_t* npt);
int svm_npt_allocate_pts_for_ranges(svm_npt_tables_t* npt);
int svm_npt_set_rw(svm_npt_tables_t* npt, UINT64 gpa, int read, int write);
int svm_npt_set_exec(svm_npt_tables_t* npt, UINT64 gpa, int exec);
int svm_npt_get_rw(svm_npt_tables_t* npt, UINT64 gpa, int* read, int* write);
int svm_npt_get_exec(svm_npt_tables_t* npt, UINT64 gpa, int* exec);
int svm_npt_is_mapped(svm_npt_tables_t* npt, UINT64 gpa);
