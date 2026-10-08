// ============================================================================
// platform_posix.cpp - POSIX Emulation Layer for Linux & macOS
// Provides Win32 API compatibility functions on POSIX systems.
// ============================================================================

#include "fdtest.h"

#ifndef _WIN32

#include <chrono>
#include <sys/file.h>
#include <thread>

// Abstract Handle Type for POSIX
enum HandleType { HANDLE_FILE, HANDLE_EVENT, HANDLE_THREAD };

struct PosixHandle {
    HandleType type;
    virtual ~PosixHandle() = default;
};

struct PosixFileHandle : public PosixHandle {
    int fd = -1;
    PosixFileHandle() { type = HANDLE_FILE; }
    ~PosixFileHandle() override {
        if (fd != -1) close(fd);
    }
};

struct PosixEventHandle : public PosixHandle, public PosixEvent {
    PosixEventHandle(bool manual, bool initial) : PosixEvent(manual, initial) {
        type = HANDLE_EVENT;
    }
};

struct PosixThreadHandle : public PosixHandle {
    std::thread th;
    PosixThreadHandle() { type = HANDLE_THREAD; }
    ~PosixThreadHandle() override {
        if (th.joinable()) th.detach();
    }
};

static auto g_startTime = std::chrono::steady_clock::now();

DWORD GetTickCount()
{
    auto now = std::chrono::steady_clock::now();
    return static_cast<DWORD>(std::chrono::duration_cast<std::chrono::milliseconds>(now - g_startTime).count());
}

HANDLE CreateEventA(void*, BOOL bManualReset, BOOL bInitialState, const char*)
{
    return new PosixEventHandle(bManualReset != FALSE, bInitialState != FALSE);
}

BOOL SetEvent(HANDLE hEvent)
{
    if (!hEvent) return FALSE;
    PosixEventHandle* ev = static_cast<PosixEventHandle*>(hEvent);
    ev->set();
    return TRUE;
}

BOOL ResetEvent(HANDLE hEvent)
{
    if (!hEvent) return FALSE;
    PosixEventHandle* ev = static_cast<PosixEventHandle*>(hEvent);
    ev->reset();
    return TRUE;
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
    if (!hHandle) return (DWORD)-1;
    PosixEventHandle* ev = static_cast<PosixEventHandle*>(hHandle);
    ev->wait();
    return 0; // WAIT_OBJECT_0
}

BOOL CloseHandle(HANDLE hObject)
{
    if (!hObject) return FALSE;
    PosixHandle* ph = static_cast<PosixHandle*>(hObject);
    delete ph;
    return TRUE;
}

HANDLE CreateThread(void*, size_t, DWORD (WINAPI *lpStartAddress)(LPVOID),
                    LPVOID lpParameter, DWORD, DWORD* lpThreadId)
{
    PosixThreadHandle* pth = new PosixThreadHandle();
    pth->th = std::thread([lpStartAddress, lpParameter]() {
        lpStartAddress(lpParameter);
    });
    if (lpThreadId) *lpThreadId = 1;
    return static_cast<HANDLE>(pth);
}

DWORD GetFileAttributesA(LPCSTR lpFileName)
{
    struct stat st;
    if (stat(lpFileName, &st) == 0)
        return 0; // Valid file
    return INVALID_FILE_ATTRIBUTES;
}

HANDLE CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode,
                     void*, DWORD dwCreationDisposition,
                     DWORD dwFlagsAndAttributes, HANDLE)
{
    int flags = 0;
    if ((dwDesiredAccess & GENERIC_READ) && (dwDesiredAccess & GENERIC_WRITE))
        flags |= O_RDWR;
    else if (dwDesiredAccess & GENERIC_WRITE)
        flags |= O_WRONLY;
    else
        flags |= O_RDONLY;

    if (dwCreationDisposition == OPEN_ALWAYS || dwCreationDisposition == 4)
        flags |= O_CREAT;
    else if (dwCreationDisposition == 2 || dwCreationDisposition == 1)
        flags |= O_CREAT | O_TRUNC;

    int fd = open(lpFileName, flags, 0666);
    if (fd == -1)
        return INVALID_HANDLE_VALUE;

    PosixFileHandle* pfh = new PosixFileHandle();
    pfh->fd = fd;
    return static_cast<HANDLE>(pfh);
}

BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead,
                DWORD* lpNumberOfBytesRead, void*)
{
    if (!hFile || hFile == INVALID_HANDLE_VALUE) return FALSE;
    PosixFileHandle* pfh = static_cast<PosixFileHandle*>(hFile);
    ssize_t res = read(pfh->fd, lpBuffer, nNumberOfBytesToRead);
    if (res < 0) {
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = 0;
        return FALSE;
    }
    if (lpNumberOfBytesRead) *lpNumberOfBytesRead = static_cast<DWORD>(res);
    return TRUE;
}

BOOL WriteFile(HANDLE hFile, const void* lpBuffer, DWORD nNumberOfBytesToWrite,
                 DWORD* lpNumberOfBytesWritten, void*)
{
    if (!hFile || hFile == INVALID_HANDLE_VALUE) return FALSE;
    PosixFileHandle* pfh = static_cast<PosixFileHandle*>(hFile);
    ssize_t res = write(pfh->fd, lpBuffer, nNumberOfBytesToWrite);
    if (res < 0) {
        if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = 0;
        return FALSE;
    }
    if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = static_cast<DWORD>(res);
    return TRUE;
}

DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LONG*, DWORD dwMoveMethod)
{
    if (!hFile || hFile == INVALID_HANDLE_VALUE) return (DWORD)-1;
    PosixFileHandle* pfh = static_cast<PosixFileHandle*>(hFile);
    int whence = SEEK_SET;
    if (dwMoveMethod == FILE_CURRENT || dwMoveMethod == 1) whence = SEEK_CUR;
    else if (dwMoveMethod == FILE_END || dwMoveMethod == 2) whence = SEEK_END;
    off_t res = lseek(pfh->fd, lDistanceToMove, whence);
    return (DWORD)res;
}

DWORD GetFileSize(HANDLE hFile, DWORD* lpFileSizeHigh)
{
    if (!hFile || hFile == INVALID_HANDLE_VALUE) return 0;
    PosixFileHandle* pfh = static_cast<PosixFileHandle*>(hFile);
    struct stat st;
    if (fstat(pfh->fd, &st) == 0) {
        if (lpFileSizeHigh) *lpFileSizeHigh = (DWORD)(st.st_size >> 32);
        return (DWORD)(st.st_size & 0xFFFFFFFF);
    }
    return 0;
}

BOOL GetDiskFreeSpaceA(LPCSTR lpRootPathName, DWORD* lpSectorsPerCluster,
                         DWORD* lpBytesPerSector, DWORD* lpNumberOfFreeClusters,
                         DWORD* lpTotalNumberOfClusters)
{
    struct statvfs st;
    const char* path = (lpRootPathName && *lpRootPathName) ? lpRootPathName : ".";
    if (statvfs(path, &st) != 0)
        return FALSE;

    if (lpBytesPerSector) *lpBytesPerSector = 512;
    if (lpSectorsPerCluster) *lpSectorsPerCluster = (DWORD)(st.f_bsize / 512);
    if (lpNumberOfFreeClusters) *lpNumberOfFreeClusters = (DWORD)st.f_bavail;
    if (lpTotalNumberOfClusters) *lpTotalNumberOfClusters = (DWORD)st.f_blocks;
    return TRUE;
}

static PHANDLER_ROUTINE g_posixCtrlHandler = nullptr;

static void PosixSignalHandler(int sig)
{
    if (g_posixCtrlHandler) {
        g_posixCtrlHandler(CTRL_C_EVENT);
    }
}

BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE HandlerRoutine, BOOL Add)
{
    if (Add) {
        g_posixCtrlHandler = HandlerRoutine;
        signal(SIGINT, PosixSignalHandler);
        signal(SIGTERM, PosixSignalHandler);
    } else {
        g_posixCtrlHandler = nullptr;
        signal(SIGINT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
    }
    return TRUE;
}

#endif // !_WIN32
