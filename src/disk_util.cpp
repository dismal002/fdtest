// ============================================================================
// disk_util.cpp - Disk space query utilities
// ============================================================================

#include "disk_util.h"

// ----------------------------------------------------------------------------
// GetDiskSizeBytes
//
// Uses GetDiskFreeSpaceA to compute:
//   totalSize = sectorsPerCluster * bytesPerSector * totalClusters
//   freeSpace = sectorsPerCluster * bytesPerSector * freeClusters
//
// The original code handles unsigned 32-bit values that may have the high bit
// set by adding 4294967296.0 (2^32) to the double conversion when needed.
// This handles the case where a DWORD > 0x7FFFFFFF is cast to a signed int.
// ----------------------------------------------------------------------------
double GetDiskSizeBytes(double* pTotalSize, const char* lpRootPathName)
{
    DWORD sectorsPerCluster = 0;
    DWORD bytesPerSector = 0;
    DWORD numberOfFreeClusters = 0;
    DWORD totalNumberOfClusters = 0;

    GetDiskFreeSpaceA(
        lpRootPathName,
        &sectorsPerCluster,
        &bytesPerSector,
        &numberOfFreeClusters,
        &totalNumberOfClusters
    );

    // Compute total size if requested
    if (pTotalSize != nullptr)
    {
        double spc = (double)(int)sectorsPerCluster;
        if ((sectorsPerCluster & 0x80000000) != 0)
            spc += BYTES_4GB;

        double bps = (double)(int)bytesPerSector;
        if ((bytesPerSector & 0x80000000) != 0)
            bps += BYTES_4GB;

        double clusterSize = spc * bps;

        double tc = (double)(int)totalNumberOfClusters;
        if ((totalNumberOfClusters & 0x80000000) != 0)
            tc += BYTES_4GB;

        *pTotalSize = clusterSize * tc;
    }

    // Compute free space
    double spc2 = (double)(int)sectorsPerCluster;
    if ((int)sectorsPerCluster < 0)
        spc2 += BYTES_4GB;

    double bps2 = (double)(int)bytesPerSector;
    if ((int)bytesPerSector < 0)
        bps2 += BYTES_4GB;

    double clusterSize2 = spc2 * bps2;

    double fc = (double)(int)numberOfFreeClusters;
    if ((numberOfFreeClusters & 0x80000000) != 0)
        fc += BYTES_4GB;

    return clusterSize2 * fc;
}
