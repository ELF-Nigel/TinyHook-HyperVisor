// Features.c - feature detection
#include "Boot.h"
#include <Library/BaseLib.h>

#define MSR_IA32_VMX_EPT_VPID_CAP 0x48C

static BOOLEAN HasVmx(void) {
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(1, &eax, &ebx, &ecx, &edx);
  return (ecx & (1U << 5)) != 0;
}

static BOOLEAN HasVpid(void) {
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(1, &eax, &ebx, &ecx, &edx);
  return (ecx & (1U << 6)) != 0;
}

static BOOLEAN HasEpt(void) {
  if (!HasVmx()) return FALSE;
  UINT64 cap = AsmReadMsr64(MSR_IA32_VMX_EPT_VPID_CAP);
  return (cap & (1ULL << 0)) != 0;
}

static BOOLEAN HasSvm(void) {
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(0x80000001, &eax, &ebx, &ecx, &edx);
  return (ecx & (1U << 2)) != 0;
}

static BOOLEAN HasNpt(void) {
  if (!HasSvm()) return FALSE;
  // AMD NPT support is reported via CPUID 0x8000000A EDX bit 0
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(0x8000000A, &eax, &ebx, &ecx, &edx);
  return (edx & 0x1) != 0;
}

VOID GetHvFeatures(THV_FEATURES* Out) {
  if (!Out) return;
  Out->Vmx = HasVmx();
  Out->Vpid = HasVpid();
  Out->Ept = HasEpt();
  Out->Svm = HasSvm();
  Out->Npt = HasNpt();
}
