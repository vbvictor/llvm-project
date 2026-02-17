// RUN: clang-tidy %s --checks='-*,google-explicit-constructor' --verify-nolints 2>&1 | FileCheck %s

// Suppression on a line with no diagnostic (unused).
class A { A(int i, int j); }; // NOLINT
// CHECK: :[[@LINE-1]]:34: warning: unused 'NOLI
// CHECK-SAME: NT' comment [clang-tidy-noli
// CHECK-SAME: nt]

// Suppression that actually suppresses a diagnostic (used).
class B { B(int i); }; // NOLINT

// Suppression with a specific check that doesn't fire here (unused).
class C { C(int i, int j); }; // NOLINT(some-other-check)
// CHECK: :[[@LINE-1]]:34: warning: unused 'NOLI
// CHECK-SAME: NT' comment

// Suppression with matching specific check (used).
class D { D(int i); }; // NOLINT(google-explicit-constructor)

// Suppression with wrong check -- diagnostic fires, suppression is unused.
class E { E(int i); }; // NOLINT(some-other-check)
// CHECK-DAG: :[[@LINE-1]]:11: warning: single-argument constructors must be marked explicit
// CHECK-DAG: :[[@LINE-2]]:27: warning: unused 'NOLI
// CHECK-SAME: NT' comment

// Next-line suppression with no diagnostic on next line (unused).
// NOLINTNEXTLINE
class F { F(int i, int j); };
// CHECK: :[[@LINE-2]]:4: warning: unused 'NOLI
// CHECK-SAME: NTNEXTLINE' comment

// Next-line suppression that suppresses a diagnostic (used).
// NOLINTNEXTLINE
class G { G(int i); };

// Next-line suppression with wrong check (unused).
// NOLINTNEXTLINE(some-other-check)
class H { H(int i, int j); };
// CHECK: :[[@LINE-2]]:4: warning: unused 'NOLI
// CHECK-SAME: NTNEXTLINE' comment

// Next-line suppression with matching check (used).
// NOLINTNEXTLINE(google-explicit-constructor)
class I { I(int i); };

// Block suppression with no diagnostic inside (unused).
// NOLINTBEGIN
class J { J(int i, int j); };
// NOLINTEND
// CHECK: :[[@LINE-3]]:4: warning: unused 'NOLI
// CHECK-SAME: NTBEGIN' comment

// Block suppression that suppresses a diagnostic (used).
// NOLINTBEGIN
class K { K(int i); };
// NOLINTEND

// Block suppression with wrong check (unused).
// NOLINTBEGIN(some-other-check)
class L { L(int i, int j); };
// NOLINTEND(some-other-check)
// CHECK: :[[@LINE-3]]:4: warning: unused 'NOLI
// CHECK-SAME: NTBEGIN' comment

// Block suppression with matching check (used).
// NOLINTBEGIN(google-explicit-constructor)
class M { M(int i); };
// NOLINTEND(google-explicit-constructor)

// Wildcard suppression with no diagnostic (unused).
class N { N(int i, int j); }; // NOLINT(*)
// CHECK: :[[@LINE-1]]:34: warning: unused 'NOLI
// CHECK-SAME: NT' comment

// Wildcard suppression that suppresses a diagnostic (used).
class O { O(int i); }; // NOLINT(*)
