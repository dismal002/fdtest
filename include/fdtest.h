#pragma once

// ============================================================================
// fdtest.h - Free Drive Test (FDTest)
// Open source drive read/write reliability testing tool (Cross-Platform)
// ============================================================================

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>

#ifdef _WIN32
  #include <windows.h>
#else
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <sys/statvfs.h>
  #include <fcntl.h>
  #include <signal.h>
  #include <dirent.h>
  #include <cstdint>

  #ifndef WINAPI
  #define WINAPI
  #endif
  #ifndef __cdecl
  #define __cdecl
  #endif

  // POSIX Win32 API Emulation Types
  typedef void* HANDLE;
  typedef uint32_t DWORD;
  typedef uint16_t WORD;
  typedef uint32_t UINT;
  typedef int BOOL;
  typedef void* LPVOID;
  typedef const char* LPCSTR;
  typedef char CHAR;
  typedef int64_t __int64;
  typedef uint8_t BYTE;
  typedef uint8_t BOOLEAN;

  #ifndef TRUE
  #define TRUE 1
  #endif
  #ifndef FALSE
  #define FALSE 0
  #endif
  #ifndef INVALID_HANDLE_VALUE
  #define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
  #endif
  #ifndef INVALID_FILE_ATTRIBUTES
  #define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
  #endif
  #ifndef GENERIC_READ
  #define GENERIC_READ 0x80000000
  #endif
  #ifndef GENERIC_WRITE
  #define GENERIC_WRITE 0x40000000
  #endif
  #ifndef FILE_SHARE_READ
  #define FILE_SHARE_READ 1
  #endif
  #ifndef FILE_SHARE_WRITE
  #define FILE_SHARE_WRITE 2
  #endif
  #ifndef OPEN_EXISTING
  #define OPEN_EXISTING 3
  #endif
  #ifndef OPEN_ALWAYS
  #define OPEN_ALWAYS 4
  #endif
  #ifndef FILE_FLAG_SEQUENTIAL_SCAN
  #define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
  #endif
  #ifndef INFINITE
  #define INFINITE 0xFFFFFFFF
  #endif
  #ifndef CTRL_C_EVENT
  #define CTRL_C_EVENT 0
  #endif
  #ifndef CTRL_BREAK_EVENT
  #define CTRL_BREAK_EVENT 1
  #endif

  typedef BOOL (WINAPI *PHANDLER_ROUTINE)(DWORD CtrlType);

  // POSIX Event Emulation
  struct PosixEvent {
      std::mutex mtx;
      std::condition_variable cv;
      bool state = false;
      bool manual = false;

      PosixEvent(bool manualReset = false, bool initialState = false)
          : state(initialState), manual(manualReset) {}

      void set() {
          std::lock_guard<std::mutex> lock(mtx);
          state = true;
          if (manual) cv.notify_all(); else cv.notify_one();
      }
      void reset() {
          std::lock_guard<std::mutex> lock(mtx);
          state = false;
      }
      void wait() {
          std::unique_lock<std::mutex> lock(mtx);
          cv.wait(lock, [this]{ return state; });
          if (!manual) state = false;
      }
  };

  HANDLE CreateEventA(void* sec, BOOL bManualReset, BOOL bInitialState, const char* name);
  BOOL SetEvent(HANDLE hEvent);
  BOOL ResetEvent(HANDLE hEvent);
  DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
  BOOL CloseHandle(HANDLE hObject);
  DWORD GetFileAttributesA(LPCSTR lpFileName);
  HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                     void* lpSecurityAttributes, DWORD dwCreationDisposition,
                     DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
  BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                DWORD* lpNumberOfBytesRead, void* lpOverlapped);
  BOOL WriteFile(HANDLE hFile, const void* lpBuffer, DWORD nNumberOfBytesToWrite,
                 DWORD* lpNumberOfBytesWritten, void* lpOverlapped);
  DWORD GetFileSize(HANDLE hFile, DWORD* lpFileSizeHigh);
  BOOL GetDiskFreeSpaceA(LPCSTR lpRootPathName, DWORD* lpSectorsPerCluster,
                         DWORD* lpBytesPerSector, DWORD* lpNumberOfFreeClusters,
                         DWORD* lpTotalNumberOfClusters);
  typedef int32_t LONG;

  #ifndef FILE_BEGIN
  #define FILE_BEGIN 0
  #endif
  #ifndef FILE_CURRENT
  #define FILE_CURRENT 1
  #endif
  #ifndef FILE_END
  #define FILE_END 2
  #endif

  DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG* lpDistanceToMoveHigh, DWORD dwMoveMethod);
  DWORD GetTickCount();
  BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE HandlerRoutine, BOOL Add);
  HANDLE CreateThread(void* lpThreadAttributes, size_t dwStackSize,
                      DWORD (WINAPI *lpStartAddress)(LPVOID lpParameter),
                      LPVOID lpParameter, DWORD dwCreationFlags, DWORD* lpThreadId);
