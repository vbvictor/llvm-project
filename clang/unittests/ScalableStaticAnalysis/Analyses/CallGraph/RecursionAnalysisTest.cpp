//===- RecursionAnalysisTest.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "clang/ScalableStaticAnalysis/Analyses/CallGraph/RecursionAnalysis.h"
#include "TestFixture.h"
#include "clang/ScalableStaticAnalysis/Analyses/CallGraph/CallGraphSummary.h"
#include "clang/ScalableStaticAnalysis/Core/EntityLinker/LUSummary.h"
#include "clang/ScalableStaticAnalysis/Core/Model/BuildNamespace.h"
#include "clang/ScalableStaticAnalysis/Core/Model/EntityId.h"
#include "clang/ScalableStaticAnalysis/Core/Model/EntityLinkage.h"
#include "clang/ScalableStaticAnalysis/Core/Model/EntityName.h"
#include "clang/ScalableStaticAnalysis/Core/WholeProgramAnalysis/AnalysisDriver.h"
#include "clang/ScalableStaticAnalysis/Core/WholeProgramAnalysis/WPASuite.h"
#include "llvm/Testing/Support/Error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <memory>
#include <set>
#include <string>

using namespace clang;
using namespace ssaf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace {

/// Tests for the whole-program recursion analysis: it consumes the
/// CallGraphSummary of every function in a link unit and reports the cycles of
/// the whole-program call graph.
class RecursionAnalysisTest : public TestFixture {
protected:
  static constexpr EntityLinkage ExternalLinkage =
      EntityLinkage(EntityLinkageType::External);

  std::unique_ptr<LUSummary> makeLUSummary() {
    return std::make_unique<LUSummary>(
        llvm::Triple("arm64-apple-macosx"),
        NestedBuildNamespace::makeLinkUnit("LU"));
  }

  EntityId addEntity(LUSummary &LU, llvm::StringRef USR) {
    EntityName Name(USR.str(), "", NestedBuildNamespace::makeLinkUnit("LU"));
    EntityId Id = getIdTable(LU).getId(Name);
    getLinkageTable(LU).insert({Id, ExternalLinkage});
    return Id;
  }

  /// Records the call-graph summary of the function \p Id: a function that is
  /// defined at \p File:\p Line:\p Col and calls \p Direct and \p Virtual.
  void addFunction(LUSummary &LU, EntityId Id, llvm::StringRef PrettyName,
                   llvm::StringRef File, unsigned Line, unsigned Col,
                   std::set<EntityId> Direct = {},
                   std::set<EntityId> Virtual = {}) {
    auto Summary = std::make_unique<CallGraphSummary>();
    Summary->PrettyName = PrettyName.str();
    Summary->Definition = {File.str(), Line, Col};
    Summary->DirectCallees = std::move(Direct);
    Summary->VirtualCallees = std::move(Virtual);
    getData(LU)[CallGraphSummary::summaryName()][Id] = std::move(Summary);
  }

  const RecursionAnalysisResult &run(std::unique_ptr<LUSummary> LU) {
    AnalysisDriver Driver(std::move(LU));
    auto WPAOrErr = Driver.run<RecursionAnalysisResult>();
    EXPECT_THAT_EXPECTED(WPAOrErr, llvm::Succeeded());
    WPA = std::move(*WPAOrErr);
    auto ROrErr = WPA.get<RecursionAnalysisResult>();
    EXPECT_THAT_EXPECTED(ROrErr, llvm::Succeeded());
    return *ROrErr;
  }

private:
  WPASuite WPA = makeWPASuite();
};

TEST_F(RecursionAnalysisTest, NoCallsNoCycles) {
  auto LU = makeLUSummary();
  EntityId F = addEntity(*LU, "c:@F@f#");
  addFunction(*LU, F, "f()", "a.cpp", 1, 6);

  const auto &R = run(std::move(LU));

  EXPECT_THAT(R.Cycles, IsEmpty());
  EXPECT_THAT(R.Functions, IsEmpty());
}

TEST_F(RecursionAnalysisTest, AcyclicChainIsNotRecursive) {
  auto LU = makeLUSummary();
  EntityId A = addEntity(*LU, "c:@F@a#");
  EntityId B = addEntity(*LU, "c:@F@b#");
  EntityId C = addEntity(*LU, "c:@F@c#");
  addFunction(*LU, A, "a()", "a.cpp", 1, 6, {B});
  addFunction(*LU, B, "b()", "a.cpp", 2, 6, {C});
  addFunction(*LU, C, "c()", "a.cpp", 3, 6);

  const auto &R = run(std::move(LU));

  EXPECT_THAT(R.Cycles, IsEmpty());
  EXPECT_THAT(R.Functions, IsEmpty());
}

TEST_F(RecursionAnalysisTest, SelfRecursion) {
  auto LU = makeLUSummary();
  EntityId F = addEntity(*LU, "c:@F@f#");
  addFunction(*LU, F, "f()", "a.cpp", 1, 6, {F});

  const auto &R = run(std::move(LU));

  ASSERT_THAT(R.Cycles, SizeIs(1));
  EXPECT_THAT(R.Cycles[0].Functions, ElementsAre(F));
  // A chain is closed: it ends in the function it starts from.
  EXPECT_THAT(R.Cycles[0].ExampleChain, ElementsAre(F, F));
}

TEST_F(RecursionAnalysisTest, MutualRecursionReportsNamesAndLocations) {
  auto LU = makeLUSummary();
  EntityId F = addEntity(*LU, "c:@F@f#");
  EntityId G = addEntity(*LU, "c:@F@g#");
  // The two functions come from different translation units.
  addFunction(*LU, F, "f()", "a.cpp", 3, 6, {G});
  addFunction(*LU, G, "g()", "b.cpp", 5, 8, {F});

  const auto &R = run(std::move(LU));

  ASSERT_THAT(R.Cycles, SizeIs(1));
  EXPECT_THAT(R.Cycles[0].Functions, UnorderedElementsAre(F, G));
  EXPECT_THAT(R.Cycles[0].ExampleChain, ElementsAre(F, G, F));

  ASSERT_THAT(R.Functions, SizeIs(2));
  const auto &FInfo = R.Functions.at(F);
  EXPECT_EQ(FInfo.PrettyName, "f()");
  EXPECT_EQ(FInfo.Definition.File, "a.cpp");
  EXPECT_EQ(FInfo.Definition.Line, 3u);
  EXPECT_EQ(FInfo.Definition.Column, 6u);
  const auto &GInfo = R.Functions.at(G);
  EXPECT_EQ(GInfo.PrettyName, "g()");
  EXPECT_EQ(GInfo.Definition.File, "b.cpp");
  EXPECT_EQ(GInfo.Definition.Line, 5u);
  EXPECT_EQ(GInfo.Definition.Column, 8u);
}

TEST_F(RecursionAnalysisTest, OnlyFunctionsOnTheCycleAreReported) {
  auto LU = makeLUSummary();
  EntityId Main = addEntity(*LU, "c:@F@main#");
  EntityId F = addEntity(*LU, "c:@F@f#");
  EntityId G = addEntity(*LU, "c:@F@g#");
  EntityId Leaf = addEntity(*LU, "c:@F@leaf#");
  addFunction(*LU, Main, "main()", "a.cpp", 1, 5, {F});
  addFunction(*LU, F, "f()", "a.cpp", 2, 6, {G});
  addFunction(*LU, G, "g()", "a.cpp", 3, 6, {F, Leaf});
  addFunction(*LU, Leaf, "leaf()", "a.cpp", 4, 6);

  const auto &R = run(std::move(LU));

  ASSERT_THAT(R.Cycles, SizeIs(1));
  EXPECT_THAT(R.Cycles[0].Functions, UnorderedElementsAre(F, G));
  // Callers of a cycle and callees out of it are not recursive themselves.
  EXPECT_EQ(R.Functions.count(Main), 0u);
  EXPECT_EQ(R.Functions.count(Leaf), 0u);
}

TEST_F(RecursionAnalysisTest, ExampleChainStartsAtFirstFunctionInSourceOrder) {
  auto LU = makeLUSummary();
  // Entity ids follow the order of creation, i.e. of linking. The chain must
  // not depend on it, so create the function that is *later* in the source
  // first.
  EntityId Later = addEntity(*LU, "c:@F@later#");
  EntityId Earlier = addEntity(*LU, "c:@F@earlier#");
  ASSERT_LT(Later, Earlier);
  addFunction(*LU, Later, "later()", "a.cpp", 20, 6, {Earlier});
  addFunction(*LU, Earlier, "earlier()", "a.cpp", 10, 6, {Later});

  const auto &R = run(std::move(LU));

  ASSERT_THAT(R.Cycles, SizeIs(1));
  EXPECT_THAT(R.Cycles[0].ExampleChain, ElementsAre(Earlier, Later, Earlier));
}

TEST_F(RecursionAnalysisTest, SourceOrderComparesFilesBeforeLines) {
  auto LU = makeLUSummary();
  EntityId InB = addEntity(*LU, "c:@F@in_b#");
  EntityId InA = addEntity(*LU, "c:@F@in_a#");
  addFunction(*LU, InB, "in_b()", "b.cpp", 1, 6, {InA});
  addFunction(*LU, InA, "in_a()", "a.cpp", 99, 6, {InB});

  const auto &R = run(std::move(LU));

  ASSERT_THAT(R.Cycles, SizeIs(1));
  EXPECT_THAT(R.Cycles[0].ExampleChain, ElementsAre(InA, InB, InA));
}

TEST_F(RecursionAnalysisTest, LongerCycle) {
  auto LU = makeLUSummary();
  EntityId A = addEntity(*LU, "c:@F@a#");
  EntityId B = addEntity(*LU, "c:@F@b#");
  EntityId C = addEntity(*LU, "c:@F@c#");
  addFunction(*LU, A, "a()", "a.cpp", 1, 6, {B});
  addFunction(*LU, B, "b()", "a.cpp", 2, 6, {C});
  addFunction(*LU, C, "c()", "a.cpp", 3, 6, {A});

  const auto &R = run(std::move(LU));

  ASSERT_THAT(R.Cycles, SizeIs(1));
  EXPECT_THAT(R.Cycles[0].Functions, UnorderedElementsAre(A, B, C));
  EXPECT_THAT(R.Cycles[0].ExampleChain, ElementsAre(A, B, C, A));
}

TEST_F(RecursionAnalysisTest, DisjointCyclesAreSeparateAndOrderedBySource) {
  auto LU = makeLUSummary();
  EntityId P = addEntity(*LU, "c:@F@p#");
  EntityId Q = addEntity(*LU, "c:@F@q#");
  EntityId X = addEntity(*LU, "c:@F@x#");
  EntityId Y = addEntity(*LU, "c:@F@y#");
  // Create the cycle that comes later in the source first.
  addFunction(*LU, P, "p()", "b.cpp", 1, 6, {Q});
  addFunction(*LU, Q, "q()", "b.cpp", 2, 6, {P});
  addFunction(*LU, X, "x()", "a.cpp", 1, 6, {Y});
  addFunction(*LU, Y, "y()", "a.cpp", 2, 6, {X});

  const auto &R = run(std::move(LU));

  ASSERT_THAT(R.Cycles, SizeIs(2));
  EXPECT_THAT(R.Cycles[0].Functions, UnorderedElementsAre(X, Y));
  EXPECT_THAT(R.Cycles[0].ExampleChain, ElementsAre(X, Y, X));
  EXPECT_THAT(R.Cycles[1].Functions, UnorderedElementsAre(P, Q));
  EXPECT_THAT(R.Cycles[1].ExampleChain, ElementsAre(P, Q, P));
}

TEST_F(RecursionAnalysisTest, OverlappingCyclesFormOneComponent) {
  auto LU = makeLUSummary();
  EntityId F = addEntity(*LU, "c:@F@f#");
  EntityId G = addEntity(*LU, "c:@F@g#");
  EntityId H = addEntity(*LU, "c:@F@h#");
  addFunction(*LU, F, "f()", "a.cpp", 1, 6, {G, H});
  addFunction(*LU, G, "g()", "a.cpp", 2, 6, {F});
  addFunction(*LU, H, "h()", "a.cpp", 3, 6, {F});

  const auto &R = run(std::move(LU));

  // f -> g -> f and f -> h -> f share f, so they are one strongly connected
  // component, reported once, with one example chain.
  ASSERT_THAT(R.Cycles, SizeIs(1));
  EXPECT_THAT(R.Cycles[0].Functions, UnorderedElementsAre(F, G, H));
  EXPECT_THAT(R.Cycles[0].ExampleChain, ElementsAre(F, G, F));
}

TEST_F(RecursionAnalysisTest, ExampleChainIsAShortestCycle) {
  auto LU = makeLUSummary();
  EntityId F = addEntity(*LU, "c:@F@f#");
  EntityId G = addEntity(*LU, "c:@F@g#");
  EntityId H = addEntity(*LU, "c:@F@h#");
  // f -> g -> h -> f has three frames, but f -> h -> f has only two.
  addFunction(*LU, F, "f()", "a.cpp", 1, 6, {G, H});
  addFunction(*LU, G, "g()", "a.cpp", 2, 6, {H});
  addFunction(*LU, H, "h()", "a.cpp", 3, 6, {F});

  const auto &R = run(std::move(LU));

  ASSERT_THAT(R.Cycles, SizeIs(1));
  EXPECT_THAT(R.Cycles[0].Functions, UnorderedElementsAre(F, G, H));
  EXPECT_THAT(R.Cycles[0].ExampleChain, ElementsAre(F, H, F));
}

TEST_F(RecursionAnalysisTest, ExampleChainStaysInsideTheComponent) {
  auto LU = makeLUSummary();
  EntityId F = addEntity(*LU, "c:@F@f#");
  EntityId Leaf = addEntity(*LU, "c:@F@leaf#");
  EntityId G = addEntity(*LU, "c:@F@g#");
  // `leaf` is called by f and sorts before g, but cannot reach f again.
  addFunction(*LU, F, "f()", "a.cpp", 10, 6, {G, Leaf});
  addFunction(*LU, Leaf, "leaf()", "a.cpp", 11, 6);
  addFunction(*LU, G, "g()", "a.cpp", 12, 6, {F});

  const auto &R = run(std::move(LU));

  ASSERT_THAT(R.Cycles, SizeIs(1));
  EXPECT_THAT(R.Cycles[0].ExampleChain, ElementsAre(F, G, F));
  EXPECT_EQ(R.Functions.count(Leaf), 0u);
}

TEST_F(RecursionAnalysisTest, VirtualCalleesAreEdgesOfTheCallGraph) {
  auto LU = makeLUSummary();
  EntityId F = addEntity(*LU, "c:@F@f#");
  EntityId M = addEntity(*LU, "c:@S@B@F@m#");
  // Same as in clang::CallGraph: a call to a virtual method is an edge to the
  // statically named method.
  addFunction(*LU, F, "f()", "a.cpp", 1, 6, /*Direct=*/{}, /*Virtual=*/{M});
  addFunction(*LU, M, "B::m()", "a.cpp", 2, 8, /*Direct=*/{F});

  const auto &R = run(std::move(LU));

  ASSERT_THAT(R.Cycles, SizeIs(1));
  EXPECT_THAT(R.Cycles[0].Functions, UnorderedElementsAre(F, M));
}

TEST_F(RecursionAnalysisTest, CalleesWithoutDefinitionAreIgnored) {
  auto LU = makeLUSummary();
  EntityId F = addEntity(*LU, "c:@F@f#");
  // `external` is only declared: it has an entity, but no summary.
  EntityId External = addEntity(*LU, "c:@F@external#");
  addFunction(*LU, F, "f()", "a.cpp", 1, 6, {External});

  const auto &R = run(std::move(LU));

  EXPECT_THAT(R.Cycles, IsEmpty());
  EXPECT_THAT(R.Functions, IsEmpty());
}

} // namespace
