//===--- PassLogParamsCheck.h - clang-tidy ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BUGPRONE_PASSLOGPARAMSCHECK_H
#define LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BUGPRONE_PASSLOGPARAMSCHECK_H

#include "../ClangTidyCheck.h"
#include "clang/AST/Expr.h"

namespace clang::tidy::bugprone {

/// Check that assert that needed parameters are bassed to format-string in
/// log::info.
///
/// For the user-facing documentation see:
/// http://clang.llvm.org/extra/clang-tidy/checks/bugprone/pass-log-params.html
class PassLogParamsCheck : public ClangTidyCheck {
public:
  PassLogParamsCheck(StringRef Name, ClangTidyContext *Context);
  void registerMatchers(ast_matchers::MatchFinder *Finder) override;
  void check(const ast_matchers::MatchFinder::MatchResult &Result) override;
  bool isLanguageVersionSupported(const LangOptions &LangOpts) const override {
    return LangOpts.CPlusPlus;
  }
  void registerPPCallbacks(const SourceManager &SM, Preprocessor *PP,
                           Preprocessor *ModuleExpanderPP) override;
  void storeOptions(ClangTidyOptions::OptionMap &Opts) override;

private:
  std::vector<StringRef> StrLogLikeFunctions;
  Preprocessor *PP = nullptr;

  /// Lazily-created c_str() call matcher
  std::optional<clang::ast_matchers::StatementMatcher>
      StringCStrCallExprMatcher;

  std::vector<clang::ast_matchers::BoundNodes> ArgCStrRemovals;

  void findArgCStrRemoval(const Expr *Arg, ASTContext *Context);
  bool checkArgumentType(const Expr *Arg, char FormatSpecifier);
};

} // namespace clang::tidy::bugprone

#endif // LLVM_CLANG_TOOLS_EXTRA_CLANG_TIDY_BUGPRONE_PASSLOGPARAMSCHECK_H
