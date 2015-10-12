// RUN: echo "src:*blacklist.cc" > %t.blacklist

// RUN: %clangxx -fsanitize=cver %s -O3 -o %t
// RUN: CVER_OPTIONS=die_on_error=1 %run %t 2>&1 | FileCheck %s --strict-whitespace
// RUN: %clangxx -fsanitize=cver -fsanitize-blacklist=%t.blacklist %s -O3 -o %t
// RUN: CVER_OPTIONS=die_on_error=1 %run %t 2>&1

class S {};
  
class T : public S {
public:
  int x;
};

int main() {
    S *ps = new S();
    // CHECK: == CastVerifier Bad-casting Reports
    T* pt = static_cast<T*>(ps);
    return 0;
}
