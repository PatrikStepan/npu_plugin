//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/passes.hpp"
#include "vpux/compiler/dialect/VPU/utils/generate_tiling.hpp"
#include "vpux/compiler/utils/permute_utils.hpp"
#include "vpux/compiler/utils/rewriter.hpp"

using namespace vpux;

namespace {

mlir::Operation* getInputPermuteLikeOp(mlir::Value addInput) {
    auto parentOp = addInput.getDefiningOp();
    while (parentOp) {
        if (mlir::isa<IE::MemPermuteOp, IE::PermuteQuantizeOp>(parentOp)) {
            return parentOp;
        } else if (auto parentShapeCast = mlir::dyn_cast<IE::ShapeCastOp>(parentOp)) {
            if (VPU::hasMultiBranches(parentShapeCast.getOperation())) {
                return nullptr;
            }
            parentOp = parentShapeCast.source().getDefiningOp();
            continue;
        } else {
            return nullptr;
        }
    }
    return nullptr;
}

IE::AddOp getAddOp(mlir::Value permuteInput) {
    auto parentOp = permuteInput.getDefiningOp();
    while (parentOp) {
        if (auto parentAdd = mlir::dyn_cast<IE::AddOp>(parentOp)) {
            return parentAdd;
        } else if (auto parentQuantizeCast = mlir::dyn_cast<IE::QuantizeCastOp>(parentOp)) {
            if (VPU::hasMultiBranches(parentQuantizeCast.getOperation())) {
                return nullptr;
            }
            parentOp = parentQuantizeCast.input().getDefiningOp();
            continue;
        } else if (auto parentShapeCast = mlir::dyn_cast<IE::ShapeCastOp>(parentOp)) {
            if (VPU::hasMultiBranches(parentShapeCast.getOperation())) {
                return nullptr;
            }
            parentOp = parentShapeCast.source().getDefiningOp();
            continue;
        } else {
            return nullptr;
        }
    }
    return nullptr;
}

// Search for pattern
// IE.MemPermute / PermuteQuantize -> [IE.ShapeCast]|
//                                                  | -> IE.Add -> [IE.ShapeCast] -> [IE.QuantizeCast] -> IE.MemPermute
// IE.MemPermute / PermuteQuantize -> [IE.ShapeCast]|
bool canBeFolded(IE::PermuteQuantizeOp permuteQuantizeOp, IE::MemPermuteOp memPermuteOp) {
    const auto permuteQuantizeOutElemType =
            permuteQuantizeOp.output().getType().cast<vpux::NDTypeInterface>().getElementType();
    // Can fuse MemPermute with PermuteQuantization in case only permutation (no quantization) is performed by this
    // PermuteQuantization Op.
    if (permuteQuantizeOutElemType.isa<mlir::quant::QuantizedType>()) {
        return false;
    }

    auto prevMemPerm = permuteQuantizeOp.mem_perm();
    auto memPerm = memPermuteOp.mem_perm();
    auto newMemPerm = memPerm.compose(prevMemPerm);

    const auto permuteQuantizeOpInType = permuteQuantizeOp.input().getType();
    const auto memPermuteOpOutType = memPermuteOp.output().getType();
    auto permuteQuantizeOpInElemType = permuteQuantizeOpInType.cast<NDTypeInterface>().getElementType();
    // For the case that permutations can be folded, PermuteQuantizeOpInType and memPermuteOpOutType are expected to be
    // the same, except elemType.
    if (permuteQuantizeOpInType !=
                memPermuteOpOutType.cast<NDTypeInterface>().changeElemType(permuteQuantizeOpInElemType) ||
        !newMemPerm.isIdentity()) {
        return false;
    }

    return true;
}

bool canBeFusedIntoPermuteCast(IE::PermuteQuantizeOp permuteQuantizeOp, IE::MemPermuteOp memPermuteOp) {
    const auto inOrder = DimsOrder::fromValue(permuteQuantizeOp.input());
    const auto inShape = getShape(permuteQuantizeOp.input());
    const auto inMemShape = inOrder.toMemoryOrder(inShape);

    auto prevMemPerm = permuteQuantizeOp.mem_perm();
    auto memPerm = memPermuteOp.mem_perm();
    auto composedMemPerm = memPerm.compose(prevMemPerm);

    return isTrivialPermute(inMemShape, composedMemPerm);
}

bool isSupportedMemPermute(IE::MemPermuteOp memPermuteOp, IE::AddOp addOp, Logger log) {
    if (!addOp.output().hasOneUse()) {
        log.trace("Add has more than one consumer");
        return false;
    }

    const SmallVector<mlir::Value> branches = addOp->getOperands();
    for (const auto& addInput : branches) {
        const auto inPermutationOp = getInputPermuteLikeOp(addInput);
        if (inPermutationOp != nullptr) {
            // Further checking for inPermuteQuantizeOp - propagate if PermuteQuantize and MemPermute can be folded.
            auto inPermuteQuantizeOp = mlir::dyn_cast<IE::PermuteQuantizeOp>(inPermutationOp);
            if (inPermuteQuantizeOp != nullptr && !canBeFolded(inPermuteQuantizeOp, memPermuteOp) &&
                !canBeFusedIntoPermuteCast(inPermuteQuantizeOp, memPermuteOp)) {
                log.trace("IE::PermuteQuantize op: {0} and IE::MemPermute op: {1} can not be folded or fused into "
                          "permuteCast",
                          inPermuteQuantizeOp.getLoc(), memPermuteOp.getLoc());
                return false;
            }
        }
    }

    if ((getInputPermuteLikeOp(branches[0]) != nullptr) || (getInputPermuteLikeOp(branches[1]) != nullptr)) {
        // As long as one of the two inputs has PermuteLikeOp, the MemPermute should be propagated.
        // If one of the branches keeps a MemPermute, such MemPermute may be optimized in later passes.
        log.trace("IE::MemPermute op: {0} can be converted", memPermuteOp.getLoc());
        return true;
    }

    return false;
}

mlir::Value processNonPermuteBranch(mlir::PatternRewriter& rewriter, IE::MemPermuteOp memPermuteOp, mlir::Value input,
                                    int64_t idx) {
    // For the branch without PermuteLike op like
    //       IE.Tile -> [IE.ShapeCast]|
    //                                | -> IE.Add -> [IE.ShapeCast] -> [IE.QuantizeCast] ->IE.MemPermute
    // IE.MemPermute -> [IE.ShapeCast]|
    // will be converted into:
    //            IE.Tile -> IE.MemPermute -> [IE.ShapeCast]|
    //                                                      | -> IE.Add -> [IE.ShapeCast] -> [IE.QuantizeCast]
    // IE.PermuteQuantize -> IE.MemPermute -> [IE.ShapeCast]|
    // For the branch IE.Tile -> IE.MemPermute -> [IE.ShapeCast], the MemPermute will be propagated to the front of the
    // tile op in the later pass, like IE.MemPermute -> IE.Tile -> [IE.ShapeCast], this MemPermute may be a trivial
    // permute or permute on a smaller tensor.
    const auto addInOrder = DimsOrder::fromValue(input);
    const auto orderInAttr = mlir::AffineMapAttr::get(addInOrder.toAffineMap(memPermuteOp.getContext()));
    auto shapeCastOp = input.getDefiningOp<IE::ShapeCastOp>();

    auto memPermuteInput = (shapeCastOp == nullptr) ? input : shapeCastOp.source();
    const auto newMemPermuteLoc = appendLoc(memPermuteOp.getLoc(), "_mem_permute_{0}", idx);
    auto newMemPermuteOp = rewriter.create<IE::MemPermuteOp>(newMemPermuteLoc, memPermuteInput,
                                                             memPermuteOp.dst_order(), memPermuteOp.mem_perm());
    const auto newLayoutCastLoc = appendLoc(memPermuteOp.getLoc(), "_in_layout_cast_{0}", idx);
    auto newLayoutCastOp = rewriter.create<IE::LayoutCastOp>(newLayoutCastLoc, newMemPermuteOp.output(), orderInAttr);
    if (shapeCastOp != nullptr) {
        const auto newShapeCastLoc = appendLoc(memPermuteOp.getLoc(), "_in_shape_cast_{0}", idx);
        auto newShapeCastOp =
                rewriter.create<IE::ShapeCastOp>(newShapeCastLoc, newLayoutCastOp.output(), shapeCastOp.shapeAttr());
        return newShapeCastOp.result();
    }
    return newLayoutCastOp.output();
}

//
// OptimizeEltwise
//

class OptimizeEltwise final : public mlir::OpRewritePattern<IE::MemPermuteOp> {
public:
    OptimizeEltwise(mlir::MLIRContext* ctx, Logger log): mlir::OpRewritePattern<IE::MemPermuteOp>(ctx), _log(log) {
        this->setDebugName("OptimizeEltwise");
    }

private:
    mlir::LogicalResult matchAndRewrite(IE::MemPermuteOp memPermuteOp, mlir::PatternRewriter& rewriter) const final;

private:
    Logger _log;
};

// Propagate last permute in the chain IE.MemPermute -> IE.ShapeCast -> IE.Add -> IE.ShapeCast -> IE.MemPermute
// This subgraph becomes IE.MemPermute -> IE.MemPermute -> IE.ShapeCast -> IE.Add -> IE.ShapeCast
// Two consecutive IE.MemPermute operations will be folded into one.
// VPU.NCE.Eltwise is layout agnostic, however, DPU operates on NHWC layouts. Layout casts must be applied.
// IE.LayoutCast (to NCHW) -> IE.Add (NHWC input, NHWC output) -> IE.LayoutCast (to original)
mlir::LogicalResult OptimizeEltwise::matchAndRewrite(IE::MemPermuteOp memPermuteOp,
                                                     mlir::PatternRewriter& rewriter) const {
    _log.trace("[{0}] Got '{1}' at '{2}'", getDebugName(), memPermuteOp->getName(), memPermuteOp->getLoc());

    auto ctx = memPermuteOp.getContext();
    auto quantizeCastOp = memPermuteOp.input().getDefiningOp<IE::QuantizeCastOp>();

    auto addOp = getAddOp(memPermuteOp.input());
    if (addOp == nullptr) {
        return matchFailed(rewriter, memPermuteOp,
                           "IE.Add -> [IE.ShapeCast] -> [IE.QuantizeCast] -> IE.MemPermute pattern not found");
    }

    if (!isSupportedMemPermute(memPermuteOp, addOp, _log.nest())) {
        return mlir::failure();
    }

    const SmallVector<mlir::Value> branches = addOp->getOperands();

    SmallVector<mlir::Value> newAddInputs;

    for (size_t inputIdx = 0; inputIdx < branches.size(); inputIdx++) {
        if (getInputPermuteLikeOp(branches[inputIdx]) == nullptr) {
            // Process branch without PermuteLike op.
            const auto newOutput = processNonPermuteBranch(rewriter, memPermuteOp, branches[inputIdx], inputIdx);
            newAddInputs.push_back(newOutput);
            continue;
        }

        const auto inPermutationOp = getInputPermuteLikeOp(branches[inputIdx]);

        const auto newMemPermuteLoc = appendLoc(memPermuteOp.getLoc(), "_mem_permute_{0}", inputIdx);
        auto newMemPermuteOp = rewriter.create<IE::MemPermuteOp>(newMemPermuteLoc, inPermutationOp->getResult(0),
                                                                 memPermuteOp.dst_order(), memPermuteOp.mem_perm());

        const auto addInShape = getShape(branches[inputIdx]).toValues();
        const auto addInShapeAttr = getIntArrayAttr(ctx, addInShape.raw());
        const auto origAddInType = branches[inputIdx].getType().cast<vpux::NDTypeInterface>();
        const auto newShapeCastOrder = DimsOrder::fromValue(newMemPermuteOp.output());
        const auto newShapeCastType = origAddInType.changeDimsOrder(newShapeCastOrder);
        auto newShapeCastOp =
                rewriter.create<IE::ShapeCastOp>(memPermuteOp.getLoc(), newShapeCastType.changeShape(addInShape),
                                                 newMemPermuteOp.output(), addInShapeAttr);

        const auto addInOrder = DimsOrder::fromValue(branches[inputIdx]);
        const auto orderInAttr = mlir::AffineMapAttr::get(addInOrder.toAffineMap(ctx));
        const auto inLayoutCastLoc = appendLoc(memPermuteOp.getLoc(), "_in_layout_cast_{0}", inputIdx);
        auto inLayoutCastOp = rewriter.create<IE::LayoutCastOp>(inLayoutCastLoc, newShapeCastOp.result(), orderInAttr);

        newAddInputs.push_back(inLayoutCastOp.output());
    }
    auto newAddOp = rewriter.create<IE::AddOp>(addOp.getLoc(), addOp.getType(), newAddInputs[0], newAddInputs[1],
                                               addOp.auto_broadcastAttr(), addOp.post_opAttr());

    const auto nceOutLayout = DimsOrder::fromValue(memPermuteOp.output());
    const auto orderOutAttr = mlir::AffineMapAttr::get(nceOutLayout.toAffineMap(ctx));
    const auto outLayoutCastLoc = appendLoc(memPermuteOp.getLoc(), "_out_layout_cast");
    auto outLayoutCastOp = rewriter.create<IE::LayoutCastOp>(outLayoutCastLoc, newAddOp.output(), orderOutAttr);

    const auto newOutShapeCastType = memPermuteOp.output().getType();
    const auto newOutShapeCastLoc = appendLoc(memPermuteOp.getLoc(), "_out_shape_cast");

    const Shape targetShape = getShape(memPermuteOp.output()).toValues();
    const auto targetShapeAttr = getIntArrayAttr(ctx, targetShape.raw());
    IE::ShapeCastOp newOutShapeCastOp;
    if (quantizeCastOp != nullptr) {
        const auto quantizeCastInElemType = quantizeCastOp.input().getType().cast<NDTypeInterface>().getElementType();
        newOutShapeCastOp = rewriter.create<IE::ShapeCastOp>(
                newOutShapeCastLoc, newOutShapeCastType.cast<NDTypeInterface>().changeElemType(quantizeCastInElemType),
                outLayoutCastOp.output(), targetShapeAttr);
        auto newQuantizeCastOp = rewriter.create<IE::QuantizeCastOp>(
                quantizeCastOp.getLoc(), newOutShapeCastOp.result(), quantizeCastOp.dstElemTypeAttr());
        rewriter.replaceOp(memPermuteOp, newQuantizeCastOp.output());
    } else {
        newOutShapeCastOp = rewriter.create<IE::ShapeCastOp>(newOutShapeCastLoc, newOutShapeCastType,
                                                             outLayoutCastOp.output(), targetShapeAttr);
        rewriter.replaceOp(memPermuteOp, newOutShapeCastOp.result());
    }

    return mlir::success();
}

//
// SwapMemPermuteWithSoftmax
//

class SwapMemPermuteWithSoftmax final : public mlir::OpRewritePattern<IE::MemPermuteOp> {
public:
    SwapMemPermuteWithSoftmax(mlir::MLIRContext* ctx, Logger log)
            : mlir::OpRewritePattern<IE::MemPermuteOp>(ctx), _log(log) {
        this->setDebugName("SwapMemPermuteWithSoftmax");
    }

private:
    mlir::LogicalResult matchAndRewrite(IE::MemPermuteOp memPermuteOp, mlir::PatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult SwapMemPermuteWithSoftmax::matchAndRewrite(IE::MemPermuteOp memPermuteOp,
                                                               mlir::PatternRewriter& rewriter) const {
    _log.trace("[{0}] Got '{1}' at '{2}'", getDebugName(), memPermuteOp->getName(), memPermuteOp->getLoc());

    auto softmaxOp = memPermuteOp.input().getDefiningOp<IE::SoftMaxOp>();
    if (softmaxOp == nullptr) {
        return matchFailed(rewriter, memPermuteOp, "No parent softmaxOp found");
    }

    if (!softmaxOp->hasOneUse()) {
        return matchFailed(rewriter, memPermuteOp, "Parent softmaxOp has multiple uses");
    }

    auto addOp = getAddOp(softmaxOp.input());
    if (addOp == nullptr || !isSupportedMemPermute(memPermuteOp, addOp, _log.nest())) {
        return matchFailed(
                rewriter, memPermuteOp,
                "IE.Add -> [IE.ShapeCast] -> [IE.QuantizeCast] -> IE.SoftMax -> IE.MemPermute pattern not found");
    }

    auto memPerm = DimsOrder::fromAffineMap(memPermuteOp.mem_perm());
    auto permuteOutOrder = DimsOrder::fromValue(memPermuteOp.output());
    auto softmaxOrder = DimsOrder::fromValue(softmaxOp.input());

    auto softmaxAxisMemDim = softmaxOrder.toMemDim(Dim(softmaxOp.axisInd()));
    auto newSoftmaxAxisMemDim = MemDim(memPerm.dimPos(Dim(softmaxAxisMemDim.ind())));
    auto newSoftmaxAxisDim = permuteOutOrder.toDim(newSoftmaxAxisMemDim);

    auto newMemPermute = rewriter.create<IE::MemPermuteOp>(memPermuteOp.getLoc(), softmaxOp.input(),
                                                           memPermuteOp.dst_orderAttr(), memPermuteOp.mem_permAttr());
    auto newSoftmaxOp =
            rewriter.create<IE::SoftMaxOp>(softmaxOp.getLoc(), newMemPermute.output(),
                                           getIntAttr(getContext(), newSoftmaxAxisDim.ind()), softmaxOp.padSizeAttr());

    rewriter.replaceOp(memPermuteOp, newSoftmaxOp.output());

    return mlir::success();
}

//
// PropagateMemPermuteThroughAddPass
//

class PropagateMemPermuteThroughAddPass final :
        public IE::PropagateMemPermuteThroughAddBase<PropagateMemPermuteThroughAddPass> {
public:
    explicit PropagateMemPermuteThroughAddPass(Logger log): _log(log) {
        _log.setName(Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;

private:
    Logger _log;
};

void PropagateMemPermuteThroughAddPass::safeRunOnFunc() {
    auto& ctx = getContext();
    auto func = getOperation();

    mlir::RewritePatternSet patterns(&ctx);
    patterns.add<OptimizeEltwise>(&ctx, _log);
    patterns.add<SwapMemPermuteWithSoftmax>(&ctx, _log);

    if (mlir::failed(mlir::applyPatternsAndFoldGreedily(func, std::move(patterns), getDefaultGreedyRewriteConfig()))) {
        signalPassFailure();
    }
}

}  // namespace

std::unique_ptr<mlir::Pass> vpux::IE::createPropagateMemPermuteThroughAddPass(Logger log) {
    return std::make_unique<PropagateMemPermuteThroughAddPass>(log);
}
