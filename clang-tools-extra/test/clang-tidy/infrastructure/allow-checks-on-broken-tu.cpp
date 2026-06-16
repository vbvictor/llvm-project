// Default behavior: clang-tidy skips checks when the TU has compilation
// errors, but still reports the compilation errors themselves, and prints
// an informational message about the skip.
// RUN: not clang-tidy -checks='-*,google-explicit-constructor' %s -- 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-DEFAULT %s

// In --quiet mode the informational skip message is suppressed.
// RUN: not clang-tidy --quiet -checks='-*,google-explicit-constructor' %s -- 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-QUIET %s

// With --allow-checks-on-broken-tu, checks run even on broken TUs.
// RUN: not clang-tidy --allow-checks-on-broken-tu -checks='-*,google-explicit-constructor' %s -- 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-ALLOW %s

// --fix-errors implies --allow-checks-on-broken-tu, so checks still run.
// RUN: grep -Ev "// *[A-Z-]+:" %s > %t.cpp
// RUN: clang-tidy --fix-errors -checks='-*,google-explicit-constructor' %t.cpp -- 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-FIX-ERRORS %s

// When there are no compilation errors, checks always run (default behavior).
// RUN: clang-tidy -checks='-*,google-explicit-constructor' %s -- -DNO_ERROR 2>&1 \
// RUN:   | FileCheck -check-prefix=CHECK-NO-ERROR %s

#ifndef NO_ERROR
broken_type x;
#endif
// CHECK-DEFAULT: Skipping clang-tidy checks for '{{.*}}allow-checks-on-broken-tu.cpp' due to compilation errors. Pass '--allow-checks-on-broken-tu' to analyze the translation unit anyway.
// CHECK-DEFAULT: error: unknown type name 'broken_type' [clang-diagnostic-error]
// CHECK-DEFAULT-NOT: warning: single-argument constructors must be marked explicit

// CHECK-QUIET-NOT: Skipping clang-tidy checks
// CHECK-QUIET: error: unknown type name 'broken_type' [clang-diagnostic-error]
// CHECK-QUIET-NOT: warning: single-argument constructors must be marked explicit

// CHECK-ALLOW-NOT: Skipping clang-tidy checks
// CHECK-ALLOW: error: unknown type name 'broken_type' [clang-diagnostic-error]
// CHECK-FIX-ERRORS-NOT: Skipping clang-tidy checks
// CHECK-FIX-ERRORS: error: unknown type name 'broken_type' [clang-diagnostic-error]

class A { A(int) {} };
// CHECK-ALLOW: warning: single-argument constructors must be marked explicit
// CHECK-FIX-ERRORS: warning: single-argument constructors must be marked explicit
// CHECK-NO-ERROR: warning: single-argument constructors must be marked explicit

// The trailing "Found compiler error(s)." message is printed when the
// run encountered compilation errors and -fix-errors was not specified
// (it is also suppressed by --quiet). When --fix-errors is passed, the
// tool returns 0 and does not print this message.
// CHECK-DEFAULT: Found compiler error(s).
// CHECK-QUIET-NOT: Found compiler error
// CHECK-ALLOW: Found compiler error(s).
// CHECK-FIX-ERRORS-NOT: Found compiler error
// CHECK-NO-ERROR-NOT: Found compiler error
