//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UseBracedInitializationCheck.h"
#include "../utils/LexerUtils.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/ExprCXX.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::misc {

namespace {

AST_MATCHER_P(VarDecl, hasInitStyle, VarDecl::InitializationStyle, Style) {
  return Node.getInitStyle() == Style;
}

AST_MATCHER(VarDecl, hasDependentType) {
  return Node.getType()->isDependentType();
}

AST_MATCHER(CXXConstructExpr, hasWrittenParens) {
  const SourceRange Range = Node.getParenOrBraceRange();
  return Range.isValid() && !Range.getBegin().isMacroID() &&
         !Range.getEnd().isMacroID();
}

/// Matches CXXConstructExpr whose target class has any constructor
/// taking std::initializer_list. When such a constructor exists, braced
/// initialization may call it instead of the intended constructor.
AST_MATCHER(CXXConstructExpr, constructsTypeWithInitListCtor) {
  const CXXRecordDecl *RD = Node.getConstructor()->getParent();
  if (!RD || !RD->hasDefinition())
    return false;
  return llvm::any_of(RD->ctors(), [](const CXXConstructorDecl *Ctor) {
    if (Ctor->getNumParams() == 0)
      return false;
    const QualType FirstParam =
        Ctor->getParamDecl(0)->getType().getNonReferenceType();
    const auto *Record = FirstParam->getAsCXXRecordDecl();
    return Record && Record->getDeclName().isIdentifier() &&
           Record->getName() == "initializer_list" &&
           Record->isInStdNamespace();
  });
}

} // namespace

static constexpr StringRef WarningText =
    "use braced initialization instead of parenthesized initialization";

void UseBracedInitializationCheck::registerMatchers(MatchFinder *Finder) {
  auto GoodCtor = allOf(
      hasWrittenParens(), unless(constructsTypeWithInitListCtor()),
      unless(isInTemplateInstantiation()), unless(isListInitialization()));

  auto ParenCtorExpr = cxxConstructExpr(GoodCtor).bind("ctor");

  auto VarGuards =
      allOf(unless(hasDependentType()), unless(hasType(autoType())));

  // Variable declarations: Simple w(1); Agg d(1, 2); (C++20)
  Finder->addMatcher(
      varDecl(anyOf(allOf(hasInitStyle(VarDecl::CallInit),
                          hasInitializer(ignoringImplicit(ParenCtorExpr))),
                    hasInitStyle(VarDecl::ParenListInit)),
              VarGuards)
          .bind("var"),
      this);

  // Functional casts: Simple(1), Simple w = Simple(1), Agg(1, 2) (C++20)
  Finder->addMatcher(
      cxxFunctionalCastExpr(optionally(has(ParenCtorExpr))).bind("cast"), this);

  // Multi-arg temporaries: Simple(1, 2.0)
  Finder->addMatcher(cxxTemporaryObjectExpr(GoodCtor).bind("ctor"), this);

  // New expressions: new Simple(1), new Agg(1, 2) (C++20)
  Finder->addMatcher(cxxNewExpr(optionally(has(ParenCtorExpr))).bind("new"),
                     this);

  // Scalar direct-init: int x(42), double d(3.14)
  Finder->addMatcher(
      varDecl(hasInitStyle(VarDecl::CallInit),
              hasInitializer(unless(ignoringImplicit(cxxConstructExpr()))),
              VarGuards)
          .bind("scalar"),
      this);
}

void UseBracedInitializationCheck::check(
    const MatchFinder::MatchResult &Result) {
  SourceLocation DiagLoc;
  SourceLocation LParen;
  SourceLocation RParen;

  if (const auto *Ctor = Result.Nodes.getNodeAs<CXXConstructExpr>("ctor")) {
    DiagLoc = Ctor->getBeginLoc();
    LParen = Ctor->getParenOrBraceRange().getBegin();
    RParen = Ctor->getParenOrBraceRange().getEnd();
  } else if (const auto *Var = Result.Nodes.getNodeAs<VarDecl>("scalar")) {
    assert(Var->hasInit());
    const SourceManager &SM = *Result.SourceManager;
    const LangOptions &LangOpts = Result.Context->getLangOpts();

    const std::optional<Token> LTok =
        utils::lexer::findPreviousTokenSkippingComments(
            Var->getInit()->getBeginLoc(), SM, LangOpts);
    if (!LTok || LTok->isNot(tok::l_paren) || LTok->getLocation().isMacroID())
      return;

    const std::optional<Token> RTok =
        utils::lexer::findNextTokenSkippingComments(Var->getInit()->getEndLoc(),
                                                    SM, LangOpts);
    if (!RTok || RTok->isNot(tok::r_paren) || RTok->getLocation().isMacroID())
      return;

    DiagLoc = Var->getLocation();
    LParen = LTok->getLocation();
    RParen = RTok->getLocation();
  } else {
    // C++20 CXXParenListInitExpr from various contexts.
    const CXXParenListInitExpr *PLE = nullptr;
    if (const auto *Var = Result.Nodes.getNodeAs<VarDecl>("var")) {
      assert(Var->hasInit());
      PLE = dyn_cast<CXXParenListInitExpr>(Var->getInit());
      DiagLoc = Var->getLocation();
    } else if (const auto *Cast =
                   Result.Nodes.getNodeAs<CXXFunctionalCastExpr>("cast")) {
      PLE = dyn_cast<CXXParenListInitExpr>(Cast->getSubExpr());
      DiagLoc = Cast->getBeginLoc();
    } else if (const auto *New = Result.Nodes.getNodeAs<CXXNewExpr>("new")) {
      if (const auto *Init = New->getInitializer())
        PLE = dyn_cast<CXXParenListInitExpr>(Init);
      DiagLoc = New->getBeginLoc();
    } else {
      llvm_unreachable("No matches found");
    }
    if (!PLE)
      return;
    LParen = PLE->getBeginLoc();
    RParen = PLE->getEndLoc();
  }

  if (LParen.isMacroID() || RParen.isMacroID())
    return;

  diag(DiagLoc, WarningText) << FixItHint::CreateReplacement(LParen, "{")
                             << FixItHint::CreateReplacement(RParen, "}");
}

} // namespace clang::tidy::misc
