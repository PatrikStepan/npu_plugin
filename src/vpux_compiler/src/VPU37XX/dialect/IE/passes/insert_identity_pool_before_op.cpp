//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/passes/insert_identity_pool_before_op.hpp"
#include "vpux/compiler/VPU37XX/dialect/IE/passes.hpp"

#include "vpux/compiler/utils/rewriter.hpp"

#include <mlir/IR/BlockAndValueMapping.h>
#include <mlir/Transforms/GreedyPatternRewriteDriver.h>

using namespace vpux;

namespace {

bool isEligiblePermute(mlir::Operation* op, Logger log) {
    const auto permuteInterface = op->getOperand(0).getDefiningOp<IE::LayerWithPermuteInterface>();
    if (permuteInterface != nullptr && permuteInterface->getResult(0).hasOneUse()) {
        log.trace("A permutation at {0} has already got a suitable producer", op->getLoc());
        return false;
    }
    const auto expectedLayout = DimsOrder::NHWC;
    const auto inputLayout = DimsOrder::fromValue(op->getOperand(0));
    if (inputLayout != expectedLayout) {
        log.trace("A permutation at {0} has got an unsupported input layout {1}, expected {2}", op->getLoc(),
                  inputLayout, expectedLayout);
        return false;
    }
    const auto shapeRef = getShape(op->getOperand(0));
    const auto batchSize = shapeRef[Dims4D::Act::N];
    if (batchSize != 1) {
        log.trace("A permutation at {0} has an unsupported batch size {1}", op->getLoc(), batchSize);
        return false;
    }

    // Activation shave version of MemPermute seems to be more efficient when MaxPool cannot be split over height.
    constexpr int64_t MIN_DIM_SIZE_FOR_TILING = 2;
    const auto height = shapeRef[Dims4D::Act::H];
    if (height < MIN_DIM_SIZE_FOR_TILING) {
        log.trace("A permutation at {0} has an unsupported height {1}", op->getLoc(), height);
        return false;
    }

    // Workaround for E#87616
    // Conversion of small tensors does not give much benefit.
    const Byte totalSize = getTotalSize(op->getOperand(0));
    const auto minimalSize = Byte(4096);
    if (totalSize < minimalSize) {
        log.trace("A permutation at {0} has {1} bytes of total size.", op->getLoc(), totalSize);
        return false;
    }

    return true;
}

mlir::Operation* insertAvgPool(mlir::Operation* concreteOp, mlir::PatternRewriter& rewriter, Logger log) {
    if (!IE::isEligiblePostOp(concreteOp, log)) {
        return nullptr;
    }
    const SmallVector<int64_t> poolStrides = {1, 1};
    const SmallVector<int64_t> poolKernels = {1, 1};
    const SmallVector<int64_t> pads = {0, 0};
    auto ctx = concreteOp->getContext();
    const auto padsAttr = getIntArrayAttr(ctx, pads);

    const auto outputType = concreteOp->getOperand(0).getType();
    return rewriter.create<IE::AvgPoolOp>(concreteOp->getLoc(), outputType, concreteOp->getOperand(0),
                                          getIntArrayAttr(ctx, poolKernels), getIntArrayAttr(ctx, poolStrides),
                                          padsAttr, padsAttr,
                                          vpux::IE::RoundingTypeAttr::get(ctx, vpux::IE::RoundingType::FLOOR),
                                          mlir::UnitAttr::get(rewriter.getContext()), nullptr);
}

bool checkPooling(mlir::Operation* op, mlir::Operation* maybePermute) {
    auto permuteInterface = mlir::dyn_cast_or_null<IE::LayerWithPermuteInterface>(op);
    if (permuteInterface == nullptr) {
        return true;
    }
    if (!permuteInterface.isSupportedPermutation(maybePermute)) {
        return false;
    }
    auto alignInterface = mlir::dyn_cast_or_null<IE::AlignedChannelsOpInterface>(op);
    if (alignInterface == nullptr) {
        return true;
    }
    return alignInterface.verifyChannels().succeeded();
}

mlir::Operation* insertMaxPool(mlir::Operation* concreteOp, mlir::PatternRewriter& rewriter, Logger log) {
    if (!isEligiblePermute(concreteOp, log)) {
        return nullptr;
    }
    const auto outputType = concreteOp->getOperand(0).getType();
    auto identityOp = createIdentityMaxPool(concreteOp->getOperand(0), outputType, rewriter);

    // This check is required to avoid processing IE.MemPermute with misaligned channels.
    if (!checkPooling(identityOp, concreteOp)) {
        rewriter.eraseOp(identityOp);
        return nullptr;
    }

    return identityOp;
}

//
// InsertIdentityPoolBeforeOpPass
//

class InsertIdentityPoolBeforeOpPass final :
        public IE::arch37xx::InsertIdentityPoolBeforeOpBase<InsertIdentityPoolBeforeOpPass> {
public:
    explicit InsertIdentityPoolBeforeOpPass(Logger log): _log(log) {
        _log.setName(Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;

private:
    Logger _log;
};

void InsertIdentityPoolBeforeOpPass::safeRunOnFunc() {
    auto& ctx = getContext();
    auto func = getOperation();

    // LeakyReLU and Clamp can bypass pooling checks.
    // The channels of resulting poolings will be aligned in the future passes.
    // This not the case for MemPermute.
    mlir::RewritePatternSet patterns(&ctx);
    patterns.add<IE::InsertIdPoolRewriter<IE::LeakyReluOp>>(&ctx, insertAvgPool, _log);
    patterns.add<IE::InsertIdPoolRewriter<IE::ClampOp>>(&ctx, insertAvgPool, _log);
    patterns.add<IE::InsertIdPoolRewriter<IE::ReLUOp>>(&ctx, insertAvgPool, _log);
    // IE.MaxPool operations are more efficient for IE.MemPermute fusion.
    patterns.add<IE::InsertIdPoolRewriter<IE::MemPermuteOp>>(&ctx, insertMaxPool, _log);

    if (mlir::failed(applyPatternsAndFoldGreedily(func, std::move(patterns), getDefaultGreedyRewriteConfig()))) {
        signalPassFailure();
    }
}

}  // namespace

std::unique_ptr<mlir::Pass> vpux::IE::arch37xx::createInsertIdentityPoolBeforeOpPass(Logger log) {
    return std::make_unique<InsertIdentityPoolBeforeOpPass>(log);
}
