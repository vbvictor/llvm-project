//===--- AnalysisConsumer.h - Front-end Analysis Engine Hooks ---*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This header contains the functions necessary for a front-end to run various
// analyses.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_STATICANALYZER_FRONTEND_ANALYSISCONSUMER_H
#define LLVM_CLANG_STATICANALYZER_FRONTEND_ANALYSISCONSUMER_H

#include "clang/AST/ASTConsumer.h"
#include "clang/Basic/LLVM.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/Support/Timer.h"
#include <functional>
#include <memory>

namespace clang {

class CompilerInstance;

namespace ento {
class PathDiagnosticConsumer;
class CheckerRegistry;

class AnalysisASTConsumer : public ASTConsumer {
public:
  virtual void
  AddDiagnosticConsumer(std::unique_ptr<PathDiagnosticConsumer> Consumer) = 0;

  /// This method allows registering statically linked custom checkers that are
  /// not a part of the Clang tree. It employs the same mechanism that is used
  /// by plugins.
  ///
  /// Example:
  ///
  ///   Consumer->AddCheckerRegistrationFn([] (CheckerRegistry& Registry) {
  ///     Registry.addChecker<MyCustomChecker>("example.MyCustomChecker",
  ///                                          "Description");
  ///   });
  virtual void
  AddCheckerRegistrationFn(std::function<void(CheckerRegistry &)> Fn) = 0;

  /// Returns timing data collected during analysis.
  ///
  /// The returned map contains timing information for different analysis phases
  /// (syntax checks, path exploration, bug reporting). Keys are phase names,
  /// values are TimeRecord structures containing wall time, CPU time, and
  /// memory usage.
  ///
  /// Returns an empty map if timing was not enabled during analysis.
  virtual llvm::StringMap<llvm::TimeRecord> getAnalyzerTimingData() const = 0;
};

/// CreateAnalysisConsumer - Creates an ASTConsumer to run various code
/// analysis passes.  (The set of analyses run is controlled by command-line
/// options.)
std::unique_ptr<AnalysisASTConsumer>
CreateAnalysisConsumer(CompilerInstance &CI);

} // namespace ento

} // end clang namespace

#endif
