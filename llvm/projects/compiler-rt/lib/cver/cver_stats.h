#ifndef CVER_STATS_H
#define CVER_STATS_H

namespace __cver {

struct CverStats {
  uptr mallocs;
  uptr malloced;
  uptr frees;
  uptr freed;
  uptr reallocs;
  uptr realloced;

  uptr stackObjAlloc;
  uptr stackObjFree;
  uptr stackObjCurrent;  
  uptr stackObjPeak;

  uptr numMalloc;
  uptr numFree;
  uptr numMallocCurrent;
  uptr numMallocPeak;  

  uptr numNew;
  uptr numDelete;
  uptr numNewCurrent;
  uptr numNewPeak;

  uptr numHandleNew;  
  
  uptr casts;

  uptr unknownCasts;
  uptr stackCasts;
  uptr globalCasts;
  uptr dynCasts;
  
  uptr cache_hits;
  
  uptr mmaps;
  uptr mmaped;
  uptr munmaps;
  uptr munmaped;
  
  // Ctor for global CverStats (accumulated stats for dead threads).
  explicit CverStats(LinkerInitialized) { }
  // Creates empty stats.
  CverStats();

  void Print();  // Prints formatted stats to stderr.
  void Clear();
  void MergeFrom(const CverStats *stats);
};

CverStats &GetCurrentThreadStats();
void FlushToDeadThreadStats(CverStats *stats);
void PrintAccumulatedStats();

} // namespace __cver


#endif // CVER_STATS_H
