// RUN: %check_clang_tidy %s readability-guard-clause %t -- \
// RUN:   -config="{CheckOptions: {readability-guard-clause.IgnoreIfWithInitializer: true, readability-guard-clause.MinimumLines: 1}}"

void sideEffect();
int getValue();

void function_with_init_var() {
  if (const auto x = getValue(); x > 0) {
    sideEffect();
  }
}

void function_with_init_multiple_vars() {
  if (int x = 42, y = 10; x + y > 0) {
    sideEffect();
  }
}

void function_without_init() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (true) {
    sideEffect();
  }
}

void function_with_init_no_var() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (sideEffect(); !getValue()) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (sideEffect(); getValue()) {
    sideEffect();
  }
}