#endif

// ============================================================================
// Constants
// ============================================================================

// Buffer sizes
static constexpr DWORD BUFFER_SIZE    = 0x100000;   // 1 MB per I/O block
static constexpr DWORD PRNG_COUNT     = 0x40000;    // 256K DWORDs per buffer (= 1 MB)

// IOCTL for STORAGE_QUERY_PROPERTY
static constexpr DWORD IOCTL_STORAGE_QUERY_PROPERTY_CODE = 0x2D1400;

// Console color attributes
static constexpr WORD COLOR_DEFAULT       = 0x07;  // Light gray on black
static constexpr WORD COLOR_RED           = 0x0C;  // Red (bright)
static constexpr WORD COLOR_YELLOW        = 0x0E;  // Yellow (bright)
static constexpr WORD COLOR_WHITE         = 0x0F;  // White (bright)
static constexpr WORD COLOR_GREEN_ON_BLK  = 0x0A;  // Green (bright)
static constexpr WORD COLOR_GREEN_BG      = 0xA0;  // Green background
static constexpr WORD COLOR_RED_BG        = 0xC0;  // Red background
static constexpr WORD COLOR_YELLOW_BG     = 0xE0;  // Yellow background

// Conversion constants
static constexpr double BYTES_4GB    = 4.294967296e9;   // 2^32 as double
static constexpr double BYTES_PER_KB = 1024.0;
static constexpr double BYTES_PER_MB = 1048576.0;
static constexpr double KB_TO_BYTES  = 0.0009765625;    // 1/1024
static constexpr double MB_TO_BYTES  = 0.001;           // ~1/1000 (metric)
static constexpr double BYTES_PER_GB = 1.073741824e9;
static constexpr double BYTES_PER_2K = 2048.0;
static constexpr double MB_PER_BLOCK = 9.5367431640625e-7; // 1/1048576
static constexpr float  MS_PER_SEC   = 1000.0f;
static constexpr float  FLOAT_4GB    = 4.2949673e9f;

// PRNG initial seed values
static constexpr int PRNG_SEED_A = 362436069;
static constexpr int PRNG_SEED_B = 521288629;
static constexpr int PRNG_SEED_C = 88675123;
static constexpr int PRNG_SEED_D = -593279510;

// Timing
static constexpr int SECONDS_PER_HOUR   = 3600;  // 0xE10
static constexpr int SECONDS_PER_MINUTE = 60;    // 0x3C

// Blocks per file: 0x800 = 2048 blocks of 1 MB each = 2 GB per file
static constexpr int BLOCKS_PER_FILE = 0x800;  // 2048

// ============================================================================
// Resource string IDs
// ============================================================================

