//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPU/ops.hpp"
#include "vpux/compiler/dialect/VPU/utils/const_utils.hpp"
#include "vpux/compiler/dialect/VPU/utils/distributed_tensor_utils.hpp"

using namespace vpux;

mlir::LogicalResult vpux::VPU::HardSigmoidOp::inferReturnTypes(mlir::MLIRContext* ctx,
                                                               mlir::Optional<mlir::Location> optLoc,
                                                               mlir::ValueRange operands, mlir::DictionaryAttr attrs,
                                                               mlir::RegionRange /*regions*/,
                                                               mlir::SmallVectorImpl<mlir::Type>& inferredReturnTypes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    VPU::HardSigmoidOpAdaptor hardSigmoid(operands, attrs);
    if (mlir::failed(hardSigmoid.verify(loc))) {
        return mlir::failure();
    }

    const auto inType = hardSigmoid.input().getType();
    inferredReturnTypes.push_back(inType);

    return mlir::success();
}

//
// ClusteredOpInterface
//

bool vpux::VPU::HardSigmoidOp::checkStrategyCompatibility(VPU::MultiClusterStrategy strategy) {
    return strategy == VPU::MultiClusterStrategy::Clustering ||
           strategy == VPU::MultiClusterStrategy::SplitOverKernel ||
           strategy == VPU::MultiClusterStrategy::SplitOverHeight ||
           strategy == VPU::MultiClusterStrategy::SplitOverWidth;
}

void vpux::VPU::HardSigmoidOp::build(::mlir::OpBuilder& odsBuilder, ::mlir::OperationState& odsState,
                                     ::mlir::Value input, ::mlir::FloatAttr alpha, ::mlir::FloatAttr beta) {
    build(odsBuilder, odsState, input.getType(), input, alpha, beta, {});
}

vpux::VPU::DistributedTensorAttr vpux::VPU::HardSigmoidOp::getExplicitDistributedTensorAttr(
        vpux::ShapeRef shape, vpux::VPU::DistributionMode distributionMode, mlir::ArrayAttr numTiles,
        mlir::IntegerAttr numClusters, mlir::ArrayAttr alignment, mlir::ArrayAttr /*kernel*/,
        vpux::VPU::PaddingAttr /*pad*/, mlir::ArrayAttr /*stride*/, mlir::UnitAttr uniformDistributedSegments) {
    return VPU::getSWExplicitDistributedTensorAttr(mlir::dyn_cast<VPU::SWOpInterface>(getOperation()), shape,
                                                   distributionMode, numTiles, numClusters, alignment,
                                                   uniformDistributedSegments);
}

//
// SWOpInterface
//

bool vpux::VPU::HardSigmoidOp::fitIntoCMX(llvm::ArrayRef<vpux::NDTypeInterface> buffers, Byte reservedMem) {
    VPUX_THROW_UNLESS(buffers.size() == 2,
                      "HardSigmoidOp requires 1 input and 1 output, but the number of buffer is {0}", buffers.size());

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

bool vpux::VPU::HardSigmoidOp::fitIntoCMX(llvm::ArrayRef<vpux::NDTypeInterface> buffers) {
    return fitIntoCMX(buffers, Byte(0));
}

bool vpux::VPU::HardSigmoidOp::supportCycleCostCalculation() {
    return false;
}

//
// TilingBuilderOpInterface
//

InputTiling vpux::VPU::HardSigmoidOp::backInferTileInfo(const vpux::TileInfo& outputTile, vpux::Logger /*log*/) {
    return TilingInfo(outputTile);
}

void vpux::VPU::HardSigmoidOp::adjustAttrs(const TilingInfo& /*inputTiling*/, const TileInfo& /*outputTile*/) {
    // Do nothing
}

mlir::FailureOr<OutputTiling> vpux::VPU::HardSigmoidOp::getTilingStrategy(TilingMode tilingMode, Logger log) {
    return vpux::getSWLayerTilingStrategy(this->getOperation(), tilingMode, log);
}

//
// serialize
//

EMU::BlobWriter::SpecificTask vpux::VPU::HardSigmoidOp::serialize(EMU::BlobWriter& writer) {
    const auto alpha = alpha_valueAttr().getValueAsDouble();
    const auto beta = beta_valueAttr().getValueAsDouble();
    const auto sigmoid = MVCNN::CreateHardSigmoidParams(writer, checked_cast<float>(alpha), checked_cast<float>(beta));

    MVCNN::PostOpsParamsBuilder builder(writer);
    builder.add_nested_params_type(MVCNN::PostOpsNestedParams_HardSigmoidParams);
    builder.add_nested_params(sigmoid.Union());
    const auto paramsOff = builder.Finish();

    return writer.createUPALayerTask(*this, {paramsOff.Union(), MVCNN::SoftwareLayerParams_PostOpsParams});
}
