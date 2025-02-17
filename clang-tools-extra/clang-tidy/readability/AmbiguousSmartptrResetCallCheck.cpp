//===--- AmbiguousSmartptrResetCallCheck.cpp - clang-tidy -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "AmbiguousSmartptrResetCallCheck.h"
#include "../utils/OptionsUtils.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::readability {

namespace {

AST_MATCHER_P(CallExpr, everyArgumentMatches,
              ast_matchers::internal::Matcher<Expr>, InnerMatcher) {
  for (const auto *Arg : Node.arguments()) {
    if (!InnerMatcher.matches(*Arg, Finder, Builder))
      return false;
  }

  return true;
}

AST_MATCHER(CXXMethodDecl, hasOnlyDefaultParameters) {
  for (const auto *Param : Node.parameters()) {
    if (!Param->hasDefaultArg())
      return false;
  }

  return true;
}

const auto DefaultSmartPointers = "::std::shared_ptr;::std::unique_ptr";
} // namespace

AmbiguousSmartptrResetCallCheck::AmbiguousSmartptrResetCallCheck(
    StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      SmartPointers(utils::options::parseStringList(
          Options.get("SmartPointers", DefaultSmartPointers))) {}

void AmbiguousSmartptrResetCallCheck::storeOptions(
    ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "SmartPointers",
                utils::options::serializeStringList(SmartPointers));
}

void AmbiguousSmartptrResetCallCheck::registerMatchers(MatchFinder *Finder) {
  const auto IsSmartptr = hasAnyName(SmartPointers);

  const auto ResetMethod =
      cxxMethodDecl(hasName("reset"), hasOnlyDefaultParameters());

  const auto TypeWithReset =
      anyOf(cxxRecordDecl(hasMethod(ResetMethod)),
            classTemplateSpecializationDecl(
                hasSpecializedTemplate(classTemplateDecl(has(ResetMethod)))));

  const auto SmartptrWithBugproneReset = classTemplateSpecializationDecl(
      IsSmartptr,
      hasTemplateArgument(
          0, templateArgument(refersToType(hasUnqualifiedDesugaredType(
                 recordType(hasDeclaration(TypeWithReset)))))));

  // Find a.reset() calls
  Finder->addMatcher(
      cxxMemberCallExpr(callee(memberExpr(member(hasName("reset")))),
                        everyArgumentMatches(cxxDefaultArgExpr()),
                        on(expr(hasType(SmartptrWithBugproneReset))))
          .bind("smartptrResetCall"),
      this);

  // Find a->reset() calls
  Finder->addMatcher(
      cxxMemberCallExpr(
          callee(memberExpr(
              member(ResetMethod),
              hasObjectExpression(
                  cxxOperatorCallExpr(
                      hasOverloadedOperatorName("->"),
                      hasArgument(
                          0, expr(hasType(
                                 classTemplateSpecializationDecl(IsSmartptr)))))
                      .bind("OpCall")))),
          everyArgumentMatches(cxxDefaultArgExpr()))
          .bind("objectResetCall"),
      this);
}

void AmbiguousSmartptrResetCallCheck::check(
    const MatchFinder::MatchResult &Result) {

  if (const auto *SmartptrResetCall =
          Result.Nodes.getNodeAs<CXXMemberCallExpr>("smartptrResetCall")) {
    const auto *Member = cast<MemberExpr>(SmartptrResetCall->getCallee());

    diag(SmartptrResetCall->getBeginLoc(),
         "ambiguous call to 'reset()' on a smart pointer with pointee that "
         "also has a 'reset()' method, prefer more explicit approach");

    diag(SmartptrResetCall->getBeginLoc(),
         "consider assigning the pointer to 'nullptr' here",
         DiagnosticIDs::Note)
        << FixItHint::CreateReplacement(
               SourceRange(Member->getOperatorLoc(),
                           Member->getOperatorLoc().getLocWithOffset(0)),
               " =")
        << FixItHint::CreateReplacement(
               SourceRange(Member->getMemberLoc(),
                           SmartptrResetCall->getEndLoc()),
               " nullptr");
    return;
  }

  if (const auto *ObjectResetCall =
          Result.Nodes.getNodeAs<CXXMemberCallExpr>("objectResetCall")) {
    const auto *Arrow = Result.Nodes.getNodeAs<CXXOperatorCallExpr>("OpCall");

    const CharSourceRange SmartptrSourceRange =
        Lexer::getAsCharRange(Arrow->getArg(0)->getSourceRange(),
                              *Result.SourceManager, getLangOpts());

    diag(ObjectResetCall->getBeginLoc(),
         "ambiguous call to 'reset()' on a pointee of a smart pointer, prefer "
         "more explicit approach");

    diag(ObjectResetCall->getBeginLoc(),
         "consider dereferencing smart pointer to call 'reset' method "
         "of the pointee here",
         DiagnosticIDs::Note)
        << FixItHint::CreateInsertion(SmartptrSourceRange.getBegin(), "(*")
        << FixItHint::CreateInsertion(SmartptrSourceRange.getEnd(), ")")
        << FixItHint::CreateReplacement(
               CharSourceRange::getCharRange(
                   Arrow->getOperatorLoc(),
                   Arrow->getOperatorLoc().getLocWithOffset(2)),
               ".");
  }
}

} // namespace clang::tidy::readability
