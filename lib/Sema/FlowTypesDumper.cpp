/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "FlowTypesDumper.h"

namespace hermes {
namespace flow {

size_t FlowTypesDumper::getNumber(const TypeInfo *type) {
  if (type->isSingleton())
    return 0;

  auto [it, inserted] = typeNumber_.try_emplace(type, types_.size());
  if (inserted)
    types_.push_back(type);
  return it->second + 1;
}

void FlowTypesDumper::printTypeRef(
    llvh::raw_ostream &os,
    const TypeInfo *type) {
  os << type->getKindName();
  if (!type->isSingleton())
    os << " %t." << getNumber(type);
}

void FlowTypesDumper::printTypeDescription(
    llvh::raw_ostream &os,
    const TypeInfo *type) {
  os << type->getKindName();
  if (type->isSingleton()) {
    os << '\n';
    return;
  }

  os << ' ';

  switch (type->getKind()) {
    case TypeKind::Void:
    case TypeKind::Null:
    case TypeKind::Boolean:
    case TypeKind::String:
    case TypeKind::CPtr:
    case TypeKind::Number:
    case TypeKind::BigInt:
    case TypeKind::Any:
    case TypeKind::Mixed:
      llvm_unreachable("singletons already handled");
      break;

    case TypeKind::Union: {
      bool first = true;
      for (Type *t : llvh::cast<UnionType>(type)->getTypes()) {
        if (!first)
          os << " | ";
        first = false;
        printTypeRef(os, t);
      }
    } break;

    case TypeKind::TypedFunction: {
      auto *funcType = llvh::cast<TypedFunctionType>(type);
      if (funcType->isAsync())
        os << "async ";
      if (funcType->isGenerator())
        os << "generator ";
      os << '(';

      bool first = true;
      if (Type *thisType = funcType->getThisParam()) {
        os << "this: ";
        printTypeRef(os, thisType);
        first = false;
      }
      for (const TypedFunctionType::Param &param : funcType->getParams()) {
        if (!first)
          os << ", ";
        first = false;
        if (param.first.isValid())
          os << param.first;
        os << ": ";
        printTypeRef(os, param.second);
      }
      os << "): ";
      printTypeRef(os, funcType->getReturnType());
    } break;

    case TypeKind::NativeFunction: {
      auto *nfuncType = llvh::cast<NativeFunctionType>(type);
      os << '(';

      bool first = true;
      for (const TypedFunctionType::Param &param : nfuncType->getParams()) {
        if (!first)
          os << ", ";
        first = false;
        if (param.first.isValid())
          os << param.first;
        os << ": ";
        printTypeRef(os, param.second);
      }
      os << "): ";
      printTypeRef(os, nfuncType->getReturnType());
      os << ", (" << *nfuncType->getSignature() << ")";
    } break;

    case TypeKind::UntypedFunction:
      os << "()";
      break;

    case TypeKind::Class: {
      auto *classType = llvh::cast<ClassType>(type);
      if (classType->getClassName().isValid())
        os << classType->getClassName();
      if (classType->getSuperClass()) {
        os << " extends ";
        printTypeRef(os, classType->getSuperClass());
      }
      os << " {\n";
      if (auto *constrType = classType->getConstructorType()) {
        os << "  %constructor: ";
        printTypeRef(os, constrType);
        os << '\n';
      }
      if (auto *homeObject = classType->getHomeObjectType()) {
        os << "  %homeObject: ";
        printTypeRef(os, homeObject);
        os << '\n';
      }
      for (const ClassType::Field &field : classType->getFields()) {
        os << "  " << field.name << ": ";
        printTypeRef(os, field.type);
        os << '\n';
      }
      os << '}';
    } break;

    case TypeKind::ClassConstructor:
      printTypeRef(os, llvh::cast<ClassConstructorType>(type)->getClassType());
      break;

    case TypeKind::Array:
      printTypeRef(os, llvh::cast<ArrayType>(type)->getElement());
      break;
  }

  os << '\n';
}

void FlowTypesDumper::printAllNumberedTypes(llvh::raw_ostream &os) {
  for (size_t i = 0; i != types_.size(); ++i) {
    os << "%t." << (i + 1) << " = ";
    printTypeDescription(os, types_[i]);
  }
}

void FlowTypesDumper::printAllTypes(
    llvh::raw_ostream &os,
    const FlowContext &flowTypes) {
  // The type numbers that have been printed.
  llvh::DenseSet<size_t> printed{};
  // Ignore singletons, which have number 0.
  printed.insert(0);
  auto printAll = [&os, this, &printed](const auto &all) {
    for (const Type &t : all) {
      // Don't print duplicate types.
      size_t number = getNumber(t.info);
      auto [it, inserted] = printed.insert(number);
      if (!inserted)
        continue;

      printTypeRef(os, &t);
      os << " = ";
      printTypeDescription(os, t.info);
    }
  };

  printAll(flowTypes.allocTypes_);
}

void FlowTypesDumper::printNativeExterns(
    llvh::raw_ostream &os,
    const NativeContext &nativeContext) {
  for (NativeExtern *ne : nativeContext.getAllExterns()) {
    os << "extern \"C\" ";
    ne->signature()->format(os, ne->name()->c_str());
    os << ';';
    if (ne->declared())
      os << " //declared";
    os << '\n';
  }
}

} // namespace flow
} // namespace hermes
