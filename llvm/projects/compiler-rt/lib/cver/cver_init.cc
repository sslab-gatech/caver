#include "cver_init.h"
#include "cver_internal.h"
#include "cver_allocator.h"
#include "cver_thread.h"
#include "cver_flags.h"
#include "cver_report.h"
#include "cver_stats.h"
#include "sanitizer_common/sanitizer_suppressions.h"
#include "sanitizer_common/sanitizer_common.h"

using namespace __cver;

extern rbtree cver_global_rbtree_root;
bool cver_initialized = false;

static NOINLINE void force_interface_symbols() {
  volatile int fake_condition = 0;  // prevent dead condition elimination.
  switch (fake_condition) {
  // default:
  //   break;
  }
}

static void cver_atexit() {
  Printf("==========================================\n");
  Printf("CastVerifier exit stats:\n");
  PrintAccumulatedStats();
  // __asan_print_accumulated_stats();
  // Print AsanMappingProfile.
}

void __cver::InitCverIfNecessary() {
#if !SANITIZER_CAN_USE_PREINIT_ARRAY
  static StaticSpinMutex init_mu;
  SpinMutexLock l(&init_mu);
#endif
  if (LIKELY(cver_initialized))
   return;

  if (0 == internal_strcmp(SanitizerToolName, "SanitizerTool")) {
    // Cver is run in a standalone mode. Initialize it now.
    SanitizerToolName = "CastVerifier";
    InitializeCommonFlags();
  }
  InitializeFlags();
  SuppressionContext::InitIfNecessary();

  cver_initialized = true;
  CverTSDInit(CverThread::TSDDtor);
  InitializeAllocator();

  // Create main thread.
  CverThread *main_thread = CverThread::Create(0, 0);
  CreateThreadContextArgs create_main_args = { main_thread, 0 };
  u32 main_tid = cverThreadRegistry().CreateThread(
      0, true, 0, &create_main_args);
  CHECK_EQ(0, main_tid);
  SetCurrentThread(main_thread);
  main_thread->ThreadStart(internal_getpid());
  force_interface_symbols();  // no-op.

  cver_global_rbtree_root = rbtree_create();

  if (flags()->stats)
    Atexit(cver_atexit);

  if (flags()->verbose)
    Printf("Cver initialized\n");
}

#ifdef CVER_USE_PREINIT_ARRAY
  __attribute__((section(".preinit_array"), used))
  void (*__local_asan_preinit)(void) = __cver::InitCverIfNecessary;
#else
// Use a dynamic initializer
class CverInitializer {
public:
  CverInitializer() {
    InitCverIfNecessary();
  }
};
static CverInitialize cver_initializer;
#endif
