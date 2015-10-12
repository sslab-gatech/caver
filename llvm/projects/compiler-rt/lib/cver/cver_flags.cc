#include "cver_flags.h"
#include "sanitizer_common/sanitizer_common.h"
#include "sanitizer_common/sanitizer_flags.h"

namespace __cver {

static const char *GetRuntimeFlagsFromCompileDefinition() {
#ifdef CVER_DEFAULT_OPTIONS
// Stringize the macro value
# define CVER_STRINGIZE(x) #x
# define CVER_STRINGIZE_OPTIONS(options) CVER_STRINGIZE(options)
  return CVER_STRINGIZE_OPTIONS(CVER_DEFAULT_OPTIONS);
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
  ParseCommonFlagsFromString(cf, GetEnv("CVER_OPTIONS"));
}

Flags cver_flags;

static void ParseFlagsFromString(Flags *f, const char *str) {
  if (!str)
    return;
  ParseFlag(str, &f->verbose, "verbose",
            "Verbose debug outputs");
  ParseFlag(str, &f->no_print_stacktrace, "no_print_stacktrace",
            "No stacktraces in an error report");
  ParseFlag(str, &f->no_check, "no_check",
            "Disable all casting checks");
  ParseFlag(str, &f->no_free, "no_free",
            "Do not free the memory from allocators");
  ParseFlag(str, &f->no_cache, "no_cache",
            "Disable type table searching cache");
  ParseFlag(str, &f->no_global, "no_global",
            "Disable global object racing");
  ParseFlag(str, &f->no_stack, "no_stack",
            "Disable stack object racing");
  ParseFlag(str, &f->no_handle_new, "no_handle_new",
            "Disable new handle");
  ParseFlag(str, &f->no_handle_cast, "no_handle_cast",
            "Disable cast handle");
  ParseFlag(str, &f->no_cast_validity, "no_cast_validity",
            "Disable cast validity checks");
  ParseFlag(str, &f->no_composition, "no_composition",
            "Disable composition checks");
  ParseFlag(str, &f->no_report, "no_report",
            "Disable all reports");
  ParseFlag(str, &f->die_on_error, "die_on_error",
            "Terminate the program on errors");
  ParseFlag(str, &f->empty_inherit, "empty_inherit",
            "Report errors even for the empty inheritance (the same layout)");
  ParseFlag(str, &f->new_stacktrace, "new_stacktrace",
            "Print stacktraces on memory allocations");
  ParseFlag(str, &f->stats, "stats",
            "Print statistics at exit");
  ParseFlag(str, &f->nullify, "nullify",
            "Return null pointers on bad-casting");
}

void InitializeFlags() {
  Flags *f = flags();
  // Default values.

  // Do not print stacktrace in the error report.
  f->no_print_stacktrace = false;
  // Verbose debug outputs.
  f->verbose = false;
  // Disable all casting checks.
  f->no_check = false;
  // Do not free the memory from allocators.
  f->no_free = false;
  // Disable type table searching cache.
  f->no_cache = false;
  // Disable global object tracing.
  f->no_global = false;
  // Disable stack object tracing.
  f->no_stack = false;
  // Disable new handle.
  f->no_handle_new = false;
  // Disable cast handle.
  f->no_handle_cast = false;
  // Disable cast validity checks.
  f->no_cast_validity = false;
  // Disable composition checks.
  f->no_composition = false;
  // Disable all reports.
  f->no_report = false;
  // Terminate the program on errors
  f->die_on_error = false;
  // Report errors for the empty inherit cases.
  f->empty_inherit = false;
  // Print stacktraces on memory allocations (new).
  f->new_stacktrace = false;
  // Print statistics at exit.
  f->stats = false;
  // Return null pointers on bad-casting.
  f->nullify = false;
  

  // Override from compile definition.
  ParseFlagsFromString(f, GetRuntimeFlagsFromCompileDefinition());
  // Override from environment variable.
  ParseFlagsFromString(f, GetEnv("CVER_OPTIONS"));
}

}  // namespace __cver
