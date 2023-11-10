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

mlir::FailureOr<SmallVector<int64_t>> getAxes(VPU::UnsqueezeOpAdaptor unsqueeze, mlir::Location loc) {
    if (unsqueeze.axes() != nullptr && unsqueeze.axes_value().has_value()) {
        return errorAt(loc, "Ambiguous axes representation");
    }
    if (unsqueeze.axes() == nullptr && !unsqueeze.axes_value().has_value()) {
        return errorAt(loc, "Missed axes representation");
    }

    if (unsqueeze.axes_value().has_value()) {
        return parseIntArrayAttr<int64_t>(unsqueeze.axes_value().value());
    }

    auto axesConst = unsqueeze.axes().getDefiningOp<Const::DeclareOp>();
    if (axesConst == nullptr) {
        return errorAt(loc, "Only constant axes are supported");
    }

    const auto axesContent = axesConst.getContent();
    auto axes = to_small_vector(axesContent.getValues<int64_t>());
    std::sort(axes.begin(), axes.end());

    const auto inType = unsqueeze.input().getType().cast<vpux::NDTypeInterface>();
    const auto inRank = inType.getRank();
    const auto numAxes = checked_cast<int64_t>(axes.size());

    for (auto& axis : axes) {
        if (axis < 0) {
            axis += inRank + numAxes;
        }
    }

    return axes;
}
}  // namespace

mlir::LogicalResult vpux::VPU::UnsqueezeOp::inferReturnTypes(mlir::MLIRContext* ctx,
                                                             mlir::Optional<mlir::Location> optLoc,
                                                             mlir::ValueRange operands, mlir::DictionaryAttr attrs,
                                                             mlir::RegionRange /*regions*/,
                                                             mlir::SmallVectorImpl<mlir::Type>& inferredReturnTypes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    VPU::UnsqueezeOpAdaptor unsqueeze(operands, attrs);
    if (mlir::failed(unsqueeze.verify(loc))) {
        return mlir::failure();
    }

    const auto axes = ::getAxes(unsqueeze, loc);
    if (mlir::failed(axes)) {
        return mlir::failure();
    }

    const auto input = unsqueeze.input();
    const auto inType = input.getType().cast<vpux::NDTypeInterface>();
    const auto inShape = inType.getShape().raw();
    const auto inOrder = DimsOrder::fromValue(input);

    SmallVector<int64_t> outShape(inShape.size() + axes->size());

    size_t inInd = 0;
    size_t axesInd = 0;
    for (auto outInd : irange(outShape.size())) {
        if (axesInd < axes.value().size()) {
            const auto nextAxisInd = checked_cast<size_t>(axes.value()[axesInd]);

            if (nextAxisInd < outInd) {
                return errorAt(loc, "Axis '{0}' was occurred twice", nextAxisInd);
            }

            if (nextAxisInd == outInd) {
                outShape[outInd] = 1;
                ++axesInd;
                continue;
            }
        }

        if (inInd < inShape.size()) {
            outShape[outInd] = inShape[inInd];
            ++inInd;
            continue;
        }
    }
    if (inInd != inShape.size() || axesInd != axes->size()) {
        return errorAt(loc, "Inconsistent parameters");
    }

    const auto typeComponents = TypeComponents()
                                        .setShape(Shape(outShape))
                                        .setDimsOrder(vpux::VPU::inferUnsqueezeOutputLayout(inOrder.toPermutation(),
                                                                                            axes.value(), inShape));
    auto outType = inType.changeTypeComponents(typeComponents);
    inferredReturnTypes.push_back(outType);

    return mlir::success();
}

//
// serialize
//

EMU::BlobWriter::SpecificTask vpux::VPU::UnsqueezeOp::serialize(EMU::BlobWriter& writer) {
    MVCNN::ReshapeParamsBuilder builder(writer);
    const auto paramsOff = builder.Finish();
    return writer.createUPALayerTask(*this, {paramsOff.Union(), MVCNN::SoftwareLayerParams_ReshapeParams});
}
