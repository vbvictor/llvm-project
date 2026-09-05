// By default, clang-tidy should skip the analysis of a translation unit with
// compiler errors and report that it did so.
// RUN: not clang-tidy %s -checks='-*,google-explicit-constructor' -- -DERROR > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-SKIP -implicit-check-not='{{warning|error}}:' %s
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-SKIP-MSG %s
//
// CHECK-SKIP-MSG: File '{{.*}}allow-compilation-errors.cpp' is skipped due to compiler errors.
// CHECK-SKIP-MSG-NEXT: Use -allow-compilation-errors to run checks anyway.

// With --allow-compilation-errors, checks should run despite compiler errors.
// RUN: not clang-tidy %s -checks='-*,google-explicit-constructor' --allow-compilation-errors -- -DERROR > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-ANALYZE -implicit-check-not='{{warning|error}}:' %s
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-ANALYZE-MSG -implicit-check-not='is skipped' %s
//
// CHECK-ANALYZE-MSG: Found compiler error(s).

// --fix-errors implies --allow-compilation-errors.
// RUN: cp %s %t.cpp
// RUN: clang-tidy %t.cpp -checks='-*,google-explicit-constructor' --fix-errors -- -DERROR > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-FIX-ERRORS -implicit-check-not='{{warning|error}}:' %s

// Errors upgraded from warnings by -Werror do not prevent compilation, so
// they should not disable the analysis.
// RUN: not clang-tidy %s -checks='-*,google-explicit-constructor,clang-diagnostic-unused-variable' -- -DWERROR -Wno-everything -Werror=unused-variable > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-WERROR -implicit-check-not='{{warning|error}}:' %s
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-WERROR-MSG -implicit-check-not='is skipped' %s
//
// CHECK-WERROR-MSG: Found compiler error(s).

// A translation unit with only compiler warnings is analyzed as usual.
// RUN: clang-tidy %s -checks='-*,google-explicit-constructor,clang-diagnostic-unused-variable' -- -DWARN -Wunused-variable > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stdout -check-prefix=CHECK-WARN -implicit-check-not='{{warning|error}}:' %s
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-WARN-MSG -implicit-check-not='is skipped' %s
//
// CHECK-WARN-MSG: warnings generated.

// The skip message is printed once per translation unit with compiler errors,
// in the order the translation units were processed.
// RUN: not clang-tidy %S/Inputs/allow-compilation-errors/first.cpp %S/Inputs/allow-compilation-errors/second.cpp -checks='-*,google-explicit-constructor' -- > %t.stdout 2> %t.stderr
// RUN: FileCheck -input-file=%t.stderr -check-prefix=CHECK-MULTI-MSG %s
//
// CHECK-MULTI-MSG: File '{{.*}}first.cpp' is skipped due to compiler errors.
// CHECK-MULTI-MSG-NEXT: Use -allow-compilation-errors to run checks anyway.
// CHECK-MULTI-MSG-EMPTY:
// CHECK-MULTI-MSG-NEXT: File '{{.*}}second.cpp' is skipped due to compiler errors.
// CHECK-MULTI-MSG-NEXT: Use -allow-compilation-errors to run checks anyway.
// CHECK-MULTI-MSG-EMPTY:
// CHECK-MULTI-MSG-NEXT: Found compiler error(s).

class A { A(int i); };
// CHECK-ANALYZE: :[[@LINE-1]]:11: warning: single-argument constructors must be marked explicit to avoid unintentional implicit conversions [google-explicit-constructor]
// CHECK-FIX-ERRORS: :[[@LINE-2]]:11: warning: single-argument constructors must be marked explicit to avoid unintentional implicit conversions [google-explicit-constructor]
// CHECK-WERROR: :[[@LINE-3]]:11: warning: single-argument constructors must be marked explicit to avoid unintentional implicit conversions [google-explicit-constructor]
// CHECK-WARN: :[[@LINE-4]]:11: warning: single-argument constructors must be marked explicit to avoid unintentional implicit conversions [google-explicit-constructor]

#if defined(ERROR)
unknown_type f();
// CHECK-SKIP: :[[@LINE-1]]:1: error: unknown type name 'unknown_type' [clang-diagnostic-error]
// CHECK-ANALYZE: :[[@LINE-2]]:1: error: unknown type name 'unknown_type' [clang-diagnostic-error]
// CHECK-FIX-ERRORS: :[[@LINE-3]]:1: error: unknown type name 'unknown_type' [clang-diagnostic-error]
#elif defined(WERROR)
void g() { int unused_var; }
// CHECK-WERROR: :[[@LINE-1]]:16: error: unused variable 'unused_var' [clang-diagnostic-unused-variable]
#elif defined(WARN)
void h() { int unused_var; }
// CHECK-WARN: :[[@LINE-1]]:16: warning: unused variable 'unused_var' [clang-diagnostic-unused-variable]
#endif
