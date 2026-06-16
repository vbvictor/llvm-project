// Tests that run-clang-tidy.py deduplicates a shared-header fix even when one
// translation unit reaches the header through a symbolic link. The two
// translation units record different paths ('inc/shared.h' vs 'link/shared.h')
// that resolve to the same file, so the fix must still be applied only once.
//
// Deduplication parses the exported YAML, which requires PyYAML.
// REQUIRES: pyyaml
// Symlink creation is unreliable on Windows.
// UNSUPPORTED: system-windows
//
// RUN: rm -rf %t
// RUN: cp -r %S/Inputs/run-clang-tidy-deduplicate-fixes-symlink %t
// RUN: ln -s %t/inc %t/link
// RUN: sed -e "s#OUT_DIR#%/t#g" %S/Inputs/run-clang-tidy-deduplicate-fixes-symlink/compile_commands.json.in > %t/compile_commands.json
// RUN: %run_clang_tidy -p %t -checks='-*,misc-const-correctness' -header-filter='.*' -fix -j 2 'a.cpp|b.cpp' 2>&1 | FileCheck %s --check-prefix=CHECK-MSG
// CHECK-MSG: Removed 1 duplicate fix(es) from headers.
// RUN: FileCheck --input-file=%t/inc/shared.h %s --check-prefix=CHECK-FIX
// CHECK-FIX: int const value = 1;
// CHECK-FIX-NOT: const const
