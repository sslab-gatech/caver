// RUN: echo "atype:_ZTI2T2" > %t.blacklist
// RUN: echo "atype:_ZTI2T1" >> %t.blacklist
// RUN: echo "atype:_ZTI4Base" >> %t.blacklist
// RUN: echo "atype:_ZTI1X" >> %t.blacklist
// RUN: echo "atype:_ZTI1T" >> %t.blacklist

// RUN: %clang_cc1 -fsanitize=cver -fsanitize=cver-stack -emit-llvm %s -o - |FileCheck %s --strict-whitespace -check-prefix=BEFORE
// RUN: %clang_cc1 -fsanitize=cver -fsanitize=cver-stack -fsanitize-blacklist=%t.blacklist -emit-llvm %s -o - |FileCheck %s --strict-whitespace -check-prefix=AFTER

class X {
};

class Base {
  int _dummy1;
};

class S : public Base {
  int _dummy2;
};

class T : public S {
};

class T1 : public T {
};

class T2 : public T {
};

class U : public S {
  X x;
};

class M {
  S s;
};

// BEFORE: define void @_Z13opt_cases_refR1S
// AFTER: define void @_Z13opt_cases_refR1S
void opt_cases_ref(S& s) {
  T t = static_cast<T&>(s);
}


// BEFORE: define void @_Z14opt_cases_heapP1S
// AFTER: define void @_Z14opt_cases_heapP1S
void opt_cases_heap(S* ps) {
  // all of these should not be traced.
  // BEFORE: call i64 @__cver_handle_new
  // BEFORE: call i64 @__cver_handle_new
  // BEFORE: call i64 @__cver_handle_new
  // BEFORE: call i64 @__cver_handle_new
  // AFTER-NOT: call i64 @__cver_handle_new    
  Base *pbase = new Base();  
  T *pt = new T();
  T1 *pt1 = new T1();
  T2 *pt2 = new T2();

  T *ptt = static_cast<T*>(ps);
  return;
}

// BEFORE: define void @_Z15opt_cases_stackP1S
// AFTER: define void @_Z15opt_cases_stackP1S
void opt_cases_stack(S* ps) {
  // all of these should not be traced.
  
  // AFTER-NOT: call i64 @__cver_handle_stack_enter
  // AFTER-NOT: call i64 @__cver_handle_stack_exit
  
  // BEFORE: call i64 @__cver_handle_stack_enter
  // BEFORE: call i64 @__cver_handle_stack_enter
  // BEFORE: call i64 @__cver_handle_stack_enter
  // BEFORE: call i64 @__cver_handle_stack_enter

  Base base;
  T t;
  T1 t1;
  T2 t2;
  opt_cases_heap(ps); // dirty hack to avoid stack prune optimizations
  // BEFORE: call i64 @__cver_handle_stack_exit
  // BEFORE: call i64 @__cver_handle_stack_exit
  // BEFORE: call i64 @__cver_handle_stack_exit
  // BEFORE: call i64 @__cver_handle_stack_exit

  return;
}

// BEFORE: define i32 @main()
// AFTER: define i32 @main()
int main(){
  // these should be traced.
  // AFTER: call i64 @__cver_handle_new
  // AFTER: call i64 @__cver_handle_new
  // AFTER: call i64 @__cver_handle_new  

  // BEFORE: call i64 @__cver_handle_new
  // BEFORE: call i64 @__cver_handle_new
  // BEFORE: call i64 @__cver_handle_new
  
  S *ps = new S();
  U *pu = new U();
  M *pm = new M();  
  opt_cases_heap(ps);
  opt_cases_stack(ps);  
  return 0;
}
