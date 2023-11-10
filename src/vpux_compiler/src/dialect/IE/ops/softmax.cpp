//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/ops.hpp"
#include "vpux/compiler/utils/attributes_utils.hpp"
#include "vpux/utils/core/checked_cast.hpp"

using namespace vpux;

mlir::LogicalResult vpux::IE::SoftMaxOp::inferReturnTypeComponents(
        mlir::MLIRContext* ctx, Optional<mlir::Location> optLoc, mlir::ValueShapeRange operands,
        mlir::DictionaryAttr attrs, mlir::RegionRange,
        SmallVectorImpl<mlir::ShapedTypeComponents>& inferredReturnShapes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    IE::SoftMaxOpAdaptor softMax(operands, attrs);
    if (mlir::failed(softMax.verify(loc))) {
        return mlir::failure();
    }

    const auto inType = softMax.input().getType().cast<mlir::ShapedType>();
    inferredReturnShapes.emplace_back(inType.getShape(), inType.getElementType());

    return mlir::success();
}

mlir::OpFoldResult vpux::IE::SoftMaxOp::fold(ArrayRef<mlir::Attribute>) {
    const auto inType = input().getType().cast<mlir::ShapedType>();
    const auto inShape = inType.getShape();
    const auto inRank = inType.getRank();

    auto axis = checked_cast<int64_t>(axisInd());

    if (axis < 0) {
        axis += inRank;
    }

    VPUX_THROW_UNLESS(axis >= 0 && axis < inRank, "Wrong SoftMax axis {0}", axis);

    if (inShape[axis] > 1) {
        return nullptr;
    }

    const auto valueType = mlir::RankedTensorType::get(inShape, mlir::Float32Type::get(getContext()));
    const auto baseContent = Const::ContentAttr::get(mlir::DenseElementsAttr::get(valueType, 1.0f));

    return baseContent.convertElemType(output().getType().cast<mlir::ShapedType>().getElementType());
}

//
// LegalizeAxisInd
//

namespace {

class LegalizeAxisInd final : public mlir::OpRewritePattern<IE::SoftMaxOp> {
public:
    using mlir::OpRewritePattern<IE::SoftMaxOp>::OpRewritePattern;

public:
    mlir::LogicalResult matchAndRewrite(IE::SoftMaxOp softmaxOp, mlir::PatternRewriter& rewriter) const final;
};

mlir::LogicalResult LegalizeAxisInd::matchAndRewrite(IE::SoftMaxOp softmaxOp, mlir::PatternRewriter& rewriter) const {
    auto inputType = softmaxOp.input().getType().cast<vpux::NDTypeInterface>();
    int64_t axis = softmaxOp.axisInd();

    if (axis >= 0) {
        return mlir::failure();
    }

    int64_t legalizeAxis = vpux::getPositiveAxisInd(softmaxOp.axisIndAttr(), inputType.getRank());
    const auto legalizeAxisAttr = getIntAttr(rewriter.getContext(), legalizeAxis);

    rewriter.replaceOpWithNewOp<IE::SoftMaxOp>(softmaxOp, softmaxOp.input(), legalizeAxisAttr, nullptr);
    return mlir::success();
}

}  // namespace

//
// getCanonicalizationPatterns
//

void vpux::IE::SoftMaxOp::getCanonicalizationPatterns(mlir::RewritePatternSet& patterns, mlir::MLIRContext* context) {
    patterns.add<LegalizeAxisInd>(context);
}
