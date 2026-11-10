//===- RecursionAnalysis.cpp ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// RecursionAnalysis joins the CallGraphSummaries of a link unit into the call
// graph of the whole program and finds the cycles of that graph.
//
//===----------------------------------------------------------------------===//

#include "clang/ScalableStaticAnalysis/Analyses/CallGraph/RecursionAnalysis.h"
#include "SSAFAnalysesCommon.h"
#include "clang/ScalableStaticAnalysis/Analyses/CallGraph/CallGraphSummary.h"
#include "clang/ScalableStaticAnalysis/Core/Model/EntityId.h"
#include "clang/ScalableStaticAnalysis/Core/Serialization/JSONFormat.h"
#include "clang/ScalableStaticAnalysis/Core/WholeProgramAnalysis/AnalysisRegistry.h"
#include "clang/ScalableStaticAnalysis/Core/WholeProgramAnalysis/SummaryAnalysis.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/JSON.h"
#include <deque>
#include <memory>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

using namespace clang::ssaf;
using namespace llvm;

//===----------------------------------------------------------------------===//
// JSON
//===----------------------------------------------------------------------===//

static constexpr StringLiteral KeyFunctions = "functions";
static constexpr StringLiteral KeyCycles = "cycles";
static constexpr StringLiteral KeyId = "id";
static constexpr StringLiteral KeyPrettyName = "pretty_name";
static constexpr StringLiteral KeyDefinition = "def";
static constexpr StringLiteral KeyFile = "file";
static constexpr StringLiteral KeyLine = "line";
static constexpr StringLiteral KeyColumn = "col";
static constexpr StringLiteral KeyExampleChain = "example_chain";

static json::Array entityIdsToJSON(const std::vector<EntityId> &Ids,
                                   JSONFormat::EntityIdToJSONFn IdToJSON) {
  json::Array Result;
  for (EntityId Id : Ids)
    Result.push_back(IdToJSON(Id));
  return Result;
}

static Expected<std::vector<EntityId>>
entityIdsFromJSON(const json::Object &Obj, StringRef Key,
                  JSONFormat::EntityIdFromJSONFn IdFromJSON) {
  const json::Array *Array = Obj.getArray(Key);
  if (!Array)
    return makeSawButExpectedError(Obj, "an object with an array field '%s'",
                                   Key.str().c_str());
  std::vector<EntityId> Ids;
  for (const json::Value &V : *Array) {
    const json::Object *IdObj = V.getAsObject();
    if (!IdObj)
      return makeSawButExpectedError(V, "an object representing EntityId");
    Expected<EntityId> Id = IdFromJSON(*IdObj);
    if (!Id)
      return Id.takeError();
    Ids.push_back(*Id);
  }
  return Ids;
}

static json::Object
serializeRecursionAnalysisResult(const RecursionAnalysisResult &R,
                                 JSONFormat::EntityIdToJSONFn IdToJSON) {
  json::Array Functions;
  for (const auto &[Id, Info] : R.Functions)
    Functions.push_back(json::Object{
        {KeyId, IdToJSON(Id)},
        {KeyPrettyName, Info.PrettyName},
        {KeyDefinition, json::Object{{KeyFile, Info.Definition.File},
                                     {KeyLine, Info.Definition.Line},
                                     {KeyColumn, Info.Definition.Column}}},
    });

  json::Array Cycles;
  for (const RecursionAnalysisResult::Cycle &C : R.Cycles)
    Cycles.push_back(json::Object{
        {KeyFunctions, entityIdsToJSON(C.Functions, IdToJSON)},
        {KeyExampleChain, entityIdsToJSON(C.ExampleChain, IdToJSON)},
    });

  return json::Object{{KeyFunctions, std::move(Functions)},
                      {KeyCycles, std::move(Cycles)}};
}

