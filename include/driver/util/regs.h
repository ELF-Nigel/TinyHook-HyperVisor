// regs.h - control/flags bitfields
#pragma once
#include <ntddk.h>

typedef union CR0 {
    struct {
        ULONG64 PE : 1;
        ULONG64 MP : 1;
        ULONG64 EM : 1;
        ULONG64 TS : 1;
        ULONG64 ET : 1;
        ULONG64 NE : 1;
        ULONG64 Reserved : 10;
        ULONG64 WP : 1;
        ULONG64 Reserved2 : 1;
        ULONG64 AM : 1;
        ULONG64 Reserved3 : 10;
        ULONG64 NW : 1;
        ULONG64 CD : 1;
        ULONG64 PG : 1;
        ULONG64 Reserved4 : 32;
    } Fields;
    UINT64 Contents;
} CR0;
static_assert(sizeof(CR0) == sizeof(void*), "cr0 size mismatch");

typedef union CR3 {
    struct {
        ULONG64 ProcessContextIdentifier : 12;
        ULONG64 PageMapLevel4BaseAddress : 40;
        ULONG64 Reserved : 12;
    } Fields;
    UINT64 Contents;
} CR3;
static_assert(sizeof(CR3) == sizeof(void*), "cr3 size mismatch");

__forceinline CR3 hv_read_cr3_union(void) {
    CR3 cr3;
    cr3.Contents = __readcr3();
    return cr3;
}

__forceinline UINT64 hv_cr3_pml4_phys(CR3 cr3) {
    return (cr3.Fields.PageMapLevel4BaseAddress << 12);
}

__forceinline UINT16 hv_cr3_pcid(CR3 cr3) {
    return (UINT16)(cr3.Fields.ProcessContextIdentifier & 0xFFF);
}

typedef union CR4 {
    struct {
        ULONG64 VME : 1;
        ULONG64 PVI : 1;
        ULONG64 TSD : 1;
        ULONG64 DE : 1;
        ULONG64 PSE : 1;
        ULONG64 PAE : 1;
        ULONG64 MCE : 1;
        ULONG64 PGE : 1;
        ULONG64 PCE : 1;
        ULONG64 OSFXSR : 1;
        ULONG64 OSXMMEXCPT : 1;
        ULONG64 UMIP : 1;
        ULONG64 LA57 : 1;
        ULONG64 Reserved : 3;
        ULONG64 FSGSBASE : 1;
        ULONG64 PCIDE : 1;
        ULONG64 OSXSAVE : 1;
        ULONG64 Reserved2 : 1;
        ULONG64 SMEP : 1;
        ULONG64 SMAP : 1;
        ULONG64 PKE : 1;
        ULONG64 CET : 1;
        ULONG64 Reserved3 : 40;
    } Fields;
    UINT64 Contents;
} CR4;
static_assert(sizeof(CR4) == sizeof(void*), "cr4 size mismatch");

typedef union RFLAGS {
    struct {
        ULONG64 CF : 1;
        ULONG64 Reserved : 1;
        ULONG64 PF : 1;
        ULONG64 Reserved2 : 1;
        ULONG64 AF : 1;
        ULONG64 Reserved3 : 1;
        ULONG64 ZF : 1;
        ULONG64 SF : 1;
        ULONG64 TF : 1;
        ULONG64 IF : 1;
        ULONG64 DF : 1;
        ULONG64 OF : 1;
        ULONG64 IOPL : 2;
        ULONG64 NT : 1;
        ULONG64 Reserved4 : 1;
        ULONG64 RF : 1;
        ULONG64 VM : 1;
        ULONG64 AC : 1;
        ULONG64 VIF : 1;
        ULONG64 VIP : 1;
        ULONG64 ID : 1;
        ULONG64 Reserved5 : 42;
    } Fields;
    UINT64 Contents;
} RFLAGS;
static_assert(sizeof(RFLAGS) == sizeof(void*), "rflags size mismatch");

typedef union MSR_EFER {
    struct {
        ULONG64 SCE : 1;
        ULONG64 Reserved : 7;
        ULONG64 LME : 1;
        ULONG64 Reserved2 : 1;
        ULONG64 LMA : 1;
        ULONG64 NXE : 1;
        ULONG64 SVME : 1;
        ULONG64 LMSLE : 1;
        ULONG64 FFXSR : 1;
        ULONG64 TCE : 1;
        ULONG64 Reserved3 : 1;
        ULONG64 MCOMMIT : 1;
        ULONG64 INTWB : 1;
        ULONG64 Reserved4 : 1;
        ULONG64 UAIE : 1;
        ULONG64 AIBRSE : 1;
        ULONG64 Reserved5 : 42;
    } Fields;
    ULONG64 Contents;
} MSR_EFER;
static_assert(sizeof(MSR_EFER) == sizeof(void*), "msr_efer size mismatch");

#define MSR_EFER_MSR 0xC0000080

typedef union PageFaultErrorCode {
    struct {
        UINT32 Present : 1;
        UINT32 Write : 1;
        UINT32 Supervisor : 1;
        UINT32 ReservedFault : 1;
        UINT32 InstructionFetch : 1;
        UINT32 ProtectionKeyViolation : 1;
        UINT32 ShadowStackAccess : 1;
        UINT32 Reserved : 24;
        UINT32 RMP : 1;
    } Fields;
    UINT32 Contents;
} PageFaultErrorCode;
