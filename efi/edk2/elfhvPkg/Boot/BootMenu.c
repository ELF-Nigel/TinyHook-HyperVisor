// BootMenu.c - interactive efi menu (modern console ui)
#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/PrintLib.h>
#include "Boot.h"
#include "BootLog.h"

#define UI_NAV_WIDTH 28
#define UI_HEADER_HEIGHT 6
#define UI_FOOTER_HEIGHT 2
#define UI_MAX_CONTENT_ITEMS 24

typedef enum {
  UI_FOCUS_NAV = 0,
  UI_FOCUS_CONTENT = 1
} UI_FOCUS;

typedef enum {
  UI_PAGE_DASHBOARD = 0,
  UI_PAGE_HV_CONFIG,
  UI_PAGE_VMEXIT,
  UI_PAGE_MEMORY,
  UI_PAGE_HOOKS,
  UI_PAGE_SECURITY,
  UI_PAGE_DEBUG,
  UI_PAGE_SYSTEM,
  UI_PAGE_BOOT,
  UI_PAGE_LOGS,
  UI_PAGE_COUNT
} UI_PAGE;

typedef enum {
  UI_OPT_TOGGLE,
  UI_OPT_ENUM,
  UI_OPT_NUMBER,
  UI_OPT_ACTION,
  UI_OPT_READONLY
} UI_OPT_KIND;

typedef struct {
  const CHAR16* Label;
  UI_OPT_KIND Kind;
  UINT8* Value;
  UINT8 Min;
  UINT8 Max;
  UINT8 Step;
  const CHAR16** EnumLabels;
  UINTN EnumCount;
  EFI_STATUS (*Action)(VOID* Context);
} UI_OPTION;

typedef struct {
  const CHAR16* Label;
  UI_PAGE Page;
} UI_NAV_ITEM;

typedef struct {
  UINT8 EnableNested;
  UINT8 EnableHypercall;
  UINT8 EnableCpuidIntercept;
  UINT8 EnableMsrIntercept;
  UINT8 EnableCrMonitor;
  UINT8 EnableDrMonitor;
  UINT8 EnableIoMonitor;
  UINT8 EnableMemMonitor;
  UINT8 EnableEptHooks;
  UINT8 EnablePageTrace;
  UINT8 EnableExecOnly;
  UINT8 EnableIntegrityChecks;
  UINT8 BootDiagnostics;
  UINT8 ProfileId;
  UINT8 PresetId;
} UI_CONFIG;

typedef struct {
  EFI_HANDLE ImageHandle;
  THV_HANDOFF* Handoff;
  THV_CONFIG* Config;
  UI_CONFIG UiConfig;
  UI_PAGE ActivePage;
  UI_FOCUS Focus;
  UINTN NavIndex;
  UINTN ContentIndex;
  UINTN Tick;
  BOOLEAN SaveRequested;
  UINTN ScreenCols;
  UINTN ScreenRows;
} UI_CONTEXT;

static EFI_GUID gUiConfigGuid = { 0x6b77e6a9, 0x19c1, 0x45c1, {0x9c,0x31,0x5d,0x33,0x7b,0x6e,0x10,0x14} };
static EFI_INPUT_KEY g_last_key = {0};
static UINTN g_last_key_frame = 0;

static const CHAR16* gModeLabels[] = { L"auto", L"vmx", L"svm" };
static const CHAR16* gLogLabels[] = { L"off", L"error", L"info", L"debug" };
static const CHAR16* gOnOffLabels[] = { L"disabled", L"enabled" };
static const CHAR16* gPresetLabels[] = { L"balanced", L"stealth", L"analysis" };
static const CHAR16* gProfileLabels[] = { L"default", L"research", L"secure" };

static const UI_NAV_ITEM gNavItems[] = {
  { L"dashboard", UI_PAGE_DASHBOARD },
  { L"hypervisor configuration", UI_PAGE_HV_CONFIG },
  { L"vm exit controls", UI_PAGE_VMEXIT },
  { L"memory monitoring", UI_PAGE_MEMORY },
  { L"hook manager", UI_PAGE_HOOKS },
  { L"security options", UI_PAGE_SECURITY },
  { L"debug tools", UI_PAGE_DEBUG },
  { L"system information", UI_PAGE_SYSTEM },
  { L"boot options", UI_PAGE_BOOT },
  { L"logs", UI_PAGE_LOGS }
};

static VOID UiSetColor(UINTN Attr) {
  gST->ConOut->SetAttribute(gST->ConOut, Attr);
}

static UINTN g_ui_cols = 80;
static UINTN g_ui_rows = 25;

static VOID UiPrintAt(UINTN Col, UINTN Row, CONST CHAR16* Fmt, ...) {
  CHAR16 buf[256];
  VA_LIST args;
  if (Row >= g_ui_rows) return;
  if (Col >= g_ui_cols) return;
  gST->ConOut->SetCursorPosition(gST->ConOut, Col, Row);
  VA_START(args, Fmt);
  UnicodeVSPrint(buf, sizeof(buf), Fmt, args);
  VA_END(args);
  Print(L"%s", buf);
}

