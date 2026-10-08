// ============================================================================
// drive_info.cpp - Drive information display (Cross-Platform)
// ============================================================================

#include "drive_info.h"
#include "disk_util.h"
#include "console.h"
#include <fstream>
#include <sstream>
#include <set>
#include <vector>

// Helper: format a byte count as human-readable size into buffer
static void FormatSize(char* buf, double bytes, bool showMetric)
{
    if (bytes > BYTES_PER_GB)
    {
        if (showMetric)
            sprintf(buf, "%4.2f GB (%4.2f gB)",
                    bytes * KB_TO_BYTES * KB_TO_BYTES * KB_TO_BYTES,
                    MB_TO_BYTES * (bytes * MB_TO_BYTES * MB_TO_BYTES));
        else
            sprintf(buf, "%4.0f GB",
                    bytes * KB_TO_BYTES * KB_TO_BYTES * KB_TO_BYTES);
    }
    else if (bytes > BYTES_PER_MB)
    {
        if (showMetric)
            sprintf(buf, "%4.2f MB (%4.2f mB)",
                    bytes * KB_TO_BYTES * KB_TO_BYTES,
                    bytes * MB_TO_BYTES * MB_TO_BYTES);
        else
            sprintf(buf, "%4.0f MB",
                    bytes * KB_TO_BYTES * KB_TO_BYTES);
    }
    else if (bytes > BYTES_PER_KB)
    {
        if (showMetric)
            sprintf(buf, "%4.2f KB (%4.2f kB)",
                    bytes * KB_TO_BYTES, MB_TO_BYTES * bytes);
        else
            sprintf(buf, "%4.0f KB", bytes * KB_TO_BYTES);
    }
    else
    {
        sprintf(buf, "%d Bytes", (int)bytes);
    }
}

int DisplayDriveInfo(CHAR* lpRootPathName)
{
#ifdef _WIN32
    // Build device path: "\\.\X:"
    CHAR devicePath[8];
    strcpy(devicePath, "\\\\.\\*:");
    devicePath[4] = *lpRootPathName;

    char displayBuffer[512];

    LoadResString(IDS_DRIVE_INFO_HDR, g_stringBuffer, 256);

    int pos = 0;
    {
        const char* src = g_stringBuffer;
        while (*src)
            displayBuffer[pos++] = *src++;
        displayBuffer[pos] = '\0';
    }

    HANDLE hDevice = CreateFileA(devicePath, 0, FILE_SHARE_READ, nullptr,
                                 OPEN_EXISTING, 0, nullptr);
    if (hDevice == INVALID_HANDLE_VALUE)
    {
        LoadResString(IDS_DRIVE_OPEN_ERR, g_stringBuffer, 256);
        printf("%s", g_stringBuffer);
        return 0;
    }

    DWORD inBuffer[3] = {0, 0, 0};
    BYTE outBuffer[0x400];
    DWORD bytesReturned = 0;
    memset(outBuffer, 0, sizeof(outBuffer));

    DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY_CODE,
                    inBuffer, sizeof(inBuffer),
                    outBuffer, sizeof(outBuffer),
                    &bytesReturned, nullptr);

    int vendorOffset   = *(int*)(outBuffer + 12);
    int productOffset  = *(int*)(outBuffer + 16);
    int revisionOffset = *(int*)(outBuffer + 20);
    BOOLEAN removable  = outBuffer[10];

    LoadResString(IDS_VENDOR_LABEL, g_stringBuffer, 256);
    pos = (int)strlen(displayBuffer);
    strcpy(&displayBuffer[pos], g_stringBuffer);
    pos = (int)strlen(displayBuffer);

    if (vendorOffset != 0)
    {
        char* src = (char*)&outBuffer[vendorOffset];
        while (*src)
            displayBuffer[pos++] = *src++;
        displayBuffer[pos] = '\0';
    }

    if (productOffset != 0)
    {
        char* src = (char*)&outBuffer[productOffset];
        int offset = (int)strlen(displayBuffer);
        while (*src)
            displayBuffer[offset++] = *src++;
        displayBuffer[offset] = '\0';
    }

    pos = (int)strlen(displayBuffer);
    displayBuffer[pos] = '\n';
    displayBuffer[pos + 1] = '\0';

    if (revisionOffset != 0)
    {
        LoadResString(IDS_REVISION_LABEL, g_stringBuffer, 256);
        pos = (int)strlen(displayBuffer);
        strcpy(&displayBuffer[pos], g_stringBuffer);
        pos = (int)strlen(displayBuffer);

        char* src = (char*)&outBuffer[revisionOffset];
        while (*src)
            displayBuffer[pos++] = *src++;
        displayBuffer[pos] = '\0';

        pos = (int)strlen(displayBuffer);
        displayBuffer[pos] = '\n';
        displayBuffer[pos + 1] = '\0';
    }

    LoadResString(IDS_SIZE_LABEL, g_stringBuffer, 256);
    pos = (int)strlen(displayBuffer);
    strcpy(&displayBuffer[pos], g_stringBuffer);
    pos = (int)strlen(displayBuffer);

    UINT mediaStringId = removable ? IDS_WRITABLE : IDS_READONLY;
    LoadResString(mediaStringId, g_stringBuffer, 256);
    strcpy(&displayBuffer[pos], g_stringBuffer);

    pos = (int)strlen(displayBuffer);
    displayBuffer[pos] = '\n';
    displayBuffer[pos + 1] = '\0';

    LoadResString(IDS_FREE_SPACE_LABEL, g_stringBuffer, 256);
    pos = (int)strlen(displayBuffer);
    strcpy(&displayBuffer[pos], g_stringBuffer);
    pos = (int)strlen(displayBuffer);

    double totalSize = 0.0;
    double freeSpace = GetDiskSizeBytes(&totalSize, lpRootPathName);

    FormatSize(&displayBuffer[pos], totalSize, true);

    pos = (int)strlen(displayBuffer);
    displayBuffer[pos] = '\n';
    displayBuffer[pos + 1] = '\0';

    LoadResString(IDS_FREE_SPACE_LABEL2, g_stringBuffer, 256);
    pos = (int)strlen(displayBuffer);
    strcpy(&displayBuffer[pos], g_stringBuffer);
    pos = (int)strlen(displayBuffer);

    FormatSize(&displayBuffer[pos], freeSpace, true);

    printf("%s", displayBuffer);
    printf("\n===========================================\n");

    CloseHandle(hDevice);
    return 1;
