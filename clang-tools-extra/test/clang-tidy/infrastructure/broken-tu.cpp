// Test that by default clang-tidy skips analysis when the TU has compiler
// errors (broken AST), and that --allow-clang-diagnostic-errors re-enables it.

// RUN: not clang-tidy -checks='-*,modernize-redundant-void-arg' %s -- \
// RUN:   2>&1 | FileCheck --check-prefix=CHECK-DEFAULT \
// RUN:   -implicit-check-not='[modernize-redundant-void-arg]' %s
// RUN: not clang-tidy -checks='-*,modernize-redundant-void-arg' \
// RUN:   -allow-clang-diagnostic-errors %s -- \
// RUN:   2>&1 | FileCheck --check-prefix=CHECK-ALLOW %s

// CHECK-ALLOW: :[[@LINE+1]]:10: warning: redundant void argument list [modernize-redundant-void-arg]
void foo(void);

void f(int a) {
  // CHECK-DEFAULT: :[[@LINE+2]]:3: error: cannot take the address of an rvalue of type 'int' [clang-diagnostic-error]
  // CHECK-ALLOW: :[[@LINE+1]]:3: error: cannot take the address of an rvalue of type 'int' [clang-diagnostic-error]
  &(a + 1);
}
