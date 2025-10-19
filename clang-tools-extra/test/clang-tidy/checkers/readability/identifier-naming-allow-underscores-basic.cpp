// RUN: %check_clang_tidy %s readability-identifier-naming %t -- \
// RUN:   -config='{CheckOptions: { \
// RUN:     readability-identifier-naming.GlobalVariableCase: lower_case, \
// RUN:     readability-identifier-naming.GlobalVariableAllowLeadingUnderscore: true, \
// RUN:     readability-identifier-naming.LocalVariableCase: lower_case, \
// RUN:     readability-identifier-naming.LocalVariableAllowTrailingUnderscore: true, \
// RUN:   }}'

// Test that AllowLeadingUnderscore works for global variables
int _global_variable = 0;
// No warning - leading underscore allowed

int global_variable = 0;
// No warning - correct style

int GlobalVariable = 0;
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for global variable 'GlobalVariable' [readability-identifier-naming]
// CHECK-FIXES: int global_variable = 0;

int global_variable_ = 0;
// CHECK-MESSAGES: :[[@LINE-1]]:5: warning: invalid case style for global variable 'global_variable_' [readability-identifier-naming]
// CHECK-FIXES: int global_variable = 0;
// Trailing underscore not allowed for global variables

// Test that AllowTrailingUnderscore works for local variables
void test() {
  int local_variable_ = 0;
  // No warning - trailing underscore allowed

  int local_variable = 0;
  // No warning - correct style

  int LocalVariable = 0;
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: invalid case style for local variable 'LocalVariable' [readability-identifier-naming]
  // CHECK-FIXES: int local_variable = 0;

  int _local_variable = 0;
  // CHECK-MESSAGES: :[[@LINE-1]]:7: warning: invalid case style for local variable '_local_variable' [readability-identifier-naming]
  // CHECK-FIXES: int local_variable = 0;
  // Leading underscore not allowed for local variables
}
