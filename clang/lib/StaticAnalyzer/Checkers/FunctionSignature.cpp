//=== FunctionSignature.cpp - Validation of functions signatures. --*- C++ -*-//
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

#include "FunctionSignature.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"

namespace clang {
namespace ento {

Signature::Signature(ArgTypes ArgTys, RetType RetTy) {
  for (const std::optional<QualType> &Arg : ArgTys) {
    if (!Arg) {
      Invalid = true;
      return;
    }
    assertArgTypeSuitableForSignature(*Arg);
    this->ArgTys.push_back(*Arg);
  }
  if (!RetTy) {
    Invalid = true;
    return;
  }
  assertRetTypeSuitableForSignature(*RetTy);
  this->RetTy = *RetTy;
}

bool Signature::isInvalid() const { return Invalid; }

bool Signature::matches(const FunctionDecl *FD) const {
  assert(!isInvalid());
  // Check the number of arguments.
  if (FD->param_size() != ArgTys.size())
    return false;

  // The "restrict" keyword is illegal in C++, however, many libc
  // implementations use the "__restrict" compiler intrinsic in functions
  // prototypes. The "__restrict" keyword qualifies a type as a restricted type
  // even in C++.
  // In case of any non-C99 languages, we don't want to match based on the
  // restrict qualifier because we cannot know if the given libc implementation
  // qualifies the paramater type or not.
  const auto RemoveRestrict = [&FD](QualType T) {
    if (!FD->getASTContext().getLangOpts().C99)
      T.removeLocalRestrict();
    return T;
  };

  // Check the return type.
  if (!isIrrelevant(RetTy)) {
    const QualType FDRetTy =
        RemoveRestrict(FD->getReturnType().getCanonicalType());
    if (RetTy != FDRetTy)
      return false;
  }

  // Check the argument types.
  for (auto [Idx, ArgTy] : llvm::enumerate(ArgTys)) {
    if (isIrrelevant(ArgTy))
      continue;
    const QualType FDArgTy =
        RemoveRestrict(FD->getParamDecl(Idx)->getType().getCanonicalType());
    if (ArgTy != FDArgTy)
      return false;
  }

  return true;
}

bool Signature::isIrrelevant(QualType T) { return T.isNull(); }

void Signature::assertArgTypeSuitableForSignature(QualType T) {
  assert((T.isNull() || !T->isVoidType()) &&
         "We should have no void types in the spec");
  assert((T.isNull() || T.isCanonical()) &&
         "We should only have canonical types in the spec");
}

void Signature::assertRetTypeSuitableForSignature(QualType T) {
  assert((T.isNull() || T.isCanonical()) &&
         "We should only have canonical types in the spec");
}

TypeFactory::TypeFactory(const ASTContext &ACtx) : ACtx(ACtx) {}

QualType TypeFactory::getVoidTy() const { return ACtx.VoidTy; }
QualType TypeFactory::getCharTy() const { return ACtx.CharTy; }
QualType TypeFactory::getWCharTy() const { return ACtx.WCharTy; }
QualType TypeFactory::getIntTy() const { return ACtx.IntTy; }
QualType TypeFactory::getUnsignedIntTy() const { return ACtx.UnsignedIntTy; }
QualType TypeFactory::getLongTy() const { return ACtx.LongTy; }
QualType TypeFactory::getSizeTy() const { return ACtx.getSizeType(); }

QualType TypeFactory::getPointerTy(QualType Ty) const {
  return ACtx.getPointerType(Ty);
}

std::optional<QualType>
TypeFactory::getPointerTy(std::optional<QualType> Ty) const {
  return Ty ? std::optional<QualType>(getPointerTy(*Ty)) : std::nullopt;
}

QualType TypeFactory::getConstTy(QualType Ty) const { return Ty.withConst(); }

std::optional<QualType>
TypeFactory::getConstTy(std::optional<QualType> Ty) const {
  return Ty ? std::optional<QualType>(getConstTy(*Ty)) : std::nullopt;
}

QualType TypeFactory::getRestrictTy(QualType Ty) const {
  return ACtx.getLangOpts().C99 ? ACtx.getRestrictType(Ty) : Ty;
}

std::optional<QualType>
TypeFactory::getRestrictTy(std::optional<QualType> Ty) const {
  return Ty ? std::optional<QualType>(getRestrictTy(*Ty)) : std::nullopt;
}

std::optional<QualType> TypeFactory::lookupTy(StringRef Name) const {
  IdentifierInfo &II = ACtx.Idents.get(Name);
  auto LookupRes = ACtx.getTranslationUnitDecl()->lookup(&II);
  if (LookupRes.empty())
    return std::nullopt;

  // Prioritize typedef declarations.
  // This is needed in case of C struct typedefs. E.g.:
  //   typedef struct FILE FILE;
  // In this case, we have a RecordDecl 'struct FILE' with the name 'FILE'
  // and we have a TypedefDecl with the name 'FILE'.
  for (Decl *D : LookupRes)
    if (auto *TD = dyn_cast<TypedefNameDecl>(D))
      return ACtx.getTypeDeclType(TD).getCanonicalType();

  // Find the first TypeDecl.
  // There maybe cases when a function has the same name as a struct.
  // E.g. in POSIX: `struct stat` and the function `stat()`:
  //   int stat(const char *restrict path, struct stat *restrict buf);
  for (Decl *D : LookupRes)
    if (auto *TD = dyn_cast<TypeDecl>(D))
      return ACtx.getTypeDeclType(TD).getCanonicalType();

  return std::nullopt;
}

QualType TypeFactory::getVoidPtrTy() const { return getPointerTy(getVoidTy()); }

QualType TypeFactory::getCharPtrTy() const { return getPointerTy(getCharTy()); }

QualType TypeFactory::getConstCharPtrTy() const {
  return getPointerTy(getConstTy(getCharTy()));
}

QualType TypeFactory::getConstVoidPtrTy() const {
  return getPointerTy(getConstTy(getVoidTy()));
}

void SignatureMatcher::addSignature(StringRef Name, const Signature &Sign) {
  Signatures.try_emplace(Name, Sign);
}

bool SignatureMatcher::matches(const FunctionDecl *FD,
                               StringRef ExpectedName) const {
  if (FD->getName() != ExpectedName)
    return false;

  const auto It = Signatures.find(ExpectedName);
  if (It == Signatures.end())
    return false;

  const Signature &Sign = It->second;
  if (Sign.isInvalid())
    return false;

  return Sign.matches(FD);
}

} // namespace ento
} // namespace clang