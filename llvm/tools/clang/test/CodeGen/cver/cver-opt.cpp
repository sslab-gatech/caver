// CVer should not instrument (__cver_handle_cast) for below cases
// DO NOTHING FOR NOW
// RUN: %clang_cc1 -fsanitize=cver -emit-llvm %s -o -
// R-UN: %clang_cc1 -fsanitize=cver -emit-llvm %s -o - | FileCheck %s --strict-whitespace

class S {
  int _dummy_s;
};

class T : public S {
  int _dummy_t;
};

#define HEAP_ALLOC(AllocTy, PointerTy, PointerVar)                      \
  PointerTy *PointerVar = reinterpret_cast<PointerTy*>(new AllocTy());

#define STACK_ALLOC(AllocTy, PointerTy, PointerVar)                     \
  AllocTy _x;                                                           \
  PointerTy *PointerVar = reinterpret_cast<PointerTy*>(&_x);

#define STATIC_CAST(Ty, FromPointerVar, ToPointerVar)   \
  Ty *ToPointerVar = static_cast<Ty*>(FromPointerVar);

void opt1_new(void) {
  // Intra-procedural analysis.
  HEAP_ALLOC(T, S, ps);
  STATIC_CAST(T, ps, pt);
  return;
}

void opt1_stack(void) {
  // Intra-procedural analysis.
  // T t;
  // S *ps = reinterpret_cast<S*>(&t);
  // T *pt = static_cast<T*>(ps);
  STACK_ALLOC(T, S, ps);
  STATIC_CAST(T, ps, pt);
  return;
}

void __opt2_new(S* ps) 
{
  // T *pt = static_cast<T*>(ps);
  STATIC_CAST(T, ps, pt);
  return;
}

void opt2_new(void) {
  // Inter-procedural analysis 
  // S *ps = reinterpret_cast<S*>(new T());
  HEAP_ALLOC(T, S, ps);
  __opt2_new(ps);
  return;
}

int main() {
  opt1_new();
  opt1_stack();  
  opt2_new();  
  return 0;
}
