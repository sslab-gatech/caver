#include "ubsan_init.h"
#include "ubsan_flags.h"
#include "sanitizer_common/sanitizer_suppressions.h"
#include "sanitizer_common/sanitizer_common.h"

using namespace __ubsan;

bool ubsan_initialized = false;

void __ubsan::InitUbsanIfNecessary() {
#if !SANITIZER_CAN_USE_PREINIT_ARRAY
  static StaticSpinMutex init_mu;
  SpinMutexLock l(&init_mu);
#endif

  if (LIKELY(ubsan_initialized))
   return;

  if (0 == internal_strcmp(SanitizerToolName, "SanitizerTool")) {
    // Ubsan is run in a standalone mode. Initialize it now.
    SanitizerToolName = "UndefinedBehaviorSanitizer";
    InitializeCommonFlags();
  }
  InitializeFlags();
  ubsan_initialized = true;
}

__attribute__((section(".preinit_array"), used))
void (*__local_asan_preinit)(void) = __ubsan::InitUbsanIfNecessary;
