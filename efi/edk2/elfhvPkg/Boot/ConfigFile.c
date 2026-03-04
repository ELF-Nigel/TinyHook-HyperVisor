// ConfigFile.c - load config from /efi/elfhv/config.cfg
#include "Boot.h"
#include "BootLog.h"
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>

static BOOLEAN ParseBool(IN CONST CHAR16* Value, OUT UINT8* Out) {
  if (!Value || !Out) return FALSE;
  if (StrCmp(Value, L"true") == 0 || StrCmp(Value, L"1") == 0 || StrCmp(Value, L"enabled") == 0) {
    *Out = 1;
    return TRUE;
  }
  if (StrCmp(Value, L"false") == 0 || StrCmp(Value, L"0") == 0 || StrCmp(Value, L"disabled") == 0) {
    *Out = 0;
    return TRUE;
  }
  return FALSE;
}

static VOID Trim(CHAR16* Str) {
  if (!Str) return;
  UINTN len = StrLen(Str);
  while (len && (Str[len - 1] == L' ' || Str[len - 1] == L'\r' || Str[len - 1] == L'\n' || Str[len - 1] == L'\t')) {
    Str[len - 1] = 0;
    len--;
  }
  UINTN i = 0;
  while (Str[i] == L' ' || Str[i] == L'\t') i++;
  if (i) {
    UINTN j = 0;
    while (Str[i]) Str[j++] = Str[i++];
    Str[j] = 0;
  }
}

static EFI_STATUS ReadFileToBuffer(EFI_HANDLE ImageHandle, CONST CHAR16* Path, VOID** OutBuf, UINTN* OutSize) {
  EFI_STATUS Status;
  EFI_LOADED_IMAGE_PROTOCOL* LoadedImage = NULL;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL* Sfsp = NULL;
  EFI_FILE_PROTOCOL* Root = NULL;
  EFI_FILE_PROTOCOL* File = NULL;

  if (!OutBuf || !OutSize) return EFI_INVALID_PARAMETER;
  *OutBuf = NULL;
  *OutSize = 0;

  Status = gBS->HandleProtocol(ImageHandle, &gEfiLoadedImageProtocolGuid, (VOID**)&LoadedImage);
  if (EFI_ERROR(Status)) return Status;
  Status = gBS->HandleProtocol(LoadedImage->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&Sfsp);
  if (EFI_ERROR(Status)) return Status;
  Status = Sfsp->OpenVolume(Sfsp, &Root);
  if (EFI_ERROR(Status)) return Status;
  Status = Root->Open(Root, &File, (CHAR16*)Path, EFI_FILE_MODE_READ, 0);
  if (EFI_ERROR(Status)) return Status;

  EFI_FILE_INFO* Info = NULL;
  UINTN InfoSize = 0;
  Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, NULL);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    Info = AllocateZeroPool(InfoSize);
    if (!Info) return EFI_OUT_OF_RESOURCES;
    Status = File->GetInfo(File, &gEfiFileInfoGuid, &InfoSize, Info);
  }
  if (EFI_ERROR(Status) || !Info) {
    if (Info) FreePool(Info);
    return Status;
  }

  UINTN Size = (UINTN)Info->FileSize;
  FreePool(Info);
  if (Size == 0) return EFI_NOT_FOUND;

  VOID* Buf = AllocateZeroPool(Size + sizeof(CHAR16));
  if (!Buf) return EFI_OUT_OF_RESOURCES;
  Status = File->Read(File, &Size, Buf);
  if (EFI_ERROR(Status)) {
    FreePool(Buf);
    return Status;
  }

  *OutBuf = Buf;
  *OutSize = Size;
  return EFI_SUCCESS;
}

EFI_STATUS LoadConfigFile(EFI_HANDLE ImageHandle, THV_CONFIG* Config) {
  if (!Config) return EFI_INVALID_PARAMETER;
  VOID* Buf = NULL;
  UINTN Size = 0;

  EFI_STATUS Status = ReadFileToBuffer(ImageHandle, L"\\efi\\elfhv\\config.cfg", &Buf, &Size);
  if (EFI_ERROR(Status)) {
    BootLogAdd(L"[cfg] config file not found");
    return Status;
  }

  CHAR16* Text = (CHAR16*)Buf;
  UINTN chars = Size / sizeof(CHAR16);
  Text[chars] = 0;

  CHAR16* line = Text;
  while (*line) {
    CHAR16* next = StrStr(line, L"\n");
    if (next) *next = 0;
    Trim(line);
    if (line[0] != 0 && line[0] != L'#') {
      CHAR16* eq = StrStr(line, L"=");
      if (eq) {
        *eq = 0;
        CHAR16* key = line;
        CHAR16* val = eq + 1;
        Trim(key);
        Trim(val);

        if (StrCmp(key, L"hypervisor_enabled") == 0) ParseBool(val, &Config->EnableHv);
        else if (StrCmp(key, L"enable_vmx") == 0) ParseBool(val, &Config->EnableVmx);
        else if (StrCmp(key, L"enable_svm") == 0) ParseBool(val, &Config->EnableSvm);
        else if (StrCmp(key, L"enable_ept") == 0) ParseBool(val, &Config->EnableEpt);
        else if (StrCmp(key, L"enable_npt") == 0) ParseBool(val, &Config->EnableNpt);
        else if (StrCmp(key, L"enable_vpid") == 0) ParseBool(val, &Config->EnableVpid);
        else if (StrCmp(key, L"enable_ept_hooks") == 0) ParseBool(val, &Config->EnableHooks);
        else if (StrCmp(key, L"enable_cpuid_monitor") == 0) ParseBool(val, &Config->HookCpuid);
        else if (StrCmp(key, L"enable_msr_monitor") == 0) ParseBool(val, &Config->HookMsr);
        else if (StrCmp(key, L"enable_syscall_monitor") == 0) ParseBool(val, &Config->HookSyscall);
        else if (StrCmp(key, L"enable_memory_tracing") == 0) ParseBool(val, &Config->EnableMemIntegrity);
        else if (StrCmp(key, L"enable_guard_pages") == 0) ParseBool(val, &Config->EnableGuardPages);
        else if (StrCmp(key, L"enable_self_protect") == 0) ParseBool(val, &Config->EnableSelfProtect);
        else if (StrCmp(key, L"enable_anti_tamper") == 0) ParseBool(val, &Config->EnableAntiTamper);
        else if (StrCmp(key, L"enable_stealth") == 0) ParseBool(val, &Config->EnableStealth);
        else if (StrCmp(key, L"debug_logging") == 0) ParseBool(val, &Config->Verbose);
        else if (StrCmp(key, L"serial_debug") == 0) ParseBool(val, &Config->SerialDebug);
        else if (StrCmp(key, L"log_level") == 0) Config->LogLevel = (UINT8)StrDecimalToUintn(val);
        else if (StrCmp(key, L"vmexit_logging") == 0) ParseBool(val, &Config->VmexitLogging);
        else if (StrCmp(key, L"vmexit_sample_rate") == 0) Config->VmexitSampleRate = (UINT8)StrDecimalToUintn(val);
        else if (StrCmp(key, L"boot_delay_step") == 0) Config->BootDelayStep = (UINT8)StrDecimalToUintn(val);
      }
    }
    if (!next) break;
    line = next + 1;
  }

  BootLogAdd(L"[cfg] config file loaded");
  FreePool(Buf);
  return EFI_SUCCESS;
}
