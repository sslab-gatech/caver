// RUN: %clangxx -fsanitize=cver -fsanitize=cver-stack %s -O3 -o %t
// RUN: %run %t 1 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-POLY-POLY
// RUN: %run %t 2 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-POLY-NONPOLY
// RUN: %run %t 3 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-NONPOLY-POLY
// RUN: %run %t 4 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-NONPOLY-NONPOLY

// TODO : Ignore void casts for now.
// R-UN: %run %t 5 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-POLY-NONPOLY-VOID
// R-UN: %run %t 6 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-POLY-POLY-VOID

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
  if (argc < 2 ) {
    fprintf(stderr, "ERROR : pick something\n");
    return -1;
  }
  fprintf(stderr, "sizeof(Base) : %lu\n", sizeof(Base));
  fprintf(stderr, "sizeof(D1) : %lu\n", sizeof(D1));
  fprintf(stderr, "sizeof(V1) : %lu\n", sizeof(V1));

  int pick = atoi(argv[1]);

  fprintf(stderr, "pick : %d\n", pick);

  switch(pick) {
  case 1: {
    V1 ap;
    Base *p = static_cast<Base*>(&ap);
    // CHECK-POLY-POLY: == CastVerifier Bad-casting Reports
    // CHECK-POLY-POLY: stack_coverage.cc:[[@LINE+2]]:14: Casting from 'V1' to 'V2'
    // CHECK-POLY-POLY: == End of reports.
    V2 *pc = static_cast<V2*>(p);
    
    printf("allocated pointer : %p\n", &ap);
    printf("changed pointer : %p\n", pc);
    break;
  }
  case 2: {
    V1 ap;
    Base *p = static_cast<Base*>(&ap);    
    // CHECK-POLY-NONPOLY: == CastVerifier Bad-casting Reports
    // CHECK-POLY-NONPOLY: stack_coverage.cc:[[@LINE+2]]:14: Casting from 'V1' to 'D1'
    // CHECK-POLY-NONPOLY: == End of reports.
    D1 *pc = static_cast<D1*>(p);

    printf("allocated pointer : %p\n", &ap);
    printf("changed pointer : %p\n", pc);
    break;
  }
  case 3: {
    D1 ap;
    Base *p = static_cast<Base*>(&ap);
    // CHECK-NONPOLY-POLY: == CastVerifier Bad-casting Reports
    // CHECK-NONPOLY-POLY: stack_coverage.cc:[[@LINE+2]]:14: Casting from 'D1' to 'V1'
    // CHECK-NONPOLY-POLY: == End of reports.
    V1 *pc = static_cast<V1*>(p);

    printf("allocated pointer : %p\n", &ap);
    printf("changed pointer : %p\n", pc);
    break;
  }
  case 4: {
    D1 ap;
    Base *p = &ap;
    // CHECK-NONPOLY-NONPOLY: == CastVerifier Bad-casting Reports
    // CHECK-NONPOLY-NONPOLY: stack_coverage.cc:[[@LINE+2]]:14: Casting from 'D1' to 'D2'
    // CHECK-NONPOLY-NONPOLY: == End of reports.
    D2 *pc = static_cast<D2*>(p);

    printf("allocated pointer : %p\n", &ap);
    printf("changed pointer : %p\n", pc);
    break;
  }
  case 5: {
    V1 ap;
    void *p = &ap;

    // CHECK-POLY-NONPOLY-VOID: == CastVerifier Bad-casting Reports
    // CHECK-POLY-NONPOLY-VOID: stack_coverage.cc:[[@LINE+7]]:13: Casting from 'V1' to 'D1'
    // CHECK-POLY-NONPOLY-VOID: == End of reports.
    D1 *pc = static_cast<D1*>(p);

    printf("allocated pointer : %p\n", &ap);
    printf("changed pointer : %p\n", pc);

    pc->_d1 = 1;
    break;
  }
  case 6: {
    V1 ap;
    void *p = &ap;
    // CHECK-POLY-POLY-VOID: == CastVerifier Bad-casting Reports
    // CHECK-POLY-POLY-VOID: stack_coverage.cc:[[@LINE+7]]:13: Casting from 'V1' to 'V2'
    // CHECK-POLY-POLY-VOID: == End of reports.
    V2 *pc = static_cast<V2*>(p);

    printf("allocated pointer : %p\n", p);
    printf("changed pointer : %p\n", pc);

    pc->_v2 = 1;
    break;
  }
  default:
    break;
  }
  return 0;
}
