#ifndef CVER_INTERNAL_H
#define CVER_INTERNAL_H

#ifndef CVER_USE_PREINIT_ARRAY
# define CVER_USE_PREINIT_ARRAY
#endif

#ifndef CVER_USE_INTERNAL_ALLOCATOR
# define CVER_USE_INTERNAL_ALLOCATOR
#endif

// Intercept malloc()/free() for Firefox.
// Firefox replaces new/free operators with their own allocators (xmalloc),
// which eventually invokes malloc() and free().
// See FF_SOURCE/memory/mozalloc/mozalloc.h
#ifndef CVER_INTERCEPT_MALLOC
# define CVER_INTERCEPT_MALLOC
#endif

#define CVER_USE_STACK_RBTREE

// #define CVER_NDEBUG
#define CVER_MEM_ALIGNMENT 8
#define CVER_INLINE __attribute__((always_inline))

namespace __cver {
// Wrapper for TLS/TSD.
void CverTSDInit(void (*destructor)(void *tsd));
void *CverTSDGet();
void CverTSDSet(void *tsd);

} // namespace __cver

#endif // CVER_INTERNAL_H
