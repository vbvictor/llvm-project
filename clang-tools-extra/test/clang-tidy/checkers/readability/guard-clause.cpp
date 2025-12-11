// RUN: %check_clang_tidy %s readability-guard-clause %t -- \
// RUN:   -config="{CheckOptions: {readability-guard-clause.MinimumLines: 0}}"

void sideEffect();
int getValue();
bool condition();

// ============================================================================
// SECTION 1: Basic patterns that SHOULD trigger (with fix-its)
// ============================================================================

// Test: Empty body - braces on same line
void empty_body_same_line() {
  // CHECK-MESSAGES: :[[@LINE+4]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!1) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  if (1) {}
}

// Test: Empty body - braces on different lines
void empty_body_different_lines() {
  // CHECK-MESSAGES: :[[@LINE+4]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!2) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  if (2) {
  }
}

// Test: Empty body - with blank line between braces
void empty_body_with_blank_line() {
  // CHECK-MESSAGES: :[[@LINE+4]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!3) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  if (3) {

  }
}

// Test: Simple function with single if
void basic_function_if() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (true) {
    sideEffect();
  }
}

// Test: For loop with if
void for_loop_if() {
  for (int i = 0; i < 10; ++i) {
    // CHECK-MESSAGES: :[[@LINE+5]]:5: warning: use guard clause with continue to reduce nesting [readability-guard-clause]
    // CHECK-FIXES:          if (!true) {
    // CHECK-FIXES-NEXT:       continue;
    // CHECK-FIXES-NEXT:     }
    // CHECK-FIXES-NEXT:       sideEffect();
    if (true) {
      sideEffect();
    }
  }
}

// Test: While loop with if
void while_loop_if() {
  while (true) {
    // CHECK-MESSAGES: :[[@LINE+5]]:5: warning: use guard clause with continue to reduce nesting [readability-guard-clause]
    // CHECK-FIXES:          if (!true) {
    // CHECK-FIXES-NEXT:       continue;
    // CHECK-FIXES-NEXT:     }
    // CHECK-FIXES-NEXT:       sideEffect();
    if (true) {
      sideEffect();
    }
  }
}

// Test: Do-while loop with if
void do_while_loop_if() {
  do {
    // CHECK-MESSAGES: :[[@LINE+5]]:5: warning: use guard clause with continue to reduce nesting [readability-guard-clause]
    // CHECK-FIXES:          if (!true) {
    // CHECK-FIXES-NEXT:       continue;
    // CHECK-FIXES-NEXT:     }
    // CHECK-FIXES-NEXT:       sideEffect();
    if (true) {
      sideEffect();
    }
  } while (true);
}

// Test: Range-based for loop with if
void range_for_loop_if() {
  int arr[] = {1, 2, 3};
  for (int x : arr) {
    // CHECK-MESSAGES: :[[@LINE+5]]:5: warning: use guard clause with continue to reduce nesting [readability-guard-clause]
    // CHECK-FIXES:          if (!true) {
    // CHECK-FIXES-NEXT:       continue;
    // CHECK-FIXES-NEXT:     }
    // CHECK-FIXES-NEXT:       sideEffect();
    if (true) {
      sideEffect();
    }
  }
}

// Test: Nested loops - inner loop should trigger
void nested_loops_if() {
  for (int i = 0; i < 10; ++i) {
    for (int j = 0; j < 10; ++j) {
      // CHECK-MESSAGES: :[[@LINE+5]]:7: warning: use guard clause with continue to reduce nesting [readability-guard-clause]
      // CHECK-FIXES:            if (!true) {
      // CHECK-FIXES-NEXT:         continue;
      // CHECK-FIXES-NEXT:       }
      // CHECK-FIXES-NEXT:         sideEffect();
      if (true) {
        sideEffect();
      }
    }
  }
}

// ============================================================================
// SECTION 2: Condition patterns
// ============================================================================

// Test: Already negated condition - removes negation
void negated_condition() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (!true) {
    sideEffect();
  }
}

// Test: Complex binary condition - wraps in parentheses
void complex_condition() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!(true && false || true)) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (true && false || true) {
    sideEffect();
  }
}

// Test: Multi-line condition
void multiline_condition() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!(true &&
  // CHECK-FIXES-NEXT:       false &&
  // CHECK-FIXES-NEXT:       true)) {
  // CHECK-FIXES-NEXT:     return;
  if (true &&
      false &&
      true) {
    sideEffect();
  }
}

// Test: Function call in condition
void function_call_condition() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!condition()) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (condition()) {
    sideEffect();
  }
}

// ============================================================================
// SECTION 3: Brace styles and if without braces
// ============================================================================

