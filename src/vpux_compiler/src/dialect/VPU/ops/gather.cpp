//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPU/ops.hpp"

#include "vpux/compiler/dialect/const/ops.hpp"
#include "vpux/compiler/utils/error.hpp"

#include "vpux/utils/core/checked_cast.hpp"

using namespace vpux;

namespace {

mlir::FailureOr<int64_t> extractAxis(mlir::Location loc, VPU::GatherOpAdaptor gather) {
    if (gather.axis() != nullptr) {
        auto axisConst = gather.axis().getDefiningOp<Const::DeclareOp>();
        if (axisConst == nullptr) {
            return errorAt(loc, "Only constant input is supported for axis");
        }

        const auto axisContent = axisConst.getContent();
        if (!axisContent.isSplat()) {
            return errorAt(loc, "Axis value must be a scalar");
        }

        int64_t axisInd = axisContent.getSplatValue<int64_t>();

        if (axisInd < 0) {
            const auto inType = gather.input().getType().cast<vpux::NDTypeInterface>();
            const auto inRank = inType.getRank();
            axisInd += inRank;
            VPUX_THROW_UNLESS(axisInd >= 0 && axisInd < inRank, "Wrong Gather axis {0}", axisInd);
        }

        return axisInd;
    } else if (gather.axis_value().has_value()) {
        return gather.axis_value().value();
    } else {
        return errorAt(loc, "Axis was not provided");
    }
}

}  // namespace

mlir::LogicalResult vpux::VPU::GatherOp::inferReturnTypes(mlir::MLIRContext* ctx, mlir::Optional<mlir::Location> optLoc,
                                                          mlir::ValueRange operands, mlir::DictionaryAttr attrs,
                                                          mlir::RegionRange /*regions*/,
                                                          mlir::SmallVectorImpl<mlir::Type>& inferredReturnTypes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    VPU::GatherOpAdaptor gather(operands, attrs);
    if (mlir::failed(gather.verify(loc))) {
        return mlir::failure();
    }

    const auto inType = gather.input().getType().cast<vpux::NDTypeInterface>();
    const auto inputShape = inType.getShape().raw();
    const auto indicesShape = gather.indices().getType().cast<vpux::NDTypeInterface>().getShape().raw();

    const auto axis = extractAxis(loc, gather);
    if (mlir::failed(axis)) {
        return mlir::failure();
    }

    SmallVector<int64_t> outShape;

    // calculate output shape
    int64_t batchDims = gather.batch_dims();
    int64_t axisVal = checked_cast<int64_t>(*axis);
    int64_t outRank = inputShape.size() + indicesShape.size() - 1 - batchDims;
    int64_t indicesRank = indicesShape.size();
    int64_t i = 0;

    for (; i < batchDims; i++) {
        outShape.push_back(inputShape[i] & indicesShape[i]);
    }
    for (; i < axisVal; i++) {
        outShape.push_back(inputShape[i]);
    }
    for (; i < axisVal + indicesRank - batchDims; i++) {
        outShape.push_back(indicesShape[batchDims - axisVal + i]);
    }
    for (; i < outRank; i++) {
        outShape.push_back(inputShape[batchDims + 1 - indicesRank + i]);
    }

    const auto outType = inType.changeShape(Shape(outShape));
    inferredReturnTypes.push_back(outType);

    return mlir::success();
}

//
// serialize
//

EMU::BlobWriter::SpecificTask vpux::VPU::GatherOp::serialize(EMU::BlobWriter& writer) {
    MVCNN::GatherParamsBuilder builder(writer);
    builder.add_axis(checked_cast<uint32_t>(axis_value().value()));
    const auto paramsOff = builder.Finish();
    return writer.createUPALayerTask(*this, {paramsOff.Union(), MVCNN::SoftwareLayerParams_GatherParams});
}

//
// TilingBuilderOpInterface
//

vpux::InputTiling vpux::VPU::GatherOp::backInferTileInfo(const vpux::TileInfo& outputTile, vpux::Logger log) {
    const auto origInputShape = getShape(input());
    const auto origIndicesShape = getShape(indices());
    bool hasAxisTensor = false;

    int64_t axisValue = 0;

    if (axis_valueAttr() != nullptr) {
        axisValue = axis_valueAttr().dyn_cast_or_null<mlir::IntegerAttr>().getValue().getSExtValue();
    }
    if (axis() != nullptr) {
        auto axisConst = axis().getDefiningOp<Const::DeclareOp>();
        VPUX_THROW_UNLESS(axisConst != nullptr, "Only constant input is supported for axis");
        const auto axisContent = axisConst.getContent();
        VPUX_THROW_UNLESS(axisContent.isSplat(), "Axis value must be a scalar");
        axisValue = axisContent.getSplatValue<int64_t>();
        hasAxisTensor = true;
    }
    int64_t batchDims = 0;
    if (batch_dimsAttr() != nullptr) {
        batchDims = batch_dimsAttr().dyn_cast_or_null<mlir::IntegerAttr>().getValue().getSExtValue();
    }

    return vpux::backInferGatherTile(outputTile, origInputShape, origIndicesShape, axisValue, batchDims, hasAxisTensor,
                                     log);
}

