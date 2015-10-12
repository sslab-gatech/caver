// RUN: %clang_cc1  -fsanitize=cver -emit-llvm %s -o - | FileCheck %s --strict-whitespace

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

// CHECK: define internal void @__cver_handle_global_wrapper() unnamed_addr #4 {
// CHECK-NEXT:  call void @__cver_handle_global_var(i8* bitcast (%class.V1* @ap to i8*), i8* bitcast ({ i64, [0 x i64], i64, [4 x i64], [5 x i8] }* @0 to i8*), i64 24)
// CHECK-NEXT: ret void
V1 ap;

// CHECK: define internal void @__cver_handle_global_wrapper1() unnamed_addr #4 {
// CHECK-NEXT: call void @__cver_handle_global_var(i8* bitcast (%class.D1* @dp to i8*), i8* bitcast ({ i64, [0 x i64], i64, [4 x i64], [5 x i8] }* @1 to i8*), i64 16)
// CHECK-NEXT: ret void
D1 dp;

int main(int argc, char **argv) {
  {
    Base *p = static_cast<Base*>(&ap);
    V2 *pc = static_cast<V2*>(p);
  }
  
  {
    Base *p = static_cast<Base*>(&dp);
    V1 *pc = static_cast<V1*>(p);
  }
  return 0;
}
