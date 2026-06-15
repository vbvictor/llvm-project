//===---------- ExprMutationAnalyzer.h ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_EXPRMUTATIONANALYZER_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_EXPRMUTATIONANALYZER_H

#include "clang/ASTMatchers/ASTMatchers.h"
#include "llvm/ADT/DenseMap.h"
#include <memory>

namespace clang {

class FunctionParmMutationAnalyzer;

/// Analyzes whether any mutative operations are applied to an expression within
/// a given statement.
class ExprMutationAnalyzer {
  friend class FunctionParmMutationAnalyzer;

public:
  struct Memoized {
    using ResultMap = llvm::DenseMap<const Expr *, const Stmt *>;
    using FunctionParaAnalyzerMap =
        llvm::SmallDenseMap<const FunctionDecl *,
                            std::unique_ptr<FunctionParmMutationAnalyzer>>;

    ResultMap Results;
    ResultMap PointeeResults;
    FunctionParaAnalyzerMap FuncParmAnalyzer;

    void clear() {
      Results.clear();
      PointeeResults.clear();
      FuncParmAnalyzer.clear();
    }
  };

  /// Configuration for the analyzer. Options are shared across this analyzer
  /// and any nested `FunctionParmMutationAnalyzer`s reached through call-site
  /// recursion.
  struct Options {
    /// When set to true, treats a call to a non-const member function as
    /// non-mutating if the same class declares a const-qualified overload
    /// with the same name, the same parameter types, and the same value
    /// return type (e.g. `int get()` paired with `int get() const`).
    ///
    /// Pair detection is intentionally narrow to avoid false positives:
    ///   - Only methods declared directly on the same class are considered;
    ///     overloads inherited via base classes are matched only when made
    ///     visible by a `using` declaration.
    ///   - The two overloads must have identical parameter types. Deleted
    ///     overloads are ignored. Other method qualifiers, such as
    ///     `volatile`, must match between the two overloads.
    ///   - **Only value return types are considered.** Reference and pointer
    ///     return types (e.g. `T&` / `const T&`, `T*` / `const T*`,
    ///     `T&&` / `const T&&`) are not treated as paired even when they
    ///     differ only by an added `const` on the pointee/referent. Such
    ///     non-const overloads can expose a mutable reference or pointer
    ///     that the caller can use to mutate the object (e.g.
    ///     `c.get() = x;`, `opt->setX(...)` via `std::optional::operator->`,
    ///     `*p = x;` via `operator*`), and the analyzer does not track
    ///     mutations through such escape paths.
    bool AllowConstOverloads = false;
  };

  struct Analyzer {
    Analyzer(const Stmt &Stm, ASTContext &Context, Memoized &Memorized,
             const Options &Opts)
        : Stm(Stm), Context(Context), Memorized(Memorized), Opts(Opts) {}

    const Stmt *findMutation(const Expr *Exp);
    const Stmt *findMutation(const Decl *Dec);

    const Stmt *findPointeeMutation(const Expr *Exp);
    const Stmt *findPointeeMutation(const Decl *Dec);

  private:
    using MutationFinder = const Stmt *(Analyzer::*)(const Expr *);

    const Stmt *findMutationMemoized(const Expr *Exp,
                                     llvm::ArrayRef<MutationFinder> Finders,
                                     Memoized::ResultMap &MemoizedResults);
    const Stmt *tryEachDeclRef(const Decl *Dec, MutationFinder Finder);

    const Stmt *findExprMutation(ArrayRef<ast_matchers::BoundNodes> Matches);
    const Stmt *findDeclMutation(ArrayRef<ast_matchers::BoundNodes> Matches);
    const Stmt *
    findExprPointeeMutation(ArrayRef<ast_matchers::BoundNodes> Matches);
    const Stmt *
    findDeclPointeeMutation(ArrayRef<ast_matchers::BoundNodes> Matches);

    const Stmt *findDirectMutation(const Expr *Exp);
    const Stmt *findMemberMutation(const Expr *Exp);
    const Stmt *findArrayElementMutation(const Expr *Exp);
    const Stmt *findCastMutation(const Expr *Exp);
    const Stmt *findRangeLoopMutation(const Expr *Exp);
    const Stmt *findReferenceMutation(const Expr *Exp);
    const Stmt *findFunctionArgMutation(const Expr *Exp);

