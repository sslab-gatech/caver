#include "cver_internal.h"
#include "cver_allocator.h"
#include "cver_thread.h"
#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_placement_new.h"
#include "sanitizer_common/sanitizer_stackdepot.h"
#include "sanitizer_common/sanitizer_tls_get_addr.h"

namespace __cver {

// CverThreadContext implementation.

void CverThreadContext::OnCreated(void *arg) {
  CreateThreadContextArgs *args = static_cast<CreateThreadContextArgs*>(arg);
  // if (args->stack)
  //   stack_id = StackDepotPut(args->stack->trace, args->stack->size);
  thread = args->thread;
  thread->set_context(this);
}

void CverThreadContext::OnFinished() {
  // Drop the link to the CverThread object.
  thread = 0;
}

// MIPS requires aligned address
static ALIGNED(16) char thread_registry_placeholder[sizeof(ThreadRegistry)];
static ThreadRegistry *cver_thread_registry;

static BlockingMutex mu_for_thread_context(LINKER_INITIALIZED);
static LowLevelAllocator allocator_for_thread_context;

static ThreadContextBase *GetCverThreadContext(u32 tid) {
  BlockingMutexLock lock(&mu_for_thread_context);
  return new(allocator_for_thread_context) CverThreadContext(tid);
}

ThreadRegistry &cverThreadRegistry() {
  static bool initialized;
  // Don't worry about thread_safety - this should be called when there is
  // a single thread.
  if (!initialized) {
    // Never reuse Cver threads: we store pointer to CverThreadContext
    // in TSD and can't reliably tell when no more TSD destructors will
    // be called. It would be wrong to reuse CverThreadContext for another
    // thread before all TSD destructors will be called for it.
    cver_thread_registry = new(thread_registry_placeholder) ThreadRegistry(
        GetCverThreadContext, kMaxNumberOfThreads, kMaxNumberOfThreads);
    initialized = true;
  }
  return *cver_thread_registry;
}

CverThreadContext *GetThreadContextByTidLocked(u32 tid) {
  return static_cast<CverThreadContext *>(
      cverThreadRegistry().GetThreadLocked(tid));
}

// CverThread implementation.

CverThread *CverThread::Create(thread_callback_t start_routine,
                               void *arg) {
  uptr PageSize = GetPageSizeCached();
  uptr size = RoundUpTo(sizeof(CverThread), PageSize);
  CverThread *thread = (CverThread*)MmapOrDie(size, __func__);
  thread->start_routine_ = start_routine;
  thread->arg_ = arg;

  return thread;
}

void CverThread::TSDDtor(void *tsd) {
  CverThreadContext *context = (CverThreadContext*)tsd;
  VReport(1, "T%d TSDDtor\n", context->tid);
  if (context->thread)
    context->thread->Destroy();
}

void CverThread::Destroy() {
  int tid = this->tid();
  VReport(1, "T%d exited\n", tid);

  malloc_storage().CommitBack();
  if (common_flags()->use_sigaltstack) UnsetAlternateSignalStack();
  cverThreadRegistry().FinishThread(tid);
  FlushToDeadThreadStats(&stats_);
  // We also clear the shadow on thread destruction because
  // some code may still be executing in later TSD destructors
  // and we don't want it to have any poisoned stack.
  uptr size = RoundUpTo(sizeof(CverThread), GetPageSizeCached());
  UnmapOrDie(this, size);
  DTLS_Destroy();
}

void CverThread::Init() {
  CHECK_EQ(this->stack_size(), 0U);
  SetThreadStackAndTls();
  CHECK_GT(this->stack_size(), 0U);

  rbtree_root = rbtree_create();
  
  int local = 0;
  VReport(1, "T%d: stack [%p,%p) size 0x%zx; local=%p\n", tid(),
          (void *)stack_bottom_, (void *)stack_top_, stack_top_ - stack_bottom_,
          &local);
}

thread_return_t CverThread::ThreadStart(uptr os_id) {
  Init();
  cverThreadRegistry().StartThread(tid(), os_id, 0);
  if (common_flags()->use_sigaltstack) SetAlternateSignalStack();

  if (!start_routine_) {
    // start_routine_ == 0 if we're on the main thread or on one of the
    // OS X libdispatch worker threads. But nobody is supposed to call
    // ThreadStart() for the worker threads.
    CHECK_EQ(tid(), 0);
    return 0;
  }

  thread_return_t res = start_routine_(arg_);

  // On POSIX systems we defer this to the TSD destructor. LSan will consider
  // the thread's memory as non-live from the moment we call Destroy(), even
  // though that memory might contain pointers to heap objects which will be
  // cleaned up by a user-defined TSD destructor. Thus, calling Destroy() before
  // the TSD destructors have run might cause false positives in LSan.
  if (!SANITIZER_POSIX)
    this->Destroy();

  return res;
}

void CverThread::SetThreadStackAndTls() {
  uptr tls_size = 0;
  GetThreadStackAndTls(tid() == 0, &stack_bottom_, &stack_size_, &tls_begin_,
                       &tls_size);
  stack_top_ = stack_bottom_ + stack_size_;
  tls_end_ = tls_begin_ + tls_size;

  int local;
  CHECK(AddrIsInStack((uptr)&local));
}

CverThread *GetCurrentThread() {
  CverThreadContext *context =
      reinterpret_cast<CverThreadContext *>(CverTSDGet());
  if (!context) {
    return 0;
  }
  return context->thread;
}

void SetCurrentThread(CverThread *t) {
  CHECK(t->context());
  VReport(2, "SetCurrentThread: %p for thread %p\n", t->context(),
          (void *)GetThreadSelf());
  // Make sure we do not reset the current CverThread.
  CHECK_EQ(0, CverTSDGet());
  CverTSDSet(t->context());
  CHECK_EQ(t->context(), CverTSDGet());
}

u32 GetCurrentTidOrInvalid() {
  CverThread *t = GetCurrentThread();
  return t ? t->tid() : kInvalidTid;
}

void EnsureMainThreadIDIsCorrect() {
  CverThreadContext *context =
      reinterpret_cast<CverThreadContext *>(CverTSDGet());
  if (context && (context->tid == 0))
    context->os_id = GetTid();
}

__cver::CverThread *GetCverThreadByOsIDLocked(uptr os_id) {
  __cver::CverThreadContext *context = static_cast<__cver::CverThreadContext *>(
      __cver::cverThreadRegistry().FindThreadContextByOsIDLocked(os_id));
  if (!context) return 0;
  return context->thread;
}

#ifdef CVER_USE_STACK_MAP
StackMapBucket *GetCurrentThreadStackMapBucket(addr_ptr Addr) {
  CverThreadContext *context =
      reinterpret_cast<CverThreadContext *>(CverTSDGet());
  if (!context) {
    return 0;
  }
  return context->GetStackMapBucket(Addr);
}

StackMapBucket *CverThreadContext::GetStackMapBucket(addr_ptr Addr) {
  unsigned First = (Addr & (STACK_MAP_SIZE-2)) ^ 1;
  unsigned Probe = First;
  for (int Tries = 5; Tries; --Tries) {
    if (!StackMap[Probe].Addr || StackMap[Probe].Addr == Addr)
      return &StackMap[Probe];
    Probe += ((Addr >> 16) & (STACK_MAP_SIZE-2)) + 1;
    if (Probe >= STACK_MAP_SIZE)
      Probe -= STACK_MAP_SIZE;
  }
  // FIXME: Pick a random entry from the probe sequence to evict rather than
  //        just taking the first.
  return &StackMap[First];
}
#endif // CVER_USE_STACK_MAP

#ifdef CVER_USE_STACK_RBTREE
rbtree GetCurrentThreadRbtreeRoot() {
  CverThreadContext *context =
      reinterpret_cast<CverThreadContext *>(CverTSDGet());
  if (!context || !context->thread) {
    return 0;
  }
  return context->thread->rbtree_root;
}

rbtree GetCurrentThreadRbtreeRootWithThread(CverThread *thread) {
  return thread->rbtree_root;
}
#endif // CVER_USE_STACK_RBTREE

}  // namespace __cver
