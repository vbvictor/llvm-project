// REQUIRES: ssaf
//
// Cross-translation-unit mode of misc-no-recursion.
//
// The call graph of one translation unit cannot show a cycle that goes through
// another translation unit. When the result of the whole-program
// RecursionAnalysisResult analysis of the Scalable Static Analysis Framework
// is given, the check reports the functions of the translation unit that are
// part of a cycle of the call graph of the whole program.

// RUN: rm -rf %t && mkdir -p %t/by-path && split-file %s %t

// Phase 1: extract the call-graph summary of every translation unit.
// RUN: %clang -fsyntax-only -std=c++17 --ssaf-extract-summaries=CallGraph \
// RUN:   --ssaf-tu-summary-file=%t/a.json --ssaf-compilation-unit-id=a %t/a.cpp
// RUN: %clang -fsyntax-only -std=c++17 --ssaf-extract-summaries=CallGraph \
// RUN:   --ssaf-tu-summary-file=%t/b.json --ssaf-compilation-unit-id=b %t/b.cpp

// Phase 2: link the summaries into the link unit `prog` and analyze it.
// RUN: clang-ssaf-linker %t/a.json %t/b.json -o %t/prog.json
// RUN: clang-ssaf-analyzer -a RecursionAnalysisResult %t/prog.json \
// RUN:   -o %t/wpa.json

// Phase 3: without the result each translation unit is analyzed on its own,
// so the cycle through both of them is not found.
// RUN: clang-tidy -checks='-*,misc-no-recursion' %t/a.cpp -- -std=c++17 \
// RUN:   | FileCheck %s --check-prefix=LOCAL-A
// RUN: clang-tidy -checks='-*,misc-no-recursion' %t/b.cpp -- -std=c++17 \
// RUN:   | FileCheck %s --check-prefix=LOCAL-B --allow-empty

// Phase 3: with the result it is found from either side.
// RUN: clang-tidy -checks='-*,misc-no-recursion' %t/a.cpp -- -std=c++17 \
// RUN:   --ssaf-global-scope-analysis-result=%t/wpa.json \
// RUN:   --ssaf-link-unit-id=prog --ssaf-compilation-unit-id=a \
// RUN:   | FileCheck %s --check-prefix=WPA-A
// RUN: clang-tidy -checks='-*,misc-no-recursion' %t/b.cpp -- -std=c++17 \
// RUN:   --ssaf-global-scope-analysis-result=%t/wpa.json \
// RUN:   --ssaf-link-unit-id=prog --ssaf-compilation-unit-id=b \
// RUN:   | FileCheck %s --check-prefix=WPA-B

// Without --ssaf-compilation-unit-id the compilation unit is identified by the
// path of the main file, which lets one invocation serve many files, as with
// run-clang-tidy. The summaries must have been extracted under the same ids.
// RUN: %clang -fsyntax-only -std=c++17 --ssaf-extract-summaries=CallGraph \
// RUN:   --ssaf-tu-summary-file=%t/by-path/a.json \
// RUN:   --ssaf-compilation-unit-id=%t/a.cpp %t/a.cpp
// RUN: %clang -fsyntax-only -std=c++17 --ssaf-extract-summaries=CallGraph \
// RUN:   --ssaf-tu-summary-file=%t/by-path/b.json \
// RUN:   --ssaf-compilation-unit-id=%t/b.cpp %t/b.cpp
// RUN: clang-ssaf-linker %t/by-path/a.json %t/by-path/b.json \
// RUN:   -o %t/by-path/prog.json
// RUN: clang-ssaf-analyzer -a RecursionAnalysisResult %t/by-path/prog.json \
// RUN:   -o %t/by-path/wpa.json
// RUN: clang-tidy -checks='-*,misc-no-recursion' %t/a.cpp -- -std=c++17 \
// RUN:   --ssaf-global-scope-analysis-result=%t/by-path/wpa.json \
// RUN:   --ssaf-link-unit-id=prog \
// RUN:   | FileCheck %s --check-prefix=WPA-A

