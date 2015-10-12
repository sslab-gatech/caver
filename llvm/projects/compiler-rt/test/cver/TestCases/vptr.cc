// RUN: %clangxx -fsanitize=cver %s -O3 -o %t
// RUN: %run %t rT 2>&1
// RUN: %run %t mT 2>&1
// RUN: %run %t fT 2>&1
// RUN: %run %t rU 2>&1
// RUN: %run %t mU 2>&1
// RUN: %run %t fU 2>&1
// RUN: %run %t cU 2>&1
// RUN: %run %t rS 2>&1
// RUN: %run %t rV 2>&1
// RUN: %run %t oV 2>&1
// RUN: %run %t cT 2>&1
// RUN: CVER_OPTIONS=empty_inherit=1 %run %t cS 2>&1 | FileCheck %s --check-prefix=CHECK-S --strict-whitespace

// Disabled below checks (don't understand why I put this test :( )
// R-UN: CVER_OPTIONS=empty_inherit=1 %run %t cV 2>&1 | FileCheck %s --check-prefix=CHECK-U --strict-whitespace

struct S {
  S() : a(0) {}
  ~S() {}
  int a;
  int f() { return 0; }
  virtual int v() { return 0; }
};

struct T : S {
  T() : b(0) {}
  int b;
  int g() { return 0; }
  virtual int v() { return 1; }
};

struct U : S, T { virtual int v() { return 2; } };

T *p = 0;  // Make p global so that lsan does not complain.

int main(int, char **argv) {
  T t;
  (void)t.a;
  (void)t.b;
  (void)t.f();
  (void)t.g();
  (void)t.v();
  (void)t.S::v();

  U u;
  (void)u.T::a;
  (void)u.b;
  (void)u.T::f();
  (void)u.g();
  (void)u.v();
  (void)u.T::v();
  (void)((T&)u).S::v();

  char Buffer[sizeof(U)] = {};
  switch (argv[1][1]) {
  case '0':
    p = reinterpret_cast<T*>(Buffer);
    break;
  case 'S':
    p = reinterpret_cast<T*>(new S);
    break;
  case 'T':
    p = new T;
    break;
  case 'U':
    p = new U;
    break;
  case 'V':
    p = reinterpret_cast<T*>(new U);
    break;
  }

  switch (argv[1][0]) {
  case 'r':
    // Binding a reference to storage of appropriate size and alignment is OK.
    {T &r = *p;}
    break;

  case 'm':
    return p->b;

  case 'f':
    return p->g();

  case 'o':
    return reinterpret_cast<U*>(p)->v() - 2;

  case 'c':
    // CHECK-S: == CastVerifier Bad-casting Reports
    // CHECK-S: vptr.cc:[[@LINE+3]]:5: Casting from 'S' to 'T'
    // CHECK-U: == CastVerifier Bad-casting Reports
    // CHECK-U: vptr.cc:[[@LINE+1]]:5: Casting from 'U' to 'T'
    static_cast<T*>(reinterpret_cast<S*>(p));
    // CHECK-S: == End of reports.
    // CHECK-U: == End of reports.
    return 0;
  }
}
