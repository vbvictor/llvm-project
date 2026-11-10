// REQUIRES: ssaf
//
// The result of a whole-program analysis of the Scalable Static Analysis
// Framework is given to clang-tidy with the options of clang that name it:
//   --ssaf-global-scope-analysis-result=<path>.<format>
//   --ssaf-link-unit-id=<id>
//   --ssaf-compilation-unit-id=<id>   (optional, the main file by default)
// This test covers how clang-tidy reacts to a result it cannot use. The
// cross-translation-unit mode of a check is covered by the test of the check.

// RUN: rm -rf %t && mkdir -p %t

// The result file does not exist.
// RUN: not clang-tidy -checks='-*,misc-no-recursion' %s -- \
// RUN:   --ssaf-global-scope-analysis-result=%t/missing.json \
// RUN:   --ssaf-link-unit-id=prog 2>&1 \
// RUN:   | FileCheck %s --check-prefix=MISSING
// MISSING: error: failed to read the whole-program analysis result '{{.*}}missing.json'
// MISSING: {{.*}} [clang-tidy-config]

// The extension of the result file names no known serialization format.
// RUN: echo '{}' > %t/wpa.unknown
// RUN: not clang-tidy -checks='-*,misc-no-recursion' %s -- \
// RUN:   --ssaf-global-scope-analysis-result=%t/wpa.unknown \
// RUN:   --ssaf-link-unit-id=prog 2>&1 \
// RUN:   | FileCheck %s --check-prefix=FORMAT
// FORMAT: error: {{.*}}wpa.unknown{{.*}} [clang-tidy-config]

// The file has the right extension, but is not a result.
// RUN: echo '{}' > %t/wpa.json
// RUN: not clang-tidy -checks='-*,misc-no-recursion' %s -- \
// RUN:   --ssaf-global-scope-analysis-result=%t/wpa.json \
// RUN:   --ssaf-link-unit-id=prog 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CORRUPT
// CORRUPT: error: failed to read the whole-program analysis result '{{.*}}wpa.json'
// CORRUPT: {{.*}} [clang-tidy-config]

// The link unit is not named: names of entities cannot be told without it.
// RUN: not clang-tidy -checks='-*,misc-no-recursion' %s -- \
// RUN:   --ssaf-global-scope-analysis-result=%t/wpa.json 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NOLU
// NOLU: error: {{.*}}--ssaf-link-unit-id{{.*}} [clang-tidy-config]

// A valid result that holds no RecursionAnalysisResult. The check cannot use
// it. This is reported, but the check still finds what one translation unit
// shows, and clang-tidy does not fail.
// RUN: %clang -fsyntax-only --ssaf-extract-summaries=PointerFlow \
// RUN:   --ssaf-tu-summary-file=%t/pointer-flow.json \
// RUN:   --ssaf-compilation-unit-id=this %s
// RUN: clang-ssaf-linker %t/pointer-flow.json -o %t/other.json
// RUN: clang-ssaf-analyzer -a PointerFlowAnalysisResult %t/other.json \
// RUN:   -o %t/other-wpa.json
// RUN: clang-tidy -checks='-*,misc-no-recursion' %s -- \
// RUN:   --ssaf-global-scope-analysis-result=%t/other-wpa.json \
// RUN:   --ssaf-link-unit-id=other 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NORESULT
// NORESULT: warning: {{.*}}RecursionAnalysisResult{{.*}} [clang-tidy-config]
// NORESULT: warning: function 'self' is within a recursive call chain [misc-no-recursion]

int *Global;
void assign(int *P) { Global = P; } // Gives PointerFlow something to extract.
void self() { self(); }
