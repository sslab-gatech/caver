// RUN: %clangxx -fsanitize=cver %s -O3 -o %t
// RUN: CVER_OPTIONS=die_on_error=1 %run %t 2>&1

// THIS TEST SHOULD NOT CRASH or REPORT AN ERROR.
class S {
public:
  int x;
};

int main() {
  for (int i=0; i<200; i++) {
    S *ps = new S;
    delete ps;
  }
  return 0;
}
