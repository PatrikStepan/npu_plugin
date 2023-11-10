//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPU/ops.hpp"
#include "vpux/compiler/dialect/VPU/utils/const_utils.hpp"
#include "vpux/compiler/dialect/VPU/utils/distributed_tensor_utils.hpp"

#include "vpux/compiler/core/layers.hpp"
#include "vpux/compiler/dialect/VPU/nce_invariant.hpp"
#include "vpux/compiler/dialect/VPU/utils/const_utils.hpp"
#include "vpux/compiler/dialect/VPU/utils/conv_utils.hpp"
#include "vpux/compiler/dialect/VPU/utils/generate_tiling.hpp"
#include "vpux/compiler/utils/empty_node.hpp"
#include "vpux/compiler/utils/error.hpp"
#include "vpux/compiler/utils/rewriter.hpp"

#include <ngraph/validation_util.hpp>

using namespace vpux;

//
// fitIntoCMX
//

bool vpux::VPU::NCEConvolutionOp::fitIntoCMX(vpux::NDTypeInterface input, vpux::NDTypeInterface filter,
                                             vpux::NDTypeInterface output) {
    return fitIntoCMX(input, filter, output, Byte(0));
}

bool vpux::VPU::NCEConvolutionOp::fitIntoCMX(vpux::NDTypeInterface input, vpux::NDTypeInterface filter,
                                             vpux::NDTypeInterface output, Byte reservedMem) {
    const auto filterShape = Shape(parseIntArrayAttr<int64_t>(rawFilterShape()));
    const auto KY = filterShape[Dims4D::Filter::KY];
    const auto KX = filterShape[Dims4D::Filter::KX];

    // These depend on a particular tile
    const auto OC = output.getShape()[Dims4D::Act::C];
    const auto IC = input.getShape()[Dims4D::Act::C];

    const auto inOrder = input.getDimsOrder();

    SmallVector<Byte> buffers = {input.getTotalAllocSize(), filter.getTotalAllocSize(), output.getTotalAllocSize(),
                                 NCEInvariant::getWeightsTableSize(OC)};

    if (inOrder == DimsOrder::NHWC) {
        // do nothing
    } else if (inOrder == DimsOrder::NCHW) {
        const auto kernelSize = Shape{KY, KX};

        const auto kernelStrides = Shape(parseIntArrayAttr<int64_t>(strides()));
        const auto strideW = kernelStrides[Dims4D::Strides::X];

        auto activationWindowSize = NCESparsity::getActivationWindowSize(NCESparsity::Mode::CM_CONV, kernelSize,
                                                                         strideW, input.getElementType(), IC);

        buffers.push_back(activationWindowSize * 1_Byte);
    } else {
        VPUX_THROW("[{0}] Unsupported input layout '{1}'", getLoc(), inOrder);
    }

    auto totalAvailableCMXSize = reservedMem.count() == 0 ? getTotalCMXSize(getOperation()).count()
                                                          : getTotalCMXFragmentationAwareSize(getOperation()).count();

    return vpux::VPU::calculateAlignedBuffersMemoryRequirement(getArch(getOperation()), buffers).count() +
                   reservedMem.count() <=
           totalAvailableCMXSize;
}

//
// isSupported
//

bool vpux::VPU::NCEConvolutionOp::isSupported(IE::ConvolutionOp op, LogCb logCb, bool checkLayout,
                                              bool checkChannelAlignment) {
    return VPU::isSupportedConv(op, logCb, checkLayout, checkChannelAlignment);
}

//
// verify
//

static mlir::LogicalResult verifyConv(mlir::Location loc, VPU::ArchKind arch, VPU::NCEConvolutionOpAdaptor op,
                                      mlir::Value output) {
    const auto filterShape = Shape(parseIntArrayAttr<int64_t>(op.rawFilterShape()));
    const auto kernelStrides = Shape(parseIntArrayAttr<int64_t>(op.strides()));
    const auto padAttr = op.pad();
    const auto weightsTableShape = getShape(op.weightsTable());

    return VPU::verifyConvUtil(loc, arch, filterShape, kernelStrides, padAttr, weightsTableShape, output);
}

