#ifndef UBSAN_FLAGS_H
#define UBSAN_FLAGS_H

namespace __ubsan {

struct Flags {
  bool no_cache;
};

extern Flags ubsan_flags;
inline Flags *flags() { return &ubsan_flags; }

void InitializeCommonFlags();
void InitializeFlags();

}  // namespace __ubsan

#endif  // UBSAN_FLAGS_H
