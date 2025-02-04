//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/attributes.hpp"
#include "vpux/compiler/dialect/IE/dialect.hpp"
#include "vpux/compiler/utils/attributes.hpp"

#include "vpux/utils/core/error.hpp"

#include <llvm/ADT/StringExtras.h>
#include <llvm/ADT/TypeSwitch.h>
#include <mlir/IR/Dialect.h>
#include <mlir/IR/DialectImplementation.h>
#include <mlir/IR/Types.h>

using namespace vpux;

//
// Generated
//

#define GET_ATTRDEF_CLASSES
#include <vpux/compiler/dialect/IE/generated/attributes.cpp.inc>

//
// Dialect hooks
//

void IE::IEDialect::registerAttributes() {
    addAttributes<
#define GET_ATTRDEF_LIST
#include <vpux/compiler/dialect/IE/generated/attributes.cpp.inc>
            >();
}

//
// TensorAttr
//

IE::TensorAttr vpux::IE::getTensorAttr(mlir::AffineMapAttr order, IndexedSymbolAttr memSpace) {
    // Initially, tensors do not have an encoding attribute, which is equivalent to an empty TensorAttr.
    // But in fact, such tensors have a different type: `tensor<1x8x4x2xf16> != tensor<1x8x4x2xf16, {}>`.
    // So let's not use empty attributes to avoid ambiguous representation of the same type.
    if ((order == nullptr || order.getValue().isIdentity()) && memSpace == nullptr) {
        return nullptr;
    }

    auto* ctx = order != nullptr ? order.getContext() : memSpace.getContext();

    return IE::TensorAttr::get(order, memSpace, ctx);
}

IE::TensorAttr vpux::IE::getTensorAttr(mlir::AffineMap order, IndexedSymbolAttr memSpace) {
    return IE::getTensorAttr(mlir::AffineMapAttr::get(order), memSpace);
}

IE::TensorAttr vpux::IE::getTensorAttr(mlir::MLIRContext* ctx, DimsOrder order, IndexedSymbolAttr memSpace) {
    return IE::getTensorAttr(order.toAffineMap(ctx), memSpace);
}

IE::TensorAttr vpux::IE::getTensorAttr(mlir::RankedTensorType type) {
    if (const auto encoding = type.getEncoding()) {
        const auto tensorAttr = encoding.dyn_cast<IE::TensorAttr>();
        VPUX_THROW_UNLESS(tensorAttr != nullptr, "Unsupported tensor encoding attribute '{0}'", encoding);

        return tensorAttr;
    }

    return nullptr;
}

mlir::AffineMap vpux::IE::getOrder(mlir::RankedTensorType type) {
    if (const auto desc = IE::getTensorAttr(type)) {
        if (const auto orderAttr = desc.order()) {
            return orderAttr.getValue();
        }
    }

    const auto numDims = checked_cast<uint32_t>(type.getRank());
    return mlir::AffineMap::getMinorIdentityMap(numDims, numDims, type.getContext());
}

IndexedSymbolAttr vpux::IE::getMemorySpace(mlir::RankedTensorType type) {
    if (const auto desc = IE::getTensorAttr(type)) {
        return desc.mem_space();
    }

    return nullptr;
}

//
// Generated
//

#include <vpux/compiler/dialect/IE/generated/attributes/enums.cpp.inc>
#include <vpux/compiler/dialect/IE/generated/attributes/structs.cpp.inc>
