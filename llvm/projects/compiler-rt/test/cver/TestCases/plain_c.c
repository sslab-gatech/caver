// RUN: %clang -fsanitize=cver %s -O3 -o %t
// RUN: CVER_OPTIONS=die_on_error=1 %run %t 2>&1

// This test does nothing but check whether cast_sanitizer can build plain c programs.

#include <stdio.h>

int main(int argc) {
  return 0;
}
