// Firefox replaces new/free operators with their own allocators (xmalloc),
// which eventually invokes malloc() and free().
// See FF_SOURCE/memory/mozalloc/mozalloc.h

// RUN: %clangxx -fsanitize=cver %s -O3 -o %t
// RUN: %run %t 2>&1 | FileCheck %s --strict-whitespace

#include <stdlib.h>
#include <iostream>

inline void* operator new(size_t size) throw(std::bad_alloc) {
    return malloc(size);
}

class S {};

class T : public S {
public:
  int x;
};

int main() {
    S *ps = new S();
    // CHECK: == CastVerifier Bad-casting Reports
    // CHECK: replace_new.cc:[[@LINE+1]]:13: Casting from 'S' to 'T'
    T* pt = static_cast<T*>(ps);
    // CHECK: == End of reports.
    return 0;
}
