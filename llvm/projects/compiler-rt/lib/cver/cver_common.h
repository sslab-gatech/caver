#ifndef CVER_COMMON_H
#define CVER_COMMON_H

#include "cver_internal.h"
#include "cver_flags.h"
#include "sanitizer_common/sanitizer_common.h"

namespace __cver {

#ifdef CVER_NDEBUG
#define VERBOSE_PRINT(...)
#define CVER_DEBUG_STMT(condition, stmt)
#else
#define VERBOSE_PRINT(...)                      \
  if (flags()->verbose) {                       \
    Printf("\tCVER\t%s : ", __FUNCTION__);      \
    Printf(__VA_ARGS__);                        \
  }
#define CVER_DEBUG_STMT(condition, stmt) if (UNLIKELY(condition)) { stmt;}
#endif // CVER_DEBUG

/// \brief An opaque handle to a value.
typedef uptr ValueHandle;

} // namespace __cver

#endif // CVER_COMMON_H
