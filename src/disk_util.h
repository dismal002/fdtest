#pragma once
// ============================================================================
// disk_util.h - Disk space query utilities
// ============================================================================

#include "fdtest.h"

// Get the free space (in bytes) of a drive.
// If pTotalSize is non-null, also writes the total disk size there.
// Returns free space in bytes as a double.
double GetDiskSizeBytes(double* pTotalSize, const char* lpRootPathName);
