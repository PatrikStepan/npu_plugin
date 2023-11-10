//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/ops.hpp"
#include "vpux/compiler/dialect/IE/utils/propagate_quantize_dequantize_utils.hpp"

#include "vpux/compiler/dialect/IE/utils/expand_utils.hpp"
#include "vpux/compiler/dialect/IE/utils/shape_infer.hpp"
#include "vpux/compiler/utils/attributes.hpp"
#include "vpux/compiler/utils/error.hpp"
#include "vpux/compiler/utils/quantization.hpp"

#include "vpux/utils/core/checked_cast.hpp"

using namespace vpux;

void vpux::IE::ExpandOp::build(mlir::OpBuilder& builder, mlir::OperationState& state, mlir::Value input,
                               Optional<ShapeRef> pads_begin, Optional<ShapeRef> pads_end) {
    VPUX_THROW_UNLESS(pads_begin.has_value() || pads_end.has_value(),
                      "pads_begin and/or pads_end must be provided for IE::ExpandOp");

    const auto origShape = getShape(input);

    const auto getPadsAttr = [&](Optional<ShapeRef> pads) {
        if (pads.has_value()) {
            return getIntArrayAttr(builder.getContext(), pads.value());
        }

        const SmallVector<int64_t> zero(origShape.size(), 0);
        return getIntArrayAttr(builder.getContext(), zero);
    };

    build(builder, state, input, getPadsAttr(pads_begin), getPadsAttr(pads_end));
}

mlir::LogicalResult vpux::IE::ExpandOp::inferReturnTypeComponents(
        mlir::MLIRContext* ctx, Optional<mlir::Location> optLoc, mlir::ValueShapeRange operands,
        mlir::DictionaryAttr attrs, mlir::RegionRange,
        SmallVectorImpl<mlir::ShapedTypeComponents>& inferredReturnShapes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    IE::ExpandOpAdaptor expand(operands, attrs);
    if (mlir::failed(expand.verify(loc))) {
        return mlir::failure();
    }

    const auto padBegin = parseIntArrayAttr<int64_t>(expand.pads_begin());
    const auto padEnd = parseIntArrayAttr<int64_t>(expand.pads_end());

    const auto inType = expand.input().getType().cast<vpux::NDTypeInterface>();
    if (!inType) {
        return mlir::failure();
    }

    const auto newType = inType.pad(ShapeRef(padBegin), ShapeRef(padEnd));
    const auto newTensorType = newType.cast<mlir::RankedTensorType>();
    inferredReturnShapes.emplace_back(newTensorType.getShape(), newTensorType.getElementType(),
                                      newTensorType.getEncoding());

    return mlir::success();
}

//
// inferElemTypeInfo
//

void vpux::IE::ExpandOp::inferElemTypeInfo(vpux::IE::LayerDataInfo<mlir::Type>& info) {
    // E#84659: implement propagate type up for per channel, currently it leads to failures in later passes.
    propagateElementTypeDown(info);
}

void vpux::IE::ExpandOp::inferElemTypeInfoUp(vpux::IE::LayerDataInfo<mlir::Type>& info) {
    // E#84659: implement propagate type up for per channel, currently it leads to failures in later passes.
    propagateElementTypeUp(info);
}

//
// fold
//

mlir::OpFoldResult vpux::IE::ExpandOp::fold(ArrayRef<mlir::Attribute> operands) {
    if (input().getType() == output().getType()) {
        return input();
    }

    // Check for Slice->Expand pair which can be optimized if ExpandOp
    // output is same as SliceOp input
    if (auto sliceOp = input().getDefiningOp<IE::SliceOp>()) {
        // If we got multiple eltwise ops in a chain, for example:
        //       Expand->Add->(Slice->Expand)->Add->Slice
        // We will want to keep the Expand between the 2 Adds instead of folding with Slice.
        // So that we can utilize AdjustInputShapeForEltwise pass to optimize the 2nd Add.
        if (this->getResult().hasOneUse()) {
            auto childOp = *output().getUsers().begin();
            auto unExpandedShape = input().getType().cast<vpux::NDTypeInterface>().getShape().toValues();
            auto expandedShape = output().getType().cast<vpux::NDTypeInterface>().getShape().toValues();
            if (beneficialToKeepExpand(unExpandedShape, expandedShape, childOp)) {
                return nullptr;
            }
        }
        if (sliceOp.source().getType() == output().getType()) {
            return sliceOp.source();
        }
    }

    if (const auto attr = operands[0].dyn_cast_or_null<Const::ContentAttr>()) {
        const auto padsBefore = Shape(parseIntArrayAttr<int64_t>(pads_begin()));
        const auto padsAfter = Shape(parseIntArrayAttr<int64_t>(pads_end()));
        return attr.padWithZero(padsBefore, padsAfter);
    }

    return nullptr;
}