static Expected<std::unique_ptr<AnalysisResult>>
deserializeRecursionAnalysisResult(const json::Object &Obj,
                                   JSONFormat::EntityIdFromJSONFn IdFromJSON) {
  auto Result = std::make_unique<RecursionAnalysisResult>();

  const json::Array *Functions = Obj.getArray(KeyFunctions);
  if (!Functions)
    return makeSawButExpectedError(Obj, "an object with an array field '%s'",
                                   KeyFunctions.data());
  for (const json::Value &V : *Functions) {
    const json::Object *Function = V.getAsObject();
    const json::Object *IdObj = Function ? Function->getObject(KeyId) : nullptr;
    const json::Object *Def =
        Function ? Function->getObject(KeyDefinition) : nullptr;
    if (!IdObj || !Def)
      return makeSawButExpectedError(
          V, "an object with fields '%s', '%s' and '%s'", KeyId.data(),
          KeyPrettyName.data(), KeyDefinition.data());
    std::optional<StringRef> PrettyName = Function->getString(KeyPrettyName);
    std::optional<StringRef> File = Def->getString(KeyFile);
    std::optional<int64_t> Line = Def->getInteger(KeyLine);
    std::optional<int64_t> Column = Def->getInteger(KeyColumn);
    if (!PrettyName || !File || !Line || !Column)
      return makeSawButExpectedError(
          V, "an object with a string '%s' and a location '%s'",
          KeyPrettyName.data(), KeyDefinition.data());
    Expected<EntityId> Id = IdFromJSON(*IdObj);
    if (!Id)
      return Id.takeError();
    Result->Functions.insert({*Id,
                              {PrettyName->str(),
                               {File->str(), static_cast<unsigned>(*Line),
                                static_cast<unsigned>(*Column)}}});
  }

  const json::Array *Cycles = Obj.getArray(KeyCycles);
  if (!Cycles)
    return makeSawButExpectedError(Obj, "an object with an array field '%s'",
                                   KeyCycles.data());
  for (const json::Value &V : *Cycles) {
    const json::Object *CycleObj = V.getAsObject();
    if (!CycleObj)
      return makeSawButExpectedError(V, "an object describing a cycle");
    Expected<std::vector<EntityId>> Members =
        entityIdsFromJSON(*CycleObj, KeyFunctions, IdFromJSON);
    if (!Members)
      return Members.takeError();
    Expected<std::vector<EntityId>> Chain =
        entityIdsFromJSON(*CycleObj, KeyExampleChain, IdFromJSON);
    if (!Chain)
      return Chain.takeError();
    Result->Cycles.push_back({std::move(*Members), std::move(*Chain)});
  }

  return std::move(Result);
}

static JSONFormat::AnalysisResultRegistry::Add<RecursionAnalysisResult>
    RegisterRecursionResultForJSON(serializeRecursionAnalysisResult,
                                   deserializeRecursionAnalysisResult);

//===----------------------------------------------------------------------===//
// RecursionAnalysis
//===----------------------------------------------------------------------===//

