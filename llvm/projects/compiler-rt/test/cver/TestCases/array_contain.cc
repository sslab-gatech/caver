// RUN: %clangxx -fsanitize=cver %s -O3 -o %t
// RUN: CVER_OPTIONS='die_on_error=1' %run %t 2>&1

class S {};
class T : public S {
  int _dummy;
};
class U : public T {
  int _dummy;
};

class Base {
public:
  char _dummy1[1000];
  T innerT[100];
  char _dummy2[1000];
};
class Container : public Base {};

int main(int argc, char **argv) {
    Container *pc = new Container[33];
    S *ps = &(pc[1].innerT[0]);
    T *pt = static_cast<T*>(ps); // benign casting
    return 0;
}
