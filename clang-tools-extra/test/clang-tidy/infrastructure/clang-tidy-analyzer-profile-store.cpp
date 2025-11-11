// Test that analyzer timing is included in stored JSON profile
// RUN: rm -rf %t.dir/out
// RUN: clang-tidy -enable-check-profile -checks='-*,clang-analyzer-core.NullDereference' -store-check-profile=%t.dir/out %s -- 2>&1 | FileCheck -implicit-check-not='{{error:}}' -check-prefix=CHECK-CONSOLE %s
// RUN: cat %t.dir/out/*-clang-tidy-analyzer-profile-store.cpp.json | FileCheck -check-prefix=CHECK-JSON %s
// RUN: rm -rf %t.dir/out

// Verify console output does not include separate analyzer timers section
// CHECK-CONSOLE-NOT: Analyzer timers

// Verify JSON contains the expected fields
// CHECK-JSON: "file": "{{.*}}clang-tidy-analyzer-profile-store.cpp",
// CHECK-JSON: "timestamp":
// CHECK-JSON: "profile": {

// Verify analyzer timing entries are in JSON with proper prefix
// CHECK-JSON: "time.clang-tidy.clang-analyzer:syntaxchecks.wall":
// CHECK-JSON: "time.clang-tidy.clang-analyzer:exprengine.wall":
// CHECK-JSON: "time.clang-tidy.clang-analyzer:bugreporter.wall":

void test_null_deref(int *ptr) {
  if (ptr)
    return;
  *ptr = 42; // This should trigger null dereference warning
}
