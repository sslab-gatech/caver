#include "cver_internal.h"
#include "cver_common.h"
#include "cver_allocator.h"
#include "cver_flags.h"
#include "cver_stats.h"
#include "cver_thread.h"

#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_allocator.h"
#include "sanitizer_common/sanitizer_interception.h"

#include <stddef.h>

using namespace __cver;

namespace std {
struct nothrow_t {};
}  // namespace std

// Do not trace stack for now.
#define GET_MALLOC_STACK_TRACE

#define STAT_NEW                                                \
  CVER_DEBUG_STMT(flags()->stats, {                             \
      CverStats &thread_stats = GetCurrentThreadStats();        \
      thread_stats.numNew++;                                    \
      thread_stats.numNewCurrent++;                             \
      if (thread_stats.numNewCurrent > thread_stats.numNewPeak) \
        thread_stats.numNewPeak = thread_stats.numNewCurrent;   \
    });

#define STAT_DELETE                                             \
  CVER_DEBUG_STMT(flags()->stats, {                             \
      CverStats &thread_stats = GetCurrentThreadStats();        \
      thread_stats.numDelete++;                                 \
      if (thread_stats.numNewCurrent > 0)                       \
        thread_stats.numNewCurrent--;                           \
    });

#define OPERATOR_NEW_BODY                                       \
  GET_MALLOC_STACK_TRACE;                                       \
  STAT_NEW;                                                     \
  return CverReallocate(0, 0, size, sizeof(u64), false)

#define OPERATOR_DELETE_BODY                    \
  GET_MALLOC_STACK_TRACE;                       \
  STAT_DELETE;                                  \
  if (ptr) CverDeallocate(0, ptr)

INTERCEPTOR_ATTRIBUTE
void *operator new(size_t size) { OPERATOR_NEW_BODY; }
INTERCEPTOR_ATTRIBUTE
void *operator new[](size_t size) { OPERATOR_NEW_BODY; }
INTERCEPTOR_ATTRIBUTE
void *operator new(size_t size, std::nothrow_t const&)
{ OPERATOR_NEW_BODY; }
INTERCEPTOR_ATTRIBUTE
void *operator new[](size_t size, std::nothrow_t const&)
{ OPERATOR_NEW_BODY; }

INTERCEPTOR_ATTRIBUTE
void operator delete(void *ptr) throw() {
  OPERATOR_DELETE_BODY;
}
INTERCEPTOR_ATTRIBUTE
void operator delete[](void *ptr) throw() {
  OPERATOR_DELETE_BODY;
}
INTERCEPTOR_ATTRIBUTE
void operator delete(void *ptr, std::nothrow_t const&) {
  OPERATOR_DELETE_BODY;
}
INTERCEPTOR_ATTRIBUTE
void operator delete[](void *ptr, std::nothrow_t const&) {
  OPERATOR_DELETE_BODY;
}
