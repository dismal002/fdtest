// ============================================================================
// prng.cpp - Xorshift128 Pseudo-Random Number Generator
// ============================================================================

#include "prng.h"

PRNGState g_prng = {
    PRNG_SEED_A,  // 362436069
    PRNG_SEED_B,  // 521288629
    PRNG_SEED_C,  // 88675123
    PRNG_SEED_D   // -593279510 (0xDC963B7A)
};

// ----------------------------------------------------------------------------
// prng_reset - Reset to initial seeds
// Called before each write pass and each verify pass to ensure deterministic
// sequences that can be compared.
// ----------------------------------------------------------------------------
void prng_reset()
{
    g_prng.a = PRNG_SEED_A;
    g_prng.b = PRNG_SEED_B;
    g_prng.c = PRNG_SEED_C;
    g_prng.d = PRNG_SEED_D;
}

// ----------------------------------------------------------------------------
// prng_fill_buffer - Fill buffer with 256K random DWORDs
// ----------------------------------------------------------------------------
void prng_fill_buffer(unsigned int* buf)
{
    int a = g_prng.a;
    int b = g_prng.b;
    int c = g_prng.c;
    unsigned int d = (unsigned int)g_prng.d;

    for (int i = PRNG_COUNT; i != 0; --i)
    {
        int t = a ^ (a << 11);
        a = b;
        b = c;
        c = (int)d;
        d ^= (unsigned int)(t ^ ((t ^ (d >> 11)) >> 8));
        *buf++ = d;
    }

    g_prng.a = a;
    g_prng.b = b;
    g_prng.c = c;
    g_prng.d = (int)d;
}

// ----------------------------------------------------------------------------
// prng_verify_buffer - Verify buffer contents against PRNG sequence
// Generates the same xorshift128 sequence and compares each value.
// Returns 1 if all match, 0 if any mismatch is found.
// The PRNG state is advanced regardless of match/mismatch.
// ----------------------------------------------------------------------------
int prng_verify_buffer(int* buf)
{
    unsigned int d = (unsigned int)g_prng.d;
    int c = g_prng.c;
    int a = g_prng.a;
    int b = g_prng.b;
    int match = 1;

    for (int i = PRNG_COUNT; i != 0; --i)
    {
        int t = a ^ (a << 11);
        a = b;
        b = c;
        c = (int)d;
        d ^= (unsigned int)(t ^ ((t ^ (d >> 11)) >> 8));

        if (*buf++ != (int)d)
            match = 0;
    }
    // dword_413EB8 = v5 (b), dword_413EB4 = v4 (a),
    // dword_413EBC = v2 (last loop's d→c), dword_413EC0 = v1 (d)
    g_prng.b = b;
    g_prng.a = a;
    g_prng.c = c;
    g_prng.d = (int)d;

    return match;
}
