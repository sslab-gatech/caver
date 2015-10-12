// RUN: %clang_cc1 -fsanitize=cver -fsanitize=cver-stack -emit-llvm %s -o - |c++filt|FileCheck %s --strict-whitespace -check-prefix=PRUNE
// RUN: %clang_cc1 -fsanitize=cver -fsanitize=cver-stack -emit-llvm %s -mllvm -disable-cver-stack-prune -o - |c++filt|FileCheck %s --strict-whitespace -check-prefix=DISABLE-PRUNE

class S {
  int _dummy;
};

class T : public S {
  int _dummy;
};


// PRUNE: define i32 @good()() #0
// DISABLE-PRUNE: define i32 @good()() #0
int good(void) {
  // PRUNE-NOT: call i64 @__cver_handle_stack_enter
  // PRUNE-NOT: call i64 @__cver_handle_stack_exit
  S s;
  return 1;
  // DISABLE-PRUNE: call i64 @__cver_handle_stack_enter
  // DISABLE-PRUNE: call i64 @__cver_handle_stack_exit
}

// PRUNE: define i32 @good_good()() #0
// DISABLE-PRUNE: define i32 @good_good()() #0
int good_good(void) {
  // PRUNE-NOT: call i64 @__cver_handle_stack_enter
  // PRUNE-NOT: call i64 @__cver_handle_stack_exit
  S s;
  good();
  good();
  // DISABLE-PRUNE: call i64 @__cver_handle_stack_enter
  // DISABLE-PRUNE: call i64 @__cver_handle_stack_exit
  return 1;
}

// PRUNE: define i32 @bad()() #0
// DISABLE-PRUNE: define i32 @bad()() #0
int bad(void) {
  // PRUNE: call i64 @__cver_handle_stack_enter
  // PRUNE: call i64 @__cver_handle_stack_exit
  S s;
  T *pt = static_cast<T*>(&s);
  // DISABLE-PRUNE: call i64 @__cver_handle_stack_enter
  // DISABLE-PRUNE: call i64 @__cver_handle_stack_exit
  return 1;
}

// PRUNE: define i32 @good_bad()() #0
// DISABLE-PRUNE: define i32 @good_bad()() #0
int good_bad(void) {
  // PRUNE: call i64 @__cver_handle_stack_enter
  // PRUNE: call i64 @__cver_handle_stack_exit
  S s;
  bad();
  return 1;
  // DISABLE-PRUNE: call i64 @__cver_handle_stack_enter
  // DISABLE-PRUNE: call i64 @__cver_handle_stack_exit
}

// PRUNE: define i32 @main() #0
// DISABLE-: define i32 @main() #0
int main() {
  // PRUNE-NOT: call i64 @__cver_handle_stack_enter
  // PRUNE-NOT: call i64 @__cver_handle_stack_exit
  good();
  bad();
  // DISABLE-PRUNE-NOT: call i64 @__cver_handle_stack_enter
  // DISABLE-PRUNE-NOT: call i64 @__cver_handle_stack_exit
}

//PRUNE: attributes #0 = {

