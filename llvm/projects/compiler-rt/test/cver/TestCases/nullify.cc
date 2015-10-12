// RUN: %clangxx -fsanitize=cver %s -O3 -o %t

// R-UN: %run %t 1 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-POLY-POLY
// R-UN: CVER_OPTIONS='nullify=1' %run %t 1 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-POLY-POLY-NULLIFY

// R-UN: %run %t 2 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-POLY-NONPOLY
// R-UN: CVER_OPTIONS='nullify=1' %run %t 2 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-POLY-NONPOLY-NULLIFY

// R-UN: %run %t 3 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-NONPOLY-POLY
// R-UN: CVER_OPTIONS='nullify=1' %run %t 3 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-NONPOLY-POLY-NULLIFY

// R-UN: %run %t 4 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-NONPOLY-NONPOLY
// R-UN: CVER_OPTIONS='nullify=1' %run %t 4 2>&1 | FileCheck %s --strict-whitespace --check-prefix=CHECK-NONPOLY-NONPOLY-NULLIFY

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
        V1 *ap = new V1();
        Base *p = ap;
        // CHECK-POLY-POLY: nullify.cc:[[@LINE+2]]:18: Casting from 'V1' to 'V2'
        // CHECK-POLY-POLY-NULLIFY: nullify.cc:[[@LINE+1]]:18: Casting from 'V1' to 'V2'
        V2 *pc = static_cast<V2*>(p);

        if (pc == 0) {
          // CHECK-POLY-POLY-NOT: CVER Nullified
          // CHECK-POLY-POLY-NULLIFY: CVER Nullified at line [[@LINE+1]]
          fprintf(stderr, "CVER Nullified at line %d\n", __LINE__);
        }
        break;
    }
    case 2: {
        V1 *ap = new V1();
        Base *p = ap;
        // CHECK-POLY-NONPOLY: nullify.cc:[[@LINE+2]]:18: Casting from 'V1' to 'D1'
        // CHECK-POLY-NONPOLY-NULLIFY: nullify.cc:[[@LINE+1]]:18: Casting from 'V1' to 'D1'
        D1 *pc = static_cast<D1*>(p);
        if (pc == 0) {
          // CHECK-POLY-NONPOLY-NOT: CVER Nullified
          // CHECK-POLY-NONPOLY-NULLIFY: CVER Nullified at line [[@LINE+1]]
          fprintf(stderr, "CVER Nullified at line %d\n", __LINE__);
        }
        break;
    }
    case 3: {
        D1 *ap = new D1();
        Base *p = static_cast<Base*>(ap);
        // CHECK-NONPOLY-POLY: nullify.cc:[[@LINE+2]]:18: Casting from 'D1' to 'V1'
        // CHECK-NONPOLY-POLY-NULLIFY: nullify.cc:[[@LINE+1]]:18: Casting from 'D1' to 'V1'
        V1 *pc = static_cast<V1*>(p);

        if (pc == 0) {
          // CHECK-NONPOLY-POLY-NOT: CVER Nullified
          // CHECK-NONPOLY-POLY-NULLIFY: CVER Nullified at line [[@LINE+1]]
          fprintf(stderr, "CVER Nullified at line %d\n", __LINE__);
        }
        break;
    }
    case 4: {
        D1 *ap = new D1();
        Base *p = ap;
        // CHECK-NONPOLY-NONPOLY: nullify.cc:[[@LINE+2]]:18: Casting from 'D1' to 'D2'
        // CHECK-NONPOLY-NONPOLY-NULLIFY: nullify.cc:[[@LINE+1]]:18: Casting from 'D1' to 'D2'
        D2 *pc = static_cast<D2*>(p);
        if (pc == 0) {
          // CHECK-NONPOLY-NONPOLY-NOT: CVER Nullified
          // CHECK-NONPOLY-NONPOLY-NULLIFY: CVER Nullified at line [[@LINE+1]]
          fprintf(stderr, "CVER Nullified at line %d\n", __LINE__);
        }
        break;
    }
    case 5: {
        V1 *ap = new V1();
        void *p = ap;

        // Bad-casting from poly to non-poly (via void*)
        // ubsan CANNOT handle
        // cver CAN handle
        // CHECK-POLY-NONPOLY-VOID: == CastVerifier Bad-casting Reports
        // CHECK-POLY-NONPOLY-VOID: nullify.cc:[[@LINE+7]]:13: Casting from 'V1' to 'D1'
        // CHECK-POLY-NONPOLY-VOID: == End of reports.
        D1 *pc = static_cast<D1*>(p);

        printf("allocated pointer : %p\n", ap);
        printf("changed pointer : %p\n", pc);

        pc->_d1 = 1;
        break;
    }
    case 6: {
        V1 *ap = new V1();
        void *p = ap;

        // Bad-casting from poly to poly (via void*)
        // ubsan CAN handle
        // cver CAN handle
        // CHECK-POLY-POLY-VOID: == CastVerifier Bad-casting Reports
        // CHECK-POLY-POLY-VOID: nullify.cc:[[@LINE+7]]:13: Casting from 'V1' to 'V2'
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
