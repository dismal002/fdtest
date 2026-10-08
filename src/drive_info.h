#pragma once
// ============================================================================
// drive_info.h - Drive information display
// ============================================================================

#include "fdtest.h"

// Display detailed information about a single drive (vendor, model, size, etc.)
// Opens the physical device via \\.\X: and queries IOCTL_STORAGE_QUERY_PROPERTY.
// Returns 1 on success, 0 on failure.
int DisplayDriveInfo(CHAR* lpRootPathName);

// List all accessible drives (A: through Z:) with brief info.
int ListAllDrives();
