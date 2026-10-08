// ============================================================================
// string_table.cpp - String Resource Loader for Cross-Platform Build
// Provides LoadResString implementation for Linux and Windows.
// ============================================================================

#include "fdtest.h"
#include <map>
#include <string>

#ifdef _WIN32
void LoadResString(UINT id, CHAR* buf, int bufSize)
{
    HMODULE hMod = GetModuleHandleA(nullptr);
    if (LoadStringA(hMod, id, buf, bufSize) == 0)
    {
        // Fallback to built-in table if resource not compiled in
        buf[0] = '\0';
    }
}
#else

static const std::map<UINT, const char*> g_embeddedStrings = {
    {101, "+------------------------------------+\n|      Free Drive Test (FDTest)      |\n|         v1.0 - Open Source         |\n+------------------------------------+\n"},
    {102, "\nCommand line usage:\n  %s X: [Y/N]\n\n  X:     Drive path/letter to test (e.g. /mnt/usb or X:)\n  [Y/N]  Verify test data immediately after writing (default: Y)"},
    {103, "Enter drive path to test (e.g. /mnt/usb or X:, Enter to continue, Ctrl-C to exit): "},
    {104, "Verify test data immediately after writing? [Y/N]: "},
    {105, "Enter number of test loops (write + verify per loop): "},
    {106, "\n *** Pass %d of %d in progress ***\n\n"},
    {107, "\nDrive [%s] Information\n===========================================\n"},
    {108, "\nUnable to retrieve drive details. Please check target path.\n"},
    {109, "Model     : "},
    {110, "Revision  : "},
    {111, "Drive Type: "},
    {112, "Fixed Disk"},
    {113, "Removable Disk"},
    {114, "Capacity  : "},
    {115, "Free Space: "},
    {116, "\nTest files already exist. Skipping write phase.\n"},
    {117, "\rTest files already exist. Proceeding directly to verification...\n\n"},
    {118, "\n\nError: Failed to create file %s !\n"},
    {119, "\n\nError: Could not write data to file %s !\n"},
    {120, "\rWriting   %8u MB %8.2f MB/s %8.2f MB/s %8u MB  %02d:%02d:%02d    "},
    {121, "\rVerifying %8u MB %8.2f MB/s %8.2f MB/s %8u MB  %02d:%02d:%02d    "},
    {122, "             Position     Cur.Speed     Avg.Speed   Remaining  Rem.Time\n"},
    {123, "\rData Err  %8u MB\n"},
    {124, "\rWritten   %8u MB        - MB/s %8.2f MB/s        - MB  --:--:--    \n"},
    {125, "\rVerified  %8u MB        - MB/s %8.2f MB/s        - MB  --:--:--    \n\n"},
    {126, "\nTest data generation complete. Verification may be performed later.\n"},
    {127, "\n\nError: Could not open file %s for reading!\n"},
    {128, "\n\nError: Could not read data from file %s!\n"},
    {129, "Data verification FAILED: %u MB corrupted!\n\n"},
    {130, "Data verification PASSED: All test data verified successfully!\n\n"},
    {131, "Error: Could not delete test file %s!\n\n"},
    {132, "\nOperation finished. Press Enter to exit..."},
    {133, "High-performance I/O test mode active.\n\n"},
    {134, "Available drive list (removable drives marked with \"*\"):\n\n"},
    {135, "Removing temporary test data files...\n\n"},
    {136, "Continue to next loop if verification errors are found? [Y/N]: "},
    {137, "Test complete! Total %u loop(s) tested, all passed with 0 errors!\n\n"},
    {138, "Test complete! Total %u loop(s) tested, %u passed. Drive instability detected!\n\n"},
    {139, "Test complete! Total %u loop(s) tested, all failed. Media error detected!\n\n"},
    {140, "Test interrupted! Tested %u loop(s), error occurred on loop %u.\n\n"},
    {141, "\rRead Err %8u MB\n"},
    {142, "Data verification FAILED: %u MB corrupted (%u MB unreadable)!\n\n"},
    {143, "\rUser Break %8u MB        - MB/s %8.2f MB/s        - MB  --:--:--    \n"},
    {144, "Verification aborted by user. No errors detected up to abort point.\n\n"},
    {146, "\rDisk Full\n\nInsufficient free space on drive. Free up space and try again.\n"}
};

void LoadResString(UINT id, CHAR* buf, int bufSize)
{
    auto it = g_embeddedStrings.find(id);
    if (it != g_embeddedStrings.end()) {
        snprintf(buf, bufSize, "%s", it->second);
    } else {
        buf[0] = '\0';
    }
}

#endif
