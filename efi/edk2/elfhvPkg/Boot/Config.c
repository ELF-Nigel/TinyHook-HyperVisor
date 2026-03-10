// config.c - read config from uefi variables (skeleton)
#include "Boot.h"
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiBootServicesTableLib.h>

EFI_GUID gThvConfigGuid = { 0x3f7a9fe2, 0x0e84, 0x4de4, {0x86,0x92,0xa2,0x9f,0x31,0x66,0x6a,0xb5} };

static VOID ConfigDefaults(THV_CONFIG* Out) {
  if (!Out) return;
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
}

static BOOLEAN IsBool(UINT8 Value) {
  return Value == 0 || Value == 1;
}

static BOOLEAN ConfigSanityOk(THV_CONFIG* Cfg) {
  if (!Cfg) return FALSE;
  if (!IsBool(Cfg->EnableHv)) return FALSE;
  if (Cfg->HvMode > 2) return FALSE;
  if (!IsBool(Cfg->EnableVmx)) return FALSE;
  if (!IsBool(Cfg->EnableSvm)) return FALSE;
  if (!IsBool(Cfg->EnableEpt)) return FALSE;
  if (!IsBool(Cfg->EnableNpt)) return FALSE;
  if (!IsBool(Cfg->EnableVpid)) return FALSE;
  if (!IsBool(Cfg->EnableHooks)) return FALSE;
  if (!IsBool(Cfg->HookSyscall)) return FALSE;
  if (!IsBool(Cfg->HookMsr)) return FALSE;
  if (!IsBool(Cfg->HookCpuid)) return FALSE;
  if (!IsBool(Cfg->EnableMemIntegrity)) return FALSE;
  if (!IsBool(Cfg->EnableGuardPages)) return FALSE;
  if (!IsBool(Cfg->EnableSelfProtect)) return FALSE;
  if (!IsBool(Cfg->EnableAntiTamper)) return FALSE;
  if (!IsBool(Cfg->EnableStealth)) return FALSE;
  if (!IsBool(Cfg->Verbose)) return FALSE;
  if (!IsBool(Cfg->SerialDebug)) return FALSE;
  if (Cfg->LogLevel > 3) return FALSE;
  if (!IsBool(Cfg->VmexitLogging)) return FALSE;
  if (Cfg->VmexitSampleRate > 10) return FALSE;
  if (Cfg->BootDelayStep > 50) return FALSE;
  if ((Cfg->BootDelayStep % 10) != 0) return FALSE;
  if (!Cfg->EnableVmx) {
    if (Cfg->EnableEpt || Cfg->EnableVpid) return FALSE;
  }
  if (!Cfg->EnableSvm) {
    if (Cfg->EnableNpt) return FALSE;
  }
  return TRUE;
}

EFI_STATUS LoadConfig(THV_CONFIG* Out) {
  if (!Out) return EFI_INVALID_PARAMETER;
  ConfigDefaults(Out);

  UINTN Size = sizeof(THV_CONFIG);
  EFI_STATUS Status = gRT->GetVariable(L"elfhvConfig", &gThvConfigGuid, NULL, &Size, Out);
  if (EFI_ERROR(Status)) {
    // keep defaults if variable not present
    return EFI_SUCCESS;
  }
  if (Size != sizeof(THV_CONFIG) || !ConfigSanityOk(Out)) {
    ConfigDefaults(Out);
    WriteConfigVariable(Out);
  }
  return EFI_SUCCESS;
}