//
// verify
//

mlir::LogicalResult vpux::IE::ExpandOp::verify() {
    const auto op = getOperation();
    if (input().getDefiningOp() != nullptr && mlir::isa<Const::DeclareOp>(input().getDefiningOp())) {
        // Limitations below are not applicable to constants.
        return mlir::success();
    }
    const auto nonZeroPadPredicate = [](const int64_t dim) -> bool {
        return dim > 0;
    };
    const auto padsEnd = parseIntArrayAttr<int64_t>(pads_end());
    const auto nonZeroPadsEnd = std::count_if(padsEnd.begin(), padsEnd.end(), nonZeroPadPredicate);
    const auto padsBegin = parseIntArrayAttr<int64_t>(pads_begin());
    const auto nonZeroPadsBegin = std::count_if(padsBegin.begin(), padsBegin.end(), nonZeroPadPredicate);
    if (nonZeroPadsEnd == 0 && nonZeroPadsBegin == 0) {
        // Such pad configuration is foldable.
        return mlir::success();
    }

    if (nonZeroPadsBegin > 1) {
        return errorAt(op, "pads_begin must contain at most one non-zero value. Got: {0}", padsEnd);
    }

    if (nonZeroPadsEnd > 1) {
        return errorAt(op, "pads_end must contain at most one non-zero value. Got: {0}", padsEnd);
    }

    const auto padBeginAxisIter = std::find_if(padsBegin.begin(), padsBegin.end(), nonZeroPadPredicate);
    if (padBeginAxisIter != padsBegin.end()) {
        const auto padAxis = std::distance(padsBegin.begin(), padBeginAxisIter);
        const auto inShape = getShape(input());
        if (padAxis >= checked_cast<int64_t>(inShape.size())) {
            return errorAt(op, "pads_begin axis {0} exceeds input rank {1}", padAxis, inShape.size());
        }
        const auto outShape = getShape(output());
        if (padAxis >= checked_cast<int64_t>(outShape.size())) {
            return errorAt(op, "pads_begin axis {0} exceeds output rank {1}", padAxis, inShape.size());
        }
    }

    const auto padEndAxisIter = std::find_if(padsEnd.begin(), padsEnd.end(), nonZeroPadPredicate);
    if (padEndAxisIter != padsEnd.end()) {
        const auto padAxis = std::distance(padsEnd.begin(), padEndAxisIter);
        const auto inShape = getShape(input());
        if (padAxis >= checked_cast<int64_t>(inShape.size())) {
            return errorAt(op, "pads_end axis {0} exceeds input rank {1}", padAxis, inShape.size());
        }
        const auto outShape = getShape(output());
        if (padAxis >= checked_cast<int64_t>(outShape.size())) {
            return errorAt(op, "pads_end axis {0} exceeds output rank {1}", padAxis, inShape.size());
        }
    }

    if (padBeginAxisIter != padsBegin.end() && padEndAxisIter != padsEnd.end()) {
        const auto padBeginAxis = std::distance(padsBegin.begin(), padBeginAxisIter);
        const auto padEndAxis = std::distance(padsEnd.begin(), padEndAxisIter);
        if (padBeginAxis != padEndAxis) {
            return errorAt(op, "pads_begin axis {0} does not match pads_end {1}", padBeginAxis, padEndAxis);
        }
    }

    return mlir::success();
}
