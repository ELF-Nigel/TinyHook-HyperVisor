// BootLog.h - bootloader log buffer (ui + diagnostics)
#pragma once
#include <Uefi.h>

#define BOOTLOG_MAX_LINES 64
#define BOOTLOG_LINE_CHARS 96
#define BOOTDBG_MAX_LINES 5

typedef struct BOOT_LOG_LINE {
  CHAR16 Text[BOOTLOG_LINE_CHARS];
} BOOT_LOG_LINE;

typedef struct BOOT_DBG_LINE {
  CHAR16 Text[BOOTLOG_LINE_CHARS];
} BOOT_DBG_LINE;

VOID BootLogInit(VOID);
VOID BootLogClear(VOID);
VOID BootLogAdd(IN CONST CHAR16* Fmt, ...);
UINTN BootLogCount(VOID);
CONST BOOT_LOG_LINE* BootLogGetLines(VOID);

VOID BootDbgInit(VOID);
VOID BootDbgClear(VOID);
VOID BootDbgAdd(IN CONST CHAR16* Fmt, ...);
UINTN BootDbgCount(VOID);
CONST BOOT_DBG_LINE* BootDbgGetLines(VOID);