namespace {

/// Collects the call graph of the whole program and, once every function is
/// known, finds its cycles.
///
/// The algorithm is deliberately simple: it searches the graph from every
/// function, so it takes time quadratic in the number of functions in the worst
/// case.
class RecursionAnalysis final
    : public SummaryAnalysis<RecursionAnalysisResult, CallGraphSummary> {
  struct Function {
    EntityId Id;
    std::string PrettyName;
    CallGraphSummary::Location Definition;
    std::set<EntityId> Callees;

    auto sortKey() const {
      return std::tie(Definition.File, Definition.Line, Definition.Column, Id);
    }
  };

  /// Functions are numbered in the order of their definitions.
  using Graph = std::vector<std::vector<unsigned>>;

  std::vector<Function> Functions;

  /// Searches \p Edges breadth-first from \p Start, following the edges in
  /// ascending order. Returns, for every function reached by at least one edge,
  /// the function it was first reached from. \p Start is among them exactly
  /// when it is on a cycle.
  static DenseMap<unsigned, unsigned> search(unsigned Start,
                                             const Graph &Edges) {
    DenseMap<unsigned, unsigned> ReachedFrom;
    std::deque<unsigned> Queue{Start};
    while (!Queue.empty()) {
      const unsigned F = Queue.front();
      Queue.pop_front();
      for (unsigned Next : Edges[F])
        if (ReachedFrom.try_emplace(Next, F).second)
          Queue.push_back(Next);
    }
    return ReachedFrom;
  }

public:
  llvm::Error add(EntityId Id, const CallGraphSummary &Summary) override {
    Function F{Id, Summary.PrettyName, Summary.Definition, {}};
    F.Callees.insert(Summary.DirectCallees.begin(),
                     Summary.DirectCallees.end());
    F.Callees.insert(Summary.VirtualCallees.begin(),
                     Summary.VirtualCallees.end());
    Functions.push_back(std::move(F));
    return llvm::Error::success();
  }

private:
  // Called by the AnalysisDriver once every function has been added.
  llvm::Error finalize() override {
    llvm::sort(Functions, [](const Function &A, const Function &B) {
      return A.sortKey() < B.sortKey();
    });

    DenseMap<EntityId, unsigned> NumberOf;
    for (const auto &[Number, F] : llvm::enumerate(Functions))
      NumberOf[F.Id] = Number;

    // A function that is only declared has no summary, and so no edges.
    Graph Callees(Functions.size()), Callers(Functions.size());
    for (const auto &[Number, F] : llvm::enumerate(Functions))
      for (EntityId Callee : F.Callees)
        if (auto It = NumberOf.find(Callee); It != NumberOf.end()) {
          Callees[Number].push_back(It->second);
          Callers[It->second].push_back(Number);
        }
    for (auto &Edges : Callees)
      llvm::sort(Edges);
    for (auto &Edges : Callers)
      llvm::sort(Edges);

    RecursionAnalysisResult &Result = getResult();
    std::vector<bool> InCycle(Functions.size(), false);
    for (unsigned Start = 0; Start != Functions.size(); ++Start) {
      if (InCycle[Start])
        continue;
      DenseMap<unsigned, unsigned> Forward = search(Start, Callees);
      if (!Forward.contains(Start))
        continue;
      DenseMap<unsigned, unsigned> Backward = search(Start, Callers);

      // The cycle is made of the functions that Start calls, directly or not,
      // and that call Start in turn. As every function before Start is already
      // in a cycle or not recursive, Start is the first of them.
      std::vector<unsigned> Members;
      for (const auto &[F, From] : Forward)
        if (Backward.contains(F))
          Members.push_back(F);
      llvm::sort(Members);

      RecursionAnalysisResult::Cycle Cycle;
      for (unsigned F : Members) {
        InCycle[F] = true;
        Cycle.Functions.push_back(Functions[F].Id);
        Result.Functions.insert(
            {Functions[F].Id,
             {Functions[F].PrettyName, Functions[F].Definition}});
      }

      // Start was first reached from the function that closes a shortest
      // cycle through it. Follow the search back from there.
      std::vector<unsigned> Backwards;
      for (unsigned F = Forward[Start]; F != Start; F = Forward[F])
        Backwards.push_back(F);
      Cycle.ExampleChain.push_back(Functions[Start].Id);
      for (unsigned F : llvm::reverse(Backwards))
        Cycle.ExampleChain.push_back(Functions[F].Id);
      Cycle.ExampleChain.push_back(Functions[Start].Id);

      Result.Cycles.push_back(std::move(Cycle));
    }

    Functions.clear();
    return llvm::Error::success();
  }
};

AnalysisRegistry::Add<RecursionAnalysis>
    RegisterRecursionAnalysis("Whole-program recursion analysis: the cycles of "
                              "the call graph of the program");

} // namespace

namespace clang::ssaf {
// NOLINTNEXTLINE(misc-use-internal-linkage)
volatile int RecursionAnalysisAnchorSource = 0;
} // namespace clang::ssaf
