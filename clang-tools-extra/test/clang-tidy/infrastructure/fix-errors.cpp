// RUN: grep -Ev "// *[A-Z-]+:" %s > %t.cpp
// RUN: not clang-tidy %t.cpp -checks='-*,google-explicit-constructor' -fix -- > %t.msg 2>&1
// RUN: FileCheck -input-file=%t.cpp -check-prefix=CHECK-FIX %s
// RUN: FileCheck -input-file=%t.msg -check-prefix=CHECK-MESSAGES -implicit-check-not='Fixes have NOT been applied' %s
// RUN: grep -Ev "// *[A-Z-]+:" %s > %t.cpp
// RUN: clang-tidy %t.cpp -checks='-*,google-explicit-constructor' -fix-errors -- > %t.msg 2>&1
// RUN: FileCheck -input-file=%t.cpp -check-prefix=CHECK-FIX2 %s
// RUN: FileCheck -input-file=%t.msg -check-prefix=CHECK-MESSAGES2 %s

class A { A(int i); }
// CHECK-FIX: class A { A(int i); }{{$}}
// Without -fix-errors the translation unit is not analyzed at all, so there are
// no fixes to withhold and the file is left untouched.
// CHECK-MESSAGES: was skipped due to compiler errors.
// CHECK-FIX2: class A { explicit A(int i); };
// CHECK-MESSAGES2: note: FIX-IT applied suggested code changes
// CHECK-MESSAGES2: clang-tidy applied 2 of 2 suggested fixes.
