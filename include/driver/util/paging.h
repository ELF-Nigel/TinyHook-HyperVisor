// paging.h - x64 page table entries + helpers
#pragma once
#include <ntddk.h>

typedef union _PML4_Entry {
    struct {
        UINT64 Valid : 1;
        UINT64 Write : 1;
        UINT64 User : 1;
        UINT64 WriteThrough : 1;
        UINT64 CacheDisable : 1;
        UINT64 Accessed : 1;
        UINT64 Reserved1 : 3;
        UINT64 Avl : 3;
        UINT64 PageFrameNumber : 40;
        UINT64 Reserved2 : 11;
        UINT64 NoExecute : 1;
    } Fields;
    UINT64 Contents;
} PML4_Entry, *PPML4_Entry;

typedef union _PDP_Entry {
    struct {
        UINT64 Valid : 1;
        UINT64 Write : 1;
        UINT64 User : 1;
        UINT64 WriteThrough : 1;
        UINT64 CacheDisable : 1;
        UINT64 Accessed : 1;
        UINT64 Dirty : 1;
        UINT64 LargePage : 1;
        UINT64 Global : 1;
        UINT64 Avl : 3;
        UINT64 PageFrameNumber : 40;
        UINT64 Reserved : 11;
        UINT64 NoExecute : 1;
    } Fields_Sub1GB;
    struct {
        UINT64 Valid : 1;
        UINT64 Write : 1;
        UINT64 User : 1;
        UINT64 WriteThrough : 1;
        UINT64 CacheDisable : 1;
        UINT64 Accessed : 1;
        UINT64 Dirty : 1;
        UINT64 LargePage : 1;
        UINT64 Global : 1;
        UINT64 Avl : 3;
        UINT64 Pat : 1;
        UINT64 Reserved1 : 17;
        UINT64 PageFrameNumber : 22;
        UINT64 Reserved2 : 11;
        UINT64 NoExecute : 1;
    } Fields_1GB;
    UINT64 Contents;
} PDP_Entry, *PPDP_Entry;

typedef union _PD_Entry {
    struct {
        UINT64 Valid : 1;
        UINT64 Write : 1;
        UINT64 User : 1;
        UINT64 WriteThrough : 1;
        UINT64 CacheDisable : 1;
        UINT64 Accessed : 1;
        UINT64 Dirty : 1;
        UINT64 LargePage : 1;
        UINT64 Global : 1;
        UINT64 Avl : 3;
        UINT64 PageFrameNumber : 40;
        UINT64 Reserved : 11;
        UINT64 NoExecute : 1;
    } Fields_4KB;
    struct {
        UINT64 Valid : 1;
        UINT64 Write : 1;
        UINT64 User : 1;
        UINT64 WriteThrough : 1;
        UINT64 CacheDisable : 1;
        UINT64 Accessed : 1;
        UINT64 Dirty : 1;
        UINT64 LargePage : 1;
        UINT64 Global : 1;
        UINT64 Avl : 3;
        UINT64 Pat : 1;
        UINT64 Reserved1 : 8;
        UINT64 PageFrameNumber : 31;
        UINT64 Reserved2 : 11;
        UINT64 NoExecute : 1;
    } Fields_2MB;
    UINT64 Contents;
} PD_Entry, *PPD_Entry;

typedef union _PT_Entry {
    struct {
        UINT64 Valid : 1;
        UINT64 Write : 1;
        UINT64 User : 1;
        UINT64 WriteThrough : 1;
        UINT64 CacheDisable : 1;
        UINT64 Accessed : 1;
        UINT64 Dirty : 1;
        UINT64 Pat : 1;
        UINT64 Global : 1;
        UINT64 Avl : 3;
        UINT64 PageFrameNumber : 40;
        UINT64 Reserved : 11;
        UINT64 NoExecute : 1;
    } Fields;
    UINT64 Contents;
} PT_Entry, *PPT_Entry;

__forceinline UINT64 hv_pml4_index(UINT64 va) { return (va >> 39) & 0x1FF; }
__forceinline UINT64 hv_pdpt_index(UINT64 va) { return (va >> 30) & 0x1FF; }
__forceinline UINT64 hv_pd_index(UINT64 va) { return (va >> 21) & 0x1FF; }
__forceinline UINT64 hv_pt_index(UINT64 va) { return (va >> 12) & 0x1FF; }
__forceinline UINT64 hv_page_offset(UINT64 va) { return va & 0xFFF; }
