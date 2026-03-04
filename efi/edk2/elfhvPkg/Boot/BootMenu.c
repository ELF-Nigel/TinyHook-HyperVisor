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

#define UI_HEADER_HEIGHT 6
#define UI_FOOTER_HEIGHT 2
#define UI_MAX_CONTENT_ITEMS 32
#define UI_MAX_PAGE_STACK 8

typedef enum {
  UI_PAGE_MAIN = 0,
  UI_PAGE_FEATURES,
  UI_PAGE_HV_CONFIG,
  UI_PAGE_STATUS,
  UI_PAGE_LOGS,
  UI_PAGE_COUNT
} UI_PAGE;

typedef enum {
  UI_ITEM_SUBPAGE,
  UI_ITEM_TOGGLE,
  UI_ITEM_ENUM,
  UI_ITEM_NUMBER,
  UI_ITEM_ACTION,
  UI_ITEM_READONLY
} UI_ITEM_KIND;

typedef struct {
  const CHAR16* Label;
  UI_ITEM_KIND Kind;
  UINT8* Value;
  UINT8 Min;
  UINT8 Max;
  UINT8 Step;
  const CHAR16** EnumLabels;
  UINTN EnumCount;
  UI_PAGE SubPage;
  EFI_STATUS (*Action)(VOID* Context);
} UI_MENU_ITEM;

typedef struct {
  const CHAR16* Title;
  UI_MENU_ITEM* Items;
  UINTN Count;
} UI_MENU_PAGE;

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

typedef enum {
  UI_ACTION_NONE = 0,
  UI_ACTION_BOOT,
  UI_ACTION_BOOT_NO_HV,
  UI_ACTION_RELOAD,
  UI_ACTION_RESET,
  UI_ACTION_SHUTDOWN
} UI_ACTION;

