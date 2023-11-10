//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/ops.hpp"
#include "vpux/compiler/dialect/IE/utils/fft_ops_utils.hpp"
#include "vpux/compiler/dialect/IE/utils/shape_infer.hpp"
#include "vpux/compiler/utils/attributes.hpp"

using namespace vpux;

mlir::LogicalResult vpux::IE::IRDFTOp::inferReturnTypeComponents(
        mlir::MLIRContext* ctx, Optional<mlir::Location> optLoc, mlir::ValueShapeRange operands,
        mlir::DictionaryAttr attrs, mlir::RegionRange,
        SmallVectorImpl<mlir::ShapedTypeComponents>& inferredReturnShapes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));
    IE::IRDFTOpAdaptor op(operands, attrs);
    if (mlir::failed(op.verify(loc))) {
        return mlir::failure();
    }
    const auto inType = op.input().getType().cast<mlir::ShapedType>();
    auto outShape = to_small_vector(inType.getShape());
    auto params = fftExtractParams(loc, op);
    if (mlir::failed(params)) {
        return mlir::failure();
    }
    auto axes = params.value().axes;
    auto signalSize = params.value().signalSize;
    // delete last size, 2 in this case
    outShape.pop_back();
    const auto lastAxis = axes.back();
    outShape[lastAxis] = (outShape[lastAxis] - 1) * 2;
    for (size_t i = 0; i < axes.size(); ++i) {
        if (signalSize[i] != -1) {
            outShape[axes[i]] = signalSize[i];
        }
    }
    inferredReturnShapes.emplace_back(outShape, inType.getElementType());
    return mlir::success();
}

//
// getCanonicalizationPatterns
//

void vpux::IE::IRDFTOp::getCanonicalizationPatterns(mlir::RewritePatternSet& patterns, mlir::MLIRContext* context) {
    patterns.add<IE::FftConvertConstToAttrAndNormalize<IE::IRDFTOp>>(context);
}
