// RUN: %clangxx -fsanitize=cver %s -O3 -o %t
// RUN: CVER_OPTIONS='die_on_error=1' %run %t 2>&1

class U {
  int _dummy;
};

class T :public U {
  int _dummy;
};

class S {
public:
  char _dummy[1000];
  T t;
};

class X {
public:
  char _dummy[1000];
  S s;
};

int main(void) {
  X *px = new X;
  U *pu = &(px->s.t);
  T *pt = static_cast<T *>(pu); // This should not crash.
}
