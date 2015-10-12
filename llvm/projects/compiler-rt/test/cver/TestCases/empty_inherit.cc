// RUN: %clangxx -fsanitize=cver %s -O3 -o %t
// RUN: CVER_OPTIONS=die_on_error=1 %run %t 2>&1

// This test SHOULD NOT generate error reports.

class S {
public:
  virtual ~S(){};
};

// Empty inherit
class T : public S {};

int main()
{
  S* ps = new S();
  T* pt = static_cast<T*>(ps);
  return 0;
}
