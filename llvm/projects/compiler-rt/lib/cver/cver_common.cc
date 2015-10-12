#include "cver_internal.h"
#include "cver_common.h"
#include "cver_allocator.h"
#include "cver_report.h"
#include "cver_thread.h"
#include "cver_cache.h"
#include "cver_stats.h"

#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_flags.h"
#include "sanitizer_common/sanitizer_libc.h"

using namespace __cver;

rbtree cver_global_rbtree_root = 0;

namespace __cver {

// Compute the hash value without isSameLayout information.
// Assuming the original hash value is 64-bits.
#define GetHashValue(v) (v & 0xfffffffffffffffe)
#define IsSameLayout(v) (v & 0x1)

struct NewHookArgs {
  void *TypeTable;
};

struct CastHookArgs {
  SourceLocation Loc;
  void *TypeTable;
  uptr Hash;
};

// The layout should be matched with CodeGenTHTables::GenerateTypeHierarchy().
//
// ------------    <-- Base of THTable
// _ContainVector
// ...
// _HashVector
// ------------
struct _ContainElem {
  uptr offset;
  uptr size;
  uptr pTHTable;
};

struct _ContainVector {
  uptr numContainment;
  _ContainElem ContainElem[1];
  // ...
};

struct _BaseElem {
  uptr BaseHash;
  uptr BaseOffset;
};

struct _HashVector {
  uptr numBases;
  _BaseElem BaseElem[1];
  // At the end of Hashes, mangledName array is located.
};

static CVER_INLINE const char *getMangledNameFromHashVector(_HashVector *hashVec) {
  return (const char *)&hashVec->BaseElem[hashVec->numBases];
}

static CVER_INLINE  _HashVector *getHashVectorFromContainVector(_ContainVector *containVec) {
  return (_HashVector *)&containVec->ContainElem[containVec->numContainment];
}

static CVER_INLINE const char *getMangledNameFromContainVector(_ContainVector *containVec) {
  _HashVector *hashVec = getHashVectorFromContainVector(containVec);
  return getMangledNameFromHashVector(hashVec);
}

// If the TargetHash matches to any of a hash value in the hash table (including
// all containments), for now we say it is a good casting.
// TODO : Should check the offset of the pointer to see where it is actually
// pointing to

// numElements is zero for all containment cases as numElements only captures
// the dynamically determined array size.
static bool CheckCastValidity(uptr Pointer, uptr objBaseAddr, uptr TargetHash,
                              _ContainVector *containVec,
                              _HashVector *hashVec) {
  bool matched = false;

  CVER_DEBUG_STMT(flags()->no_cast_validity, {
      return true;
    });
  

  VERBOSE_PRINT("\t numContain:[%zu], numHashes:[%zu] for %s\n",
                containVec->numContainment,
                hashVec->numBases,
                getMangledNameFromHashVector(hashVec));

  if (!flags()->no_composition) {
    for (unsigned i=0; i<containVec->numContainment; i++) {
      _ContainElem *elem = (_ContainElem *)&containVec->ContainElem[i];

      VERBOSE_PRINT("\t\t C[%d] : [%zu][%zu] %p : %s\n",
                    i, elem->offset, elem->size, elem->pTHTable,
                    elem->pTHTable ? getMangledNameFromContainVector(
                      (_ContainVector*)elem->pTHTable) : 0);

      // TODO : Optimize using  bit vectors.
      if (Pointer >= objBaseAddr + elem->offset &&
          Pointer < objBaseAddr + elem->offset + elem->size &&
          elem->pTHTable) {
        _ContainVector *nestedContainVec = (_ContainVector *)elem->pTHTable;

        _HashVector *nestedHashVec =
          getHashVectorFromContainVector(nestedContainVec);

        // If the given pointer is in the range of containment,
        // let the corresponding pTHTable handle this case additionally.
        VERBOSE_PRINT("\t\t\t nested with %p %p\n", nestedContainVec,
                      nestedHashVec);
        matched = CheckCastValidity(Pointer, objBaseAddr + elem->offset,
                                    TargetHash, nestedContainVec,
                                    nestedHashVec);

        if (matched)
          return true; // Matched from the hash in the containment table.
      }
    }
  }

  for (unsigned i=0; i<hashVec->numBases; i++) {
    uptr hash = hashVec->BaseElem[i].BaseHash;
    uptr offset = hashVec->BaseElem[i].BaseOffset;

    VERBOSE_PRINT("\t\t H[%d] : [%zu] [%zu]\n", i, offset, hash);
    if (hash == 0) break;

    // if (GetHashValue(TargetHash) == GetHashValue(hash) &&
    //     offset == (objBaseAddr - Pointer)) {
    if (GetHashValue(TargetHash) == GetHashValue(hash)) {
      matched = true;
      break;
    }
  }

  if (!matched) {
    VERBOSE_PRINT("\t\t not matched!\n");
  }
  return matched;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void __cver_handle_new(NewHookArgs *Data, uptr Pointer, uptr numElements) {
  CVER_DEBUG_STMT(flags()->no_check, {
      return;
    });

  CVER_DEBUG_STMT(flags()->no_handle_new, {
      return;
    });

  // Make sure Cver runtime is initialized.
#ifndef CVER_USE_PREINIT_ARRAY
  InitCverIfNecessary();
#endif

  VERBOSE_PRINT(
    "%p : %s [%d]\n", Pointer,
    getMangledNameFromContainVector((_ContainVector*)Data->TypeTable),
    numElements);

  bool res = SetCverTypeTableAndNumElements(Pointer, Data->TypeTable, numElements);

  if (UNLIKELY(!res)) {
    CVER_DEBUG_STMT(flags()->verbose, {
        Printf("\t Failed to set TypeTable\n");
        GET_CALLER_PC_BP_SP;
        MaybePrintStackTrace(sp, pc, bp);
      });
    return;
  }

  CVER_DEBUG_STMT(flags()->new_stacktrace, {
    GET_CALLER_PC_BP_SP;
    MaybePrintStackTrace(sp, pc, bp);
    });

  CVER_DEBUG_STMT(flags()->stats, {
      CverStats &thread_stats = GetCurrentThreadStats();
      thread_stats.numHandleNew++;
    });
}

static CVER_INLINE CacheKey computeCacheKey(_ContainVector *vec, uptr hash) {
  return (uptr)vec ^ (hash << 32);
}

// return 0 : Bad casting, so ignore static_cast.
// return 1 : Good casting, so do static_cast. If we can't verify it's
// bad-casting, return 1 as well.
#define BAD_CAST_RET       0
#define GOOD_CAST_RET      1
#define UNKNOWN_CAST_RET   1

enum PointerLocation {LOC_UNKNOWN, LOC_DYNAMIC, LOC_STACK, LOC_GLOBAL};
  
extern "C" SANITIZER_INTERFACE_ATTRIBUTE
int __cver_handle_cast(CastHookArgs *Data, uptr BeforePtr, uptr AfterPtr) {
  CVER_DEBUG_STMT(flags()->no_check, {
      return UNKNOWN_CAST_RET;
    });

  CVER_DEBUG_STMT(flags()->no_handle_cast, {
      return UNKNOWN_CAST_RET;
    });

  // Make sure Cver runtime is initialized.
#ifndef CVER_USE_PREINIT_ARRAY
  InitCverIfNecessary();
#endif

  if (!BeforePtr || !AfterPtr)
    return UNKNOWN_CAST_RET;

  uptr userAllocBeg = 0;
  
  // Check if it is allocated in the heap.
  _ContainVector *containVec = 0;
  uptr numElements = 0;
  uptr userRequestedSize = 0;
  PointerLocation pointerLocation = LOC_UNKNOWN;

  /////////////////////////////////////////////////
  // STACK POINTERS
  CverThread *cverThread = GetCurrentThread();
  if (!containVec && cverThread && cverThread->AddrIsInStack(BeforePtr)) {
    if (!flags()->no_stack) {
#ifdef CVER_USE_STACK_MAP
      StackMapBucket *bucket = GetCurrentThreadStackMapBucket(BeforePtr);
      if (bucket->Addr == BeforePtr) {
        // Allocated in the stack.
        VERBOSE_PRINT("Located stack bucket %p for %p\n", bucket, BeforePtr);
        containVec = (_ContainVector *)bucket->TypeTable;
        if (containVec) {
          numElements = 0;
          userRequestedSize = 0;
          pointerLocation = LOC_STACK;
        }
      }
#endif //CVER_USE_STACK_MAP

#ifdef CVER_USE_STACK_RBTREE
      rbtree t = GetCurrentThreadRbtreeRootWithThread(cverThread);
      if (t) {
        // Allocated in the stack.      
        containVec = (_ContainVector *)rbtree_lookup_range(t, BeforePtr,
                                                           &userAllocBeg);
        if (containVec) {
          numElements = 0;
          userRequestedSize = 0;
          pointerLocation = LOC_STACK;
        }
      }
    }
#endif // CVER_USE_STACK_RBTREE
    
    // If the pointer points to the stack but we failed to locate the THTable,
    // there's no point to try more on dynamic or global.
    if (!containVec)
      return UNKNOWN_CAST_RET;
  }

  /////////////////////////////////////////////////
  // DYNAMIC POINTERS
  Metadata *m = 0;
  if (!containVec && PointerIsDynamic(BeforePtr)) {
    userAllocBeg = reinterpret_cast<uptr>(GetBlockBeginAndMetaData(
                                            BeforePtr, &m));
    
    if (userAllocBeg && m) {
      // Allocated in the heap
      VERBOSE_PRINT("Located metadata %p for %p\n", m, BeforePtr);
      containVec = (_ContainVector *)m->type_table;
      if (!containVec) // This is not the dynamic object we are tracing.
        return UNKNOWN_CAST_RET;
      numElements = m->num_elements;
      userRequestedSize = m->requested_size;
      pointerLocation = LOC_DYNAMIC;
    } else {
      // Early bail out.
      return UNKNOWN_CAST_RET;
    }
  }

  /////////////////////////////////////////////////
  // GLOBAL POINTERS
  if (!containVec && !flags()->no_global) {
    containVec = (_ContainVector *)rbtree_lookup_range(
      cver_global_rbtree_root, BeforePtr, &userAllocBeg);
    pointerLocation = LOC_GLOBAL;
  }

  if (UNLIKELY(!containVec)) {
    VERBOSE_PRINT("Failed to locate any for %p\n", BeforePtr);
    return UNKNOWN_CAST_RET;
  }

  CVER_DEBUG_STMT(flags()->verbose, {
      const char *name = 0;
      if (containVec)
        name = getMangledNameFromContainVector(containVec);

      if (name)
        Printf("\t Target hash: %zu with TypeTable %p (%s)\n", Data->Hash, containVec, name);
      else
        Printf("\t Target hash: %zu with TypeTable %p\n", Data->Hash, containVec);
    });

  // TODO : casting onto non-dynamic objects (i.e., stack objects).
  // Simply returning for now.
  if (!containVec) return UNKNOWN_CAST_RET;

  CVER_DEBUG_STMT(flags()->stats, {
    CverStats &thread_stats = GetCurrentThreadStats();
    thread_stats.casts++;
    switch(pointerLocation) {
    case LOC_STACK:
      thread_stats.stackCasts++;
      break;
    case LOC_DYNAMIC:
      thread_stats.dynCasts++;
      break;
    case LOC_GLOBAL:
      thread_stats.globalCasts++;
      break;
    default:
      thread_stats.unknownCasts++;
      break;
    }
    });

  // Check if it is cached results.
  CacheKey Key;
  CacheKey *EvictSecondCacheBucket;
  if (LIKELY(!flags()->no_cache)) {
    Key = computeCacheKey(containVec, Data->Hash);
    if (IsInCache(Key, &EvictSecondCacheBucket)) {
      // Checked results found in cache.
      VERBOSE_PRINT("\t Cache matched\n");

      CVER_DEBUG_STMT(flags()->stats, {
        CverStats &thread_stats = GetCurrentThreadStats();
        thread_stats.cache_hits++;
        });
      return GOOD_CAST_RET;
    }
  }

  _HashVector *hashVec = getHashVectorFromContainVector(containVec);
  const char *allocTypeName = getMangledNameFromHashVector(hashVec);

  VERBOSE_PRINT("\t Allocated as %s\n", allocTypeName);

  if (numElements > 0) {
    CHECK_EQ(userRequestedSize%numElements, 0);
    CHECK(BeforePtr >= userAllocBeg);
    uptr elementSize = userRequestedSize / numElements;
    userAllocBeg = BeforePtr - (BeforePtr-userAllocBeg) % elementSize;
    VERBOSE_PRINT("\t\t userBeg adjusted: %p with elementSize %d\n",
                  userAllocBeg, elementSize);
  }
  bool matched = CheckCastValidity(AfterPtr, userAllocBeg, Data->Hash,
                                   containVec, hashVec);

  if (matched) {
    // Update Cache.
    if (LIKELY(!flags()->no_cache))
      UpdateCache(Key, EvictSecondCacheBucket);
    return GOOD_CAST_RET;
  }

  _ContainVector *targetContainVec = (_ContainVector *)Data->TypeTable;
  _HashVector *targetHashVec = getHashVectorFromContainVector(targetContainVec);

  // Check if the target class has any base classes with the same layout. If it
  // is, check the casting validity onto those same layout classes as well.
  if (LIKELY(!flags()->empty_inherit)) {
    // Trying to match empty inherit cases too.
    for (unsigned i=0; i<targetHashVec->numBases; i++) {
      // uptr hash = targetHashVec->Hashes[i];
      uptr hash = targetHashVec->BaseElem[i].BaseHash;

      if (IsSameLayout(hash)) {
        VERBOSE_PRINT("\t\t Found the same layout %zu\n", hash);
        // Same layout cases.
        bool targetMatched =
          CheckCastValidity(BeforePtr, userAllocBeg, hash, containVec, hashVec);

        if (targetMatched) {
          VERBOSE_PRINT("\t\t Matched with the same layout %zu\n", hash);
          // Update Cache.
          if (LIKELY(!flags()->no_cache))
            UpdateCache(Key, EvictSecondCacheBucket);
          return GOOD_CAST_RET;
        }
      }
    }
  }

  const char *dstTypeName = getMangledNameFromContainVector(targetContainVec);

  if (flags()->no_report)
    return UNKNOWN_CAST_RET;

  // Do not report if this casting is in the runtime suppression list.
  if (MatchSuppression(allocTypeName, SuppressionCastSrcType)
      || MatchSuppression(dstTypeName, SuppressionCastDstType)
      || MatchSuppression(Data->Loc.getFilename(), SuppressionCastFilename)) {
    VERBOSE_PRINT("Suppressed a bad-casting from %s to %s in %s\n",
                  allocTypeName, dstTypeName, Data->Loc.getFilename());
    // This is little awkward, but let static_cast do its job if it's in the
    // suppression list.
    return UNKNOWN_CAST_RET;
  }

  // Report a bad-casting error.
  ReportBadCasting(Data->Loc, dstTypeName, allocTypeName, BeforePtr);

  // Enforcing zero values on bad-casting is activated with runtime nullify
  // flags.
  if (flags()->nullify)
    return BAD_CAST_RET;
  return UNKNOWN_CAST_RET;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void __cver_handle_stack_enter(NewHookArgs *Data, uptr Pointer,
                               uptr numElements, uptr AllocSize) {
  CVER_DEBUG_STMT(flags()->no_check, {
      return;
    });

  // Make sure Cver runtime is initialized.
#ifndef CVER_USE_PREINIT_ARRAY
  InitCverIfNecessary();
#endif

  VERBOSE_PRINT(
    "%p : %zu %p %s [%d]\n", Pointer, AllocSize, Data->TypeTable,
    getMangledNameFromContainVector((_ContainVector*)Data->TypeTable),
    numElements);

  CVER_DEBUG_STMT(flags()->stats, {
      CverStats &thread_stats = GetCurrentThreadStats();
      thread_stats.stackObjAlloc++;
      thread_stats.stackObjCurrent++;
      if (thread_stats.stackObjCurrent > thread_stats.stackObjPeak)
        thread_stats.stackObjPeak = thread_stats.stackObjCurrent;
    });

#ifdef CVER_USE_STACK_MAP
  // Update the stack map bucket with given (Pointer, TypeTable).
  StackMapBucket *bucket = GetCurrentThreadStackMapBucket(Pointer);
  bucket->Addr = Pointer;
  bucket->TypeTable = (uptr)Data->TypeTable;
#endif // CVER_USE_STACK_MAP

#ifdef CVER_USE_STACK_RBTREE
  rbtree t = GetCurrentThreadRbtreeRoot();
  if (!t)
    return;
  KEY k;
  k.addr = Pointer;
  k.size = AllocSize;
  rbtree_insert(t, k, Data->TypeTable);
#endif // CVER_USE_STACK_RBTREE
  return;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void __cver_handle_stack_exit(NewHookArgs *Data, uptr Pointer) {
  CVER_DEBUG_STMT(flags()->no_check, {
      return;
    });

#ifndef CVER_USE_PREINIT_ARRAY
  InitCverIfNecessary();
#endif

  VERBOSE_PRINT(
    "%p : %p %s\n", Pointer, Data->TypeTable,
    getMangledNameFromContainVector((_ContainVector*)Data->TypeTable));

  CVER_DEBUG_STMT(flags()->stats, {
      CverStats &thread_stats = GetCurrentThreadStats();
      thread_stats.stackObjFree++;
      thread_stats.stackObjCurrent--; // assert if stackObjCurrent > 0
    });

  // Remove the stack map bucket with given (Pointer, TypeTable).
#ifdef CVER_USE_STACK_MAP
  StackMapBucket *bucket = GetCurrentThreadStackMapBucket(Pointer);
  if (bucket->Addr == Pointer && bucket->TypeTable == (uptr)Data->TypeTable) {
    bucket->Addr = 0;
  } else {
    VERBOSE_PRINT("\t Failed to delete stack map bucket for %p\n", Pointer);
    VERBOSE_PRINT("\t\t bucket : %p, Addr : %p, TypeTable : %p\n",
                  bucket, bucket->Addr, bucket->TypeTable);
  }
#endif // CVER_USE_STACK_MAP

#ifdef CVER_USE_STACK_RBTREE
  rbtree t = GetCurrentThreadRbtreeRoot();
  if (!t)
    return;
  KEY k;
  k.addr = Pointer;
  k.size = 0; // Doesn't matter.
  rbtree_delete(t, k);
#endif // CVER_USE_STACK_RBTREE
  return;
}

extern "C" SANITIZER_INTERFACE_ATTRIBUTE
void __cver_handle_global_var(uptr Pointer, uptr TypeTable, uptr AllocSize) {
  
  CVER_DEBUG_STMT(flags()->no_check, {
      return;
    });

  CVER_DEBUG_STMT(flags()->no_global, {
      return;
    });

  // Make sure Cver runtime is initialized.
#ifndef CVER_USE_PREINIT_ARRAY
  InitCverIfNecessary();
#endif

  if (!Pointer)
    return;
  
  VERBOSE_PRINT( "%p : %p %zu for %s\n", Pointer, TypeTable, AllocSize,
    getMangledNameFromContainVector((_ContainVector*)TypeTable));

  KEY k;
  k.addr = Pointer;
  k.size = AllocSize;
  rbtree_insert(cver_global_rbtree_root, k, (void*)TypeTable);
  return;
}

} // namespace __cver
