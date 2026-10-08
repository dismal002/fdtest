#pragma once
// ============================================================================
// console.h - Console color and Ctrl+C handler utilities
// ============================================================================

#include "fdtest.h"

// Set console text color attributes.
// Returns TRUE on success, FALSE on failure.
// Corresponds to sub_409DD0.
BOOL SetConsoleColor(WORD wAttributes);

// Ctrl+C / Ctrl+Break handler.
// Sets g_cancelFlag when the user presses Ctrl+C or Ctrl+Break.
// Corresponds to HandlerRoutine.
BOOL WINAPI CtrlHandler(DWORD ctrlType);
