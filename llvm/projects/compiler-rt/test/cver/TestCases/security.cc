// RUN: %clangxx -fsanitize=cver %s -O3 -o %t
// RUN: %run %t 2>&1 | FileCheck %s --strict-whitespace --check-prefix=DEFAULT
// R-UN: %clangxx -fsanitize=cver %s -O3 -o %t -mllvm -cver-only-security
// R-UN: %run %t 2>&1 | FileCheck %s --strict-whitespace --check-prefix=ONLY-SECURITY

class S {
public:
  int s;
};

class T : public S {
public:
  int t;
};

class U : public T {
public:
  int u;
};

void n1(void) {
  // non-security
  S *ps = new S;
  // DEFAULT: security.cc:[[@LINE+2]]
  // ONLY-SECURITY-NOT: security.cc:[[@LINE+1]]
  T* pt = static_cast<T*>(ps);
  int x = pt->s;
  pt->s = 1;
  return;
}

T *s1(void) {
  // security
  S *ps = new S;
  // DEFAULT: security.cc:[[@LINE+2]]
  // ONLY-SECURITY: security.cc:[[@LINE+1]]  
  T* pt = static_cast<T*>(ps);
  return pt;
}

void s2(void) {
  // security
  S *ps = new S;
  // DEFAULT: security.cc:[[@LINE+2]]  
  // ONLY-SECURITY: security.cc:[[@LINE+1]]
  T* pt = static_cast<T*>(ps);
  pt->t = 1; 
  return;
}

T *pt_global;
void s3(void) {
  S *ps = new S;
  // DEFAULT: security.cc:[[@LINE+2]]  
  // ONLY-SECURITY: security.cc:[[@LINE+1]]
  T* pt = static_cast<T*>(ps);
  pt_global = pt;
  return;
}

void s4(T **pt_arg) {
  S *ps = new S;
  // DEFAULT: security.cc:[[@LINE+2]]
  // ONLY-SECURITY: security.cc:[[@LINE+1]]  
  T* pt = static_cast<T*>(ps);
  *pt_arg = pt;
  return;
}

int main(void) {
  T *pt;
  
  n1();
  s1();
  s2();
  s3();
  s4(&pt);
}
