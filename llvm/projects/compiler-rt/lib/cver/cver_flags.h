#ifndef CVER_FLAGS_H
#define CVER_FLAGS_H

namespace __cver {

struct Flags {
  bool verbose;
  bool no_print_stacktrace;
  bool no_check;
  bool no_free;
  bool no_cache;
  bool no_global;
  bool no_stack;
  bool no_composition;
  bool no_report;
  bool no_handle_new;
  bool no_handle_cast;
  bool no_cast_validity;
  bool die_on_error;
  bool empty_inherit;
  bool new_stacktrace;
  bool stats;
  bool nullify;
};

extern Flags cver_flags;
inline Flags *flags() { return &cver_flags; }

void InitializeCommonFlags();
void InitializeFlags();

}  // namespace __cver

#endif  // CVER_FLAGS_H
