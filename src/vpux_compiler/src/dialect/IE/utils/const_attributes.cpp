//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/utils/const_attributes.hpp"

namespace vpux {
namespace IE {

mlir::ArrayAttr getIntArrayAttrValue(mlir::Value operand) {
    if (operand == nullptr) {
        return nullptr;
    }
    auto constOp = operand.getDefiningOp<Const::DeclareOp>();
    if (constOp == nullptr) {
        return nullptr;
    }
    const auto content = constOp.getContent();
    return getIntArrayAttr(operand.getContext(), content.getValues<int32_t>());
}

mlir::ArrayAttr getFloatArrayAttrValue(mlir::Value operand) {
    if (operand == nullptr) {
        return nullptr;
    }
    auto constOp = operand.getDefiningOp<Const::DeclareOp>();
    if (constOp == nullptr) {
        return nullptr;
    }
    const auto content = constOp.getContent();
    return getFPArrayAttr(operand.getContext(), content.getValues<double>());
}

mlir::IntegerAttr getIntAttrValue(mlir::Value operand, mlir::PatternRewriter& rewriter) {
    if (operand == nullptr) {
        return nullptr;
    }
    auto constOp = operand.getDefiningOp<Const::DeclareOp>();
    const auto content = constOp.getContent();
    if (!content.isSplat()) {
        return nullptr;
    }
    const auto attrValue = content.getSplatValue<int32_t>();
    return rewriter.getI32IntegerAttr(attrValue);
}

mlir::FailureOr<Const::DeclareOp> getConstParentOp(mlir::Value input) {
    auto parent = input.getDefiningOp();
    while (parent && mlir::isa<IE::FakeQuantizeOp, IE::TransposeOp, IE::NegativeOp>(parent)) {
        parent = parent->getOperand(0).getDefiningOp();
    }
    if (parent && mlir::isa<Const::DeclareOp>(parent)) {
        return mlir::cast<Const::DeclareOp>(parent);
    }
    return mlir::failure();
}

mlir::FailureOr<int64_t> getBaseContentNumElements(Const::DeclareOp constOp) {
    if (constOp == nullptr) {
        return mlir::failure();
    }
    auto contentAttr = constOp.getContentAttr();
    if (contentAttr == nullptr) {
        return mlir::failure();
    }
    auto baseContent = contentAttr.getBaseContent();
    if (baseContent != nullptr) {
        return baseContent.getType().getNumElements();
    }
    return mlir::failure();
}

}  // namespace IE
}  // namespace vpux
