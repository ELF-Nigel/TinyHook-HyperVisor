// BootLog.c - bootloader log buffer (ui + diagnostics)
#include "BootLog.h"
#include <Library/PrintLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/BaseLib.h>

static BOOT_LOG_LINE g_lines[BOOTLOG_MAX_LINES];
static UINTN g_count = 0;
static BOOT_DBG_LINE g_dbg_lines[BOOTDBG_MAX_LINES];
static BOOT_DBG_LINE g_dbg_view[BOOTDBG_MAX_LINES];
static UINTN g_dbg_count = 0;
static UINTN g_dbg_head = 0;
static CHAR16 g_dbg_last[BOOTLOG_LINE_CHARS];
static UINTN g_dbg_last_repeat = 0;

VOID BootLogInit(VOID) {
  BootLogClear();
  BootDbgClear();
}

VOID BootLogClear(VOID) {
  ZeroMem(g_lines, sizeof(g_lines));
  g_count = 0;
}

VOID BootDbgInit(VOID) {
  BootDbgClear();
}

VOID BootDbgClear(VOID) {
  ZeroMem(g_dbg_lines, sizeof(g_dbg_lines));
  ZeroMem(g_dbg_view, sizeof(g_dbg_view));
  ZeroMem(g_dbg_last, sizeof(g_dbg_last));
  g_dbg_count = 0;
  g_dbg_head = 0;
  g_dbg_last_repeat = 0;
}

static VOID BootDbgAddText(IN CONST CHAR16* Text) {
  if (!Text || Text[0] == 0) return;

  if (StrCmp(Text, g_dbg_last) == 0 && g_dbg_count > 0) {
    g_dbg_last_repeat++;
    CHAR16 merged[BOOTLOG_LINE_CHARS];
    UnicodeSPrint(merged, sizeof(merged), L"%s (x%u)", Text, (UINT32)g_dbg_last_repeat);
    UINTN last_index = (g_dbg_head + BOOTDBG_MAX_LINES - 1) % BOOTDBG_MAX_LINES;
    ZeroMem(g_dbg_lines[last_index].Text, sizeof(g_dbg_lines[last_index].Text));
    UnicodeSPrint(g_dbg_lines[last_index].Text, sizeof(g_dbg_lines[last_index].Text), L"%s", merged);
    return;
  }

  g_dbg_last_repeat = 1;
  ZeroMem(g_dbg_last, sizeof(g_dbg_last));
  UnicodeSPrint(g_dbg_last, sizeof(g_dbg_last), L"%s", Text);

  ZeroMem(g_dbg_lines[g_dbg_head].Text, sizeof(g_dbg_lines[g_dbg_head].Text));
  UnicodeSPrint(g_dbg_lines[g_dbg_head].Text, sizeof(g_dbg_lines[g_dbg_head].Text), L"%s", Text);
  g_dbg_head = (g_dbg_head + 1) % BOOTDBG_MAX_LINES;
  if (g_dbg_count < BOOTDBG_MAX_LINES) g_dbg_count++;
}

VOID BootLogAdd(IN CONST CHAR16* Fmt, ...) {
  if (g_count >= BOOTLOG_MAX_LINES) return;
  VA_LIST args;
  VA_START(args, Fmt);
  UnicodeVSPrint(g_lines[g_count].Text, sizeof(g_lines[g_count].Text), Fmt, args);
  VA_END(args);
  BootDbgAddText(g_lines[g_count].Text);
  g_count++;
}

UINTN BootLogCount(VOID) {
  return g_count;
}

CONST BOOT_LOG_LINE* BootLogGetLines(VOID) {
  return g_lines;
}

VOID BootDbgAdd(IN CONST CHAR16* Fmt, ...) {
  VA_LIST args;
  CHAR16 buf[BOOTLOG_LINE_CHARS];
  VA_START(args, Fmt);
  UnicodeVSPrint(buf, sizeof(buf), Fmt, args);
  VA_END(args);
  BootDbgAddText(buf);
}

UINTN BootDbgCount(VOID) {
  return g_dbg_count;
}

CONST BOOT_DBG_LINE* BootDbgGetLines(VOID) {
  if (g_dbg_count == 0) return g_dbg_view;
  UINTN start = (g_dbg_head + BOOTDBG_MAX_LINES - g_dbg_count) % BOOTDBG_MAX_LINES;
  for (UINTN i = 0; i < g_dbg_count; i++) {
    UINTN idx = (start + i) % BOOTDBG_MAX_LINES;
    CopyMem(&g_dbg_view[i], &g_dbg_lines[idx], sizeof(BOOT_DBG_LINE));
  }
  return g_dbg_view;
}
