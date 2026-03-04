// Cpu.c - cpu vendor + raw feature helpers
#include "Boot.h"
#include <Library/BaseLib.h>

VOID GetCpuVendor(CHAR8* Out) {
  UINT32 eax, ebx, ecx, edx;
  AsmCpuid(0, &eax, &ebx, &ecx, &edx);
  ((UINT32*)Out)[0] = ebx;
  ((UINT32*)Out)[1] = edx;
  ((UINT32*)Out)[2] = ecx;
  Out[12] = 0;
}
