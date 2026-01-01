// RUN: %check_clang_tidy %s cert-err09-cpp,cert-err61-cpp %t -- -- -fexceptions

// CHECK-MESSAGES: warning: found alias checks: 'cert-err61-cpp' is an alias of 'cert-err09-cpp' [clang-tidy-config]
// CHECK-MESSAGES: note: please disable the alias check to avoid running duplicate checks

void alwaysThrows() {
  int ex = 42;
  // CHECK-MESSAGES: warning: throw expression should throw anonymous temporary values instead [cert-err09-cpp,cert-err61-cpp]
  throw ex;
}

void doTheJob() {
  try {
    alwaysThrows();
  } catch (int&) {
  }
}
