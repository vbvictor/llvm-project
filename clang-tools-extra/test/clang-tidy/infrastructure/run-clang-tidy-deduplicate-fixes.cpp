// Tests that run-clang-tidy.py deduplicates identical fixes that originate from
// a header shared by several translation units, across every way the same
// header can be spelled in a compilation database. Without deduplication the
// same insertion is applied once per translation unit and the edits stack
// (e.g. "int const const value").
//
// The analysis files live under Inputs/; each scenario is copied into a fresh
// build directory and its compile_commands.json is generated from the matching
// template (OUT_DIR is replaced with the scenario's build directory).
//
// Deduplication parses the exported YAML, which requires PyYAML.
// REQUIRES: pyyaml
//
// RUN: rm -rf %t
// RUN: mkdir -p %t

//===----------------------------------------------------------------------===//
// 1) Relative '-I' anchored at different build directories.
//    The header is recorded as '../inc/shared.h' (built in d1) and
//    '../../inc/shared.h' (built in d2/sub): two different spellings, each
//    relative to its own BuildDirectory, both resolving to the same file.
//===----------------------------------------------------------------------===//
// RUN: cp -r %S/Inputs/run-clang-tidy-deduplicate-fixes/rel %t/rel
// RUN: sed -e "s#OUT_DIR#%/t/rel#g" %S/Inputs/run-clang-tidy-deduplicate-fixes/rel/compile_commands.json.in > %t/rel/compile_commands.json
//
// The merged export must contain the fix exactly once.
// RUN: %run_clang_tidy -p %t/rel -checks='-*,misc-const-correctness' -header-filter='.*' -export-fixes=%t/rel/fixes.yaml -j 2 'c.cpp|e.cpp'
// RUN: FileCheck --input-file=%t/rel/fixes.yaml %s --check-prefix=REL-YAML
// REL-YAML-COUNT-1: ReplacementText: 'const '
// REL-YAML-NOT: ReplacementText
//
// Applying the fixes inserts 'const' once, not twice.
// RUN: %run_clang_tidy -p %t/rel -checks='-*,misc-const-correctness' -header-filter='.*' -fix -j 2 'c.cpp|e.cpp' 2>&1 | FileCheck %s --check-prefix=REL-MSG
// REL-MSG: Removed 1 duplicate fix(es) from headers.
// RUN: FileCheck --input-file=%t/rel/inc/shared.h %s --check-prefix=REL-FIX
// REL-FIX: int const value = 1;
// REL-FIX-NOT: const const

//===----------------------------------------------------------------------===//
// 2) Absolute '-I' in both translation units (identical spelling).
//===----------------------------------------------------------------------===//
// RUN: cp -r %S/Inputs/run-clang-tidy-deduplicate-fixes/abs %t/abs
// RUN: sed -e "s#OUT_DIR#%/t/abs#g" %S/Inputs/run-clang-tidy-deduplicate-fixes/abs/compile_commands.json.in > %t/abs/compile_commands.json
// RUN: %run_clang_tidy -p %t/abs -checks='-*,misc-const-correctness' -header-filter='.*' -fix -j 2 'a.cpp|b.cpp' 2>&1 | FileCheck %s --check-prefix=ABS-MSG
// ABS-MSG: Removed 1 duplicate fix(es) from headers.
// RUN: FileCheck --input-file=%t/abs/inc/shared.h %s --check-prefix=ABS-FIX
// ABS-FIX: int const value = 1;
// ABS-FIX-NOT: const const

//===----------------------------------------------------------------------===//
// 3) Mixed spellings: one translation unit uses an absolute '-I', the other a
//    relative '-I', for the same physical header.
//===----------------------------------------------------------------------===//
// RUN: cp -r %S/Inputs/run-clang-tidy-deduplicate-fixes/mixed %t/mixed
// RUN: sed -e "s#OUT_DIR#%/t/mixed#g" %S/Inputs/run-clang-tidy-deduplicate-fixes/mixed/compile_commands.json.in > %t/mixed/compile_commands.json
// RUN: %run_clang_tidy -p %t/mixed -checks='-*,misc-const-correctness' -header-filter='.*' -fix -j 2 'a.cpp|b.cpp' 2>&1 | FileCheck %s --check-prefix=MIXED-MSG
// MIXED-MSG: Removed 1 duplicate fix(es) from headers.
// RUN: FileCheck --input-file=%t/mixed/inc/shared.h %s --check-prefix=MIXED-FIX
// MIXED-FIX: int const value = 1;
// MIXED-FIX-NOT: const const