void vpux::VPU::GatherOp::adjustAttrs(const TilingInfo& /*inputTiling*/, const TileInfo& /*outputTile*/) {
}

mlir::FailureOr<OutputTiling> vpux::VPU::GatherOp::getTilingStrategy(TilingMode tilingMode, Logger log) {
    auto baseOp = this->getOperation();
    VPUX_THROW_WHEN(tilingMode != TilingMode::ISOLATED,
                    "Only supporting isolated tiling for Gather currently, for op {0} at '{1}'", baseOp->getName(),
                    getLoc());
    int64_t axisValue = 0;

    if (axis_valueAttr() != nullptr) {
        axisValue = axis_valueAttr().dyn_cast_or_null<mlir::IntegerAttr>().getValue().getSExtValue();
    }
    if (axis() != nullptr) {
        auto axisConst = axis().getDefiningOp<Const::DeclareOp>();
        VPUX_THROW_UNLESS(axisConst != nullptr, "Only constant input is supported for axis");
        const auto axisContent = axisConst.getContent();
        VPUX_THROW_UNLESS(axisContent.isSplat(), "Axis value must be a scalar");
        axisValue = axisContent.getSplatValue<int64_t>();
    }

    auto tilingInfo = mlir::dyn_cast<VPU::TilingInfoOpInterface>(baseOp);
    VPUX_THROW_WHEN(tilingInfo == nullptr, "Operation '{0}' doesn't implement TilingInfoOpInterface",
                    baseOp->getName());
    const auto outputType = baseOp->getResult(0).getType().cast<vpux::NDTypeInterface>();
    const auto outputShape = outputType.getShape();
    Shape nTilesOnDimforGather(outputShape.size(), 1);
    const auto isSupportedTileSize = [baseOp, &tilingInfo, outputShape, log](ShapeRef nTilesOnDim,
                                                                             TilingMode tilingMode) -> bool {
        const auto tiles = fillDividedTiles(baseOp, nTilesOnDim, outputShape);
        if (mlir::failed(tiles)) {
            return false;
        }
        return tilingInfo.isSupportedTiling(tiles.value(), tilingMode, log);
    };

    int64_t batchDims = 0;
    if (batch_dimsAttr() != nullptr) {
        batchDims = batch_dimsAttr().dyn_cast_or_null<mlir::IntegerAttr>().getValue().getSExtValue();
    }

    const auto inputType = input().getType().cast<vpux::NDTypeInterface>();
    const auto inputSize = inputType.getCompactAllocSize();
    const auto indicesType = indices().getType().cast<vpux::NDTypeInterface>();
    const auto indicesShape = indicesType.getShape();
    const auto indicesSize = indicesType.getCompactAllocSize();
    const auto outputRank = static_cast<int64_t>(outputShape.size());
    const auto indicesRank = static_cast<int64_t>(indicesShape.size());

    SmallVector<int64_t> batchDimsRange, dataBeforeAxisRange, indicesRange, dataAfterAxisRange;
    for (int64_t i = 0; i < outputRank; ++i) {
        if (i < batchDims) {
            batchDimsRange.push_back(i);
        } else if (batchDims <= i && i < axisValue) {
            dataBeforeAxisRange.push_back(i);
        } else if (axisValue <= i && i < axisValue + indicesRank - batchDims) {
            indicesRange.push_back(i);
        } else {
            dataAfterAxisRange.push_back(i);
        }
    }
    SmallVector<int64_t> tileDimOrder;
    tileDimOrder.insert(tileDimOrder.end(), batchDimsRange.begin(), batchDimsRange.end());
    if (inputSize > indicesSize) {
        // TileDimOrder: {batchDimsRange, dataBeforeAxisRange, dataAfterAxisRange, indicesRange}.
        tileDimOrder.insert(tileDimOrder.end(), dataBeforeAxisRange.begin(), dataBeforeAxisRange.end());
        tileDimOrder.insert(tileDimOrder.end(), dataAfterAxisRange.begin(), dataAfterAxisRange.end());
        tileDimOrder.insert(tileDimOrder.end(), indicesRange.begin(), indicesRange.end());
    } else {
        // TileDimOrder: {batchDimsRange, indicesRange, dataBeforeAxisRange, dataAfterAxisRange}.
        tileDimOrder.insert(tileDimOrder.end(), indicesRange.begin(), indicesRange.end());
        tileDimOrder.insert(tileDimOrder.end(), dataBeforeAxisRange.begin(), dataBeforeAxisRange.end());
        tileDimOrder.insert(tileDimOrder.end(), dataAfterAxisRange.begin(), dataAfterAxisRange.end());
    }
    auto tileDimIter = tileDimOrder.begin();
    auto dimToTile = *tileDimIter;
    while (tileDimIter < tileDimOrder.end() && !isSupportedTileSize(nTilesOnDimforGather, tilingMode)) {
        if (nTilesOnDimforGather[Dim(dimToTile)] >= outputShape[Dim(dimToTile)]) {
            dimToTile = *(++tileDimIter);
        } else {
            ++nTilesOnDimforGather[Dim(dimToTile)];
        }
    }

    log.trace("Isolated tiling strategy: {0}", nTilesOnDimforGather);
    return fillDividedTiles(baseOp, nTilesOnDimforGather, outputShape);
}
