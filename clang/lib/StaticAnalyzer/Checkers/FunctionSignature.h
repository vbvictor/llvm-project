//=== FunctionSignature.h - Validation of functions signatures. ----*- C++ -*-//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Provides utilities for validating function signatures.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_STATICANALYZER_CHECKERS_FUNCTIONSIGNATURE_H
#define LLVM_CLANG_LIB_STATICANALYZER_CHECKERS_FUNCTIONSIGNATURE_H

#include <optional>

#include "clang/AST/Type.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"

namespace clang {

class FunctionDecl;
class ASTContext;
class IdentifierInfo;

namespace ento {

// The signature of a function we want to check. This is a concessive
// signature, meaning there may be irrelevant types in the signature
// which we do not check against a function with concrete types.
// All types in the spec need to be canonical.
class Signature {
  using ArgQualTypes = SmallVector<QualType>;
  ArgQualTypes ArgTys;
  QualType RetTy;
  // True if any component type is not found by lookup.
  bool Invalid = false;

public:
  using ArgTypes = ArrayRef<std::optional<QualType>>;
  using RetType = std::optional<QualType>;

  // Construct a signature from optional types. If any of the optional types
  // are not set then the signature will be invalid.
  Signature(ArgTypes ArgTys, RetType RetTy);
  Signature(const Signature &) = default;
  Signature(Signature &&) = default;
  Signature &operator=(const Signature &) = default;
  Signature &operator=(Signature &&) = default;

  bool isInvalid() const;
  bool matches(const FunctionDecl *FD) const;

private:
  static bool isIrrelevant(QualType T);
  static void assertArgTypeSuitableForSignature(QualType T);
  static void assertRetTypeSuitableForSignature(QualType T);
};

/// Provides type creation utilities for Signature class.
/// It encapsulates the logic for creating pointer, const, restrict types
/// and looking up types by name from the AST.
class TypeFactory {
  const ASTContext &ACtx;

public:
  explicit TypeFactory(const ASTContext &ACtx);

  // Basic types from AST
  QualType getVoidTy() const;
  QualType getCharTy() const;
  QualType getWCharTy() const;
  QualType getIntTy() const;
  QualType getUnsignedIntTy() const;
  QualType getLongTy() const;
  QualType getSizeTy() const;

  // Common type mutations
  QualType getPointerTy(QualType Ty) const;
  std::optional<QualType> getPointerTy(std::optional<QualType> Ty) const;
  QualType getConstTy(QualType Ty) const;
  std::optional<QualType> getConstTy(std::optional<QualType> Ty) const;
  QualType getRestrictTy(QualType Ty) const;
  std::optional<QualType> getRestrictTy(std::optional<QualType> Ty) const;

  // Type lookup by name in AST
  std::optional<QualType> lookupTy(StringRef Name) const;

  // Common composite types
  QualType getVoidPtrTy() const;
  QualType getCharPtrTy() const;
  QualType getConstCharPtrTy() const;
  QualType getConstVoidPtrTy() const;
};

/// Helper class for matching function signatures.
/// This class stores function signatures and provides an API to check
/// if a given FunctionDecl matches the expected signature.
class SignatureMatcher {
  using SignatureMap = llvm::StringMap<Signature>;
  SignatureMap Signatures;

public:
  // Add a function signature to the matcher.
  void addSignature(StringRef Name, const Signature &Sign);

  // Check if a function declaration matches the expected signature.
  bool matches(const FunctionDecl *FD, StringRef ExpectedName) const;
};

} // namespace ento
} // namespace clang

#endif // LLVM_CLANG_LIB_STATICANALYZER_CHECKERS_FUNCTIONSIGNATURE_H