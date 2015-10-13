### CaVer

CaVer is a runtime bad-casting detection tool. With the help of the
LLVM compiler suite, CaVer first performs program instrumentations at
compile time and then validates type casting based on a new runtime
type tracing mechanism.

For more details and demos, please check our project page,
[ESHard](https://sslab.gtisc.gatech.edu/pages/eshard-extreme-software-hardening.html).
and the published paper [Type Casting Verification: Stopping an Emerging Attack
Vector, USENIX Security 2015](http://www.cc.gatech.edu/~blee303/paper/caver.pdf).

CaVer won the [Internet Defense Prize](http://internetdefenseprize.org/) from
Facebook and USENIX Security, and we are grateful for their supports.

### How to build
```
make caver
```

This command creates the build directory `build`. For example, `clang`
binary can be located from `build/bin/clang-3.5`.

### How to test
```
make test
```

This runs two different sets of testing, `check-clang-cver`
and `check-cver`.

`check-clang-cver` tests the correctness of Clang level code
generations including THTable emits and stack variable allocation
tracing. The testing code can be found in
`llvm/tools/clang/test/CodeGen/cver`.

`check-cver` does the end-to-end testing on the caver's bad-casting
detection capability. It includes various casting cases (e.g., from
(non-)polymorhpic classes to (non-)polymoprhic classes), and the
testing code can be found in
`llvm/projects/compiler-rt/test/cver/TestCases`.


### How to use

CaVer can be easily activated by adding one extra compiler flag,
`-fsanitize=cver`. For example, suppose we want to protect the simple
bad-casting code, `test.cc` (please check more bad-casting testcases
in compiler-rt to understand how CaVer handles more complicated
castings).

```
$ cat > test.cc
class S {};
class T : public S {
  int m;
};

int main() {
  S* ps = new S;
  T* pt = static_cast<T*>(ps); // bad-casting!
  return 0;
}
```

Assuming $BUILD_DIR points to the caver build directory, 
`test.cc` can be built using CaVer as follows.

```
$ $BUILD_DIR/bin/clang++ ./test.cc -o test -fsanitize=cver
```

Once executing the CaVer instrumented binary, `test`, Caver generates
bad-casting reports at runtime including detailed informatoin on such
bad-casting and call stacks.  Note, this call stack information can be
symbolized using llvm symbolizer.

```
$ ./test 
== CastVerifier Bad-casting Reports
test.cc:8:11: Casting from 'S' to 'T'
              Pointer      0x60200000f7d0
              Alloc base   0x60200000f7d0
              User base    0x60200000f7d0
              Offset       0x000000000000
              TypeTable    0x00000041e250
    #0 0x402366 (/home/blee/project/cast_sanitizer/old_test/example/test+0x402366)
    #1 0x41925a (/home/blee/project/cast_sanitizer/old_test/example/test+0x41925a)
    #2 0x7ff496967ec4 (/lib/x86_64-linux-gnu/libc.so.6+0x21ec4)
    #3 0x41912b (/home/blee/project/cast_sanitizer/old_test/example/test+0x41912b)

== End of reports.
```

### Code browsing guideline.

The current version of CaVer made a direct changes to LLVM compiler
suites, and it is branched out from

- `LLVM`: 9853f945205a0e1193afc3e84c10be96b4932d1b
- `Clang`: adc3bbec2b4d390277094b8f33df6ec25abe96b3
- `Compiler-rt`: 23c70c8907fd669d4cbeeba59680e40fc1b61d19

, and applied a few upstream patches to fix regression bugs of LLVM
compiler suites (which unfortunately mixed up with CaVer's changes).

In order to check the true changes that CaVer made to LLVM compiler
suites, the best would be diffing from the branching point.

```
git diff 9b5950d61de7cade786239321298a874d7319a6d --stat
```

In a nutshell, CaVer's majors changes are as follows.

- `CGTHTables.h` in `llvm/tools/clang/lib/CodeGen` implements Type
Hierarchy Table.

- `CGExpr.cpp` and `CGExpr.cpp` in `llvm/tools/clang/lib/CodeGen`
inspects the casting operations, and instrument the runtime calls in
case of downcasting. Much of the implementation is based on Undefined
Sanitizers.

- `CGDecl.cpp` in `llvm/tools/clang/lib/CodeGen` instruments a tracing
routine on stack variable allocation and deallocation.

- `CastVerifier.cpp` in `llvm/lib/Transforms/Instrumentation` implements
a LLVM pass for CaVer. As an optimization technique, this pass enables
CaVer to avoiding to trace unnecesary class allocations.

- `cver_*.cc` and `cver_*.h` in `llvm/projects/compiler-rt/lib/cver`
implements a runtime library of CaVer, which keeps track of allocation
metadata and finally carries out casting verfication if required.

### Contributors

- [Byoungyoung Lee](http://www.cc.gatech.edu/~blee303/)
- [Chengyu Song](http://www.cc.gatech.edu/grads/c/csong43/)
- [Taesoo Kim](https://taesoo.gtisc.gatech.edu/)
- [Wenke Lee](http://wenke.gtisc.gatech.edu/)
