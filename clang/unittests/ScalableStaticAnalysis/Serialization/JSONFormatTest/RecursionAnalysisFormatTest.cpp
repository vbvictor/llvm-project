//===- RecursionAnalysisFormatTest.cpp ------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "JSONFormatTest.h"
#include "clang/ScalableStaticAnalysis/Analyses/CallGraph/RecursionAnalysis.h"
#include "clang/ScalableStaticAnalysis/Core/Model/BuildNamespace.h"
#include "clang/ScalableStaticAnalysis/Core/Model/EntityId.h"
#include "clang/ScalableStaticAnalysis/Core/Model/EntityName.h"
#include "clang/ScalableStaticAnalysis/Core/Serialization/JSONFormat.h"
#include "clang/ScalableStaticAnalysis/Core/WholeProgramAnalysis/WPASuite.h"
#include "llvm/Testing/Support/Error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include <memory>

using namespace clang;
using namespace ssaf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;
using testing::UnorderedElementsAre;

namespace {

/// The recursion analysis result travels from `clang-ssaf-analyzer` to the
/// consumers (such as clang-tidy) as the JSON of a WPASuite.
class RecursionAnalysisFormatTest : public JSONFormatTest {
protected:
  EntityId addEntity(WPASuite &Suite, llvm::StringRef USR) {
    return getIdTable(Suite).getId(
        EntityName(USR.str(), "", NestedBuildNamespace::makeLinkUnit("LU")));
  }

  void insertResult(WPASuite &Suite,
                    std::unique_ptr<RecursionAnalysisResult> Result) {
    getData(Suite).emplace(RecursionAnalysisResult::analysisName(),
                           std::move(Result));
  }

  /// Round-trips \p Suite through JSON write -> read and returns the
  /// recursion result of the suite that was read back.
  const RecursionAnalysisResult &roundTrip(const WPASuite &Suite) {
    PathString Path = makePath("recursion-suite.json");
    EXPECT_THAT_ERROR(JSONFormat().writeWPASuite(Suite, Path),
                      llvm::Succeeded());
    auto ReadOrErr = JSONFormat().readWPASuite(Path);
    EXPECT_THAT_EXPECTED(ReadOrErr, llvm::Succeeded());
    Read = std::move(*ReadOrErr);
    auto ResultOrErr = Read.get<RecursionAnalysisResult>();
    EXPECT_THAT_EXPECTED(ResultOrErr, llvm::Succeeded());
    return *ResultOrErr;
  }

private:
  WPASuite Read = makeWPASuite();
};

TEST_F(RecursionAnalysisFormatTest, RoundTripsCyclesAndFunctions) {
  WPASuite Suite = makeWPASuite();
  EntityId F = addEntity(Suite, "c:@F@f#");
  EntityId G = addEntity(Suite, "c:@F@g#");
  EntityId H = addEntity(Suite, "c:@F@h#");

  auto Result = std::make_unique<RecursionAnalysisResult>();
  Result->Functions[F] = {"f()", {"a.cpp", 3, 6}};
  Result->Functions[G] = {"g(int)", {"b.cpp", 5, 8}};
  Result->Functions[H] = {"h()", {"b.cpp", 9, 1}};
  Result->Cycles.push_back({{F, G}, {F, G, F}});
  Result->Cycles.push_back({{H}, {H, H}});
  insertResult(Suite, std::move(Result));

  const RecursionAnalysisResult &R = roundTrip(Suite);

  ASSERT_THAT(R.Cycles, SizeIs(2));
  EXPECT_THAT(R.Cycles[0].Functions, UnorderedElementsAre(F, G));
  EXPECT_THAT(R.Cycles[0].ExampleChain, ElementsAre(F, G, F));
  EXPECT_THAT(R.Cycles[1].Functions, ElementsAre(H));
  EXPECT_THAT(R.Cycles[1].ExampleChain, ElementsAre(H, H));

  ASSERT_THAT(R.Functions, SizeIs(3));
  EXPECT_EQ(R.Functions.at(G).PrettyName, "g(int)");
  EXPECT_EQ(R.Functions.at(G).Definition.File, "b.cpp");
  EXPECT_EQ(R.Functions.at(G).Definition.Line, 5u);
  EXPECT_EQ(R.Functions.at(G).Definition.Column, 8u);
  EXPECT_EQ(R.Functions.at(H).Definition.Column, 1u);
}

TEST_F(RecursionAnalysisFormatTest, RoundTripsAnEmptyResult) {
  WPASuite Suite = makeWPASuite();
  insertResult(Suite, std::make_unique<RecursionAnalysisResult>());

  const RecursionAnalysisResult &R = roundTrip(Suite);

  EXPECT_THAT(R.Cycles, IsEmpty());
  EXPECT_THAT(R.Functions, IsEmpty());
}

TEST_F(RecursionAnalysisFormatTest, EntityIdsAreResolvedAgainstTheSuiteTable) {
  // The ids in the result are written as references to the entity table of
  // the suite, so after reading they must denote the same entities.
  WPASuite Suite = makeWPASuite();
  EntityId F = addEntity(Suite, "c:@F@f#");

  auto Result = std::make_unique<RecursionAnalysisResult>();
  Result->Functions[F] = {"f()", {"a.cpp", 1, 6}};
  Result->Cycles.push_back({{F}, {F, F}});
  insertResult(Suite, std::move(Result));

  PathString Path = makePath("recursion-suite-ids.json");
  ASSERT_THAT_ERROR(JSONFormat().writeWPASuite(Suite, Path), llvm::Succeeded());
  auto ReadOrErr = JSONFormat().readWPASuite(Path);
  ASSERT_THAT_EXPECTED(ReadOrErr, llvm::Succeeded());

  auto ResultOrErr = ReadOrErr->get<RecursionAnalysisResult>();
  ASSERT_THAT_EXPECTED(ResultOrErr, llvm::Succeeded());
  ASSERT_THAT(ResultOrErr->Cycles, SizeIs(1));

  bool Found = false;
  ReadOrErr->getIdTable().forEach([&](const EntityName &Name, EntityId Id) {
    if (Id == ResultOrErr->Cycles[0].Functions[0]) {
      Found = true;
      EXPECT_EQ(Name, EntityName("c:@F@f#", "",
                                 NestedBuildNamespace::makeLinkUnit("LU")));
    }
  });
  EXPECT_TRUE(Found);
}

} // namespace
