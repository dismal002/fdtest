// ============================================================================
// console.cpp - Console color and Ctrl+C handler utilities (Cross-Platform)
// ============================================================================

#include "console.h"

BOOL SetConsoleColor(WORD wAttributes)
{
#ifdef _WIN32
    HANDLE hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hStdOut == INVALID_HANDLE_VALUE)
        return FALSE;
    return SetConsoleTextAttribute(hStdOut, wAttributes);
#else
    switch (wAttributes) {
        case COLOR_RED:
            printf("\033[1;31m"); break;
        case COLOR_YELLOW:
            printf("\033[1;33m"); break;
        case COLOR_WHITE:
            printf("\033[1;37m"); break;
        case COLOR_GREEN_ON_BLK:
            printf("\033[1;32m"); break;
        case COLOR_GREEN_BG:
            printf("\033[42m\033[30m"); break;
        case COLOR_RED_BG:
            printf("\033[41m\033[37m"); break;
        case COLOR_YELLOW_BG:
            printf("\033[43m\033[30m"); break;
        default:
            printf("\033[0m"); break;
    }
    fflush(stdout);
    return TRUE;
#endif
}

BOOL WINAPI CtrlHandler(DWORD ctrlType)
{
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT)
    {
        g_cancelFlag = (LPCSTR)1;
        return TRUE;
    }
    return TRUE;
}
