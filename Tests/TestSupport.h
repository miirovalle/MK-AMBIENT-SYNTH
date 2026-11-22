#pragma once

// Minimal assertion harness so the DSP core can be tested without pulling in
// any test framework or third-party dependency.

#include "DSP/Filters.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace mktest
{

constexpr double kPiD = 3.14159265358979323846;
constexpr double kTwoPiD = 6.28318530717958647692;

extern int g_checks;
extern int g_failures;

void fail (const char* file, int line, const std::string& what);

inline void expect (bool condition, const char* file, int line, const std::string& what)
{
    ++g_checks;
    if (!condition)
        fail (file, line, what);
}

inline void expectNear (double actual, double expected, double tolerance, const char* file, int line,
                        const char* actualExpr, const char* expectedExpr)
{
    ++g_checks;
    if (! (std::fabs (actual - expected) <= tolerance))
    {
        char buffer[512];
        std::snprintf (buffer, sizeof buffer, "CHECK_NEAR(%s, %s): got %.6f expected %.6f (tol %.6f)",
                       actualExpr, expectedExpr, actual, expected, tolerance);
        fail (file, line, buffer);
    }
}

#define CHECK(cond) ::mktest::expect ((cond), __FILE__, __LINE__, "CHECK(" #cond ")")
#define CHECK_MSG(cond, msg) ::mktest::expect ((cond), __FILE__, __LINE__, std::string ("CHECK(" #cond "): ") + (msg))
#define CHECK_NEAR(actual, expected, tol) \
    ::mktest::expectNear ((double) (actual), (double) (expected), (double) (tol), __FILE__, __LINE__, #actual, #expected)

// --- audio helpers ---------------------------------------------------------

inline float peak (const float* data, int n)
{
    float p = 0.0f;
    for (int i = 0; i < n; ++i)
        p = std::max (p, std::fabs (data[i]));
    return p;
}

inline double rms (const float* data, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += (double) data[i] * (double) data[i];
    return n > 0 ? std::sqrt (sum / n) : 0.0;
}

inline double mean (const float* data, int n)
{
    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += data[i];
    return n > 0 ? sum / n : 0.0;
}

inline bool allFinite (const float* data, int n)
{
    for (int i = 0; i < n; ++i)
        if (! std::isfinite (data[i]))
            return false;
    return true;
}

inline double correlation (const float* a, const float* b, int n)
{
    double sa = 0.0, sb = 0.0, sab = 0.0;
    for (int i = 0; i < n; ++i)
    {
        sa += (double) a[i] * a[i];
        sb += (double) b[i] * b[i];
        sab += (double) a[i] * b[i];
    }
    if (sa < 1.0e-12 || sb < 1.0e-12)
        return 0.0;
    return sab / std::sqrt (sa * sb);
}

struct Bands
{
    double low = 0.0;
    double high = 0.0;
    double highOverLow() const { return high / std::max (1.0e-12, low); }
};

inline Bands measureBands (const float* data, int n, double sampleRate)
{
    mk::OnePoleLowPass lowFilter, highFilter;
    lowFilter.setCutoff (sampleRate, 160.0);
    highFilter.setCutoff (sampleRate, 4000.0);

    Bands bands;
    for (int i = 0; i < n; ++i)
    {
        const float low = lowFilter.process (data[i]);
        const float high = data[i] - highFilter.process (data[i]);
        bands.low += (double) low * low;
        bands.high += (double) high * high;
    }
    return bands;
}

} // namespace mktest
