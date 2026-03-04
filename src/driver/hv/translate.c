// translate.c - memory translation helpers (skeleton)
#include "driver/util/translate.h"
#include "driver/util/alloc.h"
#include "driver/util/lock.h"

static void* hv_map_guest_phys_page(UINT64 gpa) {
    PHYSICAL_ADDRESS pa;
    pa.QuadPart = gpa & ~0xFFFULL;
    if (pa.QuadPart == 0 || !hv_is_phys_page_aligned(pa)) return NULL;
    return MmMapIoSpace(pa, PAGE_SIZE, MmNonCached);
}

static void hv_unmap_guest_phys_page(void* mapping) {
    if (mapping) MmUnmapIoSpace(mapping, PAGE_SIZE);
}

NTSTATUS hv_cr3_map_pml4(CR3 cr3, PML4_Entry** out_pml4) {
    if (!out_pml4) return STATUS_INVALID_PARAMETER;
    *out_pml4 = NULL;
    UINT64 pml4_pa = hv_cr3_pml4_phys(cr3);
    if (pml4_pa == 0) return STATUS_INVALID_PARAMETER;
    PHYSICAL_ADDRESS pa;
    pa.QuadPart = (LONGLONG)pml4_pa;
    if (!hv_is_phys_page_aligned(pa)) return STATUS_INVALID_PARAMETER;
    *out_pml4 = (PML4_Entry*)MmMapIoSpace(pa, PAGE_SIZE, MmNonCached);
    return (*out_pml4) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

void hv_cr3_unmap_pml4(PML4_Entry* pml4) {
    if (pml4) MmUnmapIoSpace(pml4, PAGE_SIZE);
}

int hv_cr3_validate_ranges(CR3 cr3) {
    PPHYSICAL_MEMORY_RANGE ranges = MmGetPhysicalMemoryRangesEx2(NULL, 0);
    if (!ranges) return 0;
    UINT64 pml4_pa = hv_cr3_pml4_phys(cr3);
    int ok = 0;
    for (ULONG i = 0; ranges[i].BaseAddress.QuadPart || ranges[i].NumberOfBytes.QuadPart; ++i) {
        UINT64 start = (UINT64)ranges[i].BaseAddress.QuadPart;
        UINT64 end = start + (UINT64)ranges[i].NumberOfBytes.QuadPart;
        if (pml4_pa >= start && pml4_pa < end) { ok = 1; break; }
    }
    ExFreePoolWithTag(ranges, 0);
    return ok;
}

typedef struct hv_gva_gpa_cache_entry_t {
    UINT64 cr3;
    UINT64 gva_base;
    UINT64 gpa_base;
    UINT32 page_shift;
    UINT32 valid;
} hv_gva_gpa_cache_entry_t;

typedef struct hv_gva_gpa_cache_t {
    hv_lock_t lock;
    hv_gva_gpa_cache_entry_t entries[64];
} hv_gva_gpa_cache_t;

static hv_gva_gpa_cache_t g_gva_cache;
static volatile LONG g_gva_cache_init;

static void hv_gva_cache_init_once(void) {
    if (InterlockedCompareExchange(&g_gva_cache_init, 1, 0) == 0) {
        hv_lock_init(&g_gva_cache.lock);
        RtlZeroMemory(g_gva_cache.entries, sizeof(g_gva_cache.entries));
    }
}

static __forceinline UINT64 hv_align_down(UINT64 v, UINT64 align) {
    return v & ~(align - 1);
}

static int hv_gva_cache_lookup(UINT64 cr3, UINT64 gva, UINT64* gpa) {
    hv_gva_cache_init_once();
    UINT64 page_base = hv_align_down(gva, PAGE_SIZE);
    UINT32 idx = (UINT32)((page_base >> 12) ^ (cr3 >> 12)) & 63;
    hv_lock_acquire(&g_gva_cache.lock);
    hv_gva_gpa_cache_entry_t* e = &g_gva_cache.entries[idx];
    int hit = 0;
    if (e->valid && e->cr3 == cr3) {
        UINT64 mask = (1ULL << e->page_shift) - 1;
        UINT64 base = hv_align_down(gva, 1ULL << e->page_shift);
        if (base == e->gva_base) {
            *gpa = e->gpa_base | (gva & mask);
            hit = 1;
        }
    }
    hv_lock_release(&g_gva_cache.lock);
    return hit;
}

static void hv_gva_cache_insert(UINT64 cr3, UINT64 gva_base, UINT64 gpa_base, UINT32 page_shift) {
    hv_gva_cache_init_once();
    UINT32 idx = (UINT32)((gva_base >> 12) ^ (cr3 >> 12)) & 63;
    hv_lock_acquire(&g_gva_cache.lock);
    g_gva_cache.entries[idx].cr3 = cr3;
    g_gva_cache.entries[idx].gva_base = gva_base;
    g_gva_cache.entries[idx].gpa_base = gpa_base;
    g_gva_cache.entries[idx].page_shift = page_shift;
    g_gva_cache.entries[idx].valid = 1;
    hv_lock_release(&g_gva_cache.lock);
}

static NTSTATUS hv_translate_gva_to_gpa_x64(UINT64 cr3, UINT64 gva, UINT64* gpa) {
    if (!gpa || cr3 == 0) return STATUS_INVALID_PARAMETER;
    if (hv_gva_cache_lookup(cr3, gva, gpa)) return STATUS_SUCCESS;
    CR3 cr3u;
    cr3u.Contents = cr3;
    if (!hv_cr3_validate_ranges(cr3u)) return STATUS_INVALID_PARAMETER;
    UINT64 pml4_pa = hv_cr3_pml4_phys(cr3u);
    PPML4_Entry pml4 = (PPML4_Entry)hv_map_guest_phys_page(pml4_pa);
    if (!pml4) return STATUS_UNSUCCESSFUL;
    PML4_Entry pml4e = pml4[hv_pml4_index(gva)];
    hv_unmap_guest_phys_page(pml4);
    if (!pml4e.Fields.Valid) return STATUS_UNSUCCESSFUL;

    UINT64 pdpt_pa = pml4e.Fields.PageFrameNumber << 12;
    PPDP_Entry pdpt = (PPDP_Entry)hv_map_guest_phys_page(pdpt_pa);
    if (!pdpt) return STATUS_UNSUCCESSFUL;
    PDP_Entry pdpte = pdpt[hv_pdpt_index(gva)];
    hv_unmap_guest_phys_page(pdpt);
    if (!pdpte.Fields_Sub1GB.Valid) return STATUS_UNSUCCESSFUL;
    if (pdpte.Fields_1GB.LargePage) {
        UINT64 gva_base = hv_align_down(gva, 1ULL << 30);
        UINT64 gpa_base = (pdpte.Fields_1GB.PageFrameNumber << 30);
        *gpa = gpa_base | (gva & ((1ULL << 30) - 1));
        hv_gva_cache_insert(cr3, gva_base, gpa_base, 30);
        return STATUS_SUCCESS;
    }

    UINT64 pd_pa = pdpte.Fields_Sub1GB.PageFrameNumber << 12;
    PPD_Entry pd = (PPD_Entry)hv_map_guest_phys_page(pd_pa);
    if (!pd) return STATUS_UNSUCCESSFUL;
    PD_Entry pde = pd[hv_pd_index(gva)];
    hv_unmap_guest_phys_page(pd);
    if (!pde.Fields_4KB.Valid) return STATUS_UNSUCCESSFUL;
    if (pde.Fields_2MB.LargePage) {
        UINT64 gva_base = hv_align_down(gva, 1ULL << 21);
        UINT64 gpa_base = (pde.Fields_2MB.PageFrameNumber << 21);
        *gpa = gpa_base | (gva & ((1ULL << 21) - 1));
        hv_gva_cache_insert(cr3, gva_base, gpa_base, 21);
        return STATUS_SUCCESS;
    }

    UINT64 pt_pa = pde.Fields_4KB.PageFrameNumber << 12;
    PPT_Entry pt = (PPT_Entry)hv_map_guest_phys_page(pt_pa);
    if (!pt) return STATUS_UNSUCCESSFUL;
    PT_Entry pte = pt[hv_pt_index(gva)];
    hv_unmap_guest_phys_page(pt);
    if (!pte.Fields.Valid) return STATUS_UNSUCCESSFUL;
    *gpa = (pte.Fields.PageFrameNumber << 12) | hv_page_offset(gva);
    hv_gva_cache_insert(cr3, hv_align_down(gva, PAGE_SIZE), (pte.Fields.PageFrameNumber << 12), 12);
    return STATUS_SUCCESS;
}

NTSTATUS hv_translate_hva_to_hpa(void* hva, PHYSICAL_ADDRESS* out_pa) {
    if (!hva || !out_pa) return STATUS_INVALID_PARAMETER;
    *out_pa = MmGetPhysicalAddress(hva);
    if (out_pa->QuadPart == 0 || !hv_is_phys_page_aligned(*out_pa)) return STATUS_UNSUCCESSFUL;
    return STATUS_SUCCESS;
}

void* hv_translate_hpa_to_hva(PHYSICAL_ADDRESS pa) {
    if (pa.QuadPart == 0 || !hv_is_phys_page_aligned(pa)) return NULL;
    return MmGetVirtualForPhysical(pa);
}

NTSTATUS hv_translate_gva_to_gpa_vmx(vmx_state_t* st, UINT64 gva, UINT64* gpa) {
    if (!st || !gpa) return STATUS_INVALID_PARAMETER;
    if (st->guest_cr3 == 0) return STATUS_INVALID_DEVICE_STATE;
    return hv_translate_gva_to_gpa_x64(st->guest_cr3, gva, gpa);
}

NTSTATUS hv_translate_gva_to_gpa_svm(svm_state_t* st, UINT64 gva, UINT64* gpa) {
    if (!st || !gpa) return STATUS_INVALID_PARAMETER;
    if (st->guest_cr3 == 0) return STATUS_INVALID_DEVICE_STATE;
    return hv_translate_gva_to_gpa_x64(st->guest_cr3, gva, gpa);
}
