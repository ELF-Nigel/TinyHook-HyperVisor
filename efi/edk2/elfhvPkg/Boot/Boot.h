// Boot.h - shared bootloader helpers
#pragma once
#include <Uefi.h>

typedef struct THV_FEATURES {
  BOOLEAN Vmx;
  BOOLEAN Ept;
  BOOLEAN Vpid;
  BOOLEAN Svm;
  BOOLEAN Npt;
} THV_FEATURES;

#pragma pack(push, 1)
typedef struct THV_CONFIG {
  UINT8 EnableHv;
  UINT8 HvMode; // 0=auto 1=vmx 2=svm
  UINT8 EnableVmx;
  UINT8 EnableSvm;
  UINT8 EnableEpt;
  UINT8 EnableNpt;
  UINT8 EnableVpid;
  UINT8 EnableHooks;
  UINT8 HookSyscall;
  UINT8 HookMsr;
  UINT8 HookCpuid;
  UINT8 EnableMemIntegrity;
  UINT8 EnableGuardPages;
  UINT8 EnableSelfProtect;
  UINT8 EnableAntiTamper;
  UINT8 EnableStealth;
  UINT8 Verbose;
  UINT8 SerialDebug;
  UINT8 LogLevel; // 0-3
  UINT8 VmexitLogging;
  UINT8 VmexitSampleRate; // 0-10
  UINT8 BootDelayStep; // 0-50, step=10ms
} THV_CONFIG;
#pragma pack(pop)

typedef struct THV_HANDOFF {
  UINT32 Signature;
  UINT32 Version;
  UINT32 Size;
  UINT32 Crc32;
  CHAR8 Vendor[13];
  THV_FEATURES Features;
  THV_CONFIG Config;
} THV_HANDOFF;

#pragma pack(push, 1)
typedef struct ELF_BOOT_STATUS {
  UINT64 BootTimestamp;
  BOOLEAN HvEnabled;
  BOOLEAN HvInitialized;
  BOOLEAN VirtualizationSupported;
  BOOLEAN EptSupported;
  BOOLEAN ConfigLoaded;
  UINT32 HvVersion;
} ELF_BOOT_STATUS;
#pragma pack(pop)

#define THV_HANDOFF_SIGNATURE 0x56484F54u // 'THOV'
#define THV_HANDOFF_VERSION   0x0001

extern EFI_GUID gThvHandoffGuid;

VOID GetCpuVendor(CHAR8* Out);
VOID GetHvFeatures(THV_FEATURES* Out);
EFI_STATUS LoadConfig(THV_CONFIG* Out);
EFI_STATUS LoadConfigFile(EFI_HANDLE ImageHandle, THV_CONFIG* Config);
EFI_STATUS CreateHandoff(THV_HANDOFF** OutHandoff);
EFI_STATUS WriteHandoffVariable(THV_HANDOFF* Handoff);
EFI_STATUS MeasureBufferTpm(IN EFI_HANDLE ImageHandle, IN CONST CHAR16* EventDesc, IN CONST VOID* Buffer, IN UINTN BufferSize);
EFI_STATUS WriteConfigVariable(THV_CONFIG* Config);
EFI_STATUS RecalcHandoffCrc(THV_HANDOFF* Handoff);
EFI_STATUS RunConfigMenu(EFI_HANDLE ImageHandle, THV_HANDOFF* Handoff);
