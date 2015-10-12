// Check if cver generates THTable correctly.
// RUN: %clang_cc1 -fsanitize=cver -emit-llvm %s -o - | FileCheck %s --strict-whitespace

class S {
  int _dummy;
};

class T : public S {
};

int main(){
  // CHECK: call i64 @__cver_handle_new(i8* bitcast ({ i8* }*
  // CHECK: call i64 @__cver_handle_cast(i8* bitcast ({ { [84 x i8]*, i32, i32 }, i8*, i64 }* @3 to 
  S *ps = new S();
  T *pt = static_cast<T*>(ps);
  return 0;
}
