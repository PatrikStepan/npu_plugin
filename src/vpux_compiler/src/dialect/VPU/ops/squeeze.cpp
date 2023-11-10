//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPU/ops.hpp"
#include "vpux/compiler/dialect/VPU/utils/layout_utils.hpp"

#include "vpux/compiler/dialect/const/ops.hpp"
#include "vpux/compiler/utils/attributes.hpp"
#include "vpux/compiler/utils/error.hpp"

#include "vpux/utils/core/checked_cast.hpp"
#include "vpux/utils/core/small_vector.hpp"

#include <mlir/IR/PatternMatch.h>

#include <numeric>

using namespace vpux;

//
// getAxes
//

namespace {

mlir::FailureOr<SmallVector<int64_t>> getAxes(VPU::SqueezeOpAdaptor squeeze, mlir::Location loc) {
    if (squeeze.axes() != nullptr && squeeze.axes_value().has_value()) {
        return errorAt(loc, "Ambiguous axes representation");
    }
    if (squeeze.axes() == nullptr && !squeeze.axes_value().has_value()) {
        return errorAt(loc, "Missed axes representation");
    }

    if (squeeze.axes_value().has_value()) {
        return parseIntArrayAttr<int64_t>(squeeze.axes_value().value());
    }

    auto axesConst = squeeze.axes().getDefiningOp<Const::DeclareOp>();
    if (axesConst == nullptr) {
        return errorAt(loc, "Only constant axes are supported");
    }

    const auto axesContent = axesConst.getContent();
    auto axes = to_small_vector(axesContent.getValues<int64_t>());
    std::sort(axes.begin(), axes.end());

    const auto inType = squeeze.input().getType().cast<vpux::NDTypeInterface>();
    const auto inRank = inType.getRank();

    for (auto& axis : axes) {
        if (axis < 0) {
            axis += inRank;
        }
    }

    return axes;
}

}  // namespace

mlir::LogicalResult vpux::VPU::SqueezeOp::inferReturnTypes(mlir::MLIRContext* ctx,
                                                           mlir::Optional<mlir::Location> optLoc,
                                                           mlir::ValueRange operands, mlir::DictionaryAttr attrs,
                                                           mlir::RegionRange /*regions*/,
                                                           mlir::SmallVectorImpl<mlir::Type>& inferredReturnTypes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    VPU::SqueezeOpAdaptor squeeze(operands, attrs);
    if (mlir::failed(squeeze.verify(loc))) {
        return mlir::failure();
    }

    const auto axes = ::getAxes(squeeze, loc);
    if (mlir::failed(axes)) {
        return mlir::failure();
    }

    const auto input = squeeze.input();
    const auto inType = input.getType().cast<vpux::NDTypeInterface>();
    const auto inShape = inType.getShape().raw();
    const auto inOrder = DimsOrder::fromValue(input);

    SmallVector<int64_t> outShape;

    if (axes->empty()) {
        for (auto dim : inShape) {
            if (dim != 1) {
                outShape.push_back(dim);
            }
        }
    } else {
        size_t axesInd = 0;
        for (auto inInd : irange(inShape.size())) {
            if (axesInd < axes->size()) {
                const auto nextAxisInd = checked_cast<size_t>(axes.value()[axesInd]);

                if (nextAxisInd < inInd) {
                    return errorAt(loc, "Axis '{0}' was occurred twice", nextAxisInd);
                }

                if (nextAxisInd == inInd) {
                    if (inShape[inInd] != 1) {
                        return errorAt(loc, "Can't exclude '{0}' dimension, it is not equal to 1", nextAxisInd);
                    }

                    ++axesInd;

                    continue;
                }
            }

            outShape.push_back(inShape[inInd]);
        }
    }

    const auto typeComponents =
            TypeComponents()
                    .setShape(Shape(outShape))
                    .setDimsOrder(vpux::VPU::inferSqueezeOutputLayout(inOrder.toPermutation(), axes.value(), inShape));

    auto outType = inType.changeTypeComponents(typeComponents);
    inferredReturnTypes.push_back(outType);

    return mlir::success();
}

//
// serialize
//

EMU::BlobWriter::SpecificTask vpux::VPU::SqueezeOp::serialize(EMU::BlobWriter& writer) {
    MVCNN::ReshapeParamsBuilder builder(writer);
    const auto paramsOff = builder.Finish();
    return writer.createUPALayerTask(*this, {paramsOff.Union(), MVCNN::SoftwareLayerParams_ReshapeParams});
}
