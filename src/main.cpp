#include "fdtest.h"
#include "prng.h"
#include "io_threads.h"
#include "drive_info.h"
#include "disk_util.h"
#include "console.h"

// ============================================================================
// Global variable definitions
// ============================================================================

HANDLE g_hReadEvent     = nullptr;   // hObject
HANDLE g_hWriteEvent    = nullptr;   // dword_414F6C
HANDLE g_hDoneEvent     = nullptr;   // hEvent
int    g_activeBuffer   = 1;         // dword_413EB0
LPCSTR g_cancelFlag     = nullptr;   // lpModuleName
HANDLE g_hConsoleOutput = (HANDLE)(intptr_t)-2;
CHAR   g_stringBuffer[256] = {0};    // Buffer

// ============================================================================
// main
// ============================================================================
int __cdecl main(int argc, const char** argv, const char** envp)
{
    // ----------------------------------------------------------------
    // Allocate I/O buffers (two 1 MB buffers for double-buffering)
    // ----------------------------------------------------------------
    LPVOID lpBuffer = malloc(BUFFER_SIZE);    // Buffer 1
    void*  block    = malloc(BUFFER_SIZE);    // Buffer 2

    // ----------------------------------------------------------------
    // Set up I/O thread parameter block
    // ----------------------------------------------------------------
    HANDLE hFile = nullptr;
    IOThreadParams ioParams;
    ioParams.pFileHandle = &hFile;
    ioParams.buffer1     = lpBuffer;
    ioParams.buffer2     = block;
    ioParams.bytesXfer1  = 0;
    ioParams.bytesXfer2  = 0;
    ioParams.result1     = 0;
    ioParams.result2     = 0;

    // ----------------------------------------------------------------
    // Initialize state variables
    // ----------------------------------------------------------------
    int  numPasses       = 1;        // v148 - how many passes to run
    int  passCounter     = 0;        // v161 - current pass number
    int  runVerify       = 1;        // v166 - whether to run verification
    int  doRandomize     = 0;        // v172 - randomize pass order
    int  passesOK        = 0;        // counter for successful passes
    char confirmChar     = 'Y';      // v145
    CHAR rootPathName[4] = {0};      // Drive root e.g. "C:\\"
    CHAR fileName[184]   = {0};      // File path buffer
    CHAR fmtBuffer[256]  = {0};      // Local format/display buffer
    DWORD writeElapsed   = 0;        // elapsed time from write phase
    int  progressBarChars = 0;       // number of progress bar character slots
    double prevMB        = 0.0;
    int  currentBlock    = 0;
    int  fileNumber      = 0;
    int  totalErrors     = 0;
    int  readErrors      = 0;
    int  writeResult     = 0;
    int  delFileNum      = 2;

    g_cancelFlag = nullptr;

    // ----------------------------------------------------------------
    // Create synchronization events
    // ----------------------------------------------------------------
    g_hReadEvent  = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    g_hWriteEvent = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    g_hDoneEvent  = CreateEventA(nullptr, FALSE, FALSE, nullptr);

    // Progress tracking map (50 slots for the progress bar)
    // 0 = not tested, 1 = OK, 2 = mismatch, 3 = read error
    char progressMap[50];
    memset(progressMap, 0, sizeof(progressMap));

    DWORD threadIdRead  = (DWORD)-1;
    DWORD threadIdWrite = (DWORD)-1;

    // ----------------------------------------------------------------
    // Print welcome banner
    // ----------------------------------------------------------------
    LoadResString(IDS_BANNER, g_stringBuffer, 256);
    printf(g_stringBuffer);

    // ----------------------------------------------------------------
    // Parse command-line arguments
    // ----------------------------------------------------------------
    if (argc == 2)
    {
        // Single argument: should be "X:" (drive letter + colon)
        if (strlen(argv[1]) != 2 || argv[1][1] != ':')
            goto INTERACTIVE_MODE;
        goto COPY_DRIVE_LETTER;
    }

    if (argc == 3)
    {
        // Two arguments: drive letter and Y/N
        const char* arg2 = argv[2];
        if (strlen(arg2) == 1)
        {
            char ch = *arg2;
            if (ch == 'Y' || ch == 'y' || ch == 'N' || ch == 'n')
                goto COPY_DRIVE_LETTER;
        }
    }

INTERACTIVE_MODE:
    {
        // Show usage
        LoadResString(IDS_USAGE, fmtBuffer, 256);
        printf(fmtBuffer, *argv);
        printf("\n\n***********************************\n\n");

        // List all drives
        rootPathName[1] = ':';
        rootPathName[2] = '\\';
        rootPathName[3] = '\0';
        ListAllDrives();

        // Prompt for drive letter
        LoadResString(IDS_PROMPT_DRIVE, fmtBuffer, 256);
        do
        {
            printf(fmtBuffer);
            scanf("%c", &rootPathName[0]);
            FlushStdin();
        }
        while ((rootPathName[0] < 'a' || rootPathName[0] > 'z') &&
               (rootPathName[0] < 'A' || rootPathName[0] > 'Z'));

        // Prompt for verification
        LoadResString(IDS_PROMPT_VERIFY, fmtBuffer, 256);
        printf(fmtBuffer);
        scanf("%c", &confirmChar);
        FlushStdin();

        if (confirmChar != 'Y' && confirmChar != 'y')
        {
            runVerify = 0;
            goto AFTER_ARGS;
        }

        // Prompt for number of passes
        LoadResString(IDS_PROMPT_PASSES, fmtBuffer, 256);
        do
        {
            printf(fmtBuffer);
            scanf("%ld", &numPasses);
        }
        while (numPasses < 1);
        FlushStdin();

        // If multiple passes, ask about randomization
        if (numPasses > 1)
        {
            LoadResString(IDS_PROMPT_RANDOMIZE, fmtBuffer, 256);
            printf(fmtBuffer);
            scanf("%c", &confirmChar);
            FlushStdin();
            if (confirmChar == 'Y' || confirmChar == 'y')
                doRandomize = 1;
        }

        goto AFTER_ARGS;
    }

COPY_DRIVE_LETTER:
    {
        // Copy drive letter from argv[1]
        const char* src = argv[1];
        char* dst = rootPathName;
        while (*src)
            *dst++ = *src++;
        *dst = '\0';

        // Check for Y/N argument
        if (argc == 3)
            confirmChar = *argv[2];

        if (confirmChar == 'N' || confirmChar == 'n')
            runVerify = 0;
    }

AFTER_ARGS:
    // Uppercase the drive letter
    rootPathName[0] &= ~0x20;

    // Ensure root path is "X:\"
    if (rootPathName[1] == '\0')
    {
        rootPathName[1] = ':';
        rootPathName[2] = '\\';
        rootPathName[3] = '\0';
    }

    // Display drive info
    if (DisplayDriveInfo(rootPathName) == 0)
        goto CLEANUP;

    // ----------------------------------------------------------------
    // Create I/O threads
    // ----------------------------------------------------------------
    CreateThread(nullptr, 0, ReaderThread, &ioParams, 0, &threadIdRead);
    CreateThread(nullptr, 0, WriterThread, &ioParams, 0, &threadIdWrite);
    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    // ================================================================
    // MAIN TEST LOOP (one iteration per pass)
    // ================================================================
PASS_LOOP:
    while (true)
    {
        ++passCounter;
        prevMB       = 0.0;
        currentBlock = 0;
        fileNumber   = 0;
        totalErrors  = 0;
        readErrors   = 0;
        writeResult  = 0;

        // Print pass header
        LoadResString(IDS_PASS_HEADER, fmtBuffer, 256);
        printf(fmtBuffer, passCounter, numPasses);

        // Check if test data files already exist
        sprintf(fileName, "%s\\FDT%05d.BIN", rootPathName, 1);

        if (GetFileAttributesA(fileName) == INVALID_FILE_ATTRIBUTES)
        {
            // ========================================================
            // WRITE PHASE - No existing files, create new ones
            // ========================================================

            // Reset PRNG seeds
            prng_reset();
            ResetEvent(g_hDoneEvent);

            // Check free space
            DWORD spc, bps, numFree, numTotal;
            LoadResString(IDS_STATUS_FMT, fmtBuffer, 256);
            printf(fmtBuffer);

            GetDiskFreeSpaceA(rootPathName, &spc, &bps, &numFree, &numTotal);
            double spcD = (double)(int)spc;
            if ((spc & 0x80000000) != 0) spcD += BYTES_4GB;
            double bpsD = (double)(int)bps;
            if ((bps & 0x80000000) != 0) bpsD += BYTES_4GB;
            double freeClusD = (double)(int)numFree;
            if ((numFree & 0x80000000) != 0) freeClusD += BYTES_4GB;

            double freeBytes = spcD * bpsD * freeClusD;

            if (freeBytes <= BYTES_PER_MB)
            {
                // Not enough space
                int msgId = 146;  // IDS_NOT_ENOUGH_SPACE
                LoadResString(msgId, fmtBuffer, 256);
                printf(fmtBuffer);
                goto CLEANUP;
            }

            if (runVerify == 0)
            {
                int msgId = 116;  // IDS_NO_FILES_SKIP
                LoadResString(msgId, fmtBuffer, 256);
                printf(fmtBuffer);
                goto CLEANUP;
            }

            // Begin writing
            LoadResString(IDS_FILES_EXIST, fmtBuffer, 256);

            // Fill the first buffer with PRNG data
            unsigned int* activeBuf = (unsigned int*)lpBuffer;
            if (g_activeBuffer == 0)
                activeBuf = (unsigned int*)block;
            prng_fill_buffer(activeBuf);

            DWORD tickStart = GetTickCount();
            writeElapsed = 0;
            int   lastReportTick = 0;
            int   blockOffset = -BLOCKS_PER_FILE;
            fileNumber = 0;

            LoadResString(IDS_WRITING_HDR, fmtBuffer, 256);
            printf(fmtBuffer);

            LoadResString(IDS_STATUS_FMT, fmtBuffer, 256);

            while (true)
            {
                // Check free space
                DWORD spc2, bps2, numFree2, numTotal2;
                GetDiskFreeSpaceA(rootPathName, &spc2, &bps2, &numFree2, &numTotal2);
                double spc2D = (double)(int)spc2;
                if ((spc2 & 0x80000000) != 0) spc2D += BYTES_4GB;
                double bps2D = (double)(int)bps2;
                if ((bps2 & 0x80000000) != 0) bps2D += BYTES_4GB;
                double free2D = (double)(int)numFree2;
                if ((numFree2 & 0x80000000) != 0) free2D += BYTES_4GB;
                double curFree = spc2D * bps2D * free2D;

                if (curFree <= BYTES_PER_MB)
                    break;  // Drive full

                double remainingMB = curFree * MB_PER_BLOCK;

                // Open new file every BLOCKS_PER_FILE blocks
                if ((currentBlock & (BLOCKS_PER_FILE - 1)) == 0)
                {
                    blockOffset += BLOCKS_PER_FILE;
                    ++fileNumber;
                    currentBlock = 0;

                    if (hFile != INVALID_HANDLE_VALUE && hFile != nullptr)
                    {
                        CloseHandle(hFile);
                        hFile = nullptr;
                    }

                    sprintf(fileName, "%s\\FDT%05d.BIN", rootPathName, fileNumber);
                    hFile = CreateFileA(fileName, GENERIC_READ | GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        nullptr, OPEN_ALWAYS,
                                        FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                    if (hFile == INVALID_HANDLE_VALUE)
                    {
                        LoadResString(IDS_CREATE_FILE_ERR, fmtBuffer, 256);
                        printf(fmtBuffer, fileName);
                        goto CLEANUP;
                    }

                    // Signal writer thread
                    SetEvent(g_hWriteEvent);
                }

                // Fill next buffer with PRNG data
                unsigned int* nextBuf = (unsigned int*)block;
                if (g_activeBuffer == 0)
                    nextBuf = (unsigned int*)lpBuffer;
                prng_fill_buffer(nextBuf);

                // Wait for I/O to complete
                WaitForSingleObject(g_hDoneEvent, INFINITE);

                // Check which buffer's result to use
                if (g_activeBuffer != 0)
                    writeResult = ioParams.result1;
                else
                    writeResult = ioParams.result2;

                // Toggle buffer
                g_activeBuffer = (g_activeBuffer == 0) ? 1 : 0;

                if (writeResult == 0)
                {
                    LoadResString(IDS_WRITE_FAIL_ERR, fmtBuffer, 256);
                    printf(fmtBuffer, fileName);
                    goto CLEANUP;
                }

                ++currentBlock;

                // Signal writer if not at file boundary
                if ((currentBlock & (BLOCKS_PER_FILE - 1)) != 0)
                    SetEvent(g_hWriteEvent);

                // Report progress periodically (every ~512ms)
                writeElapsed = GetTickCount() - tickStart;
                DWORD elapsed = writeElapsed;
                int reportBucket = elapsed >> 9;

                if (reportBucket != lastReportTick)
                {
                    int totalBlocks = currentBlock + blockOffset;
                    double totalBlocksD = (double)totalBlocks;
                    if (totalBlocks < 0)
                        totalBlocksD += FLOAT_4GB;

                    double avgSpeed = totalBlocksD * MS_PER_SEC / (double)elapsed;
                    float curSpeed = avgSpeed;

                    // Calculate instantaneous speed from delta
                    double fileBlocksD = (double)(fileNumber - 1);
                    if (fileNumber - 1 < 0)
                        fileBlocksD += BYTES_4GB;
                    double deltaMB = fileBlocksD * BYTES_PER_2K + (double)currentBlock - prevMB;
                    double deltaTime = (double)(1000 * (int)(__int64)deltaMB);
                    if (1000 * (int)(__int64)deltaMB < 0)
                        deltaTime += FLOAT_4GB;
                    float instSpeed = (float)(deltaTime / (double)(elapsed - (lastReportTick << 9)));

                    if (avgSpeed == 0.0)
                    {
                        curSpeed = instSpeed;
                        avgSpeed = (double)instSpeed;
                    }

                    // Calculate ETA
                    double remD = (double)(int)(__int64)remainingMB;
                    if ((int)(__int64)remainingMB < 0)
                        remD += FLOAT_4GB;
                    int etaSeconds = (int)(__int64)(remD / avgSpeed);
                    unsigned int hours   = (unsigned int)etaSeconds / SECONDS_PER_HOUR;
                    unsigned int minutes = ((unsigned int)etaSeconds % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
                    unsigned int seconds = ((unsigned int)etaSeconds % SECONDS_PER_HOUR) % SECONDS_PER_MINUTE;

                    printf(fmtBuffer, totalBlocks, instSpeed, (float)avgSpeed,
                           (unsigned int)(__int64)remainingMB, hours, minutes, seconds);

                    prevMB = totalBlocksD;
                    lastReportTick = reportBucket;

                    // Check for Ctrl+C
                    if (g_cancelFlag != nullptr)
                    {
                        runVerify = 0;
                        LoadResString(IDS_INTERRUPTED_FMT, fmtBuffer, 256);
                        printf(fmtBuffer, totalBlocks, instSpeed, (float)avgSpeed,
                               (unsigned int)(__int64)remainingMB, hours, minutes, seconds);
                        break;
                    }
                }
            }

            // Wait for final I/O
            WaitForSingleObject(g_hDoneEvent, INFINITE);

            if (hFile != INVALID_HANDLE_VALUE && hFile != nullptr)
            {
                CloseHandle(hFile);
                hFile = nullptr;
            }

            if (g_cancelFlag == nullptr)
            {
                // Print write completion summary
                LoadResString(IDS_WRITE_COMPLETE, fmtBuffer, 256);
                int totalWritten = ((fileNumber - 1) << 11) + currentBlock;
                double totalD = (double)totalWritten;
                if (totalWritten < 0)
                    totalD += FLOAT_4GB;
                printf(fmtBuffer, totalWritten + 1, totalD * MS_PER_SEC / (double)writeElapsed);
            }

            if (runVerify == 0)
            {
                LoadResString(IDS_VERIFY_SKIPPED, fmtBuffer, 256);
                printf(fmtBuffer);
                goto CLEANUP;
            }

            prevMB = 0.0;
        }
        else if (runVerify == 0)
        {
            // Files exist but verification not requested
            LoadResString(IDS_NO_FILES_SKIP, fmtBuffer, 256);
            printf(fmtBuffer);
            goto CLEANUP;
        }
        else
        {
            // Files already exist, skip to verification
            LoadResString(IDS_FILES_EXIST, fmtBuffer, 256);
            printf(fmtBuffer);

            LoadResString(IDS_STATUS_FMT, fmtBuffer, 256);
            printf(fmtBuffer);
        }

        // ================================================================
        // READ / VERIFY PHASE
        // ================================================================
        {
            // Find how many test files exist
            int lastFile = 1;
            do
            {
                sprintf(fileName, "%s\\FDT%05d.BIN", rootPathName, lastFile++);
            }
            while (GetFileAttributesA(fileName) != INVALID_FILE_ATTRIBUTES);

            lastFile -= 2;  // Back to last valid file
            sprintf(fileName, "%s\\FDT%05d.BIN", rootPathName, lastFile);

            // Open last file to get total block count
            HANDLE hLastFile = CreateFileA(fileName, GENERIC_READ,
                                           FILE_SHARE_READ | FILE_SHARE_WRITE,
                                           nullptr, OPEN_EXISTING,
                                           FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
            if (hLastFile == INVALID_HANDLE_VALUE)
            {
                LoadResString(IDS_OPEN_FILE_ERR, fmtBuffer, 256);
                printf(fmtBuffer, fileName);
                goto CLEANUP;
            }

            int totalBlocks = ((lastFile - 1) << 11) + (GetFileSize(hLastFile, nullptr) >> 20);
            double totalBlocksD = (double)totalBlocks;
            if (totalBlocks < 0)
                totalBlocksD += BYTES_4GB;

            double totalMBtoRead = totalBlocksD;

            CloseHandle(hLastFile);

            // Calculate progress bar scale
            int progressBarSize = (int)(__int64)(0.02 * totalBlocksD) + 1;
            int progressBarSlots = progressBarSize;
            double progressRemaining = totalBlocksD - 0.5;
            double blocksPerSlot = progressRemaining / (double)progressBarSize;
            progressBarChars = (int)blocksPerSlot + 1;

            // Open first file
            sprintf(fileName, "%s\\FDT%05d.BIN", rootPathName, 1);

            // Reset PRNG
            prng_reset();
            ResetEvent(g_hDoneEvent);

            LoadResString(IDS_READING_HDR, fmtBuffer, 256);

            DWORD tickStart2 = GetTickCount();
            int   lastReportTick2 = 0;
            int   blockOffset2 = -BLOCKS_PER_FILE;
            fileNumber = 0;
            currentBlock = 0;

            while (true)
            {
                // Open new file every BLOCKS_PER_FILE blocks
                if ((currentBlock & (BLOCKS_PER_FILE - 1)) == 0)
                {
                    ++fileNumber;
                    blockOffset2 += BLOCKS_PER_FILE;
                    currentBlock = 0;

                    if (hFile != INVALID_HANDLE_VALUE && hFile != nullptr)
                    {
                        CloseHandle(hFile);
                        hFile = nullptr;
                    }

                    sprintf(fileName, "%s\\FDT%05d.BIN", rootPathName, fileNumber);

                    if (GetFileAttributesA(fileName) == INVALID_FILE_ATTRIBUTES)
                        goto VERIFY_DONE;

                    hFile = CreateFileA(fileName, GENERIC_READ,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        nullptr, OPEN_EXISTING,
                                        FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
                    if (hFile == INVALID_HANDLE_VALUE)
                    {
                        LoadResString(IDS_OPEN_FILE_ERR, fmtBuffer, 256);
                        printf(fmtBuffer, fileName);
                        goto CLEANUP;
                    }

                    // Signal reader thread
                    SetEvent(g_hReadEvent);
                }

                // Wait for read completion
                WaitForSingleObject(g_hDoneEvent, INFINITE);

                // Get the buffer that was just read
                int* verifyBuf;
                int  readResult;
                if (g_activeBuffer != 0)
                {
                    verifyBuf = (int*)block;
                    readResult = ioParams.result2;
                }
                else
                {
                    verifyBuf = (int*)lpBuffer;
                    readResult = ioParams.result1;
                }

                ++currentBlock;

                // Signal reader for next block if not at file boundary
                if ((currentBlock & (BLOCKS_PER_FILE - 1)) != 0)
                    SetEvent(g_hReadEvent);

                unsigned int blockIndex = blockOffset2 + (currentBlock - 1);

                if (readResult == 0)
                {
                    // Read I/O error
                    ++readErrors;
                    ++totalErrors;
                    if (blockIndex / progressBarSlots < sizeof(progressMap))
                        progressMap[blockIndex / progressBarSlots] = 3;

                    prng_verify_buffer(verifyBuf);  // Advance PRNG state

                    LoadResString(IDS_READ_ERROR, fmtBuffer, 256);
                    SetConsoleColor(COLOR_YELLOW);
                }
                else if (prng_verify_buffer(verifyBuf) == 0)
                {
                    // Data mismatch
                    ++totalErrors;
                    if (blockIndex / progressBarSlots < sizeof(progressMap))
                    {
                        if (progressMap[blockIndex / progressBarSlots] < 2)
                            progressMap[blockIndex / progressBarSlots] = 2;
                    }

                    LoadResString(IDS_MISMATCH, fmtBuffer, 256);
                    SetConsoleColor(COLOR_RED);
                }
                else
                {
                    // Block verified OK
                    if (blockIndex / progressBarSlots < sizeof(progressMap))
                    {
                        if (progressMap[blockIndex / progressBarSlots] == 0)
                            progressMap[blockIndex / progressBarSlots] = 1;
                    }
                    goto AFTER_VERIFY_BLOCK;
                }

                // Print error message
                printf(fmtBuffer, blockIndex);
                SetConsoleColor(COLOR_DEFAULT);
                LoadResString(IDS_READING_HDR, fmtBuffer, 256);

            AFTER_VERIFY_BLOCK:
                totalMBtoRead -= 1.0;
                if (totalMBtoRead == 0.0)
                    goto VERIFY_DONE;

                // Report progress periodically
                DWORD elapsed2 = GetTickCount() - tickStart2;
                int reportBucket2 = elapsed2 >> 9;

                if (reportBucket2 != lastReportTick2)
                {
                    int readBlocks = (currentBlock - 1) + blockOffset2;
                    double readBlocksD = (double)readBlocks;
                    if (readBlocks < 0)
                        readBlocksD += FLOAT_4GB;

                    double avgSpeed2 = readBlocksD * MS_PER_SEC / (double)elapsed2;
                    float curSpeed2 = (float)avgSpeed2;

                    // Instantaneous speed
                    double fileBlocksD = (double)(fileNumber - 1);
                    if (fileNumber - 1 < 0)
                        fileBlocksD += BYTES_4GB;
                    double deltaMB2 = fileBlocksD * BYTES_PER_2K + (double)(currentBlock - 1) - prevMB;
                    double deltaTime2 = (double)(1000 * (int)(__int64)deltaMB2);
                    if (1000 * (int)(__int64)deltaMB2 < 0)
                        deltaTime2 += FLOAT_4GB;
                    float instSpeed2 = (float)(deltaTime2 / (double)(elapsed2 - (lastReportTick2 << 9)));

                    if (avgSpeed2 == 0.0)
                    {
                        curSpeed2 = instSpeed2;
                        avgSpeed2 = (double)instSpeed2;
                    }

                    // ETA
                    int remaining2 = (int)(__int64)totalMBtoRead;
                    double remD2 = (double)remaining2;
                    if (remaining2 < 0)
                        remD2 += FLOAT_4GB;
                    int etaSec2 = (int)(__int64)(remD2 / avgSpeed2);
                    unsigned int h2 = (unsigned int)etaSec2 / SECONDS_PER_HOUR;
                    unsigned int m2 = ((unsigned int)etaSec2 % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
                    unsigned int s2 = ((unsigned int)etaSec2 % SECONDS_PER_HOUR) % SECONDS_PER_MINUTE;

                    printf(fmtBuffer, readBlocks, instSpeed2, curSpeed2,
                           (unsigned int)(__int64)totalMBtoRead, h2, m2, s2);

                    prevMB = readBlocksD;
                    lastReportTick2 = reportBucket2;

                    // Check for Ctrl+C
                    if (g_cancelFlag != nullptr)
                    {
                        numPasses = passCounter;
                        LoadResString(IDS_INTERRUPTED_FMT, fmtBuffer, 256);
                        printf(fmtBuffer, readBlocks, instSpeed2, curSpeed2,
                               remaining2, h2, m2, s2);
                        printf("\n");
                        break;
                    }
                }
            }

        VERIFY_DONE:
            if (hFile != INVALID_HANDLE_VALUE && hFile != nullptr)
            {
                CloseHandle(hFile);
                hFile = nullptr;
            }

            if (g_cancelFlag == nullptr)
            {
                // Print read completion summary
                LoadResString(IDS_READ_COMPLETE, fmtBuffer, 256);
                int totalRead = ((fileNumber - 1) << 11) + (currentBlock - 1);
                double totalReadD = (double)totalRead;
                if (totalRead < 0)
                    totalReadD += FLOAT_4GB;
                DWORD elapsedFinal = GetTickCount() - tickStart2;
                printf(fmtBuffer, totalRead, totalReadD * MS_PER_SEC / (double)elapsedFinal);
            }

            // ============================================================
            // RESULTS
            // ============================================================
            if (totalErrors == 0)
            {
                // No errors on this pass
                ++passesOK;
                int okMsgId = (g_cancelFlag != nullptr) ? IDS_INTERRUPTED : IDS_ALL_OK;
                LoadResString(okMsgId, fmtBuffer, 256);
                printf(fmtBuffer);

                if (numPasses > passCounter)
                    goto NEXT_PASS;

                // Check if this was the final pass
                if (numPasses == passCounter)
                {
                    // Print completion summary
                    LoadResString(IDS_PASS_SUMMARY, fmtBuffer, 256);
                    printf(fmtBuffer, numPasses);

                    // Print progress bar
                    printf("0%% [");
                    for (int i = 0; i < progressBarChars; i++)
                    {
                        if (progressMap[i] == 1)
                            SetConsoleColor(COLOR_GREEN_BG);
                        else
                            SetConsoleColor(COLOR_DEFAULT);
                        printf(" ");
                        SetConsoleColor(COLOR_DEFAULT);
                    }
                    printf("] 100%%\n\n");

                    // Print "OK" ASCII art
                    SetConsoleColor(COLOR_GREEN_ON_BLK);
                    printf(
                        "   * * *     *       *\n"
                        " *       *   *     *\n"
                        " *       *   *   *\n"
                        " *       *   * *\n"
                        " *       *   *   *\n"
                        " *       *   *     *\n"
                        "   * * *     *       *\n");
                    SetConsoleColor(COLOR_DEFAULT);

                    goto CLEANUP;
                }

                // Print progress for multi-pass
                LoadResString(IDS_PASS_PROGRESS, fmtBuffer, 256);
                printf(fmtBuffer, numPasses, passesOK);
                goto SHOW_RESULTS;
            }
            else
            {
                // Errors detected
                if (readErrors != 0)
                {
                    LoadResString(IDS_ERR_WITH_READS, fmtBuffer, 256);
                    SetConsoleColor(COLOR_WHITE);
                    printf(fmtBuffer, totalErrors, readErrors);
                }
                else
                {
                    LoadResString(IDS_ERROR_COUNT, fmtBuffer, 256);
                    SetConsoleColor(COLOR_WHITE);
                    printf(fmtBuffer, totalErrors);
                }

                SetConsoleColor(COLOR_DEFAULT);

                if (doRandomize == 0)
                {
                    if (numPasses > passCounter)
                    {
                        LoadResString(IDS_TEST_INCOMPLETE, fmtBuffer, 256);
                        printf(fmtBuffer, numPasses, passCounter);
                        goto SHOW_RESULTS;
                    }

                    if (g_cancelFlag == nullptr)
                    {
                        LoadResString(IDS_TEST_COMPLETE, fmtBuffer, 256);
                        printf(fmtBuffer, numPasses);
                        goto SHOW_RESULTS;
                    }

                    LoadResString(IDS_PASS_PROGRESS, fmtBuffer, 256);
                    printf(fmtBuffer, numPasses, passesOK);
                    goto SHOW_RESULTS;
                }

                if (numPasses <= passCounter)
                {
                    if (g_cancelFlag == nullptr)
                    {
                        LoadResString(IDS_TEST_COMPLETE, fmtBuffer, 256);
                        printf(fmtBuffer, numPasses);
                        goto SHOW_RESULTS;
                    }

                    LoadResString(IDS_PASS_PROGRESS, fmtBuffer, 256);
                    printf(fmtBuffer, numPasses, passesOK);
                    goto SHOW_RESULTS;
                }

            NEXT_PASS:
                // Clean up files before next pass
                LoadResString(IDS_CLEANUP, fmtBuffer, 256);
                printf(fmtBuffer);

                sprintf(fileName, "%s\\FDT%05d.BIN", rootPathName, 1);
                if (GetFileAttributesA(fileName) == INVALID_FILE_ATTRIBUTES)
                    continue;  // No files to delete, start next pass

                // Delete files
                delFileNum = 2;
                while (remove(fileName) == 0)
                {
                    sprintf(fileName, "%s\\FDT%05d.BIN", rootPathName, delFileNum++);
                    if (GetFileAttributesA(fileName) == INVALID_FILE_ATTRIBUTES)
                        goto PASS_LOOP;
                }

                LoadResString(IDS_DELETE_ERR, fmtBuffer, 256);
                printf(fmtBuffer, fileName);
                goto SHOW_RESULTS;
            }
        }
    }

SHOW_RESULTS:
    // ================================================================
    // Final progress bar display
    // ================================================================
    printf("0%% [");
    for (int i = 0; i < (unsigned char)progressBarChars; i++)
    {
        int state = (unsigned char)progressMap[i];
        switch (state)
        {
            case 1:  // OK
                SetConsoleColor(COLOR_GREEN_BG);
                break;
            case 2:  // Mismatch
                SetConsoleColor(COLOR_RED_BG);
                break;
            case 3:  // Read error
                SetConsoleColor(COLOR_YELLOW_BG);
                break;
            default: // Not tested
                SetConsoleColor(COLOR_DEFAULT);
                break;
        }
        printf(" ");
        SetConsoleColor(COLOR_DEFAULT);
    }
    printf("] 100%%\n\n");

    // Print "NO" ASCII art in red
    SetConsoleColor(COLOR_RED);
    printf(
        " *       *     * * *\n"
        " * *     *   *       *\n"
        " *  *    *   *       *\n"
        " *   *   *   *       *\n"
        " *    *  *   *       *\n"
        " *     * *   *       *\n"
        " *       *     * * *\n");
    SetConsoleColor(COLOR_DEFAULT);

    // ================================================================
    // CLEANUP
    // ================================================================
CLEANUP:
    SetConsoleCtrlHandler(CtrlHandler, FALSE);

    if (hFile != INVALID_HANDLE_VALUE && hFile != nullptr)
    {
        CloseHandle(hFile);
        hFile = nullptr;
    }

    free(lpBuffer);
    free(block);
    CloseHandle(g_hReadEvent);
    CloseHandle(g_hWriteEvent);
    CloseHandle(g_hDoneEvent);

    // "Press Enter to exit..."
    LoadResString(IDS_PRESS_EXIT, g_stringBuffer, 256);
    printf(g_stringBuffer);
    FlushStdin();

    return 0;
}
