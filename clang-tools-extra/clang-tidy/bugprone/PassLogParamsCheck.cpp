//===--- PassLogParamsCheck.cpp - clang-tidy ------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PassLogParamsCheck.h"
#include "../utils/FixItHintUtils.h"
#include "../utils/FormatStringConverter.h"
#include "../utils/Matchers.h"
#include "../utils/OptionsUtils.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Expr.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Basic/LangOptions.h"
#include "clang/Lex/Lexer.h"
#include "clang/Lex/Preprocessor.h"
#include "clang/Tooling/FixIt.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Support/Debug.h"

using namespace clang::ast_matchers;

namespace clang::tidy::bugprone {

static bool isRealCharType(const clang::QualType &Ty) {
  using namespace clang;
  const Type *DesugaredType = Ty->getUnqualifiedDesugaredType();
  if (const auto *BT = llvm::dyn_cast<BuiltinType>(DesugaredType))
    return (BT->getKind() == BuiltinType::Char_U ||
            BT->getKind() == BuiltinType::Char_S);
  return false;
}

namespace {
AST_MATCHER(clang::QualType, isRealChar) { return isRealCharType(Node); }
} // namespace

const char *DefaultLogLikeFunctions =
    "log::trace;log::debug;log::info;log::warning;log::warn;log::error;log::"
    "critical;log::fatal;"
    "log::tracef;log::debugf;log::infof;log::warningf;log::warnf;log::errorf;"
    "log::criticalf;log::fatalf;"
    "Log::trace;Log::debug;Log::info;Log::warning;Log::warn;Log::error;Log::"
    "critical;Log::fatal;"
    "Log::tracef;Log::debugf;Log::infof;Log::warningf;Log::warnf;Log::errorf;"
    "Log::criticalf;Log::fatalf";

PassLogParamsCheck::PassLogParamsCheck(StringRef Name,
                                       ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      StrLogLikeFunctions(
          utils::options::parseStringList(Options.getLocalOrGlobal(
              "LogLikeFunctions", DefaultLogLikeFunctions))) {
  if (StrLogLikeFunctions.empty()) {
    StrLogLikeFunctions =
        utils::options::parseStringList(DefaultLogLikeFunctions);
  }
}

void PassLogParamsCheck::registerMatchers(MatchFinder *Finder) {
  // Match calls to any log-like function with string literal first arg
  Finder->addMatcher(
      callExpr(argumentCountAtLeast(1),
               hasArgument(0, stringLiteral().bind("format")),
               callee(functionDecl(
                   matchers::matchesAnyListedName(StrLogLikeFunctions))))
          .bind("logcall"),
      this);
}

void PassLogParamsCheck::registerPPCallbacks(const SourceManager &SM,
                                             Preprocessor *PP,
                                             Preprocessor *ModuleExpanderPP) {
  this->PP = PP;
}

void PassLogParamsCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *Call = Result.Nodes.getNodeAs<CallExpr>("logcall");
  const auto *FormatStr = Result.Nodes.getNodeAs<StringLiteral>("format");

  if (!Call || !FormatStr)
    return;

  // Count format specifiers in string
  StringRef Str = FormatStr->getString();
  unsigned FormatSpecifiers = 0;
  for (size_t Pos = 0; Pos < Str.size(); ++Pos) {
    if (Str[Pos] == '%' && Pos + 1 < Str.size() && Str[Pos + 1] != '%')
      ++FormatSpecifiers;
  }

  unsigned ArgIndex = 1;
  for (size_t Pos = 0; Pos < Str.size(); ++Pos) {
    if (Str[Pos] == '%' && Pos + 1 < Str.size()) {
      if (Str[Pos + 1] == '%') {
        Pos++; // Skip escaped %
        continue;
      }

      // Find format specifier
      size_t SpecPos = Pos + 1;
      while (SpecPos < Str.size() &&
             (isdigit(Str[SpecPos]) || Str[SpecPos] == '.' ||
              Str[SpecPos] == '+' || Str[SpecPos] == '-' ||
              Str[SpecPos] == ' ' || Str[SpecPos] == '#')) {
        SpecPos++;
      }

      if (SpecPos < Str.size()) {
        char FormatSpec = Str[SpecPos];
        if (ArgIndex < Call->getNumArgs()) {
          const Expr *Arg = Call->getArg(ArgIndex)->IgnoreImplicitAsWritten();
          if (!checkArgumentType(Arg, FormatSpec)) {
            diag(Arg->getBeginLoc(),
                 "argument type <%0> does not match format specifier '%%%1'")
                << Arg->getType().getAsString() << std::string(1, FormatSpec);
          }
        }
        ArgIndex++;
      }
    }
  }

  // Check if number of variadic args matches format specifiers
  unsigned NumArgs = Call->getNumArgs() - 1; // Subtract format string
  if (NumArgs != FormatSpecifiers) {
    diag(Call->getBeginLoc(),
         "format string requires %0 arguments but %1 were provided")
        << FormatSpecifiers << NumArgs;
  }

  // Process each argument
  for (unsigned I = 1; I < Call->getNumArgs(); ++I) {
    findArgCStrRemoval(Call->getArg(I), Result.Context);
  }

  // Handle found removals
  for (const auto &Match : ArgCStrRemovals) {
    if (const auto *Call = Match.getNodeAs<CXXMemberCallExpr>("call")) {
      diag(Call->getBeginLoc(), "unnecessary c_str() call")
          << FixItHint::CreateRemoval(CharSourceRange::getTokenRange(
                 Call->getExprLoc(), Call->getEndLoc()));
    }
  }
}

void PassLogParamsCheck::findArgCStrRemoval(const Expr *Arg,
                                            ASTContext *Context) {
  if (!StringCStrCallExprMatcher) {
    // Create matcher for std::string
    const auto StringDecl = type(hasUnqualifiedDesugaredType(recordType(
        hasDeclaration(cxxRecordDecl(hasName("::std::basic_string"))))));

    // Match both direct string and pointer to string
    const auto StringExpr = expr(
        anyOf(hasType(StringDecl), hasType(qualType(pointsTo(StringDecl)))));

    // Build the complete c_str() matcher
    StringCStrCallExprMatcher =
        cxxMemberCallExpr(
            on(StringExpr.bind("arg")), callee(memberExpr().bind("member")),
            callee(cxxMethodDecl(hasAnyName("c_str", "data"),
                                 returns(pointerType(pointee(isRealChar()))))))
            .bind("call");
  }

  // Run the matcher and store results
  auto CStrMatches = match(*StringCStrCallExprMatcher, *Arg, *Context);
  if (!CStrMatches.empty()) {
    ArgCStrRemovals.push_back(CStrMatches.front());
  }
}

bool PassLogParamsCheck::checkArgumentType(const Expr *Arg,
                                           char FormatSpecifier) {
  QualType ArgType = Arg->getType();
  switch (FormatSpecifier) {
  case 'd':
  case 'i':
    return ArgType->isIntegerType() || ArgType->isEnumeralType();

  case 'f':
  case 'F':
  case 'g':
  case 'G':
  case 'e':
  case 'E':
    return ArgType->isRealFloatingType();

  case 's':
    // Check for char* types
    if (ArgType->isPointerType()) {
      QualType PointeeType = ArgType->getPointeeType();
      return PointeeType->isCharType();
    }
    // Check for std::string
    if (const auto *RecordDecl = ArgType->getAsRecordDecl()) {
      // const RecordDecl *Decl = RecordType->getDefinition();
      return RecordDecl->getQualifiedNameAsString() == "std::basic_string" ||
             RecordDecl->getQualifiedNameAsString() == "std::string";
    }
    return false;

  case 'p':
    return ArgType->isPointerType();

  case 'c':
    return ArgType->isCharType();
  }

  return false;
}

void PassLogParamsCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "StrLogLikeFunctions",
                utils::options::serializeStringList(StrLogLikeFunctions));
}

} // namespace clang::tidy::bugprone
