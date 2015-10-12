#include "ubsan_flags.h"
#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_flags.h"

namespace __ubsan {

static const char *GetRuntimeFlagsFromCompileDefinition() {
#ifdef UBSAN_DEFAULT_OPTIONS
// Stringize the macro value
# define UBSAN_STRINGIZE(x) #x
# define UBSAN_STRINGIZE_OPTIONS(options) UBSAN_STRINGIZE(options)
  return UBSAN_STRINGIZE_OPTIONS(UBSAN_DEFAULT_OPTIONS);
#else
  return "";
#endif
}

void InitializeCommonFlags() {
  CommonFlags *cf = common_flags();
  SetCommonFlagsDefaults(cf);
  // Override from compile definition.
  ParseCommonFlagsFromString(cf, GetRuntimeFlagsFromCompileDefinition());
  // Override from environment variable.
  ParseCommonFlagsFromString(cf, GetEnv("UBSAN_OPTIONS"));
}

Flags ubsan_flags;

static void ParseFlagsFromString(Flags *f, const char *str) {
  if (!str)
    return;
  ParseFlag(str, &f->no_cache, "no_cache",
            "Disable type table searching cache");
}

void InitializeFlags() {
  Flags *f = flags();
  // Default values.

  // Disable type table searching cache.
  f->no_cache = false;
  // Override from compile definition.
  ParseFlagsFromString(f, GetRuntimeFlagsFromCompileDefinition());
  // Override from environment variable.
  ParseFlagsFromString(f, GetEnv("UBSAN_OPTIONS"));
}

}  // namespace __ubsan
