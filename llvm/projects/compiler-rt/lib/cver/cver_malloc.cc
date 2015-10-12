#include "cver_internal.h"
#include "cver_common.h"
#include "cver_allocator.h"
#include "cver_init.h"
#include "cver_flags.h"
#include "cver_stats.h"
#include "cver_thread.h"

#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_interception.h"
#include "sanitizer_common/sanitizer_allocator.h"
#include "sanitizer_common/sanitizer_tls_get_addr.h"

// TODO: Only tracing malloc and free for now (for Firefox).
using namespace __cver;

extern bool cver_initialized;

// Do not trace stack for now.
#define GET_MALLOC_STACK_TRACE
#define GET_FREE_STACK_TRACE

#define STAT_MALLOC                                                   \
  CVER_DEBUG_STMT(flags()->stats, {                                   \
      CverStats &thread_stats = GetCurrentThreadStats();              \
      thread_stats.numMalloc++;                                       \
      thread_stats.numMallocCurrent++;                                \
      if (thread_stats.numMallocCurrent > thread_stats.numMallocPeak) \
        thread_stats.numMallocPeak = thread_stats.numMallocCurrent;   \
    });

#define STAT_FREE                                                  \
  CVER_DEBUG_STMT(flags()->stats, {                                \
      CverStats &thread_stats = GetCurrentThreadStats();           \
      thread_stats.numFree++;                                      \
      if (thread_stats.numMallocCurrent > 0)                       \
        thread_stats.numMallocCurrent--;                           \
    });

#ifdef CVER_INTERCEPT_MALLOC

INTERCEPTOR(int, posix_memalign, void **memptr, SIZE_T alignment, SIZE_T size) {
  GET_MALLOC_STACK_TRACE;
  CHECK_EQ(alignment & (alignment - 1), 0);
  CHECK_NE(memptr, 0);
  *memptr = CverReallocate(0, 0, size, alignment, false);
  CHECK_NE(*memptr, 0);
  return 0;
}

INTERCEPTOR(void *, memalign, SIZE_T boundary, SIZE_T size) {
  GET_MALLOC_STACK_TRACE;
  CHECK_EQ(boundary & (boundary - 1), 0);
  void *ptr = CverReallocate(0, 0, size, boundary, false);
  return ptr;
}

INTERCEPTOR(void *, aligned_alloc, SIZE_T boundary, SIZE_T size) {
  GET_MALLOC_STACK_TRACE;
  CHECK_EQ(boundary & (boundary - 1), 0);
  void *ptr = CverReallocate(0, 0, size, boundary, false);
  return ptr;
}

INTERCEPTOR(void *, __libc_memalign, SIZE_T boundary, SIZE_T size) {
  GET_MALLOC_STACK_TRACE;
  CHECK_EQ(boundary & (boundary - 1), 0);
  void *ptr = CverReallocate(0, 0, size, boundary, false);
  DTLS_on_libc_memalign(ptr, size * boundary);
  return ptr;
}

INTERCEPTOR(void *, valloc, SIZE_T size) {
  GET_MALLOC_STACK_TRACE;
  void *ptr = CverReallocate(0, 0, size, GetPageSizeCached(), false);
  return ptr;
}

INTERCEPTOR(void *, pvalloc, SIZE_T size) {
  GET_MALLOC_STACK_TRACE;
  uptr PageSize = GetPageSizeCached();
  size = RoundUpTo(size, PageSize);
  if (size == 0) {
    // pvalloc(0) should allocate one page.
    size = PageSize;
  }
  void *ptr = CverReallocate(0, 0, size, PageSize, false);
  return ptr;
}

///////////////////////////

INTERCEPTOR(void, free, void *ptr) {
  GET_FREE_STACK_TRACE;
  STAT_FREE;
  if (ptr == 0) return;
  CverDeallocate(0, ptr);
}

INTERCEPTOR(void, cfree, void *ptr) {
  GET_FREE_STACK_TRACE;
  STAT_FREE;
  if (ptr == 0) return;
  CverDeallocate(0, ptr);
}

INTERCEPTOR(uptr, malloc_usable_size, void *ptr) {
  // TODO
  return __sanitizer_get_allocated_size(ptr);
}

INTERCEPTOR(int, mallopt, int cmd, int value) {
  return -1;
}

INTERCEPTOR(void, malloc_stats, void) {
  // FIXME: implement, but don't call REAL(malloc_stats)!
}

INTERCEPTOR(void*, calloc, uptr nmemb, uptr size) {
  if (CallocShouldReturnNullDueToOverflow(size, nmemb))
    return AllocatorReturnNull();
  
  GET_MALLOC_STACK_TRACE;
  STAT_MALLOC;
  
  if (!cver_initialized) {
    // Hack: dlsym calls calloc before REAL(calloc) is retrieved from dlsym.
    const SIZE_T kCallocPoolSize = 1024;
    static uptr calloc_memory_for_dlsym[kCallocPoolSize];
    static SIZE_T allocated;
    SIZE_T size_in_words = ((nmemb * size) + kWordSize - 1) / kWordSize;
    void *mem = (void*)&calloc_memory_for_dlsym[allocated];
    allocated += size_in_words;
    CHECK(allocated < kCallocPoolSize);
    return mem;
  }
  return CverReallocate(0, 0, nmemb * size, sizeof(u64), true);
}

INTERCEPTOR(void*, realloc, void *ptr, uptr size) {
  GET_MALLOC_STACK_TRACE;
  return CverReallocate(0, ptr, size, sizeof(u64), false);
}

INTERCEPTOR(void*, malloc, uptr size) {
  GET_MALLOC_STACK_TRACE;
  STAT_MALLOC;
  return CverReallocate(0, 0, size, sizeof(u64), false);
}

#endif // CVER_INTERCEPT_MALLOC
