//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/ops.hpp"
#include "vpux/compiler/utils/error.hpp"
#include "vpux/utils/core/checked_cast.hpp"

using namespace vpux;

//
// verify
//

mlir::LogicalResult vpux::IE::ScatterElementsUpdateOp::verify() {
    if (axis() != nullptr) {
        auto axisNumElements = axis().getType().cast<vpux::NDTypeInterface>().getNumElements();
        if (axisNumElements != 1) {
            return errorAt(*this, "Axis should have only 1 element, while it has {0}", axisNumElements);
        }

        if (axis_value().has_value()) {
            return errorAt(*this, "Ambiguous axis representation");
        }
    }

    if (axis() == nullptr && !axis_value().has_value()) {
        return errorAt(*this, "Axis was not provided");
    }

    return mlir::success();
}

mlir::LogicalResult vpux::IE::ScatterElementsUpdateOp::inferReturnTypeComponents(
        mlir::MLIRContext* ctx, Optional<mlir::Location> optLoc, mlir::ValueShapeRange operands,
        mlir::DictionaryAttr attrs, mlir::RegionRange,
        SmallVectorImpl<mlir::ShapedTypeComponents>& inferredReturnShapes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    IE::ScatterElementsUpdateOpAdaptor scatterElementsUpdate(operands, attrs);
    if (mlir::failed(scatterElementsUpdate.verify(loc))) {
        return mlir::failure();
    }

    const auto inType = scatterElementsUpdate.input().getType().cast<mlir::ShapedType>();

    inferredReturnShapes.emplace_back(inType.getShape(), inType.getElementType());

    return mlir::success();
}

//
// ConvertConstToAttr
//

namespace {

class ConvertConstToAttr final : public mlir::OpRewritePattern<IE::ScatterElementsUpdateOp> {
public:
    using mlir::OpRewritePattern<IE::ScatterElementsUpdateOp>::OpRewritePattern;

public:
    mlir::LogicalResult matchAndRewrite(IE::ScatterElementsUpdateOp scatterElementsUpdateOp,
                                        mlir::PatternRewriter& rewriter) const final;
};

mlir::LogicalResult ConvertConstToAttr::matchAndRewrite(IE::ScatterElementsUpdateOp scatterElementsUpdateOp,
                                                        mlir::PatternRewriter& rewriter) const {
    auto axis = scatterElementsUpdateOp.axis();
    if (axis == nullptr) {
        return mlir::failure();
    }

    auto axisConst = axis.getDefiningOp<Const::DeclareOp>();
    if (axisConst == nullptr) {
        return mlir::failure();
    }

    const auto axisContent = axisConst.getContent();
    if (!axisContent.isSplat()) {
        return mlir::failure();
    }

    const auto axisValue = static_cast<int64_t>(axisContent.getSplatValue<int32_t>());
    rewriter.replaceOpWithNewOp<IE::ScatterElementsUpdateOp>(
            scatterElementsUpdateOp, scatterElementsUpdateOp.getType(), scatterElementsUpdateOp.input(),
            scatterElementsUpdateOp.indices(), scatterElementsUpdateOp.updates(), nullptr,
            rewriter.getI64IntegerAttr(axisValue));
    return mlir::success();
}

}  // namespace

void vpux::IE::ScatterElementsUpdateOp::getCanonicalizationPatterns(mlir::RewritePatternSet& patterns,
                                                                    mlir::MLIRContext* context) {
    patterns.add<ConvertConstToAttr>(context);
}
