#ifndef CVER_CACHE_H
#define CVER_CACHE_H

#include "cver_internal.h"
#include "sanitizer_common/sanitizer_common.h"

namespace __cver {

typedef uptr CacheKey;

static const unsigned FirstCacheSize = 2048;
static CacheKey FirstCache[FirstCacheSize];

CVER_INLINE CacheKey *getSecondCacheBucket(CacheKey V) {
  static const unsigned SecondCacheSize = 65537;
  static CacheKey SecondCache[SecondCacheSize];

  unsigned First = (V & 65535) ^ 1;
  unsigned Probe = First;
  for (int Tries = 5; Tries; --Tries) {
    if (!SecondCache[Probe] || SecondCache[Probe] == V)
      return &SecondCache[Probe];
    Probe += ((V >> 16) & 65535) + 1;
    if (Probe >= SecondCacheSize)
      Probe -= SecondCacheSize;
  }
  return &SecondCache[First];
}

CVER_INLINE bool IsInCache(CacheKey Key, CacheKey **pEvictSecondCacheBucket) {
  // Check first cache.
  if (FirstCache[Key % FirstCacheSize] == Key)
    return true;

  // Check second cache.
  CacheKey *Bucket = getSecondCacheBucket(Key);
  if (*Bucket == Key) {
    // Update first cache.
    FirstCache[Key % FirstCacheSize] = Key;
    return true;
  }
  *pEvictSecondCacheBucket = Bucket;
  return false;
}

// FIXME : Should not re-do getSecondCacheBucket
CVER_INLINE void UpdateCache(CacheKey Key, CacheKey *EvictSecondCacheBucket) {
  // Update the first cache.
  FirstCache[Key % FirstCacheSize] = Key;

  // Update the second cache.
  *EvictSecondCacheBucket = Key;
}

} // namespace __cver

#endif // CVER_CACHE_H
