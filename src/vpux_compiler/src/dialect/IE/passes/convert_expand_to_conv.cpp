//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/ops.hpp"
#include "vpux/compiler/dialect/IE/passes.hpp"
#include "vpux/compiler/dialect/VPU/utils/const_utils.hpp"
#include "vpux/compiler/utils/rewriter.hpp"

#include <mlir/IR/PatternMatch.h>

using namespace vpux;

namespace {

int64_t greatestCommonDivisor(int64_t term1, int64_t term2) {
    while (term1 != term2) {
        if (term1 > term2) {
            term1 = term1 - term2;
        } else {
            term2 = term2 - term1;
        }
    }
    return term1;
}

int64_t leastCommonMultiple(int64_t term1, int64_t term2) {
    VPUX_THROW_WHEN(term1 <= 0 || term2 <= 0,
                    "This implementation of LCM expects two positive integers, got {0} and {1}", term1, term2);
    return term1 * term2 / greatestCommonDivisor(term1, term2);
}

int64_t calculateAlignment(const vpux::NDTypeInterface expandInType) {
    const auto channelAlignment = VPU::NCEInvariant::getAlignment(expandInType.getElementType());
    const auto expandInChannels = expandInType.getShape()[Dims4D::Act::C];
    const auto leastChannelMultiple = leastCommonMultiple(channelAlignment, expandInChannels);
    return leastChannelMultiple / expandInChannels;
}

Shape calculateWeightsShape(ShapeRef expandInShape, ShapeRef expandOutShape, const int64_t alignment) {
    const int64_t kernelOutputChannels = expandOutShape[Dims4D::Act::C] * alignment;
    const int64_t kernelInputChannels = expandInShape[Dims4D::Act::C] * alignment;
    const int64_t kernelY = 1;
    const int64_t kernelX = 1;
    return Shape{kernelOutputChannels, kernelInputChannels, kernelY, kernelX};
}

bool isEligibleForConversion(IE::ExpandOp expandOp, Logger log, StringRef debugName) {
    const auto expandInType = expandOp.input().getType().cast<vpux::NDTypeInterface>();
    const auto expandOutType = expandOp.output().getType().cast<vpux::NDTypeInterface>();
    const auto supportedLayout = DimsOrder::NHWC;
    const auto expandInLayout = expandInType.getDimsOrder();
    if (expandInLayout != supportedLayout) {
        log.trace("[{0}]: Expand at {1} has {2} input layout, expected {3}", debugName, expandOp.getLoc(),
                  expandInLayout, supportedLayout);
        return false;
    }
    const auto expandOutLayout = expandOutType.getDimsOrder();
    if (expandOutLayout != supportedLayout) {
        log.trace("[{0}]: Expand at {1} has {2} output layout, expected {3}", debugName, expandOp.getLoc(),
                  expandOutLayout, supportedLayout);
        return false;
    }
    const auto expandPadsBegin = parseIntArrayAttr<int64_t>(expandOp.pads_beginAttr());
    if (expandPadsBegin.size() != 4) {
        log.trace("[{0}]: Expand at {1} has {2}-d start padding. Only 4-d shapes are supported", debugName,
                  expandOp.getLoc(), expandPadsBegin.size());
        return false;
    }
    const auto isConflictingPadBegin = [](const int64_t pad) -> bool {
        return pad != 0;
    };
    if (std::any_of(expandPadsBegin.begin(), expandPadsBegin.end(), isConflictingPadBegin)) {
        log.trace("[{0}]: Expand at {1} has {2} start padding. Expected to have [0, 0, 0, 0]", debugName,
                  expandOp.getLoc(), expandPadsBegin);
        return false;
    }
    const auto expandPadsEnd = parseIntArrayAttr<int64_t>(expandOp.pads_endAttr());
    if (expandPadsEnd.size() != 4) {
        log.trace("[{0}]: Expand at {1} has {2}-d end padding. Only 4-d shapes are supported", debugName,
                  expandOp.getLoc(), expandPadsEnd.size());
        return false;
    }
    if (expandPadsEnd[Dims4D::Act::N.ind()] != 0 || expandPadsEnd[Dims4D::Act::C.ind()] <= 0 ||
        expandPadsEnd[Dims4D::Act::H.ind()] != 0 || expandPadsEnd[Dims4D::Act::W.ind()] != 0) {
        log.trace("[{0}]: Expand at {1} has {2} end padding. Expected to have [0, C, 0, 0]", debugName,
                  expandOp.getLoc(), expandPadsEnd);
        return false;
    }
    const auto expandInShape = expandInType.getShape();
    if (expandInShape.size() != 4) {
        log.trace("[{0}]: Expand at {1} has {2}-d shape. Only 4-d shapes are supported", debugName, expandOp.getLoc(),
                  expandInShape.size());
        return false;
    }
    if (expandInShape[Dims4D::Act::N] != 1) {
        log.trace("[{0}]: Expand at {1} has batch {2}. Expected to have 1", debugName, expandOp.getLoc(),
                  expandInShape[Dims4D::Act::N]);
        return false;
    }
    const auto convolutionAlignment = calculateAlignment(expandInType);
    if (expandInShape[Dims4D::Act::W] % convolutionAlignment != 0) {
        log.trace("[{0}]: Expand at {1} has width {2}. Width is expected to be a multiple of {3}", debugName,
                  expandOp.getLoc(), expandInShape[Dims4D::Act::W], convolutionAlignment);
        return false;
    }
    if (!expandInType.getElementType().isF16() && !expandInType.getElementType().isa<mlir::quant::QuantizedType>()) {
        log.trace("[{0}]: Expand at {1} has {2} element type. Only float16 and quantized types are supported",
                  debugName, expandOp.getLoc(), expandInType.getElementType());
        return false;
    }

    return true;
}

IE::AffineReshapeOp buildReshape(mlir::Location loc, mlir::Value input, ArrayRef<int64_t> targetShape,
                                 mlir::PatternRewriter& rewriter) {
    const auto ctx = rewriter.getContext();
    const auto srcType = input.getType().cast<vpux::NDTypeInterface>();
    const auto dstType = srcType.changeShape(ShapeRef(targetShape));
    SmallVector<SmallVector<int64_t>> reassociationMap(targetShape.size());
    for (const auto& dimIdx : irange(reassociationMap.size())) {
        reassociationMap[dimIdx].push_back(dimIdx);
    }
    const auto reassociationMapAttr = getIntArrayOfArray(ctx, reassociationMap);
    const auto targetShapeAttr = getIntArrayAttr(ctx, targetShape);
    auto reshapeOp = rewriter.create<IE::AffineReshapeOp>(loc, dstType, input, reassociationMapAttr, targetShapeAttr);

    return reshapeOp;
}

IE::AffineReshapeOp reshapeInput(mlir::Location loc, mlir::Value input, const int64_t channelAlignment,
                                 mlir::PatternRewriter& rewriter) {
    const auto origShape = getShape(input);
    const auto batch = origShape[Dims4D::Act::N];
    const auto channels = origShape[Dims4D::Act::C] * channelAlignment;
    const auto height = origShape[Dims4D::Act::H];
    const auto width = origShape[Dims4D::Act::W] / channelAlignment;

    const SmallVector<int64_t> targetShape = {batch, channels, height, width};
    const auto reshapedLoc = appendLoc(loc, "reshape input for DPU expand");
    return buildReshape(reshapedLoc, input, makeArrayRef(targetShape), rewriter);
}

IE::AffineReshapeOp reshapeOutput(IE::ExpandOp origOp, mlir::Value convOutput, mlir::PatternRewriter& rewriter) {
    const Shape origShape = getShape(origOp.output()).toValues();
    const SmallVector<int64_t> targetShape = origShape.raw();

    const auto reshapedLoc = appendLoc(origOp.getLoc(), "reshape output for DPU expand");
    return buildReshape(reshapedLoc, convOutput, makeArrayRef(targetShape), rewriter);
}

// The idea is to create the following structure:
// Filter #0
// 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// Filter #1
// 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// Filter #2
// 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// Filter #3
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// ...
// Filter #15
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// Filter #16
// 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// Filter #17
// 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// Filter #18
// 0 0 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// Filter #19
// 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0
// ...
std::vector<ngraph::float16> populateWeights(ShapeRef weightShape, const int64_t numInputChannels,
                                             const int64_t numOutputChannels, const int64_t convolutionAlignment) {
    const int64_t kernelInputChannels = weightShape[Dims4D::Filter::IC];
    const int64_t kernelY = weightShape[Dims4D::Filter::KY];
    const int64_t kernelX = weightShape[Dims4D::Filter::KX];

    const auto strideIC = kernelY * kernelX;
    const auto strideOC = strideIC * kernelInputChannels;
    std::vector<ngraph::float16> weightValues(weightShape.totalSize(), checked_cast<ngraph::float16>(0.f));
    // Resulting weights can be split into the number of blocks defined by convolutionAlignment.
    // convolutionAlignment is usually 16. E.g. for 64x48x1x1 weights there are 4 blocks with 16x48x1x1 geometry.
    // Block offset iterates over these chunks. Each block must contain a diagonal matrix padded with zeros.
    // outChan iterates inside each chunk. The idea is to fill main diagonals with ones.
    // However, in order to maintain continuation, diagonal must be shifted by prefixSize.
    // We want to do this:
    // 1 0 0 0 0 0 0 0
    // 0 1 0 0 0 0 0 0
    // 0 0 1 0 0 0 0 0
    // 0 0 0 0 0 0 0 0
    // 0 0 0 1 0 0 0 0
    // Not this:
    // 1 0 0 0 0 0 0 0
    // 0 1 0 0 0 0 0 0
    // 0 0 1 0 0 0 0 0
    // 0 0 0 0 0 0 0 0
    // 1 0 0 0 0 0 0 0
    // Note that outChan iterates only over origInShape[Dims4D::Act::C] because we want to skip padded rows.
    // For the simple example counters go like that:
    //
    //           prefixSize (3)
    //                   |
    //             1 0 0 0 0 0 0 0
    //             0 1 0 0 0 0 0 0
    //             0 0 1 0 0 0 0 0
    //             0 0 0 0 0 0 0 0
    // blockRow -> 0 0 0 1 0 0 0 0
    //             ^     ^
    //       outChan     inChan = prefixSize + outChan (0 + 3)
    for (int64_t blockOffset = 0; blockOffset < convolutionAlignment; blockOffset++) {
        const auto blockRow = blockOffset * numOutputChannels;
        const auto prefixSize = blockOffset * numInputChannels;
        for (int64_t outChan = 0; outChan < numInputChannels; outChan++) {
            const auto inChan = prefixSize + outChan;
            const auto pos = (blockRow + outChan) * strideOC + inChan * strideIC;
            weightValues.at(pos) = 1.f;
        }
    }

    return weightValues;
}

mlir::Type composeExpressedType(const mlir::Type convolutionInputType) {
    // Compose quantized weight type for convolution with quantized input.
    // It must have scale 1 and shift 0 and range 0-255.
    // Let's keep it obvious: quantized 0 means 0, quantized 1 means 1.
    // For float cases just use the provided element type.
    if (convolutionInputType.isa<mlir::quant::QuantizedType>()) {
        const auto ctx = convolutionInputType.getContext();
        const auto quantType = mlir::quant::UniformQuantizedType::get(
                /*flags=*/0, /*storageType=*/getUInt8Type(ctx), /*expressedType=*/mlir::Float16Type::get(ctx),
                /*scale=*/1.0, /*zeroPoint=*/0, /*storageTypeMin=*/0, /*storageTypeMax=*/255);
        return quantType;
    }
    return convolutionInputType;
}

Const::ContentAttr applyWeightTransformations(const Const::ContentAttr weightContentAttr,
                                              const mlir::Type weightExpressedElemType) {
    // For quantized types convert weights to storage type. Usually that's UInt8. Then apply quantCast.
    // For float case just convert weights to expressed element type.
    if (auto quantType = weightExpressedElemType.dyn_cast<mlir::quant::QuantizedType>()) {
        const auto contentStorageAttr = weightContentAttr.convertElemType(quantType.getStorageType());
        return contentStorageAttr.quantCast(quantType);
    }
    return weightContentAttr.convertElemType(weightExpressedElemType);
}

mlir::Value composeWeights(IE::ExpandOp origOp, const mlir::Type convolutionInputType,
                           const int64_t convolutionAlignment, mlir::PatternRewriter& rewriter) {
    const auto origInShape = getShape(origOp.input());
    const auto origOutShape = getShape(origOp.output());
    const auto weightShape = calculateWeightsShape(origInShape, origOutShape, convolutionAlignment);
    const auto weightValues = populateWeights(weightShape, origInShape[Dims4D::Act::C], origOutShape[Dims4D::Act::C],
                                              convolutionAlignment);

    const auto ctx = rewriter.getContext();
    const auto weightStorageType = mlir::RankedTensorType::get(weightShape.raw(), mlir::Float16Type::get(ctx));
    const auto weightStorageAttr = mlir::DenseElementsAttr::get(weightStorageType, makeArrayRef(weightValues));
    const auto weightContentAttr = Const::ContentAttr::get(weightStorageAttr);
    const auto declLoc = appendLoc(origOp.getLoc(), "weights for DPU expand");

    const auto weightExpressedElemType = composeExpressedType(convolutionInputType);
    const auto weightExpressedType = mlir::RankedTensorType::get(weightShape.raw(), weightExpressedElemType);
    const auto targetContentAttr = applyWeightTransformations(weightContentAttr, weightExpressedElemType);
    auto declOp = rewriter.create<Const::DeclareOp>(declLoc, weightExpressedType, targetContentAttr);

    const auto reorderLoc = appendLoc(origOp.getLoc(), "reorder weights for DPU expand");
    const auto weightTypeNCHW = declOp.getOutput().getType().cast<vpux::NDTypeInterface>();
    const auto reorderType = weightTypeNCHW.changeDimsOrder(DimsOrder::NHWC);
    const auto orderMap = DimsOrder::NHWC.toAffineMap(ctx);
    auto reorderOut = rewriter.createOrFold<IE::ReorderOp>(reorderLoc, reorderType, declOp.getOutput(), orderMap);

    return reorderOut;
}

IE::ConvolutionOp buildConvolution(IE::ExpandOp expandOp, mlir::Value activation, mlir::Value weights,
                                   mlir::Type convOutElemType, mlir::PatternRewriter& rewriter) {
    const auto ctx = rewriter.getContext();
    const auto strides = getIntArrayAttr(ctx, SmallVector<int64_t>{1, 1});
    const auto kernelPadsBegin = getIntArrayAttr(ctx, SmallVector<int64_t>{0, 0});
    const auto kernelPadsEnd = getIntArrayAttr(ctx, SmallVector<int64_t>{0, 0});
    const auto dilations = getIntArrayAttr(ctx, SmallVector<int64_t>{1, 1});

    // IE::ConvolutionOp output type inference sets NCHW output order.
    // Specify convolution output type explicitly.
    const auto origOutType = expandOp.output().getType().cast<vpux::NDTypeInterface>();
    const auto weightsShape = getShape(weights);
    const auto outChannels = weightsShape[Dims4D::Filter::OC];
    const Shape convInShape = getShape(activation).toValues();
    const Shape convOutShape = {convInShape[Dims4D::Act::N], outChannels, convInShape[Dims4D::Act::H],
                                convInShape[Dims4D::Act::W]};
    const auto convOutType = origOutType.changeShape(convOutShape).changeElemType(convOutElemType);
    return rewriter.create<IE::ConvolutionOp>(expandOp.getLoc(), convOutType, activation, weights,
                                              /*bias=*/nullptr, strides, kernelPadsBegin, kernelPadsEnd, dilations,
                                              /*postOp=*/nullptr);
}

bool isDerivedFromQuantize(mlir::Operation* op) {
    if (op->getNumOperands() != 2) {
        return false;
    }
    if (op->getOperand(0) != op->getOperand(1)) {
        return false;
    }
    const auto quantInputType = op->getOperand(0).getType().cast<vpux::NDTypeInterface>().getElementType();
    const auto quantOutputType = op->getResult(0).getType().cast<vpux::NDTypeInterface>().getElementType();
    return quantInputType.isF16() && quantOutputType.isa<mlir::quant::QuantizedType>();
}

// IE.Add -> IE.QuantizeCast -> IE.Expand is a rare case.
// The absence of IE.ShapeCast usually means that no further IE.Expand is required.
// For example IE.Add (1x16xHxW) -> IE.QuantizeCast -> IE.Convolution
// Such convolution does not need 1x16xHxW tensor expansion.
mlir::Value tryExpandWithAdd(mlir::Value in) {
    auto quantize = in.getDefiningOp<IE::AddOp>();
    if (quantize == nullptr) {
        return nullptr;
    }
    if (!isDerivedFromQuantize(quantize)) {
        return nullptr;
    }
    return quantize.input1();
}

mlir::Value tryExpandWithShapeCast(mlir::Value in) {
    auto outputShapeCast = in.getDefiningOp<IE::ShapeCastOp>();
    if (outputShapeCast == nullptr || outputShapeCast.source().isa<mlir::BlockArgument>()) {
        return nullptr;
    }
    auto quantize = outputShapeCast.source().getDefiningOp<IE::AddOp>();
    if (quantize == nullptr || quantize.input1().isa<mlir::BlockArgument>()) {
        return nullptr;
    }
    if (!isDerivedFromQuantize(quantize)) {
        return nullptr;
    }
    auto inputShapeCast = quantize.input1().getDefiningOp<IE::ShapeCastOp>();
    if (inputShapeCast == nullptr) {
        return nullptr;
    }
    return inputShapeCast.source();
}

// Search for any of these patterns:
// 1. IE.Add -> IE.QuantizeCast -> IE.Expand
// 2. IE.ShapeCast -> IE.Add -> IE.ShapeCast -> IE.QuantizeCast -> IE.Expand
// These chains of operations are usually derived from IE.Quantize.
// We want to replace this entire subgraph with a single DPU expand operation with appropriate target element type.
// Note that IE.And match is skipped on purpose.
// IE.And implies that the target architecture does not fully support mixed precision convolutions.
mlir::Value getExpandInput(mlir::Value origInput) {
    if (origInput.isa<mlir::BlockArgument>()) {
        return nullptr;
    }
    auto quantizeCast = origInput.getDefiningOp<IE::QuantizeCastOp>();
    if (quantizeCast == nullptr || quantizeCast.input().isa<mlir::BlockArgument>()) {
        return nullptr;
    }
    if (const auto input = tryExpandWithAdd(quantizeCast.input())) {
        return input;
    }
    if (const auto input = tryExpandWithShapeCast(quantizeCast.input())) {
        return input;
    }
    return nullptr;
}

// IE.Expand may have IE.Add -> IE.ShapeCast -> IE.QuantizeCast -> IE.Expand input chain.
// In that case it may be incorrect to fetch the element type type from IE.Expand output.
// For example: Input (zp = 64) -> IE.QuantizeCast (zp = 128) -> IE.QuantizeCast (zp = 0) -> IE.Expand (zp = 0)
// Canonicalizer folds such subgraphs into Input (zp = 64) -> IE.QuantizeCast (zp = 0) -> IE.Expand (zp = 0)
// Thus, when zp for convolution output element type is derived from IE.Expand, results may degrade in accuracy.
// In order to avoid the accuracy degradation, fetch the quantization parameters from IE.QuantizeCast input.
// Also, the scale must be divided by 2 because it comes from IE.Add that adds its input with itself.
mlir::Type getConvolutionOutputType(IE::ExpandOp expandOp) {
    const auto origOutType = expandOp.output().getType().cast<vpux::NDTypeInterface>();
    if (getExpandInput(expandOp.input()) == nullptr) {
        return origOutType.getElementType();
    }
    auto origInputQuantCast = expandOp.input().getDefiningOp<IE::QuantizeCastOp>();
    if (origInputQuantCast == nullptr) {
        return origOutType.getElementType();
    }

    const auto quantCastInType = origInputQuantCast.input()
                                         .getType()
                                         .cast<vpux::NDTypeInterface>()
                                         .getElementType()
                                         .dyn_cast<mlir::quant::UniformQuantizedType>();
    if (quantCastInType == nullptr) {
        return origOutType.getElementType();
    }
    const auto quantType = mlir::quant::UniformQuantizedType::get(
            quantCastInType.getFlags(), quantCastInType.getStorageType(), quantCastInType.getExpressedType(),
            quantCastInType.getScale() / 2.0, quantCastInType.getZeroPoint(), quantCastInType.getStorageTypeMin(),
            quantCastInType.getStorageTypeMax());
    return quantType;
}

// This method is required to cast the output of quantized convolution back to IE.Expand output element type.
// It usually has scale * 2 and may have different zero point, depending on how many QuantizeCast operations
// have been fused together.
mlir::Value quantCastOutput(IE::ExpandOp expandOp, mlir::Value reshapeOutput, mlir::PatternRewriter& rewriter) {
    auto origInputQuantCast = expandOp.input().getDefiningOp<IE::QuantizeCastOp>();
    if (origInputQuantCast == nullptr) {
        return reshapeOutput;
    }

    const auto quantCastLoc = appendLoc(expandOp.getLoc(), "quantize cast for DPU expand");
    auto quantCast = rewriter.create<IE::QuantizeCastOp>(
            quantCastLoc, reshapeOutput,
            origInputQuantCast.output().getType().cast<vpux::NDTypeInterface>().getElementType());
    return quantCast.output();
}

class QuantizedExpandRewriter final : public mlir::OpRewritePattern<IE::ExpandOp> {
public:
    QuantizedExpandRewriter(mlir::MLIRContext* ctx, Logger log)
            : mlir::OpRewritePattern<IE::ExpandOp>(ctx, benefitHigh), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::ExpandOp origOp, mlir::PatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult QuantizedExpandRewriter::matchAndRewrite(IE::ExpandOp origOp,
                                                             mlir::PatternRewriter& rewriter) const {
    _log.trace("[{0}] Got IE.ExpandOp at '{1}'", getDebugName(), origOp->getLoc());
    if (!isEligibleForConversion(origOp, _log, getDebugName())) {
        return matchFailed(rewriter, origOp, "[{0}] Cannot convert IE.ExpandOp at '{1}'", getDebugName(),
                           origOp->getLoc());
    }
    const auto expandInput = getExpandInput(origOp.input());
    if (expandInput == nullptr) {
        return matchFailed(rewriter, origOp, "[{0}] IE.ExpandOp at '{1}' does not have IE.Add producer", getDebugName(),
                           origOp->getLoc());
    }
    const auto expandInType = expandInput.getType().cast<vpux::NDTypeInterface>();
    const auto convolutionAlignment = calculateAlignment(expandInType);
    auto reshapeIn = reshapeInput(origOp.getLoc(), expandInput, convolutionAlignment, rewriter);
    auto weights = composeWeights(origOp, expandInType.getElementType(), convolutionAlignment, rewriter);
    auto convOutElemType = getConvolutionOutputType(origOp);
    auto convOp = buildConvolution(origOp, reshapeIn.output(), weights, convOutElemType, rewriter);
    auto reshapeOut = reshapeOutput(origOp, convOp.output(), rewriter);
    const auto quantCastOut = quantCastOutput(origOp, reshapeOut, rewriter);
    rewriter.replaceOp(origOp, quantCastOut);
    return mlir::success();
}

class DPUExpandRewriter final : public mlir::OpRewritePattern<IE::ExpandOp> {
public:
    DPUExpandRewriter(mlir::MLIRContext* ctx, Logger log)
            : mlir::OpRewritePattern<IE::ExpandOp>(ctx, benefitLow), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::ExpandOp origOp, mlir::PatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult DPUExpandRewriter::matchAndRewrite(IE::ExpandOp origOp, mlir::PatternRewriter& rewriter) const {
    _log.trace("[{0}] Got IE.ExpandOp at '{1}'", getDebugName(), origOp->getLoc());
    if (!isEligibleForConversion(origOp, _log, getDebugName())) {
        return matchFailed(rewriter, origOp, "[{0}] Cannot convert IE.ExpandOp at '{1}'", getDebugName(),
                           origOp->getLoc());
    }
    const auto expandInput = origOp.input();
    const auto expandInType = expandInput.getType().cast<vpux::NDTypeInterface>();
    const auto convolutionAlignment = calculateAlignment(expandInType);
    auto reshapeIn = reshapeInput(origOp.getLoc(), expandInput, convolutionAlignment, rewriter);
    auto weights = composeWeights(origOp, expandInType.getElementType(), convolutionAlignment, rewriter);
    const auto expandOutType = origOp.output().getType().cast<vpux::NDTypeInterface>();
    const auto convOutElemType = expandOutType.getElementType();
    auto convOp = buildConvolution(origOp, reshapeIn.output(), weights, convOutElemType, rewriter);
    auto reshapeOut = reshapeOutput(origOp, convOp.output(), rewriter);
    rewriter.replaceOp(origOp, reshapeOut.output());
    return mlir::success();
}

//
// ConvertExpandToConvPass
//

class ConvertExpandToConvPass final : public IE::ConvertExpandToConvPassBase<ConvertExpandToConvPass> {
public:
    explicit ConvertExpandToConvPass(Logger log): _log(log) {
        _log.setName(Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;

private:
    Logger _log;
};

void ConvertExpandToConvPass::safeRunOnFunc() {
    auto& ctx = getContext();
    auto func = getOperation();

    mlir::RewritePatternSet patterns(&ctx);
    patterns.add<QuantizedExpandRewriter>(&ctx, _log);
    patterns.add<DPUExpandRewriter>(&ctx, _log);

    if (mlir::failed(mlir::applyPatternsAndFoldGreedily(func, std::move(patterns), getDefaultGreedyRewriteConfig()))) {
        signalPassFailure();
    }
}

}  // namespace

//
// createConvertExpandToConvPass
//
std::unique_ptr<mlir::Pass> vpux::IE::createConvertExpandToConvPass(Logger log) {
    return std::make_unique<ConvertExpandToConvPass>(log);
}
