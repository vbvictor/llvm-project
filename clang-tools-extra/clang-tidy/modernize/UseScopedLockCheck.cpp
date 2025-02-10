//===--- UseScopedLockCheck.cpp - clang-tidy ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UseScopedLockCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Stmt.h"
#include "clang/AST/Type.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/ASTMatchers/ASTMatchers.h"
#include "clang/Basic/SourceLocation.h"
#include "clang/Lex/Lexer.h"
#include "llvm/ADT/Twine.h"
#include <iterator>
#include <optional>

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

namespace {

bool isLockGuard(const QualType &Type) {
  if (const auto *Record = Type->getAs<RecordType>()) {
    if (const RecordDecl *Decl = Record->getDecl()) {
      return Decl->getQualifiedNameAsString() == "std::lock_guard";
    }
  }

  if (const auto *TemplateSpecType =
          Type->getAs<TemplateSpecializationType>()) {
    if (const TemplateDecl *Decl =
            TemplateSpecType->getTemplateName().getAsTemplateDecl()) {
      return Decl->getQualifiedNameAsString() == "std::lock_guard";
    }
  }

  return false;
}

std::vector<const VarDecl *> getLockGuardsFromDecl(const DeclStmt *DS) {
  std::vector<const VarDecl *> LockGuards;

  for (const Decl *Decl : DS->decls()) {
    if (const auto *VD = dyn_cast<VarDecl>(Decl)) {
      const QualType Type = VD->getType().getCanonicalType();
      if (isLockGuard(Type)) {
        LockGuards.push_back(VD);
      }
    }
  }

  return LockGuards;
}

// Scans through the statements in a block and groups consecutive
// 'std::lock_guard' variable declarations together.
std::vector<std::vector<const VarDecl *>>
findLocksInCompoundStmt(const CompoundStmt *Block,
                        const ast_matchers::MatchFinder::MatchResult &Result) {
  // store groups of consecutive 'std::lock_guard' declarations
  std::vector<std::vector<const VarDecl *>> LockGuardGroups;
  std::vector<const VarDecl *> CurrentLockGuardGroup;

  auto AddAndClearCurrentGroup = [&]() {
    if (!CurrentLockGuardGroup.empty()) {
      LockGuardGroups.push_back(CurrentLockGuardGroup);
      CurrentLockGuardGroup.clear();
    }
  };

  for (const Stmt *Stmt : Block->body()) {
    if (const auto *DS = dyn_cast<DeclStmt>(Stmt)) {
      std::vector<const VarDecl *> LockGuards = getLockGuardsFromDecl(DS);

      if (!LockGuards.empty()) {
        CurrentLockGuardGroup.insert(
            CurrentLockGuardGroup.end(),
            std::make_move_iterator(LockGuards.begin()),
            std::make_move_iterator(LockGuards.end()));
      }

      if (LockGuards.empty()) {
        AddAndClearCurrentGroup();
      }
    } else {
      AddAndClearCurrentGroup();
    }
  }

  AddAndClearCurrentGroup();

  return LockGuardGroups;
}

// Find the exact source range of the 'lock_guard<...>' token
std::optional<SourceRange> getLockGuardRange(const VarDecl *LockGuard,
                                             SourceManager &SM) {
  const TypeSourceInfo *SourceInfo = LockGuard->getTypeSourceInfo();
  const TypeLoc Loc = SourceInfo->getTypeLoc();

  const auto ElaboratedLoc = Loc.getAs<ElaboratedTypeLoc>();
  if (!ElaboratedLoc)
    return std::nullopt;

  const auto TemplateLoc =
      ElaboratedLoc.getNamedTypeLoc().getAs<TemplateSpecializationTypeLoc>();
  if (!TemplateLoc)
    return std::nullopt;

  const SourceLocation LockGuardBeginLoc = TemplateLoc.getTemplateNameLoc();
  const SourceLocation LockGuardRAngleLoc = TemplateLoc.getRAngleLoc();

  return SourceRange(LockGuardBeginLoc, LockGuardRAngleLoc);
}

} // namespace

void UseScopedLockCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "WarnOnlyMultipleLocks", WarnOnlyMultipleLocks);
}

