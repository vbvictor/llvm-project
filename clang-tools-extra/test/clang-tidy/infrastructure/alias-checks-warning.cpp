// This test verifies that clang-tidy warns when alias checks are both enabled
// and that the warning is suppressed when using the --quiet flag.

// Test 1: Warning should appear when both alias checks are enabled
// RUN: clang-tidy -checks='-*,modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays' %s 2>&1 | \
// RUN:   FileCheck %s --check-prefix=CHECK-ALIAS-WARNING

// Test 2: Warning should be suppressed with --quiet flag
// RUN: clang-tidy -checks='-*,modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays' --quiet %s 2>&1 | \
// RUN:   FileCheck %s --check-prefix=CHECK-QUIET-NO-WARNING

// Test 3: Warning when using wildcard that enables multiple aliases
// RUN: clang-tidy -checks='-*,modernize-*,cppcoreguidelines-avoid-c-arrays' %s 2>&1 | \
// RUN:   FileCheck %s --check-prefix=CHECK-WILDCARD-ALIAS-WARNING

// Test 4: Multiple alias pairs detected
// RUN: clang-tidy -checks='-*,modernize-use-override,cppcoreguidelines-explicit-virtual-functions,modernize-avoid-c-arrays,cppcoreguidelines-avoid-c-arrays' %s 2>&1 | \
// RUN:   FileCheck %s --check-prefix=CHECK-MULTIPLE-ALIASES

// Test 5: No warning when only one check from alias pair is enabled
// RUN: clang-tidy -checks='-*,modernize-avoid-c-arrays' %s 2>&1 | \
// RUN:   FileCheck %s --check-prefix=CHECK-NO-ALIAS-WARNING

// CHECK-ALIAS-WARNING: warning: found alias checks: 'modernize-avoid-c-arrays' is an alias of 'cppcoreguidelines-avoid-c-arrays' [clang-tidy-config]
// CHECK-ALIAS-WARNING: note: please disable the alias check to avoid running duplicate checks

// CHECK-QUIET-NO-WARNING-NOT: warning: found alias checks

// CHECK-WILDCARD-ALIAS-WARNING: warning: found alias checks: 'modernize-avoid-c-arrays' is an alias of 'cppcoreguidelines-avoid-c-arrays' [clang-tidy-config]
// CHECK-WILDCARD-ALIAS-WARNING: note: please disable the alias check to avoid running duplicate checks

// CHECK-MULTIPLE-ALIASES: warning: found alias checks: 'modernize-avoid-c-arrays' is an alias of 'cppcoreguidelines-avoid-c-arrays', 'modernize-use-override' is an alias of 'cppcoreguidelines-explicit-virtual-functions' [clang-tidy-config]
// CHECK-MULTIPLE-ALIASES: note: please disable the alias check to avoid running duplicate checks

// CHECK-NO-ALIAS-WARNING-NOT: warning: found alias checks

int array[10]; // This would trigger the avoid-c-arrays check

class Base {
  virtual void foo();
};

class Derived : public Base {
  void foo(); // This would trigger the use-override check
};