mlir::LogicalResult vpux::VPU::NCEConvolutionOp::verify() {
    auto op = getOperation();
    const auto arch = getArch(op);

    const NCEConvolutionOpAdaptor convAdaptor(op->getOperands(), op->getAttrDictionary(), op->getRegions());
    if (mlir::failed(verifyConv(getOperation()->getLoc(), arch, convAdaptor, output()))) {
        return mlir::failure();
    }

    const auto inputOrder = DimsOrder::fromValue(input());

    const auto filterShape = Shape(parseIntArrayAttr<int64_t>(rawFilterShape()));
    const auto KY = filterShape[Dims4D::Filter::KY];
    const auto KX = filterShape[Dims4D::Filter::KX];

    const auto kernelStrides = Shape(parseIntArrayAttr<int64_t>(strides()));
    const auto SX = kernelStrides[Dims4D::Strides::X];

    const auto inputType = input().getType().cast<NDTypeInterface>();
    const auto outputType = output().getType().cast<NDTypeInterface>();

    const auto alignedFilterShape = getShape(filter());
    const auto expectedAlignedFilterShape = inferAlignedFilterShape(inputType, outputType);

    if (alignedFilterShape != expectedAlignedFilterShape) {
        return errorAt(op, "Got wrong shape for C-Major NCE Convolution 'filter' '{0}', expected '{1}'",
                       alignedFilterShape, expectedAlignedFilterShape);
    }

    if (inputOrder == DimsOrder::NHWC) {
        if (activationWindow() != nullptr) {
            return errorAt(op, "'activationWindow' should not be used with Z-Major NCE Convolution");
        }
        if (activation_window_channel_length().has_value()) {
            return errorAt(op, "'activation_window_channel_length' should not be used with Z-Major NCE Convolution");
        }
    } else {
        if (activationWindow() == nullptr) {
            return errorAt(op, "Missing 'activationWindow' operand for C-Major NCE Convolution");
        }
        if (!activation_window_channel_length().has_value()) {
            return errorAt(op, "Missing 'activation_window_channel_length' operand for C-Major NCE Convolution");
        }

        const auto IC = inputType.getShape()[Dims4D::Act::C];

        const auto kernelSize = Shape{KY, KX};

        const auto activationWindowShape = getShape(activationWindow());
        const auto expectedActivationWindowShape = NCESparsity::inferActivationWindowShape(
                NCESparsity::Mode::CM_CONV, kernelSize, SX, inputType.getElementType(), IC);

        if (activationWindowShape != expectedActivationWindowShape) {
            return errorAt(op, "Got wrong shape for 'activationWindow' '{0}', expected '{1}'", activationWindowShape,
                           expectedActivationWindowShape);
        }

        const auto bitPatternSize = VPU::NCESparsity::getBitPatternSize(VPU::NCESparsity::Mode::CM_CONV, kernelSize, SX,
                                                                        inputType.getElementType(), IC);

        if (activation_window_channel_length().value() != bitPatternSize) {
            return errorAt(op, "Got wrong value for 'activation_window_channel_length' '{0}', expected '{1}'",
                           activation_window_channel_length(), bitPatternSize);
        }
    }

    return mlir::success();
}

Shape vpux::VPU::NCEConvolutionOp::inferAlignedFilterShape(NDTypeInterface input, NDTypeInterface output) {
    const auto rawFilterShape = Shape(parseIntArrayAttr<int64_t>(this->rawFilterShape()));
    const auto KY = rawFilterShape[Dims4D::Filter::KY];
    const auto KX = rawFilterShape[Dims4D::Filter::KX];

    const auto IC = input.getShape()[Dims4D::Act::C];
    const auto OC = output.getShape()[Dims4D::Act::C];

    const auto alignment = NCEInvariant::getAlignment(output.getElementType());

    const auto remainder = (IC * KY * KX) % alignment;

    if (remainder == 0) {
        return Shape{OC, IC, KY, KX};
    }

    const auto padding = (remainder > 0) ? (alignment - remainder) : 0;

    return Shape{OC, 1, 1, IC * KY * KX + padding};
}

//
// InferTypeOpInterface
//