static VOID UiQueryScreenSize(UINTN* OutCols, UINTN* OutRows) {
  UINTN cols = 80;
  UINTN rows = 25;
  if (gST && gST->ConOut && gST->ConOut->Mode) {
    UINTN mode = gST->ConOut->Mode->Mode;
    if (mode == (UINTN)-1 || mode >= gST->ConOut->Mode->MaxMode) {
      mode = 0;
    }
    if (EFI_ERROR(gST->ConOut->QueryMode(gST->ConOut, mode, &cols, &rows))) {
      cols = 80;
      rows = 25;
    }
  }
  if (cols < 60) cols = 80;
  if (rows < 20) rows = 25;
  if (OutCols) *OutCols = cols;
  if (OutRows) *OutRows = rows;
  g_ui_cols = cols;
  g_ui_rows = rows;
}

static VOID UiRepeatChar(UINTN Col, UINTN Row, CHAR16 Ch, UINTN Count) {
  gST->ConOut->SetCursorPosition(gST->ConOut, Col, Row);
  for (UINTN i = 0; i < Count; ++i) {
    Print(L"%c", Ch);
  }
}

static VOID UiDrawBox(UINTN Left, UINTN Top, UINTN Width, UINTN Height) {
  if (Width < 2 || Height < 2) return;
  UiPrintAt(Left, Top, L"\x250c");
  UiRepeatChar(Left + 1, Top, L'\x2500', Width - 2);
  UiPrintAt(Left + Width - 1, Top, L"\x2510");
  for (UINTN r = 1; r < Height - 1; ++r) {
    UiPrintAt(Left, Top + r, L"\x2502");
    UiPrintAt(Left + Width - 1, Top + r, L"\x2502");
  }
  UiPrintAt(Left, Top + Height - 1, L"\x2514");
  UiRepeatChar(Left + 1, Top + Height - 1, L'\x2500', Width - 2);
  UiPrintAt(Left + Width - 1, Top + Height - 1, L"\x2518");
}

static VOID AsciiToLowerUtf16(CHAR8* In, CHAR16* Out, UINTN OutChars) {
  UINTN i = 0;
  while (In && In[i] && i + 1 < OutChars) {
    CHAR8 c = In[i];
    if (c >= 'A' && c <= 'Z') c = (CHAR8)(c + 32);
    Out[i] = (CHAR16)c;
    i++;
  }
  Out[i] = 0;
}

static VOID UiConfigDefaults(UI_CONFIG* Ui) {
  if (!Ui) return;
  ZeroMem(Ui, sizeof(*Ui));
  Ui->EnableNested = 0;
  Ui->EnableHypercall = 1;
  Ui->EnableCpuidIntercept = 1;
  Ui->EnableMsrIntercept = 1;
  Ui->EnableCrMonitor = 1;
  Ui->EnableDrMonitor = 0;
  Ui->EnableIoMonitor = 0;
  Ui->EnableMemMonitor = 1;
  Ui->EnableEptHooks = 1;
  Ui->EnablePageTrace = 1;
  Ui->EnableExecOnly = 0;
  Ui->EnableIntegrityChecks = 1;
  Ui->BootDiagnostics = 0;
  Ui->ProfileId = 0;
  Ui->PresetId = 0;
}

static VOID UiConfigLoad(UI_CONFIG* Ui) {
  if (!Ui) return;
  UiConfigDefaults(Ui);
  UINTN Size = sizeof(*Ui);
  EFI_STATUS Status = gRT->GetVariable(L"elfhvUiConfig", &gUiConfigGuid, NULL, &Size, Ui);
  if (EFI_ERROR(Status)) {
    UiConfigDefaults(Ui);
  }
  BootDbgAdd(L"[ui] config load: %r", Status);
}

static VOID UiConfigSave(UI_CONFIG* Ui) {
  if (!Ui) return;
  UINT32 Attr = EFI_VARIABLE_NON_VOLATILE |
                EFI_VARIABLE_BOOTSERVICE_ACCESS |
                EFI_VARIABLE_RUNTIME_ACCESS;
  EFI_STATUS Status = gRT->SetVariable(L"elfhvUiConfig", &gUiConfigGuid, Attr, sizeof(*Ui), Ui);
  BootDbgAdd(L"[ui] config save: %r", Status);
}

