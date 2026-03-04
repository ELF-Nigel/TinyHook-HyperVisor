// Handoff.c - build boot handoff block
#include "Boot.h"
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>

EFI_GUID gThvHandoffGuid = { 0x3f7a9fe2, 0x0e84, 0x4de4, {0x86,0x92,0xa2,0x9f,0x31,0x66,0x6a,0xb5} };

STATIC
UINT32
CalcCrc32(CONST VOID* Data, UINTN Size) {
  UINT32 crc = 0xFFFFFFFF;
  CONST UINT8* p = (CONST UINT8*)Data;
  for (UINTN i = 0; i < Size; ++i) {
    crc ^= p[i];
    for (UINTN b = 0; b < 8; ++b) {
      UINT32 mask = (UINT32)-(INT32)(crc & 1);
      crc = (crc >> 1) ^ (0xEDB88320u & mask);
    }
  }
  return ~crc;
}

EFI_STATUS RecalcHandoffCrc(THV_HANDOFF* Handoff) {
  if (!Handoff) return EFI_INVALID_PARAMETER;
  Handoff->Crc32 = 0;
  Handoff->Crc32 = CalcCrc32(Handoff, sizeof(THV_HANDOFF));
  return EFI_SUCCESS;
}

EFI_STATUS CreateHandoff(THV_HANDOFF** OutHandoff) {
  if (!OutHandoff) return EFI_INVALID_PARAMETER;
  *OutHandoff = AllocateZeroPool(sizeof(THV_HANDOFF));
  if (!*OutHandoff) return EFI_OUT_OF_RESOURCES;

  (*OutHandoff)->Signature = THV_HANDOFF_SIGNATURE;
  (*OutHandoff)->Version = THV_HANDOFF_VERSION;
  (*OutHandoff)->Size = (UINT32)sizeof(THV_HANDOFF);
  GetCpuVendor((*OutHandoff)->Vendor);
  GetHvFeatures(&(*OutHandoff)->Features);
  LoadConfig(&(*OutHandoff)->Config);
  RecalcHandoffCrc(*OutHandoff);
  return EFI_SUCCESS;
}

EFI_STATUS WriteHandoffVariable(THV_HANDOFF* Handoff) {
  UINT32 Attr = EFI_VARIABLE_NON_VOLATILE |
                EFI_VARIABLE_BOOTSERVICE_ACCESS |
                EFI_VARIABLE_RUNTIME_ACCESS;
  return gRT->SetVariable(L"elfhvHandoff", &gThvHandoffGuid, Attr, sizeof(*Handoff), Handoff);
}
