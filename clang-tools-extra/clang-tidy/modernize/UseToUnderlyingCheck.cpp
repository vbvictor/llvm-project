//===--- UseToUnderlyingCheck.cpp - clang-tidy ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "UseToUnderlyingCheck.h"
#include "../utils/OptionsUtils.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"

using namespace clang::ast_matchers;

namespace clang::tidy::modernize {

UseToUnderlyingCheck::UseToUnderlyingCheck(StringRef Name,
                                           ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      IncludeInserter(Options.getLocalOrGlobal("IncludeStyle",
                                               utils::IncludeSorter::IS_LLVM),
                      areDiagsSelfContained()),
      CustomUnderlyingFunction(Options.get("UnderlyingFunction", "")),
      CustomUnderlyingHeader(Options.get("UnderlyingHeader", "")) {}

void UseToUnderlyingCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "IncludeStyle", IncludeInserter.getStyle());
  Options.store(Opts, "UnderlyingFunction", CustomUnderlyingFunction);
  Options.store(Opts, "UnderlyingHeader", CustomUnderlyingHeader);
}

void UseToUnderlyingCheck::registerPPCallbacks(const SourceManager &SM,
                                               Preprocessor *PP,
                                               Preprocessor *ModuleExpanderPP) {
  IncludeInserter.registerPreprocessor(PP);
}

void UseToUnderlyingCheck::registerMatchers(MatchFinder *Finder) {
  // Match static_cast expressions where:
  // 1. The source type is an enum (scoped or unscoped)
  // 2. The destination type is an integral type
  Finder->addMatcher(
      cxxStaticCastExpr(
          hasSourceExpression(expr(hasType(enumType())).bind("enum_expr")),
          hasDestinationType(
              qualType(
                  anyOf(isInteger(), type(hasUnqualifiedDesugaredType(
                                         enumType().bind("underlying_enum")))))
                  .bind("dest_type")))
          .bind("cast"),
      this);
}

void UseToUnderlyingCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *CastExpr = Result.Nodes.getNodeAs<CXXStaticCastExpr>("cast");
  const auto *EnumExpr = Result.Nodes.getNodeAs<Expr>("enum_expr");
  const auto *DestType = Result.Nodes.getNodeAs<QualType>("dest_type");

  if (!CastExpr || !EnumExpr || !DestType)
    return;

  // Get the enum type
  QualType EnumType = EnumExpr->getType();
  const EnumDecl *Enum = nullptr;

  if (const auto *ET = EnumType->getAs<clang::EnumType>()) {
    Enum = ET->getOriginalDecl();
  }

  if (!Enum)
    return;

  // Exception: Don't flag unscoped enums without explicit underlying type
  // as their underlying type is implementation-defined
  if (!Enum->isScoped() && !Enum->isFixed())
    return;

  // Check if we're casting to a different enum's underlying type
  if (Result.Nodes.getNodeAs<clang::EnumType>("underlying_enum")) {
    // This is casting to another enum type, not to an integral type
    return;
  }

  // Check if the destination type matches the underlying type
  QualType UnderlyingType = Enum->getIntegerType();
  bool IsSameType = Result.Context->hasSameUnqualifiedType(
      DestType->getCanonicalType().getUnqualifiedType(),
      UnderlyingType.getUnqualifiedType());

  // Get source location for the static_cast
  SourceLocation CastLoc = CastExpr->getBeginLoc();

  // Build the diagnostic message
  std::string DiagMsg;
  bool UseStd = CustomUnderlyingFunction.empty();

  if (IsSameType) {
    if (UseStd) {
      // Check if we're in C++23 or later
      if (getLangOpts().CPlusPlus23) {
        DiagMsg = "use std::to_underlying instead of static_cast to "
                  "convert enum to its underlying type";
      } else {
        DiagMsg = "use std::underlying_type_t instead of static_cast to "
                  "convert enum to its underlying type";
      }
    } else {
      DiagMsg =
          "use " + CustomUnderlyingFunction.str() +
          " instead of static_cast to convert enum to its underlying type";
    }
  } else {
    if (UseStd) {
      if (getLangOpts().CPlusPlus23) {
        DiagMsg = "static_cast to a different type than the underlying type; "
                  "consider using std::to_underlying first";
      } else {
        DiagMsg = "static_cast to a different type than the underlying type; "
                  "consider using std::underlying_type_t first";
      }
    } else {
      DiagMsg = "static_cast to a different type than the underlying type; "
                "consider using " +
                CustomUnderlyingFunction.str() + " first";
    }
  }

  auto Diag = diag(CastLoc, DiagMsg);

  // Get the source text of the enum expression
  CharSourceRange ExprRange = CharSourceRange::getTokenRange(
      EnumExpr->getBeginLoc(), EnumExpr->getEndLoc());
  StringRef ExprText =
      Lexer::getSourceText(ExprRange, *Result.SourceManager, getLangOpts());

  // Prepare the replacement text
  std::string Replacement;

  if (UseStd) {
    // Check if we're in C++23 or later
    bool IsCpp23 = getLangOpts().CPlusPlus23;

    if (IsCpp23) {
      Replacement = "std::to_underlying(" + ExprText.str() + ")";

      // Add the include for <utility>
      if (auto IncludeFixit = IncludeInserter.createIncludeInsertion(
              Result.SourceManager->getFileID(CastLoc), "<utility>")) {
        Diag << *IncludeFixit;
      }
    } else {
      // For pre-C++23, suggest using underlying_type_t
      Replacement = "static_cast<std::underlying_type_t<" +
                    EnumType.getAsString() + ">>(" + ExprText.str() + ")";

      // Add the include for <type_traits>
      if (auto IncludeFixit = IncludeInserter.createIncludeInsertion(
              Result.SourceManager->getFileID(CastLoc), "<type_traits>")) {
        Diag << *IncludeFixit;
      }
    }
  } else {
    // Use custom function
    Replacement = CustomUnderlyingFunction.str() + "(" + ExprText.str() + ")";

    // Add custom header if specified
    if (!CustomUnderlyingHeader.empty()) {
      StringRef Header = CustomUnderlyingHeader;

      if (auto IncludeFixit = IncludeInserter.createIncludeInsertion(
              Result.SourceManager->getFileID(CastLoc), Header.str())) {
        Diag << *IncludeFixit;
      }
    }
  }

  // If not casting to same type and we need additional cast
  if (!IsSameType) {
    Replacement =
        "static_cast<" + DestType->getAsString() + ">(" + Replacement + ")";
  }

  // Create the fix-it hint
  CharSourceRange CastRange = CharSourceRange::getTokenRange(
      CastExpr->getBeginLoc(), CastExpr->getEndLoc());
  Diag << FixItHint::CreateReplacement(CastRange, Replacement);
}

} // namespace clang::tidy::modernize