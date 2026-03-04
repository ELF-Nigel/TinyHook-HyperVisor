// Config.c - read config from UEFI variables (skeleton)
#include "Boot.h"
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>

static EFI_GUID gThvConfigGuid = { 0x3f7a9fe2, 0x0e84, 0x4de4, {0x86,0x92,0xa2,0x9f,0x31,0x66,0x6a,0xb5} };

EFI_STATUS LoadConfig(THV_CONFIG* Out) {
  if (!Out) return EFI_INVALID_PARAMETER;
  Out->EnableHv = TRUE;
  Out->HvMode = 0;
  Out->EnableVmx = TRUE;
  Out->EnableSvm = TRUE;
  Out->EnableEpt = TRUE;
  Out->EnableNpt = TRUE;
  Out->EnableVpid = TRUE;
  Out->EnableHooks = FALSE;
  Out->HookSyscall = FALSE;
  Out->HookMsr = FALSE;
  Out->HookCpuid = FALSE;
  Out->EnableMemIntegrity = FALSE;
  Out->EnableGuardPages = FALSE;
  Out->EnableSelfProtect = FALSE;
  Out->EnableAntiTamper = FALSE;
  Out->EnableStealth = FALSE;
  Out->Verbose = TRUE;
  Out->SerialDebug = FALSE;
  Out->LogLevel = 1;
  Out->VmexitLogging = FALSE;
  Out->VmexitSampleRate = 3;
  Out->BootDelayStep = 0;

  UINTN Size = sizeof(THV_CONFIG);
  EFI_STATUS Status = gRT->GetVariable(L"elfhvConfig", &gThvConfigGuid, NULL, &Size, Out);
  if (EFI_ERROR(Status)) {
    // keep defaults if variable not present
    return EFI_SUCCESS;
  }
  return EFI_SUCCESS;
}
