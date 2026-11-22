#include "Filters.h"

// All filter behaviour lives in Filters.h so the compiler can inline the
// per-sample paths. This translation unit exists to keep the build layout
// stable and to host any future out-of-line filter helpers.

namespace mk
{
namespace
{
// Keeps the TU non-empty on strict compilers.
constexpr float kFiltersTranslationUnit = kPi;
}
} // namespace mk