//===----------------------------------------------------------------------===//
// 4) Quoted include resolved relative to the including file, reached through
//    different spellings ("shared.h" and "../shared.h").
//===----------------------------------------------------------------------===//
// RUN: cp -r %S/Inputs/run-clang-tidy-deduplicate-fixes/quote %t/quote
// RUN: sed -e "s#OUT_DIR#%/t/quote#g" %S/Inputs/run-clang-tidy-deduplicate-fixes/quote/compile_commands.json.in > %t/quote/compile_commands.json
// RUN: %run_clang_tidy -p %t/quote -checks='-*,misc-const-correctness' -header-filter='.*' -fix -j 2 'a.cpp|b.cpp' 2>&1 | FileCheck %s --check-prefix=QUOTE-MSG
// QUOTE-MSG: Removed 1 duplicate fix(es) from headers.
// RUN: FileCheck --input-file=%t/quote/shared.h %s --check-prefix=QUOTE-FIX
// QUOTE-FIX: int const value = 1;
// QUOTE-FIX-NOT: const const

//===----------------------------------------------------------------------===//
// 5) Three translation units: two duplicates must be dropped.
//===----------------------------------------------------------------------===//
// RUN: cp -r %S/Inputs/run-clang-tidy-deduplicate-fixes/three %t/three
// RUN: sed -e "s#OUT_DIR#%/t/three#g" %S/Inputs/run-clang-tidy-deduplicate-fixes/three/compile_commands.json.in > %t/three/compile_commands.json
// RUN: %run_clang_tidy -p %t/three -checks='-*,misc-const-correctness' -header-filter='.*' -fix -j 3 'a.cpp|b.cpp|c.cpp' 2>&1 | FileCheck %s --check-prefix=THREE-MSG
// THREE-MSG: Removed 2 duplicate fix(es) from headers.
// RUN: FileCheck --input-file=%t/three/inc/shared.h %s --check-prefix=THREE-FIX
// THREE-FIX: int const value = 1;
// THREE-FIX-NOT: const const

//===----------------------------------------------------------------------===//
// 6) Distinct fixes in the same shared header must all survive: deduplication
//    must not merge fixes that only differ in location. Two variables yield two
//    different fixes; each appears in two translation units, so exactly two
//    duplicates are dropped and both fixes are still applied.
//===----------------------------------------------------------------------===//
// RUN: cp -r %S/Inputs/run-clang-tidy-deduplicate-fixes/preserve %t/preserve
// RUN: sed -e "s#OUT_DIR#%/t/preserve#g" %S/Inputs/run-clang-tidy-deduplicate-fixes/preserve/compile_commands.json.in > %t/preserve/compile_commands.json
//
// The merged export keeps both distinct fixes (two replacements).
// RUN: %run_clang_tidy -p %t/preserve -checks='-*,misc-const-correctness' -header-filter='.*' -export-fixes=%t/preserve/fixes.yaml -j 2 'x.cpp|y.cpp'
// RUN: FileCheck --input-file=%t/preserve/fixes.yaml %s --check-prefix=PRESERVE-YAML
// PRESERVE-YAML-COUNT-2: ReplacementText: 'const '
// PRESERVE-YAML-NOT: ReplacementText
//
// RUN: %run_clang_tidy -p %t/preserve -checks='-*,misc-const-correctness' -header-filter='.*' -fix -j 2 'x.cpp|y.cpp' 2>&1 | FileCheck %s --check-prefix=PRESERVE-MSG
// PRESERVE-MSG: Removed 2 duplicate fix(es) from headers.
// RUN: FileCheck --input-file=%t/preserve/inc/shared.h %s --check-prefix=PRESERVE-FIX
// PRESERVE-FIX: int const a = 1;
// PRESERVE-FIX: int const b = 2;
// PRESERVE-FIX-NOT: const const