//--- a.cpp
void ping(); // Defined in b.cpp.
void self() { self(); } // A cycle that one translation unit can see on its own.
static void hop() { ping(); } // Internal linkage, yet on the cycle below.
void pong() { hop(); }
void leaf() {}
void user() { pong(); leaf(); } // Calls into the cycle, but is not on it.

// Without the whole-program result only `self` is found.
// LOCAL-A: a.cpp:{{[0-9]+}}:6: warning: function 'self' is within a recursive call chain [misc-no-recursion]
// LOCAL-A: note: Frame #1: function 'self' calls function 'self' here:
// LOCAL-A-NOT: warning:

// With it, `self` is still reported as before, with the call sites of the
// frames, and is not reported a second time.
// WPA-A:      a.cpp:{{[0-9]+}}:6: warning: function 'self' is within a recursive call chain [misc-no-recursion]
// WPA-A:      a.cpp:{{[0-9]+}}:6: note: example recursive call chain, starting from function 'self'
// WPA-A:      a.cpp:{{[0-9]+}}:15: note: Frame #1: function 'self' calls function 'self' here:
// WPA-A:      a.cpp:{{[0-9]+}}:15: note: ... which was the starting point of the recursive call chain; there may be other cycles
//
// `hop` and `pong` are on the cycle ping -> pong -> hop -> ping, which leaves
// this translation unit. The call sites in the other one are not known, so the
// frames name the functions and say where they are defined. The chain starts at
// the function that comes first in the source, `hop`. The notes belong to the
// last function of the cycle in this translation unit.
// WPA-A:      a.cpp:{{[0-9]+}}:13: warning: function 'hop' is within a recursive call chain [misc-no-recursion]
// WPA-A:      a.cpp:{{[0-9]+}}:6: warning: function 'pong' is within a recursive call chain [misc-no-recursion]
// WPA-A:      note: example recursive call chain, starting from function 'hop()' defined at {{.*}}a.cpp:{{[0-9]+}}:13
// WPA-A:      note: Frame #1: function 'hop()' calls function 'ping()' defined at {{.*}}b.cpp:{{[0-9]+}}:6
// WPA-A:      note: Frame #2: function 'ping()' calls function 'pong()' defined at {{.*}}a.cpp:{{[0-9]+}}:6
// WPA-A:      note: Frame #3: function 'pong()' calls function 'hop()' defined at {{.*}}a.cpp:{{[0-9]+}}:13
// WPA-A:      note: ... which was the starting point of the recursive call chain; there may be other cycles
//
// Neither `ping`, which is defined in the other translation unit, nor `user`
// and `leaf`, which are not on a cycle, are reported.
// WPA-A-NOT:  warning:

//--- b.cpp
void pong(); // Defined in a.cpp.
void ping() { pong(); }
int main() { ping(); }

// Nothing can be found in b.cpp alone.
// LOCAL-B-NOT: misc-no-recursion

// The same cycle seen from the other side: the same chain, the same frames.
// WPA-B:      b.cpp:{{[0-9]+}}:6: warning: function 'ping' is within a recursive call chain [misc-no-recursion]
// WPA-B:      note: example recursive call chain, starting from function 'hop()' defined at {{.*}}a.cpp:{{[0-9]+}}:13
// WPA-B:      note: Frame #1: function 'hop()' calls function 'ping()' defined at {{.*}}b.cpp:{{[0-9]+}}:6
// WPA-B:      note: Frame #2: function 'ping()' calls function 'pong()' defined at {{.*}}a.cpp:{{[0-9]+}}:6
// WPA-B:      note: Frame #3: function 'pong()' calls function 'hop()' defined at {{.*}}a.cpp:{{[0-9]+}}:13
// WPA-B:      note: ... which was the starting point of the recursive call chain; there may be other cycles
// WPA-B-NOT:  warning:
