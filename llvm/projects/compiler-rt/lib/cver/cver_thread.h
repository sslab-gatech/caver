#ifndef CVER_THREAD_H
#define CVER_THREAD_H

#include "cver_allocator.h"
#include "cver_internal.h"
#include "cver_stats.h"
#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_libc.h"
#include "sanitizer_common/sanitizer_thread_registry.h"

#ifdef CVER_USE_STACK_RBTREE
#include "cver_rbtree.h"
#endif

namespace __cver {

const u32 kInvalidTid = 0xffffff;  // Must fit into 24 bits.
const u32 kMaxNumberOfThreads = (1 << 22);  // 4M

class CverThread;

#ifdef CVER_USE_STACK_MAP
#define STACK_MAP_SIZE 65537
typedef uptr addr_ptr;
typedef uptr tt_ptr;

struct StackMapBucket {
  addr_ptr Addr;
  tt_ptr TypeTable;
};
#endif

// These objects are created for every thread and are never deleted,
// so we can find them by tid even if the thread is long dead.
class CverThreadContext : public ThreadContextBase {
 public:
  explicit CverThreadContext(int tid)
      : ThreadContextBase(tid),
        announced(false),
        destructor_iterations(kPthreadDestructorIterations),
        stack_id(0),
        thread(0) {}

  bool announced;
  u8 destructor_iterations;
  u32 stack_id;
  CverThread *thread;
  void OnCreated(void *arg);
  void OnFinished();
  
};

// CverThreadContext objects are never freed, so we need many of them.
// COMPILER_CHECK(sizeof(CverThreadContext) <= 256);

// CverThread are stored in TSD and destroyed when the thread dies.
class CverThread {
 public:
  static CverThread *Create(thread_callback_t start_routine, void *arg);
  static void TSDDtor(void *tsd);
  void Destroy();

  void Init();  // Should be called from the thread itself.
  thread_return_t ThreadStart(uptr os_id);

  uptr stack_top() { return stack_top_; }
  uptr stack_bottom() { return stack_bottom_; }
  uptr stack_size() { return stack_size_; }
  uptr tls_begin() { return tls_begin_; }
  uptr tls_end() { return tls_end_; }
  u32 tid() { return context_->tid; }
  CverThreadContext *context() { return context_; }
  void set_context(CverThreadContext *context) { context_ = context; }

  // const char *GetFrameNameByAddr(uptr addr, uptr *offset, uptr *frame_pc);

  bool AddrIsInStack(uptr addr) {
    return addr >= stack_bottom_ && addr < stack_top_;
  }

  // True is this thread is currently unwinding stack (i.e. collecting a stack
  // trace). Used to prevent deadlocks on platforms where libc unwinder calls
  // malloc internally. See PR17116 for more details.
  bool isUnwinding() const { return unwinding_; }
  void setUnwinding(bool b) { unwinding_ = b; }

  CverThreadLocalMallocStorage &malloc_storage() { return malloc_storage_; }
  CverStats &stats() { return stats_; }

#ifdef CVER_USE_STACK_MAP  
  StackMapBucket StackMap[STACK_MAP_SIZE];  
  StackMapBucket *GetStackMapBucket(addr_ptr Addr);
#endif

#ifdef CVER_USE_STACK_RBTREE
  rbtree rbtree_root;
#endif

 private:
  // NOTE: There is no CverThread constructor. It is allocated
  // via mmap() and *must* be valid in zero-initialized state.
  void SetThreadStackAndTls();

  CverThreadContext *context_;
  thread_callback_t start_routine_;
  void *arg_;
  uptr stack_top_;
  uptr stack_bottom_;
  // stack_size_ == stack_top_ - stack_bottom_;
  // It needs to be set in a async-signal-safe manner.
  uptr stack_size_;
  uptr tls_begin_;
  uptr tls_end_;

  CverThreadLocalMallocStorage malloc_storage_;
  CverStats stats_;
  bool unwinding_;
};

struct CreateThreadContextArgs {
  CverThread *thread;
  StackTrace *stack;
};

// Returns a single instance of registry.
ThreadRegistry &cverThreadRegistry();

// Must be called under ThreadRegistryLock.
CverThreadContext *GetThreadContextByTidLocked(u32 tid);

// Get the current thread. May return 0.
CverThread *GetCurrentThread();
void SetCurrentThread(CverThread *t);
u32 GetCurrentTidOrInvalid();

// Used to handle fork().
void EnsureMainThreadIDIsCorrect();

#ifdef CVER_USE_STACK_MAP
StackMapBucket *GetCurrentThreadStackMapBucket(addr_ptr Addr);
#endif

#ifdef CVER_USE_STACK_RBTREE
rbtree GetCurrentThreadRbtreeRoot();
rbtree GetCurrentThreadRbtreeRootWithThread(CverThread *thread);
#endif // CVER_USE_STACK_RBTREE

}  // namespace __cver

#endif  // CVER_THREAD_H