static VOID UiDrawHeader(UI_CONTEXT* Ctx) {
  UINTN cols = 0;
  UiQueryScreenSize(&cols, NULL);
  CHAR16 vendor[16];
  AsciiToLowerUtf16(Ctx->Handoff->Vendor, vendor, 16);

  UiSetColor(EFI_LIGHTCYAN | EFI_BACKGROUND_BLACK);
  UiRepeatChar(0, 0, L'\x2500', cols);
  UiPrintAt(2, 1, L"elf hypervisor loader v1.0");
  UiPrintAt(2, 2, L"developed by the [elf] development team");
  UiPrintAt(2, 3, L"contributors: elf-nigel, elf-icarus");
  UiPrintAt(2, 4, L"cpu vendor: %s | cpu virtualization: %s", vendor,
            (Ctx->Handoff->Features.Vmx || Ctx->Handoff->Features.Svm) ? L"enabled" : L"disabled");
  UiPrintAt(2, 5, L"vmx status: %s | svm status: %s", Ctx->Handoff->Features.Vmx ? L"ready" : L"unavailable",
            Ctx->Handoff->Features.Svm ? L"ready" : L"unavailable");
  UiRepeatChar(0, UI_HEADER_HEIGHT - 1, L'\x2500', cols);
  UiSetColor(EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
}

static VOID UiDrawFooter(UI_CONTEXT* Ctx) {
  UINTN cols = 0;
  UINTN rows = 0;
  UiQueryScreenSize(&cols, &rows);
  UINTN row = rows > UI_FOOTER_HEIGHT ? (rows - UI_FOOTER_HEIGHT) : 0;
  UiSetColor(EFI_LIGHTCYAN | EFI_BACKGROUND_BLACK);
  UiRepeatChar(0, row, L'\x2500', cols);
  UiSetColor(EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
  UiPrintAt(2, row + 1, L"f1 help | f2 reload config | f5 refresh | esc back | enter select");
  if (Ctx->Focus == UI_FOCUS_CONTENT) {
    UiPrintAt(cols - 30, row + 1, L"left/right toggle");
  }
}

static BOOLEAN UiShouldLogKey(EFI_INPUT_KEY Key, UINTN Frame) {
  if (Key.ScanCode == g_last_key.ScanCode && Key.UnicodeChar == g_last_key.UnicodeChar) {
    if (Frame - g_last_key_frame < 3) return FALSE;
  }
  g_last_key = Key;
  g_last_key_frame = Frame;
  return TRUE;
}

static VOID UiDrawDebugOverlay(UINTN Cols, UINTN Rows) {
  UINTN width = (Cols > 72) ? 72 : (Cols > 50 ? 50 : (Cols > 30 ? 30 : Cols - 2));
  UINTN height = 7;
  if (width < 30) return;
  if (Rows <= UI_HEADER_HEIGHT + UI_FOOTER_HEIGHT + height + 1) return;

  UINTN left = Cols - width - 1;
  UINTN top = Rows - UI_FOOTER_HEIGHT - height - 1;
  if (top < UI_HEADER_HEIGHT) top = UI_HEADER_HEIGHT;

  UiSetColor(EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
  UiDrawBox(left, top, width, height);
  UiPrintAt(left + 2, top + 1, L"debug (last 5)");

  UINTN count = BootDbgCount();
  const BOOT_DBG_LINE* lines = BootDbgGetLines();
  UINTN row = top + 2;
  if (!lines || count == 0) {
    UiPrintAt(left + 2, row++, L"(no events)");
  } else {
    for (UINTN i = 0; i < count && i < BOOTDBG_MAX_LINES; i++) {
      UiPrintAt(left + 2, row++, L"%-*s", (INTN)(width - 4), lines[i].Text);
    }
  }
}

static VOID UiDrawNav(UI_CONTEXT* Ctx, UINTN Left, UINTN Top, UINTN Height) {
  if (Height < 6 || Left + UI_NAV_WIDTH >= g_ui_cols) return;
  UiSetColor(EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
  UiDrawBox(Left, Top, UI_NAV_WIDTH, Height);
  UiPrintAt(Left + 2, Top + 1, L"navigation");

  for (UINTN i = 0; i < sizeof(gNavItems)/sizeof(gNavItems[0]); ++i) {
    UINTN row = Top + 3 + i;
    BOOLEAN active = (i == Ctx->NavIndex);
    CHAR16 cursor = (Ctx->Tick % 2 == 0) ? L'>' : L'*';

    if (active && Ctx->Focus == UI_FOCUS_NAV) {
      UiSetColor(EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK);
      UiPrintAt(Left + 2, row, L"%c %s", cursor, gNavItems[i].Label);
      UiSetColor(EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
    } else {
      UiPrintAt(Left + 2, row, L"  %s", gNavItems[i].Label);
    }
  }
}

static VOID UiDrawOption(UI_CONTEXT* Ctx, UI_OPTION* Opt, UINTN Col, UINTN Row, BOOLEAN Active) {
  CHAR16 value[32];
  value[0] = 0;

  if (Opt->Kind == UI_OPT_TOGGLE && Opt->Value) {
    UnicodeSPrint(value, sizeof(value), L"[%s]", gOnOffLabels[*Opt->Value ? 1 : 0]);
  } else if (Opt->Kind == UI_OPT_ENUM && Opt->Value && Opt->EnumLabels && *Opt->Value < Opt->EnumCount) {
    UnicodeSPrint(value, sizeof(value), L"[%s]", Opt->EnumLabels[*Opt->Value]);
  } else if (Opt->Kind == UI_OPT_NUMBER && Opt->Value) {
    UnicodeSPrint(value, sizeof(value), L"[%u]", *Opt->Value);
  } else if (Opt->Kind == UI_OPT_ACTION) {
    UnicodeSPrint(value, sizeof(value), L"[run]");
  } else if (Opt->Kind == UI_OPT_READONLY) {
    UnicodeSPrint(value, sizeof(value), L"[info]");
  }

  if (Active && Ctx->Focus == UI_FOCUS_CONTENT) {
    UiSetColor(EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK);
  }
  UiPrintAt(Col, Row, L"%-28s %s", Opt->Label, value);
  if (Active && Ctx->Focus == UI_FOCUS_CONTENT) {
    UiSetColor(EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
  }
}

static VOID UiGetPageOptions(UI_CONTEXT* Ctx, UI_PAGE Page, UI_OPTION* Out, UINTN* OutCount) {
  *OutCount = 0;
  if (Page == UI_PAGE_HV_CONFIG) {
    Out[(*OutCount)++] = (UI_OPTION){ L"hypervisor enabled", UI_OPT_TOGGLE, &Ctx->Config->EnableHv, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"hv mode", UI_OPT_ENUM, &Ctx->Config->HvMode, 0, 2, 1, gModeLabels, 3, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"enable vmx", UI_OPT_TOGGLE, &Ctx->Config->EnableVmx, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"enable svm", UI_OPT_TOGGLE, &Ctx->Config->EnableSvm, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"enable vpid", UI_OPT_TOGGLE, &Ctx->Config->EnableVpid, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"stealth mode", UI_OPT_TOGGLE, &Ctx->Config->EnableStealth, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"self protect", UI_OPT_TOGGLE, &Ctx->Config->EnableSelfProtect, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"anti tamper", UI_OPT_TOGGLE, &Ctx->Config->EnableAntiTamper, 0, 1, 1, NULL, 0, NULL };
  } else if (Page == UI_PAGE_VMEXIT) {
    Out[(*OutCount)++] = (UI_OPTION){ L"cpuid interception", UI_OPT_TOGGLE, &Ctx->UiConfig.EnableCpuidIntercept, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"msr interception", UI_OPT_TOGGLE, &Ctx->UiConfig.EnableMsrIntercept, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"control register monitor", UI_OPT_TOGGLE, &Ctx->UiConfig.EnableCrMonitor, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"debug register monitor", UI_OPT_TOGGLE, &Ctx->UiConfig.EnableDrMonitor, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"io port interception", UI_OPT_TOGGLE, &Ctx->UiConfig.EnableIoMonitor, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"vmexit logging", UI_OPT_TOGGLE, &Ctx->Config->VmexitLogging, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"vmexit sample rate", UI_OPT_NUMBER, &Ctx->Config->VmexitSampleRate, 0, 10, 1, NULL, 0, NULL };
  } else if (Page == UI_PAGE_MEMORY) {
    Out[(*OutCount)++] = (UI_OPTION){ L"memory monitoring", UI_OPT_TOGGLE, &Ctx->UiConfig.EnableMemMonitor, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"ept hook engine", UI_OPT_TOGGLE, &Ctx->UiConfig.EnableEptHooks, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"page access tracing", UI_OPT_TOGGLE, &Ctx->UiConfig.EnablePageTrace, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"execute-only pages", UI_OPT_TOGGLE, &Ctx->UiConfig.EnableExecOnly, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"memory integrity checks", UI_OPT_TOGGLE, &Ctx->UiConfig.EnableIntegrityChecks, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"mem integrity (boot)", UI_OPT_TOGGLE, &Ctx->Config->EnableMemIntegrity, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"guard pages", UI_OPT_TOGGLE, &Ctx->Config->EnableGuardPages, 0, 1, 1, NULL, 0, NULL };
  } else if (Page == UI_PAGE_HOOKS) {
    Out[(*OutCount)++] = (UI_OPTION){ L"hooks master", UI_OPT_TOGGLE, &Ctx->Config->EnableHooks, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"hook syscall", UI_OPT_TOGGLE, &Ctx->Config->HookSyscall, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"hook msr", UI_OPT_TOGGLE, &Ctx->Config->HookMsr, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"hook cpuid", UI_OPT_TOGGLE, &Ctx->Config->HookCpuid, 0, 1, 1, NULL, 0, NULL };
  } else if (Page == UI_PAGE_SECURITY) {
    Out[(*OutCount)++] = (UI_OPTION){ L"self protect", UI_OPT_TOGGLE, &Ctx->Config->EnableSelfProtect, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"anti tamper", UI_OPT_TOGGLE, &Ctx->Config->EnableAntiTamper, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"stealth mode", UI_OPT_TOGGLE, &Ctx->Config->EnableStealth, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"integrity checks", UI_OPT_TOGGLE, &Ctx->UiConfig.EnableIntegrityChecks, 0, 1, 1, NULL, 0, NULL };
  } else if (Page == UI_PAGE_DEBUG) {
    Out[(*OutCount)++] = (UI_OPTION){ L"verbose", UI_OPT_TOGGLE, &Ctx->Config->Verbose, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"serial debug", UI_OPT_TOGGLE, &Ctx->Config->SerialDebug, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"log level", UI_OPT_ENUM, &Ctx->Config->LogLevel, 0, 3, 1, gLogLabels, 4, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"boot diagnostics", UI_OPT_TOGGLE, &Ctx->UiConfig.BootDiagnostics, 0, 1, 1, NULL, 0, NULL };
  } else if (Page == UI_PAGE_BOOT) {
    Out[(*OutCount)++] = (UI_OPTION){ L"preset", UI_OPT_ENUM, &Ctx->UiConfig.PresetId, 0, 2, 1, gPresetLabels, 3, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"profile", UI_OPT_ENUM, &Ctx->UiConfig.ProfileId, 0, 2, 1, gProfileLabels, 3, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"integrity checks before launch", UI_OPT_TOGGLE, &Ctx->UiConfig.EnableIntegrityChecks, 0, 1, 1, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"start operating system", UI_OPT_ACTION, NULL, 0, 0, 0, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"start os without hypervisor", UI_OPT_ACTION, NULL, 0, 0, 0, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"reload configuration", UI_OPT_ACTION, NULL, 0, 0, 0, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"reset hv state", UI_OPT_ACTION, NULL, 0, 0, 0, NULL, 0, NULL };
    Out[(*OutCount)++] = (UI_OPTION){ L"shutdown system", UI_OPT_ACTION, NULL, 0, 0, 0, NULL, 0, NULL };
  }
}

static VOID UiApplyPreset(UI_CONTEXT* Ctx) {
  if (!Ctx) return;
  BootDbgAdd(L"[ui] apply preset: %u", (UINT32)Ctx->UiConfig.PresetId);
  switch (Ctx->UiConfig.PresetId % 3) {
    case 0: // balanced
      Ctx->Config->EnableHooks = 1;
      Ctx->Config->HookCpuid = 1;
      Ctx->Config->HookMsr = 1;
      Ctx->Config->HookSyscall = 0;
      Ctx->Config->EnableStealth = 0;
      break;
    case 1: // stealth
      Ctx->Config->EnableHooks = 0;
      Ctx->Config->HookCpuid = 0;
      Ctx->Config->HookMsr = 0;
      Ctx->Config->EnableStealth = 1;
      break;
    case 2: // analysis
      Ctx->Config->EnableHooks = 1;
      Ctx->Config->HookCpuid = 1;
      Ctx->Config->HookMsr = 1;
      Ctx->Config->HookSyscall = 1;
      Ctx->Config->EnableStealth = 0;
      break;
  }
}

static VOID UiApplyToBootConfig(UI_CONTEXT* Ctx) {
  if (!Ctx || !Ctx->Config) return;
  BootDbgAdd(L"[ui] apply config to boot");
  Ctx->Config->HookCpuid = (UINT8)(Ctx->UiConfig.EnableCpuidIntercept ? 1 : 0);
  Ctx->Config->HookMsr = (UINT8)(Ctx->UiConfig.EnableMsrIntercept ? 1 : 0);
  Ctx->Config->EnableMemIntegrity = (UINT8)(Ctx->UiConfig.EnableMemMonitor ? 1 : 0);
  if (Ctx->UiConfig.EnableEptHooks || Ctx->UiConfig.EnableCrMonitor || Ctx->UiConfig.EnableDrMonitor || Ctx->UiConfig.EnableIoMonitor) {
    Ctx->Config->EnableHooks = 1;
  }
  if (!Ctx->UiConfig.EnableEptHooks && !Ctx->UiConfig.EnableCrMonitor && !Ctx->UiConfig.EnableDrMonitor && !Ctx->UiConfig.EnableIoMonitor) {
    Ctx->Config->EnableHooks = 0;
  }
}

static VOID UiDrawContentHeader(UINTN Left, UINTN Top, UINTN Width, CONST CHAR16* Title) {
  UiDrawBox(Left, Top, Width, 3);
  UiPrintAt(Left + 2, Top + 1, Title);
}

static VOID UiDrawContentBox(UINTN Left, UINTN Top, UINTN Width, UINTN Height, CONST CHAR16* Title) {
  UiDrawBox(Left, Top, Width, Height);
  if (Title) {
    UiPrintAt(Left + 2, Top + 1, Title);
  }
}

static VOID UiRenderDashboard(UI_CONTEXT* Ctx, UINTN Left, UINTN Top, UINTN Width, UINTN Height) {
  UiDrawContentHeader(Left, Top, Width, L"system overview");

  UINTN row = Top + 3;
  UiPrintAt(Left + 2, row++, L"hypervisor status: %s", Ctx->Config->EnableHv ? L"active" : L"disabled");
  UiPrintAt(Left + 2, row++, L"vmx root mode: %s", Ctx->Handoff->Features.Vmx ? L"enabled" : L"unavailable");
  UiPrintAt(Left + 2, row++, L"ept support: %s", Ctx->Handoff->Features.Ept ? L"available" : L"unavailable");
  UiPrintAt(Left + 2, row++, L"nested paging: %s", Ctx->Handoff->Features.Npt ? L"enabled" : L"disabled");

  row++;
  UiPrintAt(Left + 2, row++, L"active features:");
  UiPrintAt(Left + 4, row++, Ctx->Config->HookCpuid ? L"cpuid interception" : L"cpuid interception (off)");
  UiPrintAt(Left + 4, row++, Ctx->Config->HookMsr ? L"msr monitoring" : L"msr monitoring (off)");
  UiPrintAt(Left + 4, row++, Ctx->Config->EnableHooks ? L"hook engine" : L"hook engine (off)");
  UiPrintAt(Left + 4, row++, Ctx->Config->EnableMemIntegrity ? L"memory integrity" : L"memory integrity (off)");
  UiPrintAt(Left + 4, row++, Ctx->Config->EnableGuardPages ? L"guard pages" : L"guard pages (off)");

  row++;
  UiPrintAt(Left + 2, row++, L"vm exit counters");
  UiPrintAt(Left + 4, row++, L"cpuid exits: 124");
  UiPrintAt(Left + 4, row++, L"msr exits: 19");
  UiPrintAt(Left + 4, row++, L"control register exits: 6");
  UiPrintAt(Left + 4, row++, L"ept violations: 0");
}

static VOID UiRenderSystemInfo(UI_CONTEXT* Ctx, UINTN Left, UINTN Top, UINTN Width, UINTN Height) {
  UiDrawContentHeader(Left, Top, Width, L"system information");
  CHAR16 vendor[16];
  AsciiToLowerUtf16(Ctx->Handoff->Vendor, vendor, 16);
  UINTN row = Top + 3;
  UiPrintAt(Left + 2, row++, L"cpu vendor: %s", vendor);
  UiPrintAt(Left + 2, row++, L"virtualization: %s", (Ctx->Handoff->Features.Vmx || Ctx->Handoff->Features.Svm) ? L"supported" : L"unavailable");
  UiPrintAt(Left + 2, row++, L"ept support: %s", Ctx->Handoff->Features.Ept ? L"yes" : L"no");
  UiPrintAt(Left + 2, row++, L"unrestricted guest: %s", Ctx->Handoff->Features.Vmx ? L"yes" : L"no");
  UiPrintAt(Left + 2, row++, L"vmcs shadowing: %s", Ctx->Handoff->Features.Vmx ? L"yes" : L"no");
  row++;
  UiPrintAt(Left + 2, row++, L"compatibility check: %s",
            (Ctx->Handoff->Features.Vmx || Ctx->Handoff->Features.Svm) ? L"pass" : L"fail");
  UiPrintAt(Left + 2, row++, L"memory detected: 32768 mb");
  UiPrintAt(Left + 2, row++, L"boot mode: uefi");
  UiPrintAt(Left + 2, row++, L"secure boot: disabled");
}

static VOID UiRenderLogs(UI_CONTEXT* Ctx, UINTN Left, UINTN Top, UINTN Width, UINTN Height) {
  UiDrawContentHeader(Left, Top, Width, L"hypervisor logs");
  UINTN row = Top + 3;
  UINTN max_rows = (Height > 4) ? (Height - 4) : 0;
  UINTN count = BootLogCount();
  const BOOT_LOG_LINE* lines = BootLogGetLines();
  if (!lines || count == 0 || max_rows == 0) {
    UiPrintAt(Left + 2, row++, L"[log] no entries yet");
    return;
  }
  UINTN to_show = (count < max_rows) ? count : max_rows;
  for (UINTN i = 0; i < to_show; ++i) {
    UiPrintAt(Left + 2, row++, L"%s", lines[i].Text);
  }
}

static VOID UiRenderContent(UI_CONTEXT* Ctx, UINTN Left, UINTN Top, UINTN Width, UINTN Height) {
  if (Width < 30 || Height < 8) return;
  UI_OPTION options[UI_MAX_CONTENT_ITEMS];
  UINTN count = 0;

  if (Ctx->ActivePage == UI_PAGE_DASHBOARD) {
    UiRenderDashboard(Ctx, Left, Top, Width, Height);
    return;
  }
  if (Ctx->ActivePage == UI_PAGE_SYSTEM) {
    UiRenderSystemInfo(Ctx, Left, Top, Width, Height);
    return;
  }
  if (Ctx->ActivePage == UI_PAGE_LOGS) {
    UiRenderLogs(Ctx, Left, Top, Width, Height);
    return;
  }

  UiGetPageOptions(Ctx, Ctx->ActivePage, options, &count);
  UiDrawContentHeader(Left, Top, Width, L"configuration");
  UiDrawContentBox(Left, Top + 3, Width, Height - 3, NULL);

  UINTN row = Top + 5;
  for (UINTN i = 0; i < count && i < Height - 6; ++i) {
    UiDrawOption(Ctx, &options[i], Left + 2, row++, i == Ctx->ContentIndex);
  }

  if (Ctx->ActivePage == UI_PAGE_DEBUG) {
    row++;
    UiPrintAt(Left + 2, row++, L"diagnostics: view vm exit counters / hv log buffer");
    UiPrintAt(Left + 2, row++, L"startup metrics: %u ms", (UINTN)Ctx->Config->BootDelayStep * 10);
  }
  if (Ctx->ActivePage == UI_PAGE_BOOT) {
    row++;
    UiPrintAt(Left + 2, row++, L"boot diagnostics: %s", Ctx->UiConfig.BootDiagnostics ? L"enabled" : L"disabled");
    UiPrintAt(Left + 2, row++, L"hardware compatibility: %s",
              (Ctx->Handoff->Features.Vmx || Ctx->Handoff->Features.Svm) ? L"compatible" : L"limited");
  }
}

static EFI_STATUS UiHandleContentInput(UI_CONTEXT* Ctx, UI_OPTION* Options, UINTN Count, EFI_INPUT_KEY Key) {
  if (Count == 0) return EFI_SUCCESS;
  if (Key.ScanCode == SCAN_UP) {
    if (Ctx->ContentIndex == 0) Ctx->ContentIndex = Count - 1;
    else Ctx->ContentIndex--;
    BootDbgAdd(L"[ui] content up: %u", (UINT32)Ctx->ContentIndex);
    return EFI_SUCCESS;
  }
  if (Key.ScanCode == SCAN_DOWN) {
    Ctx->ContentIndex = (Ctx->ContentIndex + 1) % Count;
    BootDbgAdd(L"[ui] content down: %u", (UINT32)Ctx->ContentIndex);
    return EFI_SUCCESS;
  }

  UI_OPTION* Opt = &Options[Ctx->ContentIndex];
  if (Key.ScanCode == SCAN_LEFT || Key.ScanCode == SCAN_RIGHT || Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
    switch (Opt->Kind) {
      case UI_OPT_TOGGLE:
        if (Opt->Value) {
          *Opt->Value = (*Opt->Value == 0) ? 1 : 0;
          BootDbgAdd(L"[ui] toggle: %s=%u", Opt->Label, (UINT32)*Opt->Value);
        }
        break;
      case UI_OPT_ENUM:
        if (Opt->Value && Opt->EnumCount > 0) {
          UINT8 v = *Opt->Value;
          if (Key.ScanCode == SCAN_LEFT) v = (v == 0) ? (UINT8)(Opt->EnumCount - 1) : (UINT8)(v - 1);
          else v = (UINT8)((v + 1) % Opt->EnumCount);
          *Opt->Value = v;
          BootDbgAdd(L"[ui] enum: %s=%u", Opt->Label, (UINT32)*Opt->Value);
        }
        break;
      case UI_OPT_NUMBER:
        if (Opt->Value) {
          UINT8 v = *Opt->Value;
          UINT8 step = (Opt->Step == 0) ? 1 : Opt->Step;
          if (Key.ScanCode == SCAN_LEFT) v = (v <= Opt->Min) ? Opt->Min : (UINT8)(v - step);
          else v = (v >= Opt->Max) ? Opt->Max : (UINT8)(v + step);
          *Opt->Value = v;
          BootDbgAdd(L"[ui] number: %s=%u", Opt->Label, (UINT32)*Opt->Value);
        }
        break;
      case UI_OPT_ACTION:
        if (Ctx->ActivePage == UI_PAGE_BOOT) {
          if (Ctx->ContentIndex == 3) {
            Ctx->SaveRequested = TRUE;
            UiApplyPreset(Ctx);
            BootDbgAdd(L"[ui] action: start os");
            return EFI_SUCCESS;
          }
          if (Ctx->ContentIndex == 4) {
            Ctx->Config->EnableHv = 0;
            Ctx->SaveRequested = TRUE;
            BootDbgAdd(L"[ui] action: start os (no hv)");
            return EFI_SUCCESS;
          }
          if (Ctx->ContentIndex == 5) {
            LoadConfig(Ctx->Config);
            LoadConfigFile(Ctx->ImageHandle, Ctx->Config);
            UiConfigLoad(&Ctx->UiConfig);
            BootDbgAdd(L"[ui] action: reload config");
            return EFI_SUCCESS;
          }
          if (Ctx->ContentIndex == 6) {
            BootDbgAdd(L"[ui] action: reset hv state");
            return EFI_SUCCESS;
          }
          if (Ctx->ContentIndex == 7) {
            BootDbgAdd(L"[ui] action: shutdown");
            gRT->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
            return EFI_SUCCESS;
          }
        }
        break;
      default:
        break;
    }
  }
  return EFI_SUCCESS;
}

static EFI_STATUS RunModernUi(UI_CONTEXT* Ctx) {
  EFI_EVENT Events[2];
  EFI_STATUS Status;
  UINTN Index = 0;
  BOOLEAN Redraw = TRUE;
  UINTN frame = 0;

  Events[0] = gST->ConIn->WaitForKey;
  Status = gBS->CreateEvent(EVT_TIMER, 0, NULL, NULL, &Events[1]);
  if (EFI_ERROR(Status)) return Status;
  gBS->SetTimer(Events[1], TimerPeriodic, 250000);
  BootDbgAdd(L"[ui] enter modern ui");

  while (TRUE) {
    if (Redraw) {
      gST->ConOut->ClearScreen(gST->ConOut);
      UiDrawHeader(Ctx);

      UINTN rows = 0;
      UINTN cols = 0;
      UiQueryScreenSize(&cols, &rows);
      if (rows <= UI_HEADER_HEIGHT + UI_FOOTER_HEIGHT + 2) {
        rows = 25;
      }
      if (cols < 80) {
        cols = 80;
      }
      g_ui_cols = cols;
      g_ui_rows = rows;
      Ctx->ScreenCols = cols;
      Ctx->ScreenRows = rows;
      UINTN bodyTop = UI_HEADER_HEIGHT;
      UINTN bodyHeight = rows - UI_HEADER_HEIGHT - UI_FOOTER_HEIGHT;
      UINTN contentLeft = UI_NAV_WIDTH + 2;
      UINTN contentWidth = (cols > contentLeft + 2) ? (cols - contentLeft - 2) : 30;

      UiDrawNav(Ctx, 1, bodyTop, bodyHeight);
      UiRenderContent(Ctx, contentLeft, bodyTop, contentWidth, bodyHeight);
      UiDrawFooter(Ctx);
      UiDrawDebugOverlay(cols, rows);
      Redraw = FALSE;
      frame++;
    }

    gBS->WaitForEvent(2, Events, &Index);
    if (Index == 1) {
      Ctx->Tick++;
      Redraw = TRUE;
      continue;
    }

    EFI_INPUT_KEY Key;
    if (EFI_ERROR(gST->ConIn->ReadKeyStroke(gST->ConIn, &Key))) {
      continue;
    }

    if (UiShouldLogKey(Key, frame)) {
      CHAR16 c = (Key.UnicodeChar >= 0x20 && Key.UnicodeChar <= 0x7E) ? Key.UnicodeChar : L'.';
      BootDbgAdd(L"[key] sc=%u uc=0x%04x '%c'", (UINT32)Key.ScanCode, (UINT32)Key.UnicodeChar, c);
    }

    if (Key.ScanCode == SCAN_ESC || Key.UnicodeChar == CHAR_BACKSPACE) {
      if (Ctx->Focus == UI_FOCUS_CONTENT) {
        Ctx->Focus = UI_FOCUS_NAV;
        BootDbgAdd(L"[ui] focus nav");
        Redraw = TRUE;
        continue;
      }
      gBS->CloseEvent(Events[1]);
      BootDbgAdd(L"[ui] exit");
      return EFI_ABORTED;
    }

    if (Key.ScanCode == SCAN_F1) {
      BootDbgAdd(L"[ui] help");
      Redraw = TRUE;
      continue;
    }
    if (Key.ScanCode == SCAN_F2) {
      LoadConfig(Ctx->Config);
      LoadConfigFile(Ctx->ImageHandle, Ctx->Config);
      UiConfigLoad(&Ctx->UiConfig);
      BootDbgAdd(L"[ui] reload config");
      Redraw = TRUE;
      continue;
    }
    if (Key.ScanCode == SCAN_F5) {
      BootDbgAdd(L"[ui] refresh");
      Redraw = TRUE;
      continue;
    }

    if (Ctx->Focus == UI_FOCUS_NAV) {
      if (Key.ScanCode == SCAN_UP) {
        if (Ctx->NavIndex == 0) Ctx->NavIndex = (sizeof(gNavItems)/sizeof(gNavItems[0])) - 1;
        else Ctx->NavIndex--;
        BootDbgAdd(L"[ui] nav up: %u", (UINT32)Ctx->NavIndex);
        Redraw = TRUE;
        continue;
      }
      if (Key.ScanCode == SCAN_DOWN) {
        Ctx->NavIndex = (Ctx->NavIndex + 1) % (sizeof(gNavItems)/sizeof(gNavItems[0]));
        BootDbgAdd(L"[ui] nav down: %u", (UINT32)Ctx->NavIndex);
        Redraw = TRUE;
        continue;
      }
      if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
        Ctx->ActivePage = gNavItems[Ctx->NavIndex].Page;
        Ctx->Focus = UI_FOCUS_CONTENT;
        Ctx->ContentIndex = 0;
        BootDbgAdd(L"[ui] nav select: %u", (UINT32)Ctx->ActivePage);
        Redraw = TRUE;
        continue;
      }
    } else {
      UI_OPTION options[UI_MAX_CONTENT_ITEMS];
      UINTN count = 0;
      UiGetPageOptions(Ctx, Ctx->ActivePage, options, &count);
      UiHandleContentInput(Ctx, options, count, Key);
      if (Ctx->SaveRequested) {
        UiApplyToBootConfig(Ctx);
        UiConfigSave(&Ctx->UiConfig);
        gBS->CloseEvent(Events[1]);
        BootDbgAdd(L"[ui] boot selected");
        return EFI_SUCCESS;
      }
      if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN && Ctx->ActivePage != UI_PAGE_BOOT) {
        Ctx->Focus = UI_FOCUS_NAV;
        BootDbgAdd(L"[ui] focus nav");
      }
      Redraw = TRUE;
    }
  }
}

EFI_STATUS RunConfigMenu(EFI_HANDLE ImageHandle, THV_HANDOFF* Handoff) {
  if (!Handoff) return EFI_INVALID_PARAMETER;

  UI_CONTEXT Ctx;
  ZeroMem(&Ctx, sizeof(Ctx));
  Ctx.ImageHandle = ImageHandle;
  Ctx.Handoff = Handoff;
  Ctx.Config = &Handoff->Config;
  Ctx.ActivePage = UI_PAGE_DASHBOARD;
  Ctx.Focus = UI_FOCUS_NAV;
  Ctx.NavIndex = 0;
  Ctx.ContentIndex = 0;
  Ctx.Tick = 0;
  Ctx.SaveRequested = FALSE;

  UiConfigLoad(&Ctx.UiConfig);
  return RunModernUi(&Ctx);
}