void UseScopedLockCheck::registerMatchers(MatchFinder *Finder) {
  auto LockGuardType = qualType(hasDeclaration(namedDecl(
      hasName("::std::lock_guard"),
      anyOf(classTemplateDecl(), classTemplateSpecializationDecl()))));
  auto LockVarDecl = varDecl(hasType(LockGuardType));

  // Match CompoundStmt with only one 'std::lock_guard'
  if (!WarnOnlyMultipleLocks) {
    Finder->addMatcher(
        compoundStmt(has(declStmt(has(LockVarDecl.bind("lock-decl-single")))),
                     unless(hasDescendant(declStmt(has(varDecl(
                         hasType(LockGuardType),
                         unless(equalsBoundNode("lock-decl-single")))))))),
        this);
  }

  // Match CompoundStmt with multiple 'std::lock_guard'
  Finder->addMatcher(
      compoundStmt(has(declStmt(has(LockVarDecl.bind("lock-decl-multiple")))),
                   hasDescendant(declStmt(has(varDecl(
                       hasType(LockGuardType),
                       unless(equalsBoundNode("lock-decl-multiple")))))))
          .bind("block-multiple"),
      this);
}

void UseScopedLockCheck::check(const MatchFinder::MatchResult &Result) {
  if (const auto *Decl = Result.Nodes.getNodeAs<VarDecl>("lock-decl-single")) {
    emitDiag(Decl, Result);
  }

  if (const auto *Compound =
          Result.Nodes.getNodeAs<CompoundStmt>("block-multiple")) {
    emitDiag(findLocksInCompoundStmt(Compound, Result), Result);
  }
}

void UseScopedLockCheck::emitDiag(const VarDecl *LockGuard,
                                  const MatchFinder::MatchResult &Result) {
  auto Diag = diag(LockGuard->getBeginLoc(),
                   "use 'std::scoped_lock' instead of 'std::lock_guard'");

  const std::optional<SourceRange> LockGuardTypeRange =
      getLockGuardRange(LockGuard, *Result.SourceManager);

  if (!LockGuardTypeRange) {
    return;
  }

  // Create Fix-its only if we can find the constructor call to handle
  // 'std::lock_guard l(m, std::adopt_lock)' case
  const auto *CtorCall = dyn_cast<CXXConstructExpr>(LockGuard->getInit());
  if (!CtorCall) {
    return;
  }

  switch (CtorCall->getNumArgs()) {
  case 1:
    Diag << FixItHint::CreateReplacement(LockGuardTypeRange.value(),
                                         "scoped_lock");
    return;
  case 2:
    const Expr *const *CtorArgs = CtorCall->getArgs();

    const Expr *MutexArg = CtorArgs[0];
    const Expr *AdoptLockArg = CtorArgs[1];

    const StringRef MutexSourceText = Lexer::getSourceText(
        CharSourceRange::getTokenRange(MutexArg->getSourceRange()),
        *Result.SourceManager, Result.Context->getLangOpts());
    const StringRef AdoptLockSourceText = Lexer::getSourceText(
        CharSourceRange::getTokenRange(AdoptLockArg->getSourceRange()),
        *Result.SourceManager, Result.Context->getLangOpts());

    Diag << FixItHint::CreateReplacement(LockGuardTypeRange.value(),
                                         "scoped_lock")
         << FixItHint::CreateReplacement(
                SourceRange(MutexArg->getBeginLoc(), AdoptLockArg->getEndLoc()),
                (llvm::Twine(AdoptLockSourceText) + ", " + MutexSourceText)
                    .str());
    return;
  }

  llvm_unreachable("Invalid argument number of std::lock_guard constructor");
}

void UseScopedLockCheck::emitDiag(
    const std::vector<std::vector<const VarDecl *>> &LockGuardGroups,
    const MatchFinder::MatchResult &Result) {
  for (const std::vector<const VarDecl *> &Group : LockGuardGroups) {
    if (Group.size() == 1 && !WarnOnlyMultipleLocks) {
      emitDiag(Group[0], Result);
    } else {
      diag(Group[0]->getBeginLoc(),
           "use single 'std::scoped_lock' instead of multiple "
           "'std::lock_guard'");

      for (size_t I = 1; I < Group.size(); ++I) {
        diag(Group[I]->getLocation(),
             "additional 'std::lock_guard' declared here", DiagnosticIDs::Note);
      }
    }
  }
}

} // namespace clang::tidy::modernize