// Test: If without braces - single statement
void if_without_braces() {
  // CHECK-MESSAGES: :[[@LINE+4]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true)
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   sideEffect();
  if (true)
    sideEffect();
}

// Test: If without braces - multi-line statement (lambda)
void if_without_braces_multiline() {
  // CHECK-MESSAGES: :[[@LINE+7]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true)
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   []() {
  // CHECK-FIXES-NEXT:       sideEffect();
  // CHECK-FIXES-NEXT:       sideEffect();
  // CHECK-FIXES-NEXT:     }();
  if (true)
    []() {
      sideEffect();
      sideEffect();
    }();
}

// Test: Opening brace on next line
void brace_on_next_line() {
  // CHECK-MESSAGES: :[[@LINE+6]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true)
  // CHECK-FIXES-NEXT:   {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (true)
  {
    sideEffect();
  }
}

// ============================================================================
// SECTION 4: Nested if statements
// ============================================================================

// Test: Outer if triggers, inner if preserved
void nested_if_outer_triggers() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     if (false) {
  if (true) {
    if (false) {
      sideEffect();
    }
    sideEffect();
  }
}

// Test: Deeply nested if - only outermost triggers
void deeply_nested_if() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     if (true) {
  if (true) {
    if (true) {
      if (false) {
        sideEffect();
      }
    }
    sideEffect();
  }
}

// ============================================================================
// SECTION 5: Init-statement patterns (C++17)
// ============================================================================

// Test: Init-statement with variable - warning but NO fix-it (unsafe)
void init_with_variable() {
  // CHECK-MESSAGES: :[[@LINE+1]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  if (const auto x = getValue(); x > 0) {
    sideEffect();
  }
}

// Test: Init-statement with multiple variables - warning but NO fix-it
void init_with_multiple_variables() {
  // CHECK-MESSAGES: :[[@LINE+1]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  if (int x = 42, y = 10; x + y > 0) {
    sideEffect();
  }
}

// Test: Init-statement without variable declaration - HAS fix-it
void init_without_variable() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (sideEffect(); !getValue()) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (sideEffect(); getValue()) {
    sideEffect();
  }
}

// Test: Condition with variable declaration - warning but NO fix-it (unsafe)
// e.g., if (auto *x = getPtr()) - variable would go out of scope after transformation
void condition_with_variable() {
  // CHECK-MESSAGES: :[[@LINE+1]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  if (const int x = getValue()) {
    sideEffect();
  }
}

// Test: Condition with pointer variable declaration - warning but NO fix-it
void condition_with_pointer_variable() {
  int val = 42;
  // CHECK-MESSAGES: :[[@LINE+1]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  if (const int *ptr = &val) {
    sideEffect();
  }
}

// ============================================================================
// SECTION 6: Comment preservation
// ============================================================================

// Test: Comment before first statement - preserved
void comment_before_statement() {
  // CHECK-MESSAGES: :[[@LINE+6]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     // Comment before
  // CHECK-FIXES-NEXT:     sideEffect();
  if (true) {
    // Comment before
    sideEffect();
  }
}

// Test: Inline comment on statement - preserved
void comment_inline() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect(); // Inline comment
  if (true) {
    sideEffect(); // Inline comment
  }
}

// Test: Block comment inline - preserved
void comment_block_inline() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect(); /* block */
  if (true) {
    sideEffect(); /* block */
  }
}

// Test: Comment between condition and brace - preserved
void comment_between_condition_brace() {
  // CHECK-MESSAGES: :[[@LINE+7]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true)
  // CHECK-FIXES-NEXT:   /* between */
  // CHECK-FIXES-NEXT:   {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (true)
  /* between */
  {
    sideEffect();
  }
}

// Test: Multi-line comment - preserved
void comment_multiline() {
  // CHECK-MESSAGES: :[[@LINE+7]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     /* Line 1
  // CHECK-FIXES-NEXT:        Line 2 */
  // CHECK-FIXES-NEXT:     sideEffect();
  if (true) {
    /* Line 1
       Line 2 */
    sideEffect();
  }
}

// TODO: Comment on opening brace line gets moved to after guard's closing brace
void comment_on_brace_line() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   } // brace comment
  // CHECK-FIXES-NEXT:     sideEffect();
  if (true) { // brace comment
    sideEffect();
  }
}

// TODO: Comment on closing brace gets moved to last statement
void comment_on_closing_brace() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect(); // close comment
  if (true) {
    sideEffect();
  } // close comment
}

// ============================================================================
// SECTION 7: Patterns that should NOT trigger
// ============================================================================

// Test: If is NOT the last statement - should NOT trigger
// (guard clause would change behavior of code after the if)
void if_not_last_statement() {
  if (true) {
    sideEffect();
  }
  sideEffect();  // This code runs regardless of condition
}

