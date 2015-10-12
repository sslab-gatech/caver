// RUN: %clangxx -fsanitize=cver %s -O3 -o %t
// RUN: CVER_OPTIONS=die_on_error=1 %run %t 2>&1

struct S {};
struct T: public S{};

struct Base {
public:
  // U innerU;
};

struct Container : public Base {
  T innerT;
};

int main(int argc, char **argv) {
  Container *pc = new Container();
  T *pt = &(pc->innerT);
  S *ps = static_cast<S*>(pt); // OK casting.
  T *pt2 = static_cast<T*>(ps); // OK casting.
  return 0;
}
