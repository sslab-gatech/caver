
// RUN: echo "src:*bits/stl*" > %t.blacklist
// RUN: %clangxx -fsanitize=cver -fsanitize-blacklist=%t.blacklist %s -O3 -o %t
// RUN: CVER_OPTIONS=die_on_error=1 %run %t 2>&1

// This test assumes that the stl code is not patched.
// Testing whether libstdc++ is patched for the undefined behavior,
// reported in https://gcc.gnu.org/bugzilla/show_bug.cgi?id=63345

// This test should not report an error.

#include <map>

class Y {
public:
  std::map<int, int> xs;
};

int main ()
{
  Y *py = new Y;
  py->xs.insert(std::make_pair(0, 0));

  for( std::map<int,int>::iterator it=py->xs.begin(); it != py->xs.end(); ++it) {
  }
  return 0;
}
