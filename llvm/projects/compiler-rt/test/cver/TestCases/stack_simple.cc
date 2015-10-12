// RUN: %clangxx -fsanitize=cver -fsanitize=cver-stack %s -O3 -o %t
// RUN: %run %t 2>&1 | FileCheck %s --strict-whitespace

#include <stdio.h>
#include <stdlib.h>

class Base {
public:
  unsigned long x;
};

class D1 : public Base {
public:
  unsigned long _d1;
};

class D2 : public Base {
public:
  unsigned long _d2x;
  unsigned long _d2y;
  unsigned long _d2z;
};

class V1 : public Base {
public:
  unsigned long _v1;
  virtual ~V1(){}
};

class V2 : public Base {
public:
  unsigned long _v2;
  virtual ~V2(){}
};

int main(int argc, char **argv) {
  fprintf(stderr, "sizeof(Base) : %lu\n", sizeof(Base));
  fprintf(stderr, "sizeof(D1) : %lu\n", sizeof(D1));
  fprintf(stderr, "sizeof(V1) : %lu\n", sizeof(V1));

  Base ap;
  Base *p = static_cast<Base*>(&ap);
  // CHECK: == CastVerifier Bad-casting Reports
  // CHECK: stack_simple.cc:[[@LINE+2]]:12: Casting from 'Base' to 'V2'
  // CHECK: == End of reports.
  V2 *pc = static_cast<V2*>(p);
  
  return 0;
}
