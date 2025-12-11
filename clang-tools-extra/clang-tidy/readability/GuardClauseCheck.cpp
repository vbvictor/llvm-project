//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "GuardClauseCheck.h"
#include "clang/AST/ASTContext.h"
#include "clang/ASTMatchers/ASTMatchFinder.h"
#include "clang/Lex/Lexer.h"
#include "clang/Tooling/FixIt.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/Twine.h"

namespace clang::tidy::readability {

using namespace clang::ast_matchers;

static unsigned countLinesBetween(SourceLocation Begin, SourceLocation End,
                                  const SourceManager &SM) {
  if (Begin.isInvalid() || End.isInvalid())
    return 0;

  return SM.getSpellingLineNumber(End) - SM.getSpellingLineNumber(Begin) + 1;
}

// Count the number of physical lines inside a Then block (between braces for
// compound statements).
static unsigned countThenLines(const Stmt *Then, const SourceManager &SM) {
  if (Then == nullptr)
    return 0;

  // For compound statements, count actual physical lines between braces.
  // Formula: closing_brace_line - opening_brace_line - 1
  if (const auto *CS = dyn_cast<CompoundStmt>(Then)) {
    SourceLocation LBraceLoc = CS->getLBracLoc();
    SourceLocation RBraceLoc = CS->getRBracLoc();

    if (LBraceLoc.isInvalid() || RBraceLoc.isInvalid())
      return 0;

    unsigned BeginLine = SM.getSpellingLineNumber(LBraceLoc);
    unsigned EndLine = SM.getSpellingLineNumber(RBraceLoc);

    // If braces are on the same line, there are 0 lines between them
    if (EndLine <= BeginLine)
      return 0;

    return EndLine - BeginLine - 1;
  }

  // For non-compound statements, count the statement itself.
  return countLinesBetween(Then->getBeginLoc(), Then->getEndLoc(), SM);
}

// Check if the then branch is already a guard clause (just return/continue/break).
static bool isAlreadyGuardClause(const Stmt *Then) {
  if (!Then)
    return false;

  // Direct return/continue/break statement
  if (isa<ReturnStmt>(Then) || isa<ContinueStmt>(Then) || isa<BreakStmt>(Then))
    return true;

  // Compound statement with only a return/continue/break
  if (const auto *CS = dyn_cast<CompoundStmt>(Then)) {
    if (CS->size() == 1) {
      const Stmt *Single = CS->body_front();
      if (isa<ReturnStmt>(Single) || isa<ContinueStmt>(Single) ||
          isa<BreakStmt>(Single))
        return true;
    }
  }

  return false;
}

// Invert a condition by adding/removing negation.
static std::string invertCondition(const Expr *Cond,
                                   const ASTContext &Context) {
  const StringRef CondStr = tooling::fixit::getText(*Cond, Context);

  // If the condition is already a unary not operator, remove it.
  if (const auto *UnOp = dyn_cast<UnaryOperator>(Cond->IgnoreParenImpCasts())) {
    if (UnOp->getOpcode() == UO_LNot) {
      const Expr *SubExpr = UnOp->getSubExpr()->IgnoreParenImpCasts();
      return tooling::fixit::getText(*SubExpr, Context).str();
    }
  }

  // Otherwise, wrap in negation.
  // Add parentheses if the condition has low precedence operators.
  if (isa<BinaryOperator>(Cond->IgnoreParenImpCasts()) ||
      isa<ConditionalOperator>(Cond->IgnoreParenImpCasts()))
    return ("!(" + CondStr + ")").str();

  return ("!" + CondStr).str();
}

// Check if the if statement has an init-statement with variable declarations.
static bool hasInitWithVarDecl(const IfStmt *If) {
  const Stmt *Init = If->getInit();
  if (!Init)
    return false;

  // Check if the init-statement contains any variable declarations
  if (const auto *DS = dyn_cast<DeclStmt>(Init)) {
    for (const Decl *D : DS->decls()) {
      if (isa<VarDecl>(D))
        return true;
    }
  }

  return false;
}


GuardClauseCheck::GuardClauseCheck(StringRef Name, ClangTidyContext *Context)
    : ClangTidyCheck(Name, Context),
      MinimumLines(Options.get("MinimumLines", 5U)),
      IgnoreIfWithInitializer(Options.get("IgnoreIfWithInitializer", false)) {}

void GuardClauseCheck::storeOptions(ClangTidyOptions::OptionMap &Opts) {
  Options.store(Opts, "MinimumLines", MinimumLines);
  Options.store(Opts, "IgnoreIfWithInitializer", IgnoreIfWithInitializer);
}

void GuardClauseCheck::registerMatchers(MatchFinder *Finder) {
  // Common if statement constraints:
  // - Not constexpr/consteval (these have different semantics)
  // Note: else clause emptiness is checked in check() function
  const auto CommonIfConstraints =
      allOf(unless(isConstexpr()), unless(isConsteval()));

  // Match if statements in function bodies
  Finder->addMatcher(
      ifStmt(CommonIfConstraints,
             hasParent(compoundStmt(hasParent(functionDecl().bind("function")))
                           .bind("scope")))
          .bind("if"),
      this);

  // Match if statements in loop bodies
  const auto LoopTypes =
      anyOf(forStmt().bind("loop"), cxxForRangeStmt().bind("loop"),
            whileStmt().bind("loop"), doStmt().bind("loop"));

  Finder->addMatcher(
      ifStmt(CommonIfConstraints,
             hasParent(compoundStmt(hasParent(stmt(LoopTypes))).bind("scope")))
          .bind("if"),
      this);
}

void GuardClauseCheck::check(const MatchFinder::MatchResult &Result) {
  const auto *If = Result.Nodes.getNodeAs<IfStmt>("if");
  const auto *Scope = Result.Nodes.getNodeAs<CompoundStmt>("scope");
  const auto *Function = Result.Nodes.getNodeAs<FunctionDecl>("function");
  const auto *Loop = Result.Nodes.getNodeAs<Stmt>("loop");

  if (!If || !Scope)
    return;

  const SourceManager &SM = *Result.SourceManager;

  // Skip if there is any else clause (fix-it can't handle it properly).
  if (If->getElse())
    return;

  // Get the 'then' branch.
  const Stmt *Then = If->getThen();
  if (!Then)
    return;

  // Skip if already a guard clause (body is just return/continue/break).
  if (isAlreadyGuardClause(Then))
    return;

  // Guard clause transformation only works when the if statement is the LAST
  // statement in its scope. Otherwise, the code after the if would change
  // behavior (it would only run when the guard condition is false).
  const Stmt *LastNonNullStmt = nullptr;
  for (const Stmt *S : Scope->body()) {
    if (!isa<NullStmt>(S))
      LastNonNullStmt = S;
  }
  if (LastNonNullStmt != If)
    return;

  // Calculate lines in the Then branch (excluding braces).
  const unsigned ThenLines = countThenLines(Then, SM);

  // Only trigger if the Then branch has at least MinimumLines lines.
  if (ThenLines < MinimumLines)
    return;

  const Expr *Cond = If->getCond();
  if (!Cond)
    return;

  // Determine the appropriate message and exit statement based on context.
  StringRef ExitStatement;
  StringRef DiagMessage;

  if (Loop != nullptr) {
    ExitStatement = "continue";
    DiagMessage = "use guard clause with continue to reduce nesting";
  } else if (Function != nullptr) {
    ExitStatement = "return";
    DiagMessage = "use guard clause with early return to reduce nesting";
  } else {
    return;
  }

  // Check if the if statement has an init-statement with variable declarations.
  const bool HasInitVarDecl = hasInitWithVarDecl(If);

  // Check if the condition itself declares a variable (e.g., if (auto *x = ...))
  // Such variables would go out of scope after guard clause transformation.
  const bool HasConditionVarDecl = If->getConditionVariable() != nullptr;

  // If IgnoreIfWithInitializer is true, skip such if statements entirely.
  if (IgnoreIfWithInitializer && (HasInitVarDecl || HasConditionVarDecl))
    return;

  // If there's a variable declaration in init or condition, emit a warning
  // but don't provide fix-its (unsafe transformation).
  if (HasInitVarDecl || HasConditionVarDecl) {
    diag(If->getIfLoc(), DiagMessage);
    return;
  }

  // Build the inverted condition.
  const std::string InvertedCond = invertCondition(Cond, *Result.Context);

  // Calculate indentation for the if statement.
  const unsigned IndentSize = SM.getSpellingColumnNumber(If->getIfLoc()) - 1;
  llvm::SmallString<16> Indent;
  Indent.resize(IndentSize, ' ');

  auto Diag = diag(If->getIfLoc(), DiagMessage);

  // Fix-it 1: Replace condition with inverted condition
  Diag << FixItHint::CreateReplacement(Cond->getSourceRange(), InvertedCond);

  // Handle compound statements (if with braces)
  if (const auto *CS = dyn_cast<CompoundStmt>(Then)) {
    SourceLocation LBraceLoc = CS->getLBracLoc();
    SourceLocation RBraceLoc = CS->getRBracLoc();

    if (!LBraceLoc.isValid() || !RBraceLoc.isValid())
      return;

    // Fix-it 2: Insert guard statement right after opening brace
    SourceLocation InsertLoc = LBraceLoc.getLocWithOffset(1);

    llvm::SmallString<64> GuardInsertion;
    GuardInsertion += "\n";
    GuardInsertion += Indent;
    GuardInsertion += "  ";
    GuardInsertion += ExitStatement;
    GuardInsertion += ";\n";
    GuardInsertion += Indent;
    GuardInsertion += "}";

    Diag << FixItHint::CreateInsertion(InsertLoc, GuardInsertion);

    // Fix-it 3: Remove the closing brace (including newline before it if present)
    // Find the start of the line containing the closing brace
    SourceLocation LineStart = SM.getSpellingLoc(RBraceLoc);
    FileID FID = SM.getFileID(LineStart);
    unsigned Offset = SM.getFileOffset(LineStart);

    // Get the buffer
    bool Invalid = false;
    StringRef Buffer = SM.getBufferData(FID, &Invalid);
    if (!Invalid && Offset > 0) {
      // Scan backwards to find the start of the line (or last non-whitespace)
      unsigned Start = Offset;
      while (Start > 0 && (Buffer[Start - 1] == ' ' || Buffer[Start - 1] == '\t')) {
        --Start;
      }
      // Check if we hit a newline - if so, include it in the removal
      if (Start > 0 && Buffer[Start - 1] == '\n') {
        --Start;
      }

      SourceLocation RemovalStart = LineStart.getLocWithOffset(Start - Offset);
      Diag << FixItHint::CreateRemoval(CharSourceRange::getTokenRange(RemovalStart, RBraceLoc));
    } else {
      Diag << FixItHint::CreateRemoval(RBraceLoc);
    }
  } else {
    // For non-compound statements, add guard clause without braces
    SourceLocation ThenStart = Then->getBeginLoc();

    if (ThenStart.isValid()) {
      // Insert guard statement before Then (preserving brace-less style)
      // The guard replaces the original statement, so we insert "return;\n"
      // followed by the indent for the original statement
      llvm::SmallString<64> GuardPrefix;
      GuardPrefix += ExitStatement;
      GuardPrefix += ";\n";
      GuardPrefix += Indent;

      Diag << FixItHint::CreateInsertion(ThenStart, GuardPrefix);
    }
  }
}

} // namespace clang::tidy::readability
