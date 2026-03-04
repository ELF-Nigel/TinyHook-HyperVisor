// ConfigWriter.c - write config variable (UEFI)
#include "Boot.h"
#include <Library/UefiRuntimeServicesTableLib.h>

EFI_STATUS WriteConfigVariable(THV_CONFIG* Config) {
  if (!Config) return EFI_INVALID_PARAMETER;
  UINT32 Attr = EFI_VARIABLE_NON_VOLATILE |
                EFI_VARIABLE_BOOTSERVICE_ACCESS |
                EFI_VARIABLE_RUNTIME_ACCESS;
  return gRT->SetVariable(L"elfhvConfig", &gThvHandoffGuid, Attr, sizeof(*Config), Config);
}