mlir::LogicalResult vpux::VPU::NCEConvolutionOp::inferReturnTypes(
        mlir::MLIRContext* ctx, mlir::Optional<mlir::Location> optLoc, mlir::ValueRange operands,
        mlir::DictionaryAttr attrs, mlir::RegionRange /*regions*/,
        mlir::SmallVectorImpl<mlir::Type>& inferredReturnTypes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    NCEConvolutionOpAdaptor op(operands, attrs);
    if (mlir::failed(op.verify(loc))) {
        return mlir::failure();
    }

    const auto inShape = getShape(op.input());
    const auto filterShape = Shape(parseIntArrayAttr<int64_t>(op.rawFilterShape()));

    if (inShape[Dims4D::Act::C] != filterShape[Dims4D::Filter::IC]) {
        return errorAt(loc, "Input tensor channels and filter shape must be the same");
    }

    const auto windowStrides = parseIntArrayAttr<int64_t>(op.strides());
    const auto windowDilations = ngraph::Strides({1, 1});

    const auto padTop = op.pad().getTop().getValue().getSExtValue();
    const auto padBottom = op.pad().getBottom().getValue().getSExtValue();
    const auto padLeft = op.pad().getLeft().getValue().getSExtValue();
    const auto padRight = op.pad().getRight().getValue().getSExtValue();

    const auto dataPaddingBelow = ngraph::CoordinateDiff({padTop, padLeft});
    const auto dataPaddingAbove = ngraph::CoordinateDiff({padBottom, padRight});

    const auto outputShapeNG = ngraph::infer_convolution_forward(
            EmptyNode::instance(), ngraph::Shape(inShape.begin(), inShape.end()),
            ngraph::Strides(windowStrides.size(), 1),  // dummy data dilations
            dataPaddingBelow, dataPaddingAbove, ngraph::Shape(filterShape.begin(), filterShape.end()),
            ngraph::Strides(windowStrides.begin(), windowStrides.end()), windowDilations);

    const auto outputShape = to_small_vector(outputShapeNG.get_shape() | transformed([](size_t val) {
                                                 return checked_cast<int64_t>(val);
                                             }));

    auto inputType = op.input().getType();
    if (auto sparseInputType = inputType.dyn_cast<VPU::SparseTensorType>()) {
        inputType = sparseInputType.getData();
    }
    const auto outputType = inputType.cast<vpux::NDTypeInterface>().changeShape(Shape(outputShape));

    inferredReturnTypes.push_back(outputType);
    return mlir::success();
}

//
// TilingBuilderOpInterface
//

vpux::InputTiling vpux::VPU::NCEConvolutionOp::backInferTileInfo(const vpux::TileInfo& outputTile, vpux::Logger log) {
    const auto origInputShape = getShape(input());
    const auto origFilterShape = Shape(parseIntArrayAttr<int64_t>(rawFilterShape()));
    const auto origPadding = toPadInfo(pad());

    // This op incorporates bias values in WeightsTable
    const auto origBiasShape = ShapeRef();

    auto inputTiling =
            backInferConvTile(outputTile, origInputShape, origFilterShape, origBiasShape, strides(), origPadding);
    VPUX_THROW_UNLESS(mlir::succeeded(checkAndAlignActInputTiling(
                              mlir::cast<VPU::NCEOpInterface>(*this->getOperation()), inputTiling, log)),
                      "Failed to get an aligned act input tiling");

    // Remove bias input tile if present
    if (inputTiling.tiles.size() > 2) {
        // Drop the bias tile
        inputTiling.tiles.pop_back();
    }

    // Adjust filter tile for the aligned filter
    inputTiling.tiles[1].shape = getShape(filter()).toValues();
    inputTiling.tiles[1].shape[Dims4D::Filter::OC] = outputTile.shape[Dims4D::Act::C];

    inputTiling.tiles.push_back(VPU::getWeightsTableTile(this, outputTile));

    if (instructionListTable() != nullptr) {
        inputTiling.tiles.push_back(VPU::getInstructionListTableTile(this, outputTile));
    }

    if (activationWindow() != nullptr) {
        inputTiling.tiles.push_back(VPU::getActivationWindowTile(this, outputTile));
    }

    return inputTiling;
}

void vpux::VPU::NCEConvolutionOp::adjustAttrs(const TilingInfo& inputTiling, const TileInfo& outputTile) {
    VPU::adjustPaddings(this, inputTiling);
    VPU::adjustRawFilterShape(this, outputTile);
}

mlir::FailureOr<OutputTiling> vpux::VPU::NCEConvolutionOp::getTilingStrategy(TilingMode tilingMode, Logger log) {
    return vpux::getHWLayerTilingStrategy(this->getOperation(), tilingMode, log);
}

//
// NCEOpInterface
//

SmallVector<int64_t> vpux::VPU::NCEConvolutionOp::getKernelSizeVal() {
    const auto kernelShape = Shape(parseIntArrayAttr<int64_t>(rawFilterShape()));
    const auto KY = kernelShape[Dims4D::Filter::KY];
    const auto KX = kernelShape[Dims4D::Filter::KX];
    return {KY, KX};
}

