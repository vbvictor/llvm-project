//===- RecursionAnalysis.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines RecursionAnalysisResult, the cycles of the whole-program call graph
// that the recursion analysis finds in the CallGraphSummaries of a link unit.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_SCALABLESTATICANALYSIS_ANALYSES_CALLGRAPH_RECURSIONANALYSIS_H
#define LLVM_CLANG_SCALABLESTATICANALYSIS_ANALYSES_CALLGRAPH_RECURSIONANALYSIS_H

#include "clang/ScalableStaticAnalysis/Analyses/CallGraph/CallGraphSummary.h"
#include "clang/ScalableStaticAnalysis/Core/Model/EntityId.h"
#include "clang/ScalableStaticAnalysis/Core/WholeProgramAnalysis/AnalysisName.h"
#include "clang/ScalableStaticAnalysis/Core/WholeProgramAnalysis/AnalysisResult.h"
#include "llvm/ADT/StringRef.h"
#include <map>
#include <string>
#include <vector>

namespace clang::ssaf {

constexpr llvm::StringLiteral RecursionAnalysisResultName =
    "RecursionAnalysisResult";

/// The recursion of a whole program: the strongly connected components of its
/// call graph that contain a cycle, i.e. the functions that can, directly or
/// through other functions, call themselves.
///
/// Calls to virtual methods are edges to the statically named method, as in
/// \c clang::CallGraph. Calls through function pointers are not known.
///
/// Functions are ordered by where they are defined: by file, then line, then
/// column, with the \c EntityId as the last resort. This does not depend on
/// the order in which translation units were linked.
struct RecursionAnalysisResult final : AnalysisResult {
  static AnalysisName analysisName() {
    return AnalysisName(RecursionAnalysisResultName.str());
  }

  /// What is known about a function of a cycle.
  struct FunctionInfo {
    /// A human-readable name of the function, such as `f(int)`.
    std::string PrettyName;
    /// Where the function is defined.
    CallGraphSummary::Location Definition;
  };

  /// A strongly connected component of the call graph that has a cycle.
  struct Cycle {
    /// All the functions of the component, in the order of their definitions.
    std::vector<EntityId> Functions;

    /// A shortest cycle that goes through the first function of the
    /// component: F0, F1, ..., Fn, F0, where every function calls the next one.
    /// The chain is closed, so it has at least two elements: a function that
    /// calls itself is the chain F0, F0.
    std::vector<EntityId> ExampleChain;
  };

  /// Every function that is part of a cycle.
  std::map<EntityId, FunctionInfo> Functions;

  /// The cycles, in the order of the definitions of their first functions.
  std::vector<Cycle> Cycles;
};

} // namespace clang::ssaf

#endif // LLVM_CLANG_SCALABLESTATICANALYSIS_ANALYSES_CALLGRAPH_RECURSIONANALYSIS_H