    const Stmt *findPointeeValueMutation(const Expr *Exp);
    const Stmt *findPointeeMemberMutation(const Expr *Exp);
    const Stmt *findPointeeToNonConst(const Expr *Exp);

    const Stmt &Stm;
    ASTContext &Context;
    Memoized &Memorized;
    const Options &Opts;
  };

  ExprMutationAnalyzer(const Stmt &Stm, ASTContext &Context)
      : Memorized(), Opts(), A(Stm, Context, Memorized, Opts) {}

  /// When set to true, treats a call to a non-const member function as
  /// non-mutating if the same class declares a const-qualified overload with
  /// the same name and parameter types. This is useful for callers that want
  /// to treat such overload pairs (e.g. `T& get()` paired with `const T& get()
  /// const`) as equivalent in terms of object mutation.
  /// Must be set before any mutation queries on this analyzer; toggling it
  /// later does not invalidate previously memoized results.
  void setAllowConstOverloads(bool Value) { Opts.AllowConstOverloads = Value; }

  /// check whether stmt is unevaluated. mutation analyzer will ignore the
  /// content in unevaluated stmt.
  static bool isUnevaluated(const Stmt *Stm, ASTContext &Context);

  bool isMutated(const Expr *Exp) { return findMutation(Exp) != nullptr; }
  bool isMutated(const Decl *Dec) { return findMutation(Dec) != nullptr; }
  const Stmt *findMutation(const Expr *Exp) { return A.findMutation(Exp); }
  const Stmt *findMutation(const Decl *Dec) { return A.findMutation(Dec); }

  bool isPointeeMutated(const Expr *Exp) {
    return findPointeeMutation(Exp) != nullptr;
  }
  bool isPointeeMutated(const Decl *Dec) {
    return findPointeeMutation(Dec) != nullptr;
  }
  const Stmt *findPointeeMutation(const Expr *Exp) {
    return A.findPointeeMutation(Exp);
  }
  const Stmt *findPointeeMutation(const Decl *Dec) {
    return A.findPointeeMutation(Dec);
  }

private:
  Memoized Memorized;
  Options Opts;
  Analyzer A;
};

// A convenient wrapper around ExprMutationAnalyzer for analyzing function
// params.
class FunctionParmMutationAnalyzer {
public:
  static FunctionParmMutationAnalyzer *
  getFunctionParmMutationAnalyzer(const FunctionDecl &Func, ASTContext &Context,
                                  ExprMutationAnalyzer::Memoized &Memorized,
                                  const ExprMutationAnalyzer::Options &Opts) {
    auto it = Memorized.FuncParmAnalyzer.find(&Func);
    if (it == Memorized.FuncParmAnalyzer.end()) {
      // Creating a new instance of FunctionParmMutationAnalyzer below may add
      // additional elements to FuncParmAnalyzer. If we did try_emplace before
      // creating a new instance, the returned iterator of try_emplace could be
      // invalidated.
      it =
          Memorized.FuncParmAnalyzer
              .try_emplace(&Func, std::unique_ptr<FunctionParmMutationAnalyzer>(
                                      new FunctionParmMutationAnalyzer(
                                          Func, Context, Memorized, Opts)))
              .first;
    }
    return it->getSecond().get();
  }

  bool isMutated(const ParmVarDecl *Parm) {
    return findMutation(Parm) != nullptr;
  }
  const Stmt *findMutation(const ParmVarDecl *Parm);

private:
  ExprMutationAnalyzer::Analyzer BodyAnalyzer;
  llvm::DenseMap<const ParmVarDecl *, const Stmt *> Results;

  FunctionParmMutationAnalyzer(const FunctionDecl &Func, ASTContext &Context,
                               ExprMutationAnalyzer::Memoized &Memorized,
                               const ExprMutationAnalyzer::Options &Opts);
};

} // namespace clang

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_EXPRMUTATIONANALYZER_H
