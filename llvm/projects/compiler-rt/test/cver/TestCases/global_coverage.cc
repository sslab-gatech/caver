// RUN: %clangxx -fsanitize=cver %s -O0 -o %t
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

V1 ap;
D1 dp;

int main(int argc, char **argv) {
  if (argc < 2 ) {
    fprintf(stderr, "ERROR : pick something\n");
    return -1;
  }

  fprintf(stderr, "&ap : %p\n", &ap);
  fprintf(stderr, "&dp : %p\n", &dp);

  int pick = atoi(argv[1]);

  fprintf(stderr, "pick : %d\n", pick);

  switch(pick) {
  case 1: {
    Base *p = static_cast<Base*>(&ap);
    // CHECK-POLY-POLY: == CastVerifier Bad-casting Reports
    // CHECK-POLY-POLY: global_coverage.cc:[[@LINE+2]]:14: Casting from 'V1' to 'V2'
    // CHECK-POLY-POLY: == End of reports.
    V2 *pc = static_cast<V2*>(p);
    
    printf("allocated pointer : %p\n", &ap);
    printf("changed pointer : %p\n", pc);
    break;
  }
  case 2: {
    Base *p = static_cast<Base*>(&ap);    
    // CHECK-POLY-NONPOLY: == CastVerifier Bad-casting Reports
    // CHECK-POLY-NONPOLY: global_coverage.cc:[[@LINE+2]]:14: Casting from 'V1' to 'D1'
    // CHECK-POLY-NONPOLY: == End of reports.
    D1 *pc = static_cast<D1*>(p);

    printf("allocated pointer : %p\n", &ap);
    printf("changed pointer : %p\n", pc);
    break;
  }
  case 3: {
    Base *p = static_cast<Base*>(&dp);
    // CHECK-NONPOLY-POLY: == CastVerifier Bad-casting Reports
    // CHECK-NONPOLY-POLY: global_coverage.cc:[[@LINE+2]]:14: Casting from 'D1' to 'V1'
    // CHECK-NONPOLY-POLY: == End of reports.
    V1 *pc = static_cast<V1*>(p);

    printf("allocated pointer : %p\n", &ap);
    printf("changed pointer : %p\n", pc);
    break;
  }
  case 4: {
    Base *p = &dp;
    // CHECK-NONPOLY-NONPOLY: == CastVerifier Bad-casting Reports
    // CHECK-NONPOLY-NONPOLY: global_coverage.cc:[[@LINE+2]]:14: Casting from 'D1' to 'D2'
    // CHECK-NONPOLY-NONPOLY: == End of reports.
    D2 *pc = static_cast<D2*>(p);

    printf("allocated pointer : %p\n", &ap);
    printf("changed pointer : %p\n", pc);
    break;
  }
  case 5: {
    void *p = &ap;

    // CHECK-POLY-NONPOLY-VOID: == CastVerifier Bad-casting Reports
    // CHECK-POLY-NONPOLY-VOID: global_coverage.cc:[[@LINE+7]]:13: Casting from 'V1' to 'D1'
    // CHECK-POLY-NONPOLY-VOID: == End of reports.
    D1 *pc = static_cast<D1*>(p);

    printf("allocated pointer : %p\n", &ap);
    printf("changed pointer : %p\n", pc);

    pc->_d1 = 1;
    break;
  }
  case 6: {
    void *p = &ap;
    // CHECK-POLY-POLY-VOID: == CastVerifier Bad-casting Reports
    // CHECK-POLY-POLY-VOID: global_coverage.cc:[[@LINE+7]]:13: Casting from 'V1' to 'V2'
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