#else
    // POSIX / Linux / macOS / Android Drive Info
    double totalSize = 0.0;
    double freeSpace = GetDiskSizeBytes(&totalSize, lpRootPathName);

    if (totalSize == 0.0)
    {
        LoadResString(IDS_DRIVE_OPEN_ERR, g_stringBuffer, 256);
        printf("%s", g_stringBuffer);
        return 0;
    }

    printf("\nDrive [%s] Information\n===========================================\n", lpRootPathName);
    printf("Model     : POSIX Storage Target (%s)\n", lpRootPathName);
    
    char capStr[64], freeStr[64];
    FormatSize(capStr, totalSize, true);
    FormatSize(freeStr, freeSpace, true);
    printf("Capacity  : %s\n", capStr);
    printf("Free Space: %s\n", freeStr);
    printf("===========================================\n");
    return 1;
#endif
}

int ListAllDrives()
{
#ifdef _WIN32
    char lineBuffer[48];
    char devicePath[8];

    strcpy(lineBuffer, "[x:] 12345678123456789ABCDEF 0.00 XXX.XX GB\n");
    strcpy(devicePath, "\\\\.\\*:");

    LoadResString(IDS_DRIVE_LIST_HDR, g_stringBuffer, 256);
    printf("%s", g_stringBuffer);

    for (char driveLetter = 'A'; driveLetter <= 'Z'; driveLetter++)
    {
        devicePath[4] = driveLetter;

        HANDLE hDevice = CreateFileA(devicePath, 0, FILE_SHARE_READ, nullptr,
                                     OPEN_EXISTING, 0, nullptr);
        if (hDevice == INVALID_HANDLE_VALUE)
            continue;

        lineBuffer[1] = driveLetter;

        DWORD inBuffer[3] = {0, 0, 0};
        BYTE outBuffer[0x400];
        DWORD bytesReturned = 0;
        memset(outBuffer, 0, sizeof(outBuffer));

        DeviceIoControl(hDevice, IOCTL_STORAGE_QUERY_PROPERTY_CODE,
                        inBuffer, sizeof(inBuffer),
                        outBuffer, sizeof(outBuffer),
                        &bytesReturned, nullptr);

        int vendorOffset   = *(int*)(outBuffer + 12);
        int productOffset  = *(int*)(outBuffer + 16);
        int revisionOffset = *(int*)(outBuffer + 20);
        BOOLEAN removable  = outBuffer[10];

        unsigned int pos = 5;

        if (vendorOffset != 0)
        {
            char* src = (char*)&outBuffer[vendorOffset];
            while (*src)
                lineBuffer[pos++] = *src++;
            lineBuffer[pos] = '\0';
            pos = (unsigned int)strlen(lineBuffer);
        }

        if (productOffset != 0)
        {
            char* src = (char*)&outBuffer[productOffset];
            unsigned int base = (unsigned int)strlen(lineBuffer);
            pos = base;
            while (*src)
                lineBuffer[pos++] = *src++;
            lineBuffer[pos] = '\0';
            pos = (unsigned int)strlen(lineBuffer);

            if (pos < 29)
            {
                memset(&lineBuffer[pos], ' ', 29 - pos);
                pos = 29;
                lineBuffer[pos] = '\0';
            }
        }

        if (revisionOffset != 0)
        {
            lineBuffer[pos] = ' ';
            pos++;
            char* src = (char*)&outBuffer[revisionOffset];
            while (*src)
                lineBuffer[pos++] = *src++;
            lineBuffer[pos] = '\0';
            pos = (unsigned int)strlen(lineBuffer);
            lineBuffer[pos] = ' ';
            pos++;
            lineBuffer[pos] = '\0';
        }

        printf(removable ? "* " : "  ");

        lineBuffer[pos] = ' ';
        lineBuffer[pos + 1] = '\0';
        pos = (unsigned int)strlen(lineBuffer);

        double totalSize = 0.0;
        char drivePath[4];
        drivePath[0] = driveLetter;
        drivePath[1] = ':';
        drivePath[2] = '\\';
        drivePath[3] = '\0';

        GetDiskSizeBytes(&totalSize, drivePath);
        FormatSize(&lineBuffer[pos], totalSize, false);

        pos = (unsigned int)strlen(lineBuffer);
        lineBuffer[pos] = '\n';
        lineBuffer[pos + 1] = '\0';

        CloseHandle(hDevice);
        printf("%s", lineBuffer);

        strcpy(lineBuffer, "[x:] 12345678123456789ABCDEF 0.00 XXX.XX GB\n");
    }

    return printf("\n");
#elif defined(__APPLE__)
    // macOS Volume Listing
    LoadResString(IDS_DRIVE_LIST_HDR, g_stringBuffer, 256);
    printf("%s", g_stringBuffer);

    DIR* dir = opendir("/Volumes");
    if (dir)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_name[0] != '.')
            {
                std::string path = std::string("/Volumes/") + entry->d_name;
                double total = 0.0;
                GetDiskSizeBytes(&total, path.c_str());
                if (total > 0.0)
                {
                    char sizeBuf[64];
                    FormatSize(sizeBuf, total, false);
                    printf("  * [%s] %s\n", path.c_str(), sizeBuf);
                }
            }
        }
        closedir(dir);
    }
    double rootTotal = 0.0;
    GetDiskSizeBytes(&rootTotal, "/");
    char rootSizeBuf[64];
    FormatSize(rootSizeBuf, rootTotal, false);
    printf("    [/] %s (macOS System Volume)\n\n", rootSizeBuf);
    return 1;
