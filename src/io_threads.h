#pragma once
// ============================================================================
// io_threads.h - Double-buffered threaded I/O
// ============================================================================

#include "fdtest.h"

// Parameter block shared between main thread and I/O threads.
struct IOThreadParams {
    HANDLE* pFileHandle;    // Parameter[0] - pointer to file handle
    LPVOID  buffer1;        // Parameter[1] - first I/O buffer
    LPVOID  buffer2;        // Parameter[2] - second I/O buffer
    DWORD   bytesXfer1;     // Parameter[3] - bytes transferred for buffer1
    DWORD   bytesXfer2;     // Parameter[4] - bytes transferred for buffer2
    BOOL    result1;        // v174 - success/failure for buffer1
    BOOL    result2;        // v175 - success/failure for buffer2
};

// Reader thread entry point.
// Waits on g_hReadEvent, reads BUFFER_SIZE bytes into the active buffer,
// toggles g_activeBuffer, signals g_hDoneEvent.
DWORD WINAPI ReaderThread(LPVOID lpParam);

// Writer thread entry point.
// Waits on g_hWriteEvent, writes BUFFER_SIZE bytes from the active buffer,
// signals g_hDoneEvent.
DWORD WINAPI WriterThread(LPVOID lpParam);
