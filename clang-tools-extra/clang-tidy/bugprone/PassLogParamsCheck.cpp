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

      // Find format specifier start
      size_t SpecStart = Pos + 1;
      size_t SpecPos = SpecStart;

      // Skip width/precision numbers and flags
      while (SpecPos < Str.size() &&
             (isdigit(Str[SpecPos]) || Str[SpecPos] == '.' ||
              Str[SpecPos] == '+' || Str[SpecPos] == '-' ||
              Str[SpecPos] == ' ' || Str[SpecPos] == '#')) {
        SpecPos++;
      }

      // Find length modifier and format specifier
      size_t LengthStart = SpecPos;
      while (SpecPos < Str.size() &&
             (Str[SpecPos] == 'h' || Str[SpecPos] == 'l' ||
              Str[SpecPos] == 'j' || Str[SpecPos] == 'z' ||
              Str[SpecPos] == 't' || Str[SpecPos] == 'L')) {
        SpecPos++;
      }

      if (SpecPos < Str.size()) {
        // Get the complete format specifier including length modifier
        StringRef CompleteSpec =
            Str.substr(LengthStart, SpecPos - LengthStart + 1);

        if (ArgIndex < Call->getNumArgs()) {
          const Expr *Arg = Call->getArg(ArgIndex)->IgnoreImplicitAsWritten();
          if (!checkArgumentType(Arg, CompleteSpec, Result.Context)) {
            diag(Arg->getBeginLoc(),
                 "argument type <%0> does not match format specifier '%%%1'")
                << Arg->getType().getAsString() << CompleteSpec;
          }
        }
        ArgIndex++;
      }

      // Update position to end of format specifier
      Pos = SpecPos;
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
                                           StringRef FormatSpecifier,
                                           ASTContext *Context) {
  QualType ArgType = Arg->getType();

  // Extract length modifier and format specifier
  StringRef LengthMod;
  char BaseSpec = FormatSpecifier.back();
  if (FormatSpecifier.size() > 1) {
    LengthMod = FormatSpecifier.drop_back();
  }

  switch (BaseSpec) {
  // case 'x':
  // case 'o':
  case 'u':
  case 'd':
  case 'i': {
    if (!ArgType->isIntegerType() && !ArgType->isEnumeralType())
      return false;

    if (BaseSpec == 'u' && !ArgType->isUnsignedIntegerType())
      return false;
    if (BaseSpec != 'u' && ArgType->isUnsignedIntegerType()) {
      return false;
    }

    uint64_t Width = Context->getTypeSize(ArgType);

    // Check exact width based on length modifier
    if (LengthMod == "hh")
      return Width == 8; // char
    if (LengthMod == "h")
      return Width == 16; // short
    if (LengthMod == "l")
      return Width == 32; // long
    if (LengthMod == "ll" || LengthMod == "z")
      return Width == 64; // long long
    return Width == 32;   // default int
  }

  case 'f':
  case 'F':
  case 'g':
  case 'G':
  case 'e':
  case 'E': {
    if (!ArgType->isRealFloatingType())
      return false;

    uint64_t Width = Context->getTypeSize(ArgType);

    if (LengthMod == "l")
      return Width == 64; // double
    return Width == 32;   // float
  }

  case 'c': {
    if (!ArgType->isCharType() && !ArgType->isIntegerType())
      return false;
    return Context->getTypeSize(ArgType) == 8; // Ensure 8-bit
  }

  // Rest of the cases remain same
  case 's':
    if (ArgType->isPointerType()) {
      QualType PointeeType = ArgType->getPointeeType();
      return PointeeType->isCharType();
    }
    if (const auto *RecordDecl = ArgType->getAsRecordDecl()) {
      return RecordDecl->getQualifiedNameAsString() == "std::basic_string" ||
             RecordDecl->getQualifiedNameAsString() == "std::string";
    }
    return false;

  case 'p':
    return ArgType->isPointerType();
  }

  return true; // Don't know what to do, skip
}

void PassLogParamsCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "StrLogLikeFunctions",
                utils::options::serializeStringList(StrLogLikeFunctions));
}

} // namespace clang::tidy::bugprone
