// Some runs below apply fixes, so they must not be given this file itself.
// Strip the FileCheck directives from the copy, otherwise checking the fixed
// file would match the expectations spelled out here instead of the code.
// RUN: grep -Ev "// *[A-Z-]+:" %s > %t-input.cpp

// By default, clang-tidy skips the analysis of a translation unit that has
// compiler errors, and names the skipped file on stderr.
// RUN: not clang-tidy %s -checks='-*,google-explicit-constructor' -- -DERROR > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-SKIP -implicit-check-not='{{warning|error}}:' %s
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-SKIP-MSG %s
//
// CHECK-SKIP-MSG: File '{{.*}}allow-compilation-errors.cpp' was skipped due to compiler errors.
// CHECK-SKIP-MSG-NEXT: Use -allow-compilation-errors to run checks anyway.

// Checks that run over the preprocessor have already reported by the time the
// AST turns out to be broken. Their diagnostics are dropped as well, so that a
// file reported as skipped really produces nothing but compiler diagnostics.
// RUN: not clang-tidy %s -checks='-*,bugprone-macro-parentheses' -- -DERROR > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-SKIP -implicit-check-not='{{warning|error}}:' %s

// With --allow-compilation-errors, both AST-based and preprocessor-based checks
// run despite the compiler errors.
// RUN: not clang-tidy %s -checks='-*,google-explicit-constructor,bugprone-macro-parentheses' --allow-compilation-errors -- -DERROR > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-ALLOW -implicit-check-not='{{warning|error}}:' %s
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-ALLOW-MSG -implicit-check-not='was skipped' %s
//
// CHECK-ALLOW-MSG: Found compiler error(s).

// --fix-errors implies --allow-compilation-errors, and the fixes are applied.
// RUN: cp %t-input.cpp %t-fix-errors.cpp
// RUN: clang-tidy %t-fix-errors.cpp -checks='-*,google-explicit-constructor' --fix-errors -- -DERROR > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-FIX-ERRORS -implicit-check-not='{{warning|error}}:' %s
// RUN: FileCheck -input-file=%t-fix-errors.cpp -check-prefix=CHECK-FIXES %s
//
// CHECK-FIX-ERRORS: warning: single-argument constructors must be marked explicit to avoid unintentional implicit conversions [google-explicit-constructor]
// CHECK-FIX-ERRORS: error: unknown type name 'unknown_type' [clang-diagnostic-error]
// CHECK-FIXES: class A { explicit A(int i); };

// Nothing ran on a skipped file, so there are no withheld fixes to report:
// --fix prints the skip message only.
// RUN: cp %t-input.cpp %t-fix.cpp
// RUN: not clang-tidy %t-fix.cpp -checks='-*,google-explicit-constructor' --fix -- -DERROR > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-FIX-MSG -implicit-check-not='-fix-errors was not specified' %s
//
// CHECK-FIX-MSG: was skipped due to compiler errors.
// CHECK-FIX-MSG-NEXT: Use -allow-compilation-errors to run checks anyway.

// Errors upgraded from warnings by -Werror do not prevent compilation, so they
// do not disable the analysis.
// RUN: not clang-tidy %s -checks='-*,google-explicit-constructor,clang-diagnostic-unused-variable' -- -DWERROR -Wno-everything -Werror=unused-variable > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-WERROR -implicit-check-not='{{warning|error}}:' %s
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-WERROR-MSG -implicit-check-not='was skipped' %s
//
// CHECK-WERROR-MSG: Found compiler error(s).

// A translation unit with only compiler warnings is analyzed as usual.
// RUN: clang-tidy %s -checks='-*,google-explicit-constructor,clang-diagnostic-unused-variable' -- -DWARN -Wunused-variable > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-WARN -implicit-check-not='{{warning|error}}:' %s
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-WARN-MSG -implicit-check-not='was skipped' %s
//
// CHECK-WARN-MSG: warnings generated.

