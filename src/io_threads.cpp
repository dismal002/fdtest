// ============================================================================
// io_threads.cpp - Double-buffered threaded I/O
// ============================================================================

#include "io_threads.h"

// ----------------------------------------------------------------------------
// ReaderThread
//
// This thread implements double-buffered reading:
// 1. Wait for g_hReadEvent (main thread signals when it's time to read)
// 2. Read BUFFER_SIZE bytes into whichever buffer is active
// 3. If read fails, seek forward by BUFFER_SIZE to skip the bad block
// 4. Toggle g_activeBuffer
// 5. Signal g_hDoneEvent (main thread can now process the data)
// 6. Loop forever
// ----------------------------------------------------------------------------
DWORD WINAPI ReaderThread(LPVOID lpParam)
{
    IOThreadParams* params = (IOThreadParams*)lpParam;

    while (true)
    {
        WaitForSingleObject(g_hReadEvent, INFINITE);

        HANDLE hFile = *params->pFileHandle;
        BOOL result;

        if (g_activeBuffer != 0)
        {
            result = ReadFile(hFile, params->buffer1, BUFFER_SIZE,
                              &params->bytesXfer1, nullptr);
            params->result1 = result;
        }
        else
        {
            result = ReadFile(hFile, params->buffer2, BUFFER_SIZE,
                              &params->bytesXfer2, nullptr);
            params->result2 = result;
        }

        // If read failed, seek forward past the bad block
        if (!result)
        {
            SetFilePointer(*params->pFileHandle, BUFFER_SIZE, nullptr,
                           FILE_CURRENT);
        }

        // Toggle active buffer
        g_activeBuffer = (g_activeBuffer == 0) ? 1 : 0;

        // Signal completion
        SetEvent(g_hDoneEvent);
    }

    return 0;  // Never reached
}

// ----------------------------------------------------------------------------
// WriterThread
//
// This thread implements double-buffered writing:
// 1. Wait for g_hWriteEvent (main thread signals when data is ready)
// 2. Write BUFFER_SIZE bytes from whichever buffer is active
// 3. Signal g_hDoneEvent
// 4. Loop forever
// ----------------------------------------------------------------------------
DWORD WINAPI WriterThread(LPVOID lpParam)
{
    IOThreadParams* params = (IOThreadParams*)lpParam;

    while (true)
    {
        WaitForSingleObject(g_hWriteEvent, INFINITE);

        BOOL result;
        HANDLE hFile = *params->pFileHandle;

        if (g_activeBuffer != 0)
        {
            result = WriteFile(hFile,
                               params->buffer1, BUFFER_SIZE,
                               &params->bytesXfer1, nullptr);
            params->result1 = result;
        }
        else
        {
            result = WriteFile(hFile,
                               params->buffer2, BUFFER_SIZE,
                               &params->bytesXfer2, nullptr);
            params->result2 = result;
        }

        // Signal completion
        SetEvent(g_hDoneEvent);
    }

    return 0;  // Never reached
}
