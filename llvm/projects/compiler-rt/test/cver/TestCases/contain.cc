// RUN: %clangxx -fsanitize=cver %s -O3 -o %t
// RUN: CVER_OPTIONS='verbose=1 empty_inherit=1' %run %t 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-EMPTY-INHERIT
// RUN: CVER_OPTIONS='verbose=1' %run %t 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-IGNORE-EMPTY-INHERIT

class S {};
class T : public S{};
class U : public T{};

class Base {
public:
  U innerU;
};
class Container : public Base {
public:
  T innerT;
};

int main(int argc, char **argv) {
  Container *pc = new Container();
  T *pt = &(pc->innerT);
  // CHECK-EMPTY-INHERIT: == CastVerifier Bad-casting Reports
  // CHECK-EMPTY-INHERIT: contain.cc:[[@LINE+2]]:11: Casting from 'Container' to 'U'
  // CHECK-EMPTY-INHERIT: == End of reports.
  U *pu = static_cast<U*>(pt); // bad down-casting


  // CHECK-EMPTY-INHERIT-NOT: contain.cc:[[@LINE+2]]:12: Casting from 'Container' to 'U'
  T *pt2 = &(pc->innerU);
  U *pu2 = static_cast<U*>(pt2); // benign down-casting

  // CHECK-IGNORE-EMPTY-INHERIT-NOT: == CastVerifier Bad-casting Reports
  return 0;
}
