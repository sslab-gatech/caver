#include "cver_internal.h"
#include "cver_common.h"
#include "cver_stats.h"
#include "cver_thread.h"
#include "cver_allocator.h"

#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_mutex.h"

namespace __cver {

CverStats::CverStats() {
  Clear();
}

void CverStats::Clear() {
  internal_memset(this, 0, sizeof(CverStats));  
}

void CverStats::Print() {
  Printf("Stats: %zuM malloced by %zu calls\n", malloced>>20, mallocs);
  Printf("Stats: %zuM realloced by %zu calls\n", realloced>>20, reallocs);
  Printf("Stats: %zuM freed by %zu calls\n", freed>>20, frees);
  Printf("Stats: %zuM (%zuM-%zuM) mmaped; %zu maps, %zu unmaps\n",
         (mmaped-munmaped)>>20, mmaped>>20, munmaped>>20, mmaps, munmaps);
  Printf("\n");
  
  Printf("Stats: %zu stackObjAlloc\n", stackObjAlloc);
  Printf("Stats: %zu stackObjFree\n", stackObjFree);
  Printf("Stats: %zu stackObjCurrent\n",stackObjCurrent);
  Printf("Stats: %zu stackObjPeak\n", stackObjPeak);
  Printf("\n");

  Printf("Stats: %zu numNew\n", numNew);
  Printf("Stats: %zu numDelete\n", numDelete);
  Printf("Stats: %zu numNewCurrent\n", numNewCurrent);
  Printf("Stats: %zu numNewPeak\n", numNewPeak);
  Printf("\n");  

  Printf("Stats: %zu numMalloc\n", numMalloc);
  Printf("Stats: %zu numFree\n", numFree);
  Printf("Stats: %zu numMallocCurrent\n", numMallocCurrent);
  Printf("Stats: %zu numMallocPeak\n", numMallocPeak);
  Printf("\n");    
  
  Printf("Stats: %zu numHandleNew\n", numHandleNew);
  Printf("\n");

  Printf("Stats: %zu stackCasts\n", stackCasts);
  Printf("Stats: %zu dynCasts\n", dynCasts);
  Printf("Stats: %zu globalCasts\n", globalCasts);
  Printf("Stats: %zu unknownCasts\n", unknownCasts);  
  Printf("Stats: %zu casts\n", casts);
  Printf("Stats: %zu cache hit / %zu cache miss\n",
         cache_hits, casts-cache_hits);
}

void CverStats::MergeFrom(const CverStats *stats) {
  uptr *dst_ptr = reinterpret_cast<uptr*>(this);
  const uptr *src_ptr = reinterpret_cast<const uptr*>(stats);
  uptr num_fields = sizeof(*this) / sizeof(uptr);
  for (uptr i = 0; i < num_fields; i++)
    dst_ptr[i] += src_ptr[i];
}

static BlockingMutex print_lock(LINKER_INITIALIZED);

static CverStats unknown_thread_stats(LINKER_INITIALIZED);
static CverStats dead_threads_stats(LINKER_INITIALIZED);
static BlockingMutex dead_threads_stats_lock(LINKER_INITIALIZED);

static void MergeThreadStats(ThreadContextBase *tctx_base, void *arg) {
  CverStats *accumulated_stats = reinterpret_cast<CverStats*>(arg);
  CverThreadContext *tctx = static_cast<CverThreadContext*>(tctx_base);
  if (CverThread *t = tctx->thread)
    accumulated_stats->MergeFrom(&t->stats());
}

static void GetAccumulatedStats(CverStats *stats) {
  stats->Clear();
  {
    ThreadRegistryLock l(&cverThreadRegistry());
    cverThreadRegistry()
        .RunCallbackForEachThreadLocked(MergeThreadStats, stats);
  }
  stats->MergeFrom(&unknown_thread_stats);
  {
    BlockingMutexLock lock(&dead_threads_stats_lock);
    stats->MergeFrom(&dead_threads_stats);
  }
}

void FlushToDeadThreadStats(CverStats *stats) {
   BlockingMutexLock lock(&dead_threads_stats_lock);
   dead_threads_stats.MergeFrom(stats);
   stats->Clear();
}

CverStats &GetCurrentThreadStats() {
  CverThread *t = GetCurrentThread();
  return (t) ? t->stats() : unknown_thread_stats;
}

void PrintAccumulatedStats() {
  CverStats stats;
  GetAccumulatedStats(&stats);
  // Use lock to keep reports from mixing up.
  BlockingMutexLock lock(&print_lock);
  
  stats.Print();
  PrintInternalAllocatorStats();
}

} // namespace __cver
