#ifndef CVER_ALLOCATOR_H
#define CVER_ALLOCATOR_H

#include "cver_internal.h"
#include "sanitizer_common/sanitizer_common.h"
// #include "sanitizer_common/sanitizer_allocator.h"
// #include "sanitizer_common/sanitizer_stacktrace.h"
// #include "sanitizer_common/sanitizer_list.h"

namespace __cver {

struct Metadata {
  uptr requested_size;
  uptr type_table;
  uptr num_elements;
};

void InitializeAllocator();

struct CverThreadLocalMallocStorage {
  uptr quarantine_cache[16];
  ALIGNED(8) uptr allocator_cache[96 * (512 * 8 + 16)];  // Opaque.
  void CommitBack();
private:
  // These objects are allocated via mmap() and are zero-initialized.
  CverThreadLocalMallocStorage() {}
};

void PrintInternalAllocatorStats();

void *GetAllocBegin(uptr p);
void *GetAllocUserBegin(uptr p);
// CverChunk *GetCverChunkByAddr(uptr p);


void *CverReallocate(StackTrace *stack, void *old_p, uptr new_size,
                     uptr alignment, bool zeroise);
void CverDeallocate(StackTrace *stack, void *p);

bool PointerIsDynamic(uptr p);
void *GetBlockBeginAndMetaData(uptr p, Metadata **ppMetaData);
Metadata *GetCverMetaDataFromBegin(void *beg);
Metadata *GetCverMetaData(void *p);
bool SetCverTypeTableAndNumElements(uptr p, void *TypeTable, uptr numElements);
void *GetCverTypeTable(uptr p);
uptr GetCverArrayNumElements(uptr p);
uptr AllocationSize(uptr p);

uptr __sanitizer_get_allocated_size(const void *p);

} // namespace __cver

#endif // CVER_ALLOCATOR_H
