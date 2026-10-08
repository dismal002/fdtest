#pragma once
// ============================================================================
// prng.h - Xorshift128 Pseudo-Random Number Generator
// ============================================================================

#include "fdtest.h"

// PRNG state
struct PRNGState {
    int a;  // dword_413EB4
    int b;  // dword_413EB8
    int c;  // dword_413EBC
    int d;  // dword_413EC0
};

extern PRNGState g_prng;

// Reset the PRNG to its initial seed state
void prng_reset();

// Fill a buffer with PRNG_COUNT (256K) random DWORDs (1 MB of data)
// Corresponds to sub_401000
void prng_fill_buffer(unsigned int* buf);

// Verify a buffer against the PRNG sequence
// Returns 1 if all values match, 0 otherwise
int prng_verify_buffer(int* buf);
