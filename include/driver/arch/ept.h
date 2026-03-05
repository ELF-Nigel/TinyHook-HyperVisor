// ept.h - ept skeleton
#pragma once
#include <ntddk.h>

typedef struct ept_state_t {
    void* pml4_raw;
    void* pml4;
    PHYSICAL_ADDRESS pml4_pa;
} ept_state_t;

typedef enum ept_perm_t {
    EPT_PERM_NONE = 0,
    EPT_PERM_R = 1 << 0,
    EPT_PERM_W = 1 << 1,
    EPT_PERM_X = 1 << 2,
    EPT_PERM_RW = EPT_PERM_R | EPT_PERM_W,
    EPT_PERM_RWX = EPT_PERM_R | EPT_PERM_W | EPT_PERM_X
} ept_perm_t;

int ept_init(ept_state_t* ept);
void ept_shutdown(ept_state_t* ept);
int ept_set_rw(ept_state_t* ept, UINT64 gpa, int read, int write);
int ept_set_exec(ept_state_t* ept, UINT64 gpa, int exec);
int ept_map_page(ept_state_t* ept, UINT64 gpa, UINT64 hpa, int r, int w, int x);
int ept_unmap_page(ept_state_t* ept, UINT64 gpa);
int ept_set_page_perm(ept_state_t* ept, UINT64 gpa, ept_perm_t perm);
