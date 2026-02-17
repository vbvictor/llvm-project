// Verify that without --verify-nolints, unused suppressions are NOT warned about.
// RUN: clang-tidy %s --checks='-*,google-explicit-constructor' 2>&1 | FileCheck %s

// Unused suppression: should NOT produce a warning without the flag.
class A { A(int i, int j); }; // NOLINT

// CHECK-NOT: warning: unused
