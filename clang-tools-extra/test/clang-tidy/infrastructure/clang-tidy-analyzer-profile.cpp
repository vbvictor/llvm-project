// Test that analyzer timing is integrated with clang-tidy profiling
// RUN: clang-tidy -enable-check-profile -checks='-*,clang-analyzer-core.NullDereference' %s -- 2>&1 | FileCheck --match-full-lines -implicit-check-not='{{error:}}' %s

// CHECK: ===-------------------------------------------------------------------------===
// CHECK-NEXT:                          clang-tidy checks profiling
// CHECK-NEXT: ===-------------------------------------------------------------------------===
// CHECK-NEXT: Total Execution Time: {{.*}} seconds ({{.*}} wall clock)

// CHECK: {{.*}}  --- Name ---
// CHECK-DAG: {{.*}}  clang-analyzer:syntaxchecks
// CHECK-DAG: {{.*}}  clang-analyzer:exprengine
// CHECK-DAG: {{.*}}  clang-analyzer:bugreporter
// CHECK: {{.*}}  Total

// CHECK-NOT: Analyzer timers

void test_null_deref(int *ptr) {
  if (ptr)
    return;
  *ptr = 42; // This should trigger null dereference warning
}