#elif defined(__ANDROID__)
    // Rooted Android Storage Listing
    LoadResString(IDS_DRIVE_LIST_HDR, g_stringBuffer, 256);
    printf("%s", g_stringBuffer);

    std::vector<std::string> androidPaths = {
        "/sdcard",
        "/storage/emulated/0",
        "/data"
    };

    DIR* dir = opendir("/storage");
    if (dir)
    {
        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_name[0] != '.' && strcmp(entry->d_name, "emulated") != 0 && strcmp(entry->d_name, "self") != 0)
            {
                androidPaths.push_back(std::string("/storage/") + entry->d_name);
            }
        }
        closedir(dir);
    }

    DIR* mntDir = opendir("/mnt/media_rw");
    if (mntDir)
    {
        struct dirent* entry;
        while ((entry = readdir(mntDir)) != nullptr)
        {
            if (entry->d_name[0] != '.')
            {
                androidPaths.push_back(std::string("/mnt/media_rw/") + entry->d_name);
            }
        }
        closedir(mntDir);
    }

    std::set<std::string> seenPaths;
    for (const auto& path : androidPaths)
    {
        if (seenPaths.find(path) != seenPaths.end()) continue;
        seenPaths.insert(path);

        double total = 0.0;
        GetDiskSizeBytes(&total, path.c_str());
        if (total > 0.0)
        {
            char sizeBuf[64];
            FormatSize(sizeBuf, total, false);
            bool isExternal = (path.find("/storage/") == 0 && path != "/storage/emulated/0") || path.find("/mnt/media_rw/") == 0;
            printf("  %s [%s] %s\n", isExternal ? "*" : " ", path.c_str(), sizeBuf);
        }
    }
    return printf("\n");
#else
    // Linux Mount Points Listing
    LoadResString(IDS_DRIVE_LIST_HDR, g_stringBuffer, 256);
    printf("%s", g_stringBuffer);

    std::ifstream mounts("/proc/mounts");
    if (!mounts.is_open())
    {
        printf("  / (Root Filesystem)\n");
        return 1;
    }

    std::set<std::string> seenDevs;
    std::string line;
    while (std::getline(mounts, line))
    {
        std::istringstream iss(line);
        std::string dev, mountPoint, fstype;
        if (iss >> dev >> mountPoint >> fstype)
        {
            if (dev.rfind("/dev/", 0) == 0 && fstype != "tmpfs" && fstype != "devtmpfs" && fstype != "squashfs")
            {
                if (seenDevs.find(dev) == seenDevs.end() || mountPoint.rfind("/media/", 0) == 0 || mountPoint.rfind("/mnt/", 0) == 0)
                {
                    seenDevs.insert(dev);
                    double total = 0.0;
                    double freeSpace = GetDiskSizeBytes(&total, mountPoint.c_str());
                    if (total > 0.0)
                    {
                        char sizeBuf[64];
                        FormatSize(sizeBuf, total, false);
                        printf("  * [%s] %s (%s)\n", mountPoint.c_str(), sizeBuf, dev.c_str());
                    }
                }
            }
        }
    }
    return printf("\n");
#endif
}
