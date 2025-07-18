//===- EmitC.h - EmitC Dialect ----------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares EmitC in MLIR.
//
//===----------------------------------------------------------------------===//

#ifndef MLIR_DIALECT_EMITC_IR_EMITC_H
#define MLIR_DIALECT_EMITC_IR_EMITC_H

#include "mlir/Bytecode/BytecodeOpInterface.h"
#include "mlir/Dialect/EmitC/IR/EmitCInterfaces.h"
#include "mlir/IR/Builders.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Dialect.h"
#include "mlir/Interfaces/CastInterfaces.h"
#include "mlir/Interfaces/ControlFlowInterfaces.h"
#include "mlir/Interfaces/FunctionInterfaces.h"
#include "mlir/Interfaces/SideEffectInterfaces.h"

#include "mlir/Dialect/EmitC/IR/EmitCDialect.h.inc"
#include "mlir/Dialect/EmitC/IR/EmitCEnums.h.inc"

#include <variant>

namespace mlir {
namespace emitc {
void buildTerminatedBody(OpBuilder &builder, Location loc);

/// Determines whether \p type is valid in EmitC.
bool isSupportedEmitCType(mlir::Type type);

/// Determines whether \p type is a valid integer type in EmitC.
bool isSupportedIntegerType(mlir::Type type);

/// Determines whether \p type is integer like, i.e. it's a supported integer,
/// an index or opaque type.
bool isIntegerIndexOrOpaqueType(Type type);

/// Determines whether \p type is a valid floating-point type in EmitC.
bool isSupportedFloatType(mlir::Type type);

/// Determines whether \p type is a emitc.size_t/ssize_t type.
bool isPointerWideType(mlir::Type type);

// Either a literal string, or an placeholder for the fmtArgs.
struct Placeholder {};
using ReplacementItem = std::variant<StringRef, Placeholder>;

} // namespace emitc
} // namespace mlir

#define GET_ATTRDEF_CLASSES
#include "mlir/Dialect/EmitC/IR/EmitCAttributes.h.inc"

#define GET_TYPEDEF_CLASSES
#include "mlir/Dialect/EmitC/IR/EmitCTypes.h.inc"

#define GET_OP_CLASSES
#include "mlir/Dialect/EmitC/IR/EmitC.h.inc"

#endif // MLIR_DIALECT_EMITC_IR_EMITC_H