static constexpr UINT IDS_BANNER             = 101;  // 0x65
static constexpr UINT IDS_USAGE              = 102;  // 0x66
static constexpr UINT IDS_PROMPT_DRIVE       = 103;  // 0x67
static constexpr UINT IDS_PROMPT_VERIFY      = 104;  // 0x68
static constexpr UINT IDS_PROMPT_PASSES      = 105;  // 0x69
static constexpr UINT IDS_PASS_HEADER        = 106;  // 0x6A
static constexpr UINT IDS_DRIVE_INFO_HDR     = 107;  // 0x6B
static constexpr UINT IDS_DRIVE_OPEN_ERR     = 108;  // 0x6C
static constexpr UINT IDS_VENDOR_LABEL       = 109;  // 0x6D
static constexpr UINT IDS_REVISION_LABEL     = 110;  // 0x6E
static constexpr UINT IDS_SIZE_LABEL         = 111;  // 0x6F
static constexpr UINT IDS_WRITABLE           = 112;  // 0x70
static constexpr UINT IDS_READONLY           = 113;  // 0x71
static constexpr UINT IDS_FREE_SPACE_LABEL   = 114;  // 0x72
static constexpr UINT IDS_FREE_SPACE_LABEL2  = 115;  // 0x73
static constexpr UINT IDS_NO_FILES_SKIP      = 116;  // 0x74
static constexpr UINT IDS_FILES_EXIST        = 117;  // 0x75
static constexpr UINT IDS_CREATE_FILE_ERR    = 118;  // 0x76
static constexpr UINT IDS_WRITE_FAIL_ERR     = 119;  // 0x77
static constexpr UINT IDS_WRITING_HDR        = 120;  // 0x78
static constexpr UINT IDS_READING_HDR        = 121;  // 0x79
static constexpr UINT IDS_STATUS_FMT         = 122;  // 0x7A
static constexpr UINT IDS_MISMATCH           = 123;  // 0x7B
static constexpr UINT IDS_WRITE_COMPLETE     = 124;  // 0x7C
static constexpr UINT IDS_READ_COMPLETE      = 125;  // 0x7D
static constexpr UINT IDS_VERIFY_SKIPPED     = 126;  // 0x7E
static constexpr UINT IDS_OPEN_FILE_ERR      = 127;  // 0x7F
static constexpr UINT IDS_PLACEHOLDER_128    = 128;  // 0x80
static constexpr UINT IDS_ERROR_COUNT        = 129;  // 0x81
static constexpr UINT IDS_ALL_OK             = 130;  // 0x82
static constexpr UINT IDS_DELETE_ERR         = 131;  // 0x83
static constexpr UINT IDS_PRESS_EXIT         = 132;  // 0x84
static constexpr UINT IDS_PLACEHOLDER_133    = 133;  // 0x85
static constexpr UINT IDS_DRIVE_LIST_HDR     = 134;  // 0x86
static constexpr UINT IDS_CLEANUP            = 135;  // 0x87
static constexpr UINT IDS_PROMPT_RANDOMIZE   = 136;  // 0x88
static constexpr UINT IDS_PASS_SUMMARY       = 137;  // 0x89
static constexpr UINT IDS_PASS_PROGRESS      = 138;  // 0x8A
static constexpr UINT IDS_TEST_COMPLETE      = 139;  // 0x8B
static constexpr UINT IDS_TEST_INCOMPLETE    = 140;  // 0x8C
static constexpr UINT IDS_READ_ERROR         = 141;  // 0x8D
static constexpr UINT IDS_ERR_WITH_READS     = 142;  // 0x8E
static constexpr UINT IDS_INTERRUPTED_FMT    = 143;  // 0x8F
static constexpr UINT IDS_INTERRUPTED        = 144;  // 0x90
static constexpr UINT IDS_NOT_ENOUGH_SPACE   = 146;  // 0x92

// ============================================================================
// Global state (defined in main.cpp)
// ============================================================================

// Synchronization handles
extern HANDLE g_hReadEvent;       // signals reader thread
extern HANDLE g_hWriteEvent;      // signals writer thread
extern HANDLE g_hDoneEvent;       // signals I/O completion

// Double-buffer toggle: 1 = use buffer1, 0 = use buffer2
extern int g_activeBuffer;

// Ctrl+C cancellation flag
extern LPCSTR g_cancelFlag;

// Console output handle
extern HANDLE g_hConsoleOutput;

// Global format buffer
extern CHAR g_stringBuffer[256];

// ============================================================================
// Cross-Platform LoadResString declaration
// ============================================================================
void LoadResString(UINT id, CHAR* buf, int bufSize);

// Utility: Flush stdin after scanf
inline int FlushStdin() {
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
    return c;
}