typedef struct {
  EFI_HANDLE ImageHandle;
  THV_HANDOFF* Handoff;
  THV_CONFIG* Config;
  UI_CONFIG UiConfig;
  UI_PAGE PageStack[UI_MAX_PAGE_STACK];
  UINTN PageDepth;
  UINTN Selection[UI_PAGE_COUNT];
  UINTN Tick;
  BOOLEAN SaveRequested;
  UI_ACTION Action;
  ELF_BOOT_STATUS LastBoot;
  BOOLEAN LastBootValid;
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

static VOID UiLoadLastBoot(UI_CONTEXT* Ctx) {
  if (!Ctx) return;
  ZeroMem(&Ctx->LastBoot, sizeof(Ctx->LastBoot));
  Ctx->LastBootValid = FALSE;
  UINTN Size = sizeof(Ctx->LastBoot);
  EFI_STATUS Status = gRT->GetVariable(L"elf_boot_status", &gElfBootStatusGuid, NULL, &Size, &Ctx->LastBoot);
  if (!EFI_ERROR(Status) && Size == sizeof(Ctx->LastBoot)) {
    Ctx->LastBootValid = TRUE;
  }
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
  UiPrintAt(2, 4, L"cpu vendor: %s | virtualization: %s", vendor,
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
  UiPrintAt(2, row + 1, L"up/down select | left/right change | enter action | esc back");
  UiPrintAt(cols - 26, row + 1, L"f2 reload | f5 refresh");
  (void)Ctx;
}

static BOOLEAN UiShouldLogKey(EFI_INPUT_KEY Key, UINTN Frame) {
  if (Key.ScanCode == g_last_key.ScanCode && Key.UnicodeChar == g_last_key.UnicodeChar) {
    if (Frame - g_last_key_frame < 3) return FALSE;
  }
  g_last_key = Key;
  g_last_key_frame = Frame;
  return TRUE;
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

static VOID UiDrawOption(UI_CONTEXT* Ctx, UI_MENU_ITEM* Item, UINTN Col, UINTN Row, BOOLEAN Active) {
  CHAR16 value[32];
  value[0] = 0;

  switch (Item->Kind) {
    case UI_ITEM_TOGGLE:
      if (Item->Value) {
        UnicodeSPrint(value, sizeof(value), L"[%s]", gOnOffLabels[*Item->Value ? 1 : 0]);
      }
      break;
    case UI_ITEM_ENUM:
      if (Item->Value && Item->EnumLabels && *Item->Value < Item->EnumCount) {
        UnicodeSPrint(value, sizeof(value), L"[%s]", Item->EnumLabels[*Item->Value]);
      }
      break;
    case UI_ITEM_NUMBER:
      if (Item->Value) {
        UnicodeSPrint(value, sizeof(value), L"[%u]", *Item->Value);
      }
      break;
    case UI_ITEM_ACTION:
      UnicodeSPrint(value, sizeof(value), L"[run]");
      break;
    case UI_ITEM_SUBPAGE:
      UnicodeSPrint(value, sizeof(value), L"[open]");
      break;
    case UI_ITEM_READONLY:
      UnicodeSPrint(value, sizeof(value), L"[info]");
      break;
    default:
      break;
  }

  if (Active) {
    UiSetColor(EFI_LIGHTGREEN | EFI_BACKGROUND_BLACK);
  }
  UiPrintAt(Col, Row, L"%-30s %s", Item->Label, value);
  if (Active) {
    UiSetColor(EFI_LIGHTGRAY | EFI_BACKGROUND_BLACK);
  }
  (void)Ctx;
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

static VOID UiDrawLogPanel(UINTN Left, UINTN Top, UINTN Width, UINTN Height, CONST CHAR16* Title) {
  UiDrawContentBox(Left, Top, Width, Height, Title);
  UINTN row = Top + 2;
  UINTN max_rows = (Height > 3) ? (Height - 3) : 0;
  UINTN count = BootLogCount();
  const BOOT_LOG_LINE* lines = BootLogGetLines();
  if (!lines || count == 0 || max_rows == 0) {
    UiPrintAt(Left + 2, row++, L"(no log entries)");
    return;
  }
  UINTN to_show = (count < max_rows) ? count : max_rows;
  UINTN start = (count > to_show) ? (count - to_show) : 0;
  for (UINTN i = 0; i < to_show; ++i) {
    UiPrintAt(Left + 2, row++, L"%s", lines[start + i].Text);
  }
}

static VOID UiRenderStatus(UI_CONTEXT* Ctx, UINTN Left, UINTN Top, UINTN Width, UINTN Height) {
  UiDrawContentHeader(Left, Top, Width, L"status / diagnostics");
  UINTN row = Top + 3;
  CHAR16 vendor[16];
  AsciiToLowerUtf16(Ctx->Handoff->Vendor, vendor, 16);

  UiPrintAt(Left + 2, row++, L"platform");
  UiPrintAt(Left + 4, row++, L"cpu vendor: %s", vendor);
  UiPrintAt(Left + 4, row++, L"virtualization: %s", (Ctx->Handoff->Features.Vmx || Ctx->Handoff->Features.Svm) ? L"supported" : L"unavailable");
  UiPrintAt(Left + 4, row++, L"ept support: %s", Ctx->Handoff->Features.Ept ? L"yes" : L"no");
  UiPrintAt(Left + 4, row++, L"npt support: %s", Ctx->Handoff->Features.Npt ? L"yes" : L"no");

  row++;
  UiPrintAt(Left + 2, row++, L"current config");
  UiPrintAt(Left + 4, row++, L"hypervisor: %s", Ctx->Config->EnableHv ? L"enabled" : L"disabled");
  UiPrintAt(Left + 4, row++, L"vmx: %s | svm: %s", Ctx->Config->EnableVmx ? L"on" : L"off", Ctx->Config->EnableSvm ? L"on" : L"off");
  UiPrintAt(Left + 4, row++, L"ept: %s | npt: %s | vpid: %s",
            Ctx->Config->EnableEpt ? L"on" : L"off",
            Ctx->Config->EnableNpt ? L"on" : L"off",
            Ctx->Config->EnableVpid ? L"on" : L"off");
  UiPrintAt(Left + 4, row++, L"hooks: %s | mem integrity: %s", Ctx->Config->EnableHooks ? L"on" : L"off",
            Ctx->Config->EnableMemIntegrity ? L"on" : L"off");

  row++;
  UiPrintAt(Left + 2, row++, L"last boot status");
  if (Ctx->LastBootValid) {
    UiPrintAt(Left + 4, row++, L"hv enabled: %s", Ctx->LastBoot.HvEnabled ? L"yes" : L"no");
    UiPrintAt(Left + 4, row++, L"hv initialized: %s", Ctx->LastBoot.HvInitialized ? L"yes" : L"no");
    UiPrintAt(Left + 4, row++, L"virt supported: %s", Ctx->LastBoot.VirtualizationSupported ? L"yes" : L"no");
    UiPrintAt(Left + 4, row++, L"ept supported: %s", Ctx->LastBoot.EptSupported ? L"yes" : L"no");
    UiPrintAt(Left + 4, row++, L"config loaded: %s", Ctx->LastBoot.ConfigLoaded ? L"yes" : L"no");
    UiPrintAt(Left + 4, row++, L"hv version: %u", (UINT32)Ctx->LastBoot.HvVersion);
    UiPrintAt(Left + 4, row++, L"boot timestamp: %lu", (UINT64)Ctx->LastBoot.BootTimestamp);
  } else {
    UiPrintAt(Left + 4, row++, L"no previous boot status found");
  }

  if (Height > 20) {
    UINTN log_top = Top + (Height > 10 ? (Height - 10) : (Top + row));
    if (log_top < row + 1) log_top = row + 1;
    UiDrawLogPanel(Left, log_top, Width, Height - (log_top - Top), L"boot log (latest)");
  }
}

static VOID UiRenderLogs(UI_CONTEXT* Ctx, UINTN Left, UINTN Top, UINTN Width, UINTN Height) {
  UiDrawContentHeader(Left, Top, Width, L"boot log");
  UiDrawLogPanel(Left, Top + 3, Width, Height - 3, NULL);
  (void)Ctx;
}

static VOID UiRenderPage(UI_CONTEXT* Ctx, UI_MENU_PAGE* Page, UINTN Left, UINTN Top, UINTN Width, UINTN Height) {
  UiDrawContentHeader(Left, Top, Width, Page->Title);
  UiDrawContentBox(Left, Top + 3, Width, Height - 3, NULL);

  UINTN row = Top + 5;
  for (UINTN i = 0; i < Page->Count && i < Height - 6; ++i) {
    UiDrawOption(Ctx, &Page->Items[i], Left + 2, row++, i == Ctx->Selection[Ctx->PageStack[Ctx->PageDepth - 1]]);
  }
}

static VOID UiBuildPages(UI_CONTEXT* Ctx, UI_MENU_PAGE* Pages) {
  static UI_MENU_ITEM main_items[12];
  static UI_MENU_ITEM feature_items[16];
  static UI_MENU_ITEM hv_items[16];

  UINTN m = 0;
  main_items[m++] = (UI_MENU_ITEM){ L"feature toggles", UI_ITEM_SUBPAGE, NULL, 0, 0, 0, NULL, 0, UI_PAGE_FEATURES, NULL };
  main_items[m++] = (UI_MENU_ITEM){ L"hypervisor configuration", UI_ITEM_SUBPAGE, NULL, 0, 0, 0, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  main_items[m++] = (UI_MENU_ITEM){ L"status / diagnostics", UI_ITEM_SUBPAGE, NULL, 0, 0, 0, NULL, 0, UI_PAGE_STATUS, NULL };
  main_items[m++] = (UI_MENU_ITEM){ L"boot log", UI_ITEM_SUBPAGE, NULL, 0, 0, 0, NULL, 0, UI_PAGE_LOGS, NULL };
  main_items[m++] = (UI_MENU_ITEM){ L"start windows", UI_ITEM_ACTION, NULL, 0, 0, 0, NULL, 0, UI_PAGE_MAIN, NULL };
  main_items[m++] = (UI_MENU_ITEM){ L"start windows without hypervisor", UI_ITEM_ACTION, NULL, 0, 0, 0, NULL, 0, UI_PAGE_MAIN, NULL };
  main_items[m++] = (UI_MENU_ITEM){ L"reload config", UI_ITEM_ACTION, NULL, 0, 0, 0, NULL, 0, UI_PAGE_MAIN, NULL };
  main_items[m++] = (UI_MENU_ITEM){ L"reset hv state", UI_ITEM_ACTION, NULL, 0, 0, 0, NULL, 0, UI_PAGE_MAIN, NULL };
  main_items[m++] = (UI_MENU_ITEM){ L"shutdown system", UI_ITEM_ACTION, NULL, 0, 0, 0, NULL, 0, UI_PAGE_MAIN, NULL };

  UINTN f = 0;
  feature_items[f++] = (UI_MENU_ITEM){ L"cpuid interception", UI_ITEM_TOGGLE, &Ctx->UiConfig.EnableCpuidIntercept, 0, 1, 1, NULL, 0, UI_PAGE_FEATURES, NULL };
  feature_items[f++] = (UI_MENU_ITEM){ L"msr interception", UI_ITEM_TOGGLE, &Ctx->UiConfig.EnableMsrIntercept, 0, 1, 1, NULL, 0, UI_PAGE_FEATURES, NULL };
  feature_items[f++] = (UI_MENU_ITEM){ L"control register monitor", UI_ITEM_TOGGLE, &Ctx->UiConfig.EnableCrMonitor, 0, 1, 1, NULL, 0, UI_PAGE_FEATURES, NULL };
  feature_items[f++] = (UI_MENU_ITEM){ L"debug register monitor", UI_ITEM_TOGGLE, &Ctx->UiConfig.EnableDrMonitor, 0, 1, 1, NULL, 0, UI_PAGE_FEATURES, NULL };
  feature_items[f++] = (UI_MENU_ITEM){ L"io port interception", UI_ITEM_TOGGLE, &Ctx->UiConfig.EnableIoMonitor, 0, 1, 1, NULL, 0, UI_PAGE_FEATURES, NULL };
  feature_items[f++] = (UI_MENU_ITEM){ L"memory monitoring", UI_ITEM_TOGGLE, &Ctx->UiConfig.EnableMemMonitor, 0, 1, 1, NULL, 0, UI_PAGE_FEATURES, NULL };
  feature_items[f++] = (UI_MENU_ITEM){ L"ept hook engine", UI_ITEM_TOGGLE, &Ctx->UiConfig.EnableEptHooks, 0, 1, 1, NULL, 0, UI_PAGE_FEATURES, NULL };
  feature_items[f++] = (UI_MENU_ITEM){ L"page access tracing", UI_ITEM_TOGGLE, &Ctx->UiConfig.EnablePageTrace, 0, 1, 1, NULL, 0, UI_PAGE_FEATURES, NULL };
  feature_items[f++] = (UI_MENU_ITEM){ L"execute-only pages", UI_ITEM_TOGGLE, &Ctx->UiConfig.EnableExecOnly, 0, 1, 1, NULL, 0, UI_PAGE_FEATURES, NULL };
  feature_items[f++] = (UI_MENU_ITEM){ L"integrity checks", UI_ITEM_TOGGLE, &Ctx->UiConfig.EnableIntegrityChecks, 0, 1, 1, NULL, 0, UI_PAGE_FEATURES, NULL };

  UINTN h = 0;
  hv_items[h++] = (UI_MENU_ITEM){ L"hypervisor enabled", UI_ITEM_TOGGLE, &Ctx->Config->EnableHv, 0, 1, 1, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"hv mode", UI_ITEM_ENUM, &Ctx->Config->HvMode, 0, 2, 1, gModeLabels, 3, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"enable vmx", UI_ITEM_TOGGLE, &Ctx->Config->EnableVmx, 0, 1, 1, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"enable svm", UI_ITEM_TOGGLE, &Ctx->Config->EnableSvm, 0, 1, 1, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"enable ept", UI_ITEM_TOGGLE, &Ctx->Config->EnableEpt, 0, 1, 1, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"enable npt", UI_ITEM_TOGGLE, &Ctx->Config->EnableNpt, 0, 1, 1, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"enable vpid", UI_ITEM_TOGGLE, &Ctx->Config->EnableVpid, 0, 1, 1, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"stealth mode", UI_ITEM_TOGGLE, &Ctx->Config->EnableStealth, 0, 1, 1, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"self protect", UI_ITEM_TOGGLE, &Ctx->Config->EnableSelfProtect, 0, 1, 1, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"anti tamper", UI_ITEM_TOGGLE, &Ctx->Config->EnableAntiTamper, 0, 1, 1, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"verbose", UI_ITEM_TOGGLE, &Ctx->Config->Verbose, 0, 1, 1, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"serial debug", UI_ITEM_TOGGLE, &Ctx->Config->SerialDebug, 0, 1, 1, NULL, 0, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"log level", UI_ITEM_ENUM, &Ctx->Config->LogLevel, 0, 3, 1, gLogLabels, 4, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"preset", UI_ITEM_ENUM, &Ctx->UiConfig.PresetId, 0, 2, 1, gPresetLabels, 3, UI_PAGE_HV_CONFIG, NULL };
  hv_items[h++] = (UI_MENU_ITEM){ L"profile", UI_ITEM_ENUM, &Ctx->UiConfig.ProfileId, 0, 2, 1, gProfileLabels, 3, UI_PAGE_HV_CONFIG, NULL };

  Pages[UI_PAGE_MAIN] = (UI_MENU_PAGE){ L"main menu", main_items, m };
  Pages[UI_PAGE_FEATURES] = (UI_MENU_PAGE){ L"feature toggles", feature_items, f };
  Pages[UI_PAGE_HV_CONFIG] = (UI_MENU_PAGE){ L"hypervisor configuration", hv_items, h };
  Pages[UI_PAGE_STATUS] = (UI_MENU_PAGE){ L"status / diagnostics", NULL, 0 };
  Pages[UI_PAGE_LOGS] = (UI_MENU_PAGE){ L"boot log", NULL, 0 };
}

static EFI_STATUS UiHandleAction(UI_CONTEXT* Ctx, UINTN Index) {
  if (!Ctx) return EFI_INVALID_PARAMETER;
  switch (Index) {
    case 4:
      Ctx->Action = UI_ACTION_BOOT;
      Ctx->SaveRequested = TRUE;
      UiApplyPreset(Ctx);
      BootDbgAdd(L"[ui] action: start os");
      return EFI_SUCCESS;
    case 5:
      Ctx->Config->EnableHv = 0;
      Ctx->Action = UI_ACTION_BOOT_NO_HV;
      Ctx->SaveRequested = TRUE;
      BootDbgAdd(L"[ui] action: start os (no hv)");
      return EFI_SUCCESS;
    case 6:
      LoadConfig(Ctx->Config);
      LoadConfigFile(Ctx->ImageHandle, Ctx->Config);
      UiConfigLoad(&Ctx->UiConfig);
      BootDbgAdd(L"[ui] action: reload config");
      return EFI_SUCCESS;
    case 7:
      BootDbgAdd(L"[ui] action: reset hv state");
      return EFI_SUCCESS;
    case 8:
      BootDbgAdd(L"[ui] action: shutdown");
      gRT->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
      return EFI_SUCCESS;
    default:
      return EFI_SUCCESS;
  }
}

static EFI_STATUS UiHandleMenuInput(UI_CONTEXT* Ctx, UI_MENU_PAGE* Page, EFI_INPUT_KEY Key) {
  if (!Ctx || !Page || Page->Count == 0) return EFI_SUCCESS;
  UINTN page = Ctx->PageStack[Ctx->PageDepth - 1];
  UINTN sel = Ctx->Selection[page];

  if (Key.ScanCode == SCAN_UP) {
    sel = (sel == 0) ? (Page->Count - 1) : (sel - 1);
    Ctx->Selection[page] = sel;
    BootDbgAdd(L"[ui] nav up: %u", (UINT32)sel);
    return EFI_SUCCESS;
  }
  if (Key.ScanCode == SCAN_DOWN) {
    sel = (sel + 1) % Page->Count;
    Ctx->Selection[page] = sel;
    BootDbgAdd(L"[ui] nav down: %u", (UINT32)sel);
    return EFI_SUCCESS;
  }

  UI_MENU_ITEM* Item = &Page->Items[sel];
  if (Key.ScanCode == SCAN_LEFT || Key.ScanCode == SCAN_RIGHT || Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
    switch (Item->Kind) {
      case UI_ITEM_TOGGLE:
        if (Item->Value) {
          *Item->Value = (*Item->Value == 0) ? 1 : 0;
          BootDbgAdd(L"[ui] toggle: %s=%u", Item->Label, (UINT32)*Item->Value);
        }
        break;
      case UI_ITEM_ENUM:
        if (Item->Value && Item->EnumCount > 0) {
          UINT8 v = *Item->Value;
          if (Key.ScanCode == SCAN_LEFT) v = (v == 0) ? (UINT8)(Item->EnumCount - 1) : (UINT8)(v - 1);
          else v = (UINT8)((v + 1) % Item->EnumCount);
          *Item->Value = v;
          BootDbgAdd(L"[ui] enum: %s=%u", Item->Label, (UINT32)*Item->Value);
        }
        break;
      case UI_ITEM_NUMBER:
        if (Item->Value) {
          UINT8 v = *Item->Value;
          UINT8 step = (Item->Step == 0) ? 1 : Item->Step;
          if (Key.ScanCode == SCAN_LEFT) v = (v <= Item->Min) ? Item->Min : (UINT8)(v - step);
          else v = (v >= Item->Max) ? Item->Max : (UINT8)(v + step);
          *Item->Value = v;
          BootDbgAdd(L"[ui] number: %s=%u", Item->Label, (UINT32)*Item->Value);
        }
        break;
      case UI_ITEM_SUBPAGE:
        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
          if (Ctx->PageDepth < UI_MAX_PAGE_STACK) {
            Ctx->PageStack[Ctx->PageDepth++] = Item->SubPage;
            BootDbgAdd(L"[ui] open page: %u", (UINT32)Item->SubPage);
          }
        }
        break;
      case UI_ITEM_ACTION:
        if (Key.UnicodeChar == CHAR_CARRIAGE_RETURN) {
          if (Ctx->PageStack[Ctx->PageDepth - 1] == UI_PAGE_MAIN) {
            UiHandleAction(Ctx, sel);
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
      UINTN contentLeft = 2;
      UINTN contentWidth = (cols > 4) ? (cols - 4) : cols;

      UI_MENU_PAGE pages[UI_PAGE_COUNT];
      UiBuildPages(Ctx, pages);
      UI_PAGE current = Ctx->PageStack[Ctx->PageDepth - 1];
      if (current == UI_PAGE_STATUS) {
        UiRenderStatus(Ctx, contentLeft, bodyTop, contentWidth, bodyHeight);
      } else if (current == UI_PAGE_LOGS) {
        UiRenderLogs(Ctx, contentLeft, bodyTop, contentWidth, bodyHeight);
      } else {
        UiRenderPage(Ctx, &pages[current], contentLeft, bodyTop, contentWidth, bodyHeight);
      }

      UiDrawFooter(Ctx);
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
      if (Ctx->PageDepth > 1) {
        Ctx->PageDepth--;
        BootDbgAdd(L"[ui] page back");
        Redraw = TRUE;
        continue;
      }
      gBS->CloseEvent(Events[1]);
      BootDbgAdd(L"[ui] exit");
      return EFI_ABORTED;
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

    UI_MENU_PAGE pages[UI_PAGE_COUNT];
    UiBuildPages(Ctx, pages);
    UI_PAGE current = Ctx->PageStack[Ctx->PageDepth - 1];
    if (current != UI_PAGE_STATUS && current != UI_PAGE_LOGS) {
      UiHandleMenuInput(Ctx, &pages[current], Key);
    }

    if (Ctx->SaveRequested) {
      UiApplyToBootConfig(Ctx);
      UiConfigSave(&Ctx->UiConfig);
      gBS->CloseEvent(Events[1]);
      BootDbgAdd(L"[ui] boot selected");
      return EFI_SUCCESS;
    }

    Redraw = TRUE;
  }
}

EFI_STATUS RunConfigMenu(EFI_HANDLE ImageHandle, THV_HANDOFF* Handoff) {
  if (!Handoff) return EFI_INVALID_PARAMETER;

  UI_CONTEXT Ctx;
  ZeroMem(&Ctx, sizeof(Ctx));
  Ctx.ImageHandle = ImageHandle;
  Ctx.Handoff = Handoff;
  Ctx.Config = &Handoff->Config;
  Ctx.PageDepth = 1;
  Ctx.PageStack[0] = UI_PAGE_MAIN;
  Ctx.Tick = 0;
  Ctx.SaveRequested = FALSE;
  Ctx.Action = UI_ACTION_NONE;

  UiConfigLoad(&Ctx.UiConfig);
  UiLoadLastBoot(&Ctx);
  return RunModernUi(&Ctx);
}
