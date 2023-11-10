//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPU/ops.hpp"
#include "vpux/compiler/dialect/VPU/utils/const_utils.hpp"
#include "vpux/compiler/dialect/VPU/utils/distributed_tensor_utils.hpp"

using namespace vpux;

mlir::LogicalResult vpux::VPU::SoftMaxOp::inferReturnTypes(mlir::MLIRContext* ctx,
                                                           mlir::Optional<mlir::Location> optLoc,
                                                           mlir::ValueRange operands, mlir::DictionaryAttr attrs,
                                                           mlir::RegionRange /*regions*/,
                                                           mlir::SmallVectorImpl<mlir::Type>& inferredReturnTypes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    VPU::SoftMaxOpAdaptor softMax(operands, attrs);
    if (mlir::failed(softMax.verify(loc))) {
        return mlir::failure();
    }

    const auto inType = softMax.input().getType();
    inferredReturnTypes.push_back(inType);

    return mlir::success();
}

//
// serialize
//

EMU::BlobWriter::SpecificTask vpux::VPU::SoftMaxOp::serialize(EMU::BlobWriter& writer) {
    MVCNN::SoftmaxParamsBuilder builder(writer);
    builder.add_axis(checked_cast<uint32_t>(axisInd()));
    const auto paramsOff = builder.Finish();

    return writer.createUPALayerTask(*this, {paramsOff.Union(), MVCNN::SoftwareLayerParams_SoftmaxParams});
}

//
// TilingBuilderOpInterface
//

vpux::InputTiling vpux::VPU::SoftMaxOp::backInferTileInfo(const vpux::TileInfo& outputTile, vpux::Logger /*log*/) {
    return TilingInfo(outputTile);
}

void vpux::VPU::SoftMaxOp::adjustAttrs(const TilingInfo& /*inputTiling*/, const TileInfo& /*outputTile*/) {
}

mlir::FailureOr<OutputTiling> vpux::VPU::SoftMaxOp::getTilingStrategy(TilingMode tilingMode, Logger log) {
    return vpux::getSWLayerTilingStrategy(this->getOperation(), tilingMode, log);
}

//
// SWOpInterface
//

bool vpux::VPU::SoftMaxOp::checkStrategyCompatibility(VPU::MultiClusterStrategy strategy) {
    const auto inputType = input().getType().cast<vpux::NDTypeInterface>();
    const auto inShape = inputType.getShape();

    if (strategy == VPU::MultiClusterStrategy::Clustering) {
        return true;
    }

    // Split input/output by H dim when axisInd is not point to H
    if (strategy == VPU::MultiClusterStrategy::SplitOverHeight && axisInd() != Dims4D::Act::H.ind() &&
        inShape[Dims4D::Act::H] > 1) {
        return true;
    }

    // Split input/output by C dim when axisInd is not point to C
    if (strategy == VPU::MultiClusterStrategy::SplitOverKernel && axisInd() != Dims4D::Act::C.ind() &&
        inShape[Dims4D::Act::C] > 1) {
        return true;
    }

    // Split input/output by W dim when axisInd is not point to W
    if (strategy == VPU::MultiClusterStrategy::SplitOverWidth && axisInd() != Dims4D::Act::W.ind() &&
        inShape[Dims4D::Act::W] > 1) {
        return true;
    }

    return false;
}

vpux::VPU::DistributedTensorAttr vpux::VPU::SoftMaxOp::getExplicitDistributedTensorAttr(
        vpux::ShapeRef shape, vpux::VPU::DistributionMode distributionMode, mlir::ArrayAttr numTiles,
        mlir::IntegerAttr numClusters, mlir::ArrayAttr alignment, mlir::ArrayAttr /*kernel*/,
        vpux::VPU::PaddingAttr /*pad*/, mlir::ArrayAttr /*stride*/, mlir::UnitAttr uniformDistributedSegments) {
    return VPU::getSWExplicitDistributedTensorAttr(mlir::dyn_cast<VPU::SWOpInterface>(getOperation()), shape,
                                                   distributionMode, numTiles, numClusters, alignment,
                                                   uniformDistributedSegments);
}

void vpux::VPU::SoftMaxOp::build(::mlir::OpBuilder& odsBuilder, ::mlir::OperationState& odsState, ::mlir::Value input,
                                 ::mlir::IntegerAttr axisInd, ::mlir::IntegerAttr padSize) {
    build(odsBuilder, odsState, input.getType(), input, axisInd, padSize, {});
}

bool vpux::VPU::SoftMaxOp::fitIntoCMX(llvm::ArrayRef<vpux::NDTypeInterface> buffers, Byte reservedMem) {
    VPUX_THROW_UNLESS(buffers.size() == 2, "SoftMaxOp requires 1 input and 1 output, but the number of buffer is {0}",
                      buffers.size());

    SmallVector<Byte> buffersSize;
    std::transform(buffers.begin(), buffers.end(), std::back_inserter(buffersSize), [](const auto buffer) {
        return buffer.getTotalAllocSize();
    });

    auto totalAvailableCMXSize = reservedMem.count() == 0 ? getTotalCMXSize(getOperation()).count()
                                                          : getTotalCMXFragmentationAwareSize(getOperation()).count();

    return vpux::VPU::calculateAlignedBuffersMemoryRequirement(getArch(getOperation()), buffersSize).count() +
                   reservedMem.count() <=
           totalAvailableCMXSize;
}

bool vpux::VPU::SoftMaxOp::fitIntoCMX(llvm::ArrayRef<vpux::NDTypeInterface> buffers) {
    return fitIntoCMX(buffers, Byte(0));
}

// Cost model now compare SOH and SOK, but not include SOW.
// After stride access was supported in kernel, softmax can use all them,
// the only limitation is choosing dim not point to axisInd.
// So that use default strategy with order SOH->SOK->SOW

bool vpux::VPU::SoftMaxOp::supportCycleCostCalculation() {
    return false;
}
