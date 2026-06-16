// Tests run-clang-tidy.py's fix deduplication against YAML shapes that are hard
// to produce from a real clang-tidy run. run-clang-tidy.py deduplicates every
// replacement file in the -export-fixes directory, so the test seeds that
// directory with crafted fixes and checks which survive. Files whose fixes are
// dropped are rewritten with an empty 'Diagnostics' list; untouched files keep
// their contents.
//
// Deduplication parses the exported YAML, which requires PyYAML.
// REQUIRES: pyyaml
//
// RUN: rm -rf %t
// RUN: cp -r %S/Inputs/run-clang-tidy-deduplicate-fixes-yaml %t
// RUN: mkdir -p %t/fixes
// RUN: cp %S/Inputs/deduplicate-fixes/*.yaml %t/fixes/
// RUN: sed -e "s#OUT_DIR#%/t#g" %S/Inputs/run-clang-tidy-deduplicate-fixes-yaml/compile_commands.json.in > %t/compile_commands.json
//
// Four duplicates are dropped: builddir-2, resolve-2, note-2 and multi-2. The
// empty and null-document files must be skipped without error.
// RUN: %run_clang_tidy -p %t -checks='-*,misc-const-correctness' -export-fixes=%t/fixes/ 'a.cpp' 2>&1 | FileCheck %s --check-prefix=MSG
// MSG: Removed 4 duplicate fix(es) from headers.

// Identical fixes with a missing BuildDirectory deduplicate.
// RUN: FileCheck --input-file=%t/fixes/builddir-1.yaml %s --check-prefix=BD-KEEP
// RUN: FileCheck --input-file=%t/fixes/builddir-2.yaml %s --check-prefix=BD-DROP
// BD-KEEP: ReplacementText: 'const '
// BD-DROP: Diagnostics: []
// BD-DROP-NOT: ReplacementText

// The same file reached through different relative spellings, each anchored at
// its own BuildDirectory, deduplicates.
// RUN: FileCheck --input-file=%t/fixes/resolve-1.yaml %s --check-prefix=RR-KEEP
// RUN: FileCheck --input-file=%t/fixes/resolve-2.yaml %s --check-prefix=RR-DROP
// RR-KEEP: ReplacementText: 'const '
// RR-DROP: Diagnostics: []
// RR-DROP-NOT: ReplacementText

// Diagnostics without replacements (plain warnings) are never collapsed.
// RUN: FileCheck --input-file=%t/fixes/nofix-1.yaml %s --check-prefix=NF-1
// RUN: FileCheck --input-file=%t/fixes/nofix-2.yaml %s --check-prefix=NF-2
// NF-1: DiagnosticName: some-warning
// NF-2: DiagnosticName: some-warning
// NF-2-NOT: Diagnostics: []

// Fixes attached to notes participate in deduplication.
// RUN: FileCheck --input-file=%t/fixes/note-1.yaml %s --check-prefix=NT-KEEP
// RUN: FileCheck --input-file=%t/fixes/note-2.yaml %s --check-prefix=NT-DROP
// NT-KEEP: ReplacementText: 'const '
// NT-DROP: Diagnostics: []
// NT-DROP-NOT: ReplacementText

// A diagnostic with several replacements deduplicates only against an identical
// set; a different replacement text keeps the diagnostic.
// RUN: FileCheck --input-file=%t/fixes/multi-1.yaml %s --check-prefix=MU-KEEP
// RUN: FileCheck --input-file=%t/fixes/multi-2.yaml %s --check-prefix=MU-DROP
// RUN: FileCheck --input-file=%t/fixes/multi-3.yaml %s --check-prefix=MU-DIFF
// MU-KEEP-COUNT-2: ReplacementText:
// MU-DROP: Diagnostics: []
// MU-DROP-NOT: ReplacementText
// MU-DIFF: ReplacementText: 'Z'

// Distinct fixes (different offsets) in one file are all kept.
// RUN: FileCheck --input-file=%t/fixes/offset-1.yaml %s --check-prefix=OF
// OF-COUNT-2: ReplacementText: 'const '
