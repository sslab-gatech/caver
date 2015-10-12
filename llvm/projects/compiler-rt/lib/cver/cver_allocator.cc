#include "sanitizer_common/sanitizer_allocator.h"
#include "sanitizer_common/sanitizer_allocator_interface.h"
#include "sanitizer_common/sanitizer_stackdepot.h"

#include "cver_internal.h"
#include "cver_allocator.h"
#include "cver_thread.h"
#include "cver_init.h"
#include "cver_flags.h"

#ifndef CVER_USE_INTERNAL_ALLOCATOR
#include <malloc.h>
#endif

#include <stdlib.h>

extern bool cver_initialized;

namespace __cver {

struct CverMapUnmapCallback {
  void OnMap(uptr p, uptr size) const {
    if (UNLIKELY(flags()->stats)) {
      CverStats &thread_stats = GetCurrentThreadStats();
      thread_stats.mmaps++;
      thread_stats.mmaped += size;
    }
  }
  void OnUnmap(uptr p, uptr size) const {
    if (UNLIKELY(flags()->stats)) {
      CverStats &thread_stats = GetCurrentThreadStats();
      thread_stats.munmaps++;
      thread_stats.munmaped += size;
    }
  }
};

static const uptr kAllocatorSpace = 0x600000000000ULL;
static const uptr kAllocatorSize   = 0x80000000000;  // 8T.
static const uptr kMetadataSize  = sizeof(Metadata);
static const uptr kMaxAllowedMallocSize = 8UL << 30;

typedef SizeClassAllocator64<kAllocatorSpace, kAllocatorSize, kMetadataSize,
                             DefaultSizeClassMap,
                             CverMapUnmapCallback> PrimaryAllocator;
typedef SizeClassAllocatorLocalCache<PrimaryAllocator> AllocatorCache;
typedef LargeMmapAllocator<CverMapUnmapCallback> SecondaryAllocator;
typedef CombinedAllocator<PrimaryAllocator, AllocatorCache,
                          SecondaryAllocator> Allocator;

static Allocator allocator;
static AllocatorCache fallback_allocator_cache;
static SpinMutex fallback_mutex;

AllocatorCache *GetAllocatorCache(CverThreadLocalMallocStorage *cs) {
  CHECK(cs);
  CHECK_LE(sizeof(AllocatorCache), sizeof(cs->allocator_cache));
  return reinterpret_cast<AllocatorCache *>(cs->allocator_cache);
}

void CverThreadLocalMallocStorage::CommitBack() {
  allocator.SwallowCache(GetAllocatorCache(this));
}

void InitializeAllocator() {
  allocator.Init();
}

static void *CverAllocate(StackTrace *stack, uptr size, uptr alignment,
                          bool zeroise) {
  if (size > kMaxAllowedMallocSize) {
    Report("WARNING: MemorySanitizer failed to allocate %p bytes\n",
           (void *)size);
    return AllocatorReturnNull();
  }
  CverThread *t = GetCurrentThread();
  void *allocated;
  if (t) {
    AllocatorCache *cache = GetAllocatorCache(&t->malloc_storage());
    allocated = allocator.Allocate(cache, size, alignment, false);
  } else {
    SpinMutexLock l(&fallback_mutex);
    AllocatorCache *cache = &fallback_allocator_cache;
    allocated = allocator.Allocate(cache, size, alignment, false);
  }
  Metadata *meta =
      reinterpret_cast<Metadata *>(allocator.GetMetaData(allocated));
  meta->requested_size = size;
  meta->type_table = 0;
  meta->num_elements = 0;

  if (zeroise)
    internal_memset(allocated, 0, size);

  return allocated;
}

void CverDeallocate(StackTrace *stack, void *p) {
  CHECK(p);
  const void *beg = allocator.GetBlockBegin(p);
  if (beg != p) return;
  Metadata *meta = reinterpret_cast<Metadata *>(allocator.GetMetaData(p));
  meta->requested_size = 0;
  meta->type_table = 0;
  meta->num_elements = 0;

  CverThread *t = GetCurrentThread();
  if (t) {
    AllocatorCache *cache = GetAllocatorCache(&t->malloc_storage());
    allocator.Deallocate(cache, p);
  } else {
    SpinMutexLock l(&fallback_mutex);
    AllocatorCache *cache = &fallback_allocator_cache;
    allocator.Deallocate(cache, p);
  }
}

void *CverReallocate(StackTrace *stack, void *old_p, uptr new_size,
                     uptr alignment, bool zeroise) {
  if (!cver_initialized)
    InitCverIfNecessary();

  if (!old_p)
    return CverAllocate(stack, new_size, alignment, zeroise);
  if (!new_size) {
    CverDeallocate(stack, old_p);
    return 0;
  }
  const void *old_beg = allocator.GetBlockBegin(old_p);
  if (old_beg != old_p) return 0;

  Metadata *meta = reinterpret_cast<Metadata*>(allocator.GetMetaData(old_p));
  uptr old_size = meta->requested_size;
  uptr actually_allocated_size = allocator.GetActuallyAllocatedSize(old_p);
  if (new_size <= actually_allocated_size) {
    meta->requested_size = new_size;
    return old_p;
  }
  uptr memcpy_size = Min(new_size, old_size);
  void *new_p = CverAllocate(stack, new_size, alignment, zeroise);
  // Printf("realloc: old_size %zd new_size %zd\n", old_size, new_size);
  if (new_p) {
    internal_memcpy(new_p, old_p, memcpy_size);
    CverDeallocate(stack, old_p);
  }
  return new_p;
}

///////////////////////
bool PointerIsDynamic(uptr p) {
  return allocator.FromPrimary(reinterpret_cast<void *>(p));
}

void *GetAllocBegin(uptr p) {
  return allocator.GetBlockBegin(reinterpret_cast<void *>(p));
}

CVER_INLINE void *GetBlockBeginAndMetaData(uptr p, Metadata **ppMetaData) {
  return allocator.primary_.GetBlockBeginAndMetaData(
    reinterpret_cast<void *>(p), reinterpret_cast<void**>(ppMetaData));
}

void *GetAllocUserBegin(uptr p) {
  return allocator.GetBlockBeginPrimary(reinterpret_cast<void *>(p));
}

Metadata *GetCverMetaDataFromBegin(void *beg) {
  return (Metadata *)allocator.GetMetaDataPrimary(beg);
}

Metadata *GetCverMetaData(void *p) {
  if (p == 0) return 0;
  const void *beg = allocator.GetBlockBeginPrimary(p);
  if (beg == 0) return 0;
  return (Metadata *)allocator.GetMetaDataPrimary(beg);
}

bool SetCverTypeTableAndNumElements(uptr p, void *TypeTable, uptr numElements) {
  Metadata *m = GetCverMetaData((void *)p);

  if (!m) return false;
  m->type_table = (uptr)TypeTable;
  m->num_elements = (uptr)numElements;
  return true;
}

void *GetCverTypeTable(uptr p) {
  Metadata *m = GetCverMetaData((void *)p);
  if (!m) return 0;
  return (void*)m->type_table;
}

uptr GetCverArrayNumElements(uptr p) {
  Metadata *m = GetCverMetaData((void *)p);
  if (!m) return 0;
  return (uptr)m->num_elements;
}

uptr AllocationSize(uptr p) {
  Metadata *m = GetCverMetaData((void *)p);
  if (!m) return 0;
  return m->requested_size;
}

void PrintInternalAllocatorStats() {
  allocator.PrintStats();
}

uptr __sanitizer_get_allocated_size(const void *p) {
  return AllocationSize((uptr)p);
}

} // namespace __cver