// The skip message is printed once per skipped translation unit, in the order
// the translation units were processed.
// RUN: echo 'unknown_type f();' > %t-first.cpp
// RUN: echo 'unknown_type g();' > %t-second.cpp
// RUN: not clang-tidy %t-first.cpp %t-second.cpp -checks='-*,google-explicit-constructor' -- > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-MULTI-MSG %s
//
// CHECK-MULTI-MSG: File '{{.*}}-first.cpp' was skipped due to compiler errors.
// CHECK-MULTI-MSG-NEXT: Use -allow-compilation-errors to run checks anyway.
// CHECK-MULTI-MSG-EMPTY:
// CHECK-MULTI-MSG-NEXT: File '{{.*}}-second.cpp' was skipped due to compiler errors.
// CHECK-MULTI-MSG-NEXT: Use -allow-compilation-errors to run checks anyway.
// CHECK-MULTI-MSG-EMPTY:
// CHECK-MULTI-MSG-NEXT: Found compiler error(s).

// When only some of the translation units were skipped, the warning about
// fixes that were not applied is still relevant for the analyzed ones.
// RUN: echo 'class B { B(int i); };' > %t-clean.cpp
// RUN: not clang-tidy %t-first.cpp %t-clean.cpp -checks='-*,google-explicit-constructor' --fix -- > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-PARTIAL-MSG %s
//
// CHECK-PARTIAL-MSG: File '{{.*}}-first.cpp' was skipped due to compiler errors.
// CHECK-PARTIAL-MSG: Found compiler errors, but -fix-errors was not specified.

// Dropping the check diagnostics of a skipped translation unit must not disturb
// the diagnostics already collected for the translation units around it.
// RUN: echo 'class C { C(int i); };' > %t-analyzed.cpp
// RUN: echo '#define BAD(x) x * 2' > %t-broken.cpp
// RUN: echo 'unknown_type h();' >> %t-broken.cpp
// RUN: not clang-tidy %t-analyzed.cpp %t-broken.cpp -checks='-*,google-explicit-constructor,bugprone-macro-parentheses' -- > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-MIXED -implicit-check-not='{{warning|error}}:' %s
//
// CHECK-MIXED: -analyzed.cpp:1:11: warning: single-argument constructors must be marked explicit to avoid unintentional implicit conversions [google-explicit-constructor]
// CHECK-MIXED: -broken.cpp:2:1: error: unknown type name 'unknown_type' [clang-diagnostic-error]

// Same, but the preceding translation unit ends on a diagnostic that the check
// filter discards ('clang-diagnostic-unused-variable' is not enabled below).
// Discarding it shifts the diagnostics of the skipped file, which must not make
// the preprocessor check's warning escape.
// RUN: echo 'void filtered() { int unused_var; }' > %t-filtered.cpp
// RUN: not clang-tidy %t-filtered.cpp %t-broken.cpp -checks='-*,bugprone-macro-parentheses' -- -Wunused-variable > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-FILTERED -implicit-check-not='{{warning|error}}:' %s
//
// CHECK-FILTERED: -broken.cpp:2:1: error: unknown type name 'unknown_type' [clang-diagnostic-error]

class A { A(int i); };
// CHECK-ALLOW: :[[@LINE-1]]:11: warning: single-argument constructors must be marked explicit to avoid unintentional implicit conversions [google-explicit-constructor]
// CHECK-WERROR: :[[@LINE-2]]:11: warning: single-argument constructors must be marked explicit to avoid unintentional implicit conversions [google-explicit-constructor]
// CHECK-WARN: :[[@LINE-3]]:11: warning: single-argument constructors must be marked explicit to avoid unintentional implicit conversions [google-explicit-constructor]

#if defined(ERROR)
#define BAD(x) x * 2
// CHECK-ALLOW: :[[@LINE-1]]:18: warning: macro replacement list should be enclosed in parentheses [bugprone-macro-parentheses]
unknown_type f();
// CHECK-SKIP: :[[@LINE-1]]:1: error: unknown type name 'unknown_type' [clang-diagnostic-error]
// CHECK-ALLOW: :[[@LINE-2]]:1: error: unknown type name 'unknown_type' [clang-diagnostic-error]
#elif defined(WERROR)
void g() { int unused_var; }
// CHECK-WERROR: :[[@LINE-1]]:16: error: unused variable 'unused_var' [clang-diagnostic-unused-variable]
#elif defined(WARN)
void h() { int unused_var; }
// CHECK-WARN: :[[@LINE-1]]:16: warning: unused variable 'unused_var' [clang-diagnostic-unused-variable]
#endif
