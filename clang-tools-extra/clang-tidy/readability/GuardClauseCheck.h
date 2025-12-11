//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_READABILITY_GUARDCLAUSECHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_READABILITY_GUARDCLAUSECHECK_H

#include "../ClangTidyCheck.h"

namespace clang::tidy::readability {

/// Detects opportunities to introduce guard clauses to reduce nesting.
///
/// The check identifies when an if statement wraps most of a function's or
/// loop's logic and suggests using an inverted condition with an early return
/// or continue statement instead.
///
/// For the user-facing documentation see:
/// https://clang.llvm.org/extra/clang-tidy/checks/readability/guard-clause.html
class GuardClauseCheck : public ClangTidyCheck {
public:
  GuardClauseCheck(StringRef Name, ClangTidyContext *Context);
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
  void storeOptions(ClangTidyOptions::OptionMap &Opts) override;
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return LangOpts.CPlusPlus;
  }

private:
  const unsigned MinimumLines;
  const bool IgnoreIfWithInitializer;
};

} // namespace clang::tidy::readability

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_READABILITY_GUARDCLAUSECHECK_H
