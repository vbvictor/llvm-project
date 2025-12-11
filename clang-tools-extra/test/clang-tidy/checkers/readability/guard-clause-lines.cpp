// RUN: %check_clang_tidy -check-suffixes=DEFAULT %s readability-guard-clause %t
// RUN: %check_clang_tidy -check-suffixes=TWO %s readability-guard-clause %t -- \
// RUN:   -config="{CheckOptions: {readability-guard-clause.MinimumLines: 2}}"

void f();

void less_then_2_lines() {
  if (true) 
  {
    // 1 long long long long long long long long long long long long long long long long line
  }
}

void less_then_2_lines_empty() {
  if (true) {

  }
}

void two_lines() {
  f();
  // CHECK-MESSAGES-TWO: :[[@LINE+6]]:3: warning: use guard clause with early return to reduce nesting
  // CHECK-FIXES:        if (!5) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     // 1
  // CHECK-FIXES-NEXT:     // 2
  if (5) {
    // 1
    // 2
  }
}

void two_lines_empty() {
  f();
  // CHECK-MESSAGES-TWO: :[[@LINE+8]]:3: warning: use guard clause with early return to reduce nesting
  // CHECK-FIXES:        if (!5)
  // CHECK-FIXES-NEXT:   {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:
  // CHECK-FIXES-NEXT:
  // CHECK-FIXES-NEXT:}
  if (5)
  {


  }
}

void five_lines() {
  f();
  // CHECK-MESSAGES-DEFAULT: :[[@LINE+8]]:3: warning: use guard clause with early return to reduce nesting
  // CHECK-MESSAGES-TWO: :[[@LINE+7]]:3: warning: use guard clause with early return to reduce nesting
  // CHECK-FIXES:        if (!6) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     f();
  // CHECK-FIXES-NEXT:
  // CHECK-FIXES-NEXT:     f();
  if (6) {
    f();

    f();

    f();
  }
}

void one_line() {
  f();
  if (8) {
    f();
  }
}

void loop_one_line() {
  for (int i = 0; i < 10; ++i) {
    if (9) {
      f();
    }
  }
}

void loop_two_lines() {
  for (int i = 0; i < 10; ++i) {
    f();
    // CHECK-MESSAGES-TWO: :[[@LINE+6]]:5: warning: use guard clause with continue to reduce nesting
    // CHECK-FIXES:        if (!10) {
    // CHECK-FIXES-NEXT:     continue;
    // CHECK-FIXES-NEXT:   }
    // CHECK-FIXES-NEXT:     f();
    // CHECK-FIXES-NEXT:     f();
    if (10) {
      f();
      f();
    }
  }
}

void loop_five_lines() {
  for (int i = 0; i < 10; ++i) {
    f();
    // CHECK-MESSAGES-DEFAULT: :[[@LINE+7]]:5: warning: use guard clause with continue to reduce nesting
    // CHECK-MESSAGES-TWO: :[[@LINE+6]]:5: warning: use guard clause with continue to reduce nesting
    // CHECK-FIXES:        if (!11) {
    // CHECK-FIXES-NEXT:     continue;
    // CHECK-FIXES-NEXT:   }
    // CHECK-FIXES-NEXT:     f();
    // CHECK-FIXES-NEXT:     f();
    if (11) {
      f();
      f();
      f();
      f();
      f();
    }
  }
}

void long_condition() {
  f();
  if (true &&
      false &&
      true &&
      false) {
    f();
  }
}

void long_condition_fix() {
  f();
  // CHECK-MESSAGES-TWO: :[[@LINE+9]]:3: warning: use guard clause with early return to reduce nesting
  // CHECK-FIXES:      if (!(true &&
  // CHECK-FIXES-NEXT:     false &&
  // CHECK-FIXES-NEXT:     true &&
  // CHECK-FIXES-NEXT:     false)) {
  // CHECK-FIXES-NEXT:   return;
  // CHECK-FIXES-NEXT: }
  // CHECK-FIXES-NEXT:   f();
  // CHECK-FIXES-NEXT:   // 2
  if (true &&
      false &&
      true &&
      false) {
    f();
    // 2
  }
}

// ============================================================================
// Non-compound statements (if without braces) with different line thresholds
// ============================================================================

// Non-compound: 1 line - should NOT trigger for either threshold
void non_compound_1_line() {
  if (true)
    f();
}

// Non-compound: 2 lines - should trigger for TWO but not DEFAULT
void non_compound_2_lines() {
  f();
  // CHECK-MESSAGES-TWO: :[[@LINE+4]]:3: warning: use guard clause with early return to reduce nesting
  // CHECK-FIXES:      if (!true)
  // CHECK-FIXES-NEXT:   return;
  // CHECK-FIXES-NEXT: []() {
  if (true)
    []() {
    }();
}

// Non-compound: 5 lines - should trigger for both DEFAULT and TWO
void non_compound_5_lines() {
  f();
  // CHECK-MESSAGES-DEFAULT: :[[@LINE+8]]:3: warning: use guard clause with early return to reduce nesting
  // CHECK-MESSAGES-TWO: :[[@LINE+7]]:3: warning: use guard clause with early return to reduce nesting
  // CHECK-FIXES:      if (!true)
  // CHECK-FIXES-NEXT:   return;
  // CHECK-FIXES-NEXT: []() {
  // CHECK-FIXES-NEXT:     f();
  // CHECK-FIXES-NEXT:     f();
  // CHECK-FIXES-NEXT:     f();
  if (true)
    []() {
      f();
      f();
      f();
    }();
}

// Non-compound in loop: 2 lines - should trigger for TWO but not DEFAULT
void loop_non_compound_2_lines() {
  for (int i = 0; i < 10; ++i) {
    f();
    // CHECK-MESSAGES-TWO: :[[@LINE+4]]:5: warning: use guard clause with continue to reduce nesting
    // CHECK-FIXES:        if (!true)
    // CHECK-FIXES-NEXT:     continue;
    // CHECK-FIXES-NEXT:   []() {
    if (true)
      []() {
      }();
  }
}

// Non-compound in loop: 5 lines - should trigger for both DEFAULT and TWO
void loop_non_compound_5_lines() {
  for (int i = 0; i < 10; ++i) {
    f();
    // CHECK-MESSAGES-DEFAULT: :[[@LINE+8]]:5: warning: use guard clause with continue to reduce nesting
    // CHECK-MESSAGES-TWO: :[[@LINE+7]]:5: warning: use guard clause with continue to reduce nesting
    // CHECK-FIXES:        if (!true)
    // CHECK-FIXES-NEXT:     continue;
    // CHECK-FIXES-NEXT:   []() {
    // CHECK-FIXES-NEXT:       f();
    // CHECK-FIXES-NEXT:       f();
    // CHECK-FIXES-NEXT:       f();
    if (true)
      []() {
        f();
        f();
        f();
      }();
  }
}
