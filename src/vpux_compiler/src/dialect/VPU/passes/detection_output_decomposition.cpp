//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/attributes.hpp"
#include "vpux/compiler/dialect/VPU/ops.hpp"
#include "vpux/compiler/dialect/VPU/passes.hpp"
#include "vpux/compiler/utils/attributes.hpp"
#include "vpux/compiler/utils/error.hpp"
#include "vpux/compiler/utils/rewriter.hpp"

#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/PatternMatch.h>
#include <mlir/IR/Value.h>
#include <mlir/Support/LogicalResult.h>
#include <mlir/Transforms/DialectConversion.h>

#include <numeric>
#include <vpux/compiler/conversion.hpp>
#include <vpux/utils/core/error.hpp>

using namespace vpux;

namespace {

//
// Helper functions
//

auto getNumPriors(VPU::DetectionOutputOp origOp) {
    const auto priorBoxSize = origOp.attr().getNormalized().getValue() ? 4 : 5;
    const auto numPriors = getShape(origOp.in_proposals())[Dim(2)] / priorBoxSize;
    return numPriors;
}

mlir::Value createReshape(mlir::PatternRewriter& rewriter, mlir::Value tensor, ArrayRef<int64_t> newShape) {
    const auto newShapeAttr = getIntArrayAttr(rewriter.getContext(), newShape);
    auto reshape = rewriter.create<VPU::ReshapeOp>(tensor.getLoc(), tensor, nullptr, false, newShapeAttr);
    return reshape.output();
}

mlir::Value reshapeLogitsTo4D(mlir::PatternRewriter& rewriter, mlir::Value input, int numPriors) {
    const auto shape = getShape(input);
    VPUX_THROW_UNLESS(shape.size() == 2, "BoxLogits shape must be 2D");

    const auto batch = shape[Dim(0)];
    const auto width = shape[Dim(1)];
    const auto boxSize = 4;

    VPUX_THROW_UNLESS((width % (boxSize * numPriors)) == 0, "DetectionOutput BoxLogits tensor shape {0} is incorrect",
                      shape);

    const auto numLocClasses = width / (boxSize * numPriors);
    const auto newShape = SmallVector<int64_t>{batch, numPriors, numLocClasses, boxSize};
    return createReshape(rewriter, input, newShape);
}

mlir::Value reshapeClassPredictionsTo4D(mlir::PatternRewriter& rewriter, mlir::Value input, int numPriors) {
    const auto shape = getShape(input);
    VPUX_THROW_UNLESS(shape.size() == 2, "ClassPredictions shape must be 2D");

    const auto batch = shape[Dim(0)];
    const auto width = shape[Dim(1)];

    VPUX_THROW_UNLESS(width % numPriors == 0, "DetectionOutput ClassPredictions tensor shape {0} is incorrect", shape);

    const auto numClasses = width / numPriors;
    const auto newShape = SmallVector<int64_t>{batch, 1, numPriors, numClasses};
    return createReshape(rewriter, input, newShape);
}

mlir::Value reshapePriorBoxesTo4D(mlir::PatternRewriter& rewriter, mlir::Value input, int numPriors) {
    const auto shape = getShape(input);
    VPUX_THROW_UNLESS(shape.size() == 3, "PriorBoxes shape must be 3D");

    const auto batch = shape[Dim(0)];
    const auto height = shape[Dim(1)];
    const auto width = shape[Dim(2)];

    VPUX_THROW_UNLESS(width % numPriors == 0, "DetectionOutput PriorBoxes tensor shape {0} is incorrect", shape);
    const auto boxSize = width / numPriors;

    const auto newShape = SmallVector<int64_t>{batch, height, numPriors, boxSize};

    return createReshape(rewriter, input, newShape);
}

mlir::LogicalResult checkSupportedArguments(VPU::DetectionOutputOp origOp) {
    const auto attrs = origOp.attr();

    const auto normalized = origOp.attr().getNormalized().getValue();
    const auto varianceEncodedInTarget = origOp.attr().getVarianceEncodedInTarget().getValue();
    if (!normalized && !varianceEncodedInTarget) {
        return errorAt(origOp,
                       "DetectionOutput: undefined case - normalized == false && varianceEncodedItTarget == false");
    }

    const auto keepTopKArray = origOp.attr().getKeepTopK().getValue();
    if (keepTopKArray.size() != 1) {
        return errorAt(origOp, "DetectionOutput: keepTopK array size={0} is not supported", keepTopKArray.size());
    }

    const auto hasAdditionalInputs = (origOp->getNumOperands() > 3);
    if (hasAdditionalInputs) {
        return errorAt(origOp, "DetectionOutput: only 3 inputs is supported");
    }

    const auto codeType = attrs.getCodeType().getValue();
    if (codeType != IE::DetectionOutputCodeType::CENTER_SIZE) {
        return errorAt(origOp, "DetectionOutput: only CodeType::CENTER_SIZE is supported");
    }

    const auto mxNetNms = (attrs.getDecreaseLabelId().getValue() == true);
    if (mxNetNms) {
        return errorAt(origOp, "DetectionOutput: decreaseLabelId == true is not supported");
    }

    return mlir::success();
}

mlir::Value transposeClassPredictions(mlir::PatternRewriter& rewriter, mlir::Value tensor) {
    const auto ctx = rewriter.getContext();

    // keep default 4D order because this is the order that will be used throughout.
    // pretend that the data was never reordered in the first place.
    const auto defaultOrder = DimsOrder::NCHW.toAffineMap(ctx);    //              \/--swap--\/
    const auto permutationMap = DimsOrder::NCWH.toAffineMap(ctx);  // [Batch, 1, Priors, Classes]

    return rewriter.create<VPU::MemPermuteOp>(tensor.getLoc(), tensor, defaultOrder, permutationMap);
}

mlir::Value transposeBoxes(mlir::PatternRewriter& rewriter, mlir::Value boxes4D) {
    const auto ctx = rewriter.getContext();

    // keep default 4D order because this is the order that will be used throughout
    // pretend that the data was never reordered in the first place
    const auto defaultOrder = DimsOrder::NCHW.toAffineMap(ctx);    //           \/--swap--\/
    const auto permutationMap = DimsOrder::NHCW.toAffineMap(ctx);  // [Batch, Priors, Classes, 4 or 5]

    return rewriter.create<VPU::MemPermuteOp>(boxes4D.getLoc(), boxes4D, defaultOrder, permutationMap);
}

//
// DetectionOutputDecompositionPass
//

class DetectionOutputDecomposition final : public mlir::OpRewritePattern<VPU::DetectionOutputOp> {
public:
    DetectionOutputDecomposition(mlir::MLIRContext* ctx, Logger log)
            : mlir::OpRewritePattern<VPU::DetectionOutputOp>(ctx), _log(log) {
        setDebugName("DetectionOutputDecomposition");
    }

public:
    mlir::LogicalResult matchAndRewrite(VPU::DetectionOutputOp origOp, mlir::PatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult DetectionOutputDecomposition::matchAndRewrite(VPU::DetectionOutputOp origOp,
                                                                  mlir::PatternRewriter& rewriter) const {
    const auto loc = origOp->getLoc();
    const auto supported = checkSupportedArguments(origOp);
    if (!mlir::succeeded(supported)) {
        return supported;
    }

    const auto numPriors = getNumPriors(origOp);
    auto priors4D = reshapePriorBoxesTo4D(rewriter, origOp.in_proposals(), numPriors);

    const auto normalized = origOp.attr().getNormalized().getValue();
    if (!normalized) {
        const auto inputWidth = origOp.attr().getInputWidth().getInt();
        const auto inputHeight = origOp.attr().getInputHeight().getInt();
        priors4D =
                rewriter.create<VPU::DetectionOutputNormalizeOp>(origOp->getLoc(), priors4D, inputWidth, inputHeight);
    }

    const auto boxLogits = reshapeLogitsTo4D(rewriter, origOp.in_box_logits(), numPriors);
    const auto boxLogitsTransposed = transposeBoxes(rewriter, boxLogits);

    const auto codeType = origOp.attr().getCodeType().getValue();
    const auto clipBeforeNms = origOp.attr().getClipBeforeNms().getValue();
    auto decodeBoxes = rewriter.create<VPU::DetectionOutputDecodeBoxesOp>(
            appendLoc(loc, "decodeBoxes"), boxLogitsTransposed, priors4D, codeType, clipBeforeNms);

    const auto classPredictions = reshapeClassPredictionsTo4D(rewriter, origOp.in_class_preds(), numPriors);
    const auto transposedClassPredictions = transposeClassPredictions(rewriter, classPredictions);
    const auto confidenceThreshold = origOp.attr().getConfidenceThreshold();
    const auto topK = origOp.attr().getTopK();
    const auto backgroundId = origOp.attr().getBackgroundLabelId();
    auto sortTopK = rewriter.create<VPU::DetectionOutputSortTopKOp>(
            appendLoc(loc, "sortTopK"), transposedClassPredictions, confidenceThreshold, topK, backgroundId);

    auto selectBoxes = rewriter.create<VPU::DetectionOutputSelectBoxesOp>(
            appendLoc(loc, "selectBoxes"), decodeBoxes, sortTopK.out_indices(), sortTopK.out_sizes(), topK);

    const auto selectBoxes4DShape = getShape(selectBoxes).toValues();
    const auto selectBoxes3DShape = SmallVector<int64_t>{selectBoxes4DShape[Dim(0)], selectBoxes4DShape[Dim(1)],
                                                         selectBoxes4DShape[Dim(2)] * selectBoxes4DShape[Dim(3)]};
    const auto selectBoxes3D = createReshape(rewriter, selectBoxes, selectBoxes3DShape);

    const auto nmsThreshold = origOp.attr().getNmsThreshold();
    auto nmsCaffe =
            rewriter.create<VPU::DetectionOutputNmsCaffeOp>(appendLoc(loc, "nmsCaffe"), sortTopK.out_top_k_confidence(),
                                                            selectBoxes3D, sortTopK.out_sizes(), nmsThreshold);

    const auto keepTopKValue = origOp.attr().getKeepTopK()[0].cast<mlir::IntegerAttr>().getInt();
    const auto keepTopK = getIntAttr(rewriter.getContext(), keepTopKValue);
    const auto clipAfterNms = origOp.attr().getClipAfterNms();
    rewriter.replaceOpWithNewOp<VPU::DetectionOutputCollectResultsOp>(
            origOp, nmsCaffe.out_confidence(), nmsCaffe.out_boxes(), nmsCaffe.out_sizes(), keepTopK, clipAfterNms);

    return mlir::success();
}

class DetectionOutputDecompositionPass final :
        public VPU::DetectionOutputDecompositionBase<DetectionOutputDecompositionPass> {
public:
    explicit DetectionOutputDecompositionPass(Logger log) {
        Base::initLogger(log, Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;
};

void DetectionOutputDecompositionPass::safeRunOnFunc() {
    auto& ctx = getContext();

    mlir::ConversionTarget target(ctx);
    target.addIllegalOp<VPU::DetectionOutputOp>();
    target.addLegalOp<VPU::DetectionOutputNormalizeOp>();
    target.addLegalOp<VPU::DetectionOutputDecodeBoxesOp>();
    target.addLegalOp<VPU::DetectionOutputSortTopKOp>();
    target.addLegalOp<VPU::DetectionOutputSelectBoxesOp>();
    target.addLegalOp<VPU::DetectionOutputNmsCaffeOp>();
    target.addLegalOp<VPU::DetectionOutputCollectResultsOp>();

    target.addLegalOp<VPU::ReshapeOp>();
    target.addLegalOp<VPU::MemPermuteOp>();

    mlir::RewritePatternSet patterns(&ctx);
    patterns.add<DetectionOutputDecomposition>(&ctx, _log);

    if (mlir::failed(mlir::applyPartialConversion(getOperation(), target, std::move(patterns)))) {
        signalPassFailure();
    }
}

}  // namespace

//
// createDetectionOutputDecompositionPass
//

std::unique_ptr<mlir::Pass> vpux::VPU::createDetectionOutputDecompositionPass(Logger log) {
    return std::make_unique<DetectionOutputDecompositionPass>(log);
}