SmallVector<int64_t> vpux::VPU::NCEConvolutionOp::getStridesVal() {
    return parseIntArrayAttr<int64_t>(strides());
}

bool vpux::VPU::NCEConvolutionOp::checkStrategyCompatibility(VPU::MultiClusterStrategy strategy) {
    const auto arch = VPU::getArch(getOperation());
    const bool isCMajor =
            VPU::NCEInvariant::isChannelMajorCompatible(arch, input().getType().cast<vpux::NDTypeInterface>());
    if (isCMajor) {
        return strategy == VPU::MultiClusterStrategy::SplitOverHeightOverlapped ||
               strategy == VPU::MultiClusterStrategy::Clustering;
    }
    return strategy == VPU::MultiClusterStrategy::Clustering ||
           strategy == VPU::MultiClusterStrategy::SplitOverHeight ||
           strategy == VPU::MultiClusterStrategy::SplitOverKernel || strategy == VPU::MultiClusterStrategy::HKSwitch;
}

vpux::VPU::DistributedTensorAttr vpux::VPU::NCEConvolutionOp::getExplicitDistributedTensorAttr(
        vpux::ShapeRef shape, vpux::VPU::DistributionMode distributionMode, mlir::ArrayAttr numTiles,
        mlir::IntegerAttr numClusters, mlir::ArrayAttr alignment, mlir::ArrayAttr kernel, vpux::VPU::PaddingAttr pad,
        mlir::ArrayAttr stride, mlir::UnitAttr uniformDistributedSegments) {
    return VPU::getNCEExplicitDistributedTensorAttr(mlir::dyn_cast<VPU::NCEOpInterface>(getOperation()), shape,
                                                    distributionMode, numTiles, numClusters, alignment, kernel, pad,
                                                    stride, uniformDistributedSegments);
}

mlir::LogicalResult vpux::VPU::NCEConvolutionOp::verifyChannels() {
    auto arch = VPU::getArch(*this);
    return mlir::success(
            vpux::VPU::NCEInvariant::isInputActTypeSupported(arch, input().getType().cast<vpux::NDTypeInterface>(),
                                                             getInputChannelAlignment(), true) &&
            vpux::VPU::NCEInvariant::isOutputActTypeSupported(output().getType().cast<vpux::NDTypeInterface>(),
                                                              getOutputChannelAlignment()));
}

mlir::LogicalResult vpux::VPU::NCEConvolutionOp::verifyInputType(vpux::NDTypeInterface inputType) {
    return mlir::success(vpux::VPU::NCEInvariant::isInputActTypeSupported(VPU::getArch(*this), inputType,
                                                                          getInputChannelAlignment(), true));
}

bool vpux::VPU::NCEConvolutionOp::isVFSupported() {
    return vpux::VPU::isVFNCESupported(*this);
}

//
// serialize
//

EMU::BlobWriter::SpecificTask vpux::VPU::NCEConvolutionOp::serialize(EMU::BlobWriter& /*writer*/) {
    VPUX_THROW("NCEConvolutionOp shouldn't have a serializer");
}

//
// sparsitySupport
//

vpux::VPU::SparsitySupport vpux::VPU::NCEConvolutionOp::sparsitySupport() {
    // Super-dense mode does not support ODU sparsity
    const auto arch = getArch(getOperation());
    const auto outputType = output().getType().cast<vpux::NDTypeInterface>();
    auto excludeMode = VPU::NCESparsity::bitwiseNot(VPU::SparsitySupport::NONE);
    if (VPU::NCESparsity::isSuperdenseRequired(arch, outputType.getDimsOrder(), outputType.getShape(),
                                               outputType.getElementType())) {
        excludeMode = VPU::NCESparsity::bitwiseNot(VPU::SparsitySupport::SPARSE_OUTPUTS);
    }

    switch (arch) {
    case VPU::ArchKind::VPUX30XX: {
        // Weights sparsity is only supported for ZMajor Convolutions
        const auto inputType = input().getType().cast<vpux::NDTypeInterface>();
        const auto sparsityMode = inputType.getDimsOrder() == DimsOrder::NHWC ? VPU::SparsitySupport::SPARSE_WEIGHTS
                                                                              : VPU::SparsitySupport::NONE;
        return sparsityMode & excludeMode;
    }
    case VPU::ArchKind::VPUX37XX:
        return NCESparsity::FULLY_SUPPORTED_SPARSITY_MODE & excludeMode;
    default:
        VPUX_THROW("Unknown sparsity support mode for {0}", arch);
    }
}