// Test: If is NOT the last statement in loop - should NOT trigger
void loop_if_not_last_statement() {
  for (int i = 0; i < 10; ++i) {
    if (true) {
      sideEffect();
    }
    sideEffect();  // This code runs regardless of condition
  }
}

// Test: If-else with non-empty else - should NOT trigger
void if_else_nonempty() {
  if (true) {
    sideEffect();
  } else {
    sideEffect();
  }
}

// Test: If-else-if chain - should NOT trigger
void if_else_if_chain() {
  if (true) {
    sideEffect();
  } else if (false) {
    sideEffect();
  } else {
    sideEffect();
  }
}

// Test: Constexpr if - should NOT trigger (different semantics)
void constexpr_if() {
  if constexpr (true) {
    sideEffect();
  }
}

// Test: Already a guard clause (return) - should NOT trigger
void already_guard_return() {
  if (condition())
    return;
  sideEffect();
}

// Test: Already a guard clause (continue) - should NOT trigger
void already_guard_continue() {
  for (int i = 0; i < 10; ++i) {
    if (condition())
      continue;
    sideEffect();
  }
}

// Test: Already a guard clause with braces - should NOT trigger
void already_guard_braces() {
  if (condition()) {
    return;
  }
  sideEffect();
}

// Test: Empty function - should NOT trigger
void empty_function() {
}

// Test: If with empty else - should NOT trigger (fix-it can't handle else removal)
void if_with_empty_else() {
  if (true) {
    sideEffect();
  } else {
    // Empty else is OK
  }
}

// ============================================================================
// SECTION 8: Edge cases
// ============================================================================

// Test: Multiple statements in body
void multiple_statements() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (true) {
    sideEffect();
    sideEffect();
    sideEffect();
  }
}

// Test: If with break in loop context - already guard clause, should NOT trigger
void loop_with_break_guard() {
  for (int i = 0; i < 10; ++i) {
    if (condition())
      break;
    sideEffect();
  }
}

// Test: Numeric literal condition
void numeric_condition() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!42) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (42) {
    sideEffect();
  }
}

// ============================================================================
// SECTION 9: Different function types
// ============================================================================

// Test: Lambda expression
// CHECK-MESSAGES: :[[@LINE+6]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
// CHECK-FIXES:        if (!true) {
// CHECK-FIXES-NEXT:     return;
// CHECK-FIXES-NEXT:   }
// CHECK-FIXES-NEXT:     sideEffect();
auto lambda_test = []() {
  if (true) {
    sideEffect();
  }
};

// Test: Class with constructor, destructor, methods
class TestClass {
public:
  // Constructor
  TestClass() {
    // CHECK-MESSAGES: :[[@LINE+5]]:5: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
    // CHECK-FIXES:          if (!true) {
    // CHECK-FIXES-NEXT:       return;
    // CHECK-FIXES-NEXT:     }
    // CHECK-FIXES-NEXT:       sideEffect();
    if (true) {
      sideEffect();
    }
  }

  // Destructor
  ~TestClass() {
    // CHECK-MESSAGES: :[[@LINE+5]]:5: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
    // CHECK-FIXES:          if (!true) {
    // CHECK-FIXES-NEXT:       return;
    // CHECK-FIXES-NEXT:     }
    // CHECK-FIXES-NEXT:       sideEffect();
    if (true) {
      sideEffect();
    }
  }

  // Instance method
  void method() {
    // CHECK-MESSAGES: :[[@LINE+5]]:5: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
    // CHECK-FIXES:          if (!true) {
    // CHECK-FIXES-NEXT:       return;
    // CHECK-FIXES-NEXT:     }
    // CHECK-FIXES-NEXT:       sideEffect();
    if (true) {
      sideEffect();
    }
  }

  // Static method
  static void staticMethod() {
    // CHECK-MESSAGES: :[[@LINE+5]]:5: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
    // CHECK-FIXES:          if (!true) {
    // CHECK-FIXES-NEXT:       return;
    // CHECK-FIXES-NEXT:     }
    // CHECK-FIXES-NEXT:       sideEffect();
    if (true) {
      sideEffect();
    }
  }
};

// Test: Template function
template <typename T>
void templateFunc() {
  // CHECK-MESSAGES: :[[@LINE+5]]:3: warning: use guard clause with early return to reduce nesting [readability-guard-clause]
  // CHECK-FIXES:        if (!true) {
  // CHECK-FIXES-NEXT:     return;
  // CHECK-FIXES-NEXT:   }
  // CHECK-FIXES-NEXT:     sideEffect();
  if (true) {
    sideEffect();
  }
}

// Explicit instantiations to ensure template is analyzed
template void templateFunc<int>();
template void templateFunc<double>();
