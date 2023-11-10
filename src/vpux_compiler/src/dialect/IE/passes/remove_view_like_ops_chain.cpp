//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/core/layers.hpp"
#include "vpux/compiler/dialect/IE/ops.hpp"
#include "vpux/compiler/dialect/IE/ops_interfaces.hpp"
#include "vpux/compiler/dialect/IE/passes.hpp"
#include "vpux/compiler/dialect/IE/utils/broadcast_utils.hpp"
#include "vpux/compiler/dialect/IE/utils/scale_shift_utils.hpp"
#include "vpux/compiler/dialect/const/attributes/content.hpp"
#include "vpux/compiler/dialect/const/ops.hpp"
#include "vpux/compiler/utils/attributes.hpp"
#include "vpux/compiler/utils/error.hpp"
#include "vpux/compiler/utils/rewriter.hpp"
#include "vpux/compiler/utils/types.hpp"

#include "vpux/utils/core/checked_cast.hpp"

#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/DialectConversion.h>

using namespace vpux;

namespace {

//
// ViewLikeOpsChainRewriter
//

class ViewLikeOpsChainRewriter final : public mlir::OpInterfaceRewritePattern<IE::ViewLikeOpInterface> {
public:
    ViewLikeOpsChainRewriter(mlir::MLIRContext* ctx, Logger log)
            : mlir::OpInterfaceRewritePattern<IE::ViewLikeOpInterface>(ctx), _log(log) {
    }

private:
    mlir::LogicalResult matchAndRewrite(IE::ViewLikeOpInterface origOp, mlir::PatternRewriter& rewriter) const final;

private:
    Logger _log;
};

/*
case 1:
     LayerOp                                LayerOp
        |                                      |
    ViewLikeOp1                             LayerOp
        |
       ...                =>
        |
    ViewLikeOpX
        |
    LayerOp

case 2:
     LayerOp                             LayerOp
        |                                   |
    ViewLikeOp1                         ViewLikeOpY
        |                                   |
       ...                =>               ...
        |                                LayerOp
    ViewLikeOpX
        |
    ViewLikeOpY
       ...
        |
     LayerOp
*/
mlir::LogicalResult ViewLikeOpsChainRewriter::matchAndRewrite(IE::ViewLikeOpInterface origOp,
                                                              mlir::PatternRewriter& rewriter) const {
    _log.trace("[{0}] Got '{1}' at '{2}'", getDebugName(), origOp->getName(), origOp->getLoc());

    if (!IE::isPureViewOp(origOp) || !origOp->hasOneUse()) {
        return mlir::failure();
    }

    auto inputType = origOp->getOperand(0).getType().cast<vpux::NDTypeInterface>();
    vpux::NDTypeInterface outputType;

    SmallVector<mlir::Operation*> viewLikeOps;
    viewLikeOps.push_back(origOp);
    auto potentialViewLikeOp = *origOp->getUsers().begin();
    auto lastViewLikeOp = potentialViewLikeOp;
    while (IE::isPureViewOp(potentialViewLikeOp)) {
        viewLikeOps.push_back(potentialViewLikeOp);
        lastViewLikeOp = potentialViewLikeOp;
        if (!potentialViewLikeOp->hasOneUse()) {
            return mlir::failure();
        }

        outputType = potentialViewLikeOp->getResult(0).getType().cast<vpux::NDTypeInterface>();
        // Find the first ViewLikeOp in the traversing chain and break the current loop, remove the sub-chain and then
        // traversing from next ViewLikeOp greedily in the next iteration.
        if (inputType == outputType) {
            break;
        }

        potentialViewLikeOp = *potentialViewLikeOp->getUsers().begin();

        if (!IE::isPureViewOp(potentialViewLikeOp)) {
            break;
        }
    }

    if (inputType != outputType) {
        return mlir::failure();
    }

    lastViewLikeOp->getResult(0).replaceAllUsesWith(origOp->getOperand(0));
    for (auto op : viewLikeOps) {
        op->dropAllUses();
        rewriter.eraseOp(op);
    }

    return mlir::success();
}

//
// RemoveViewLikeOpsChainPass
//

class RemoveViewLikeOpsChainPass final : public IE::RemoveViewLikeOpsChainPassBase<RemoveViewLikeOpsChainPass> {
public:
    explicit RemoveViewLikeOpsChainPass(Logger log): _log(log) {
        _log.setName(Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;

private:
    Logger _log;
};

void RemoveViewLikeOpsChainPass::safeRunOnFunc() {
    auto func = getOperation();
    auto& ctx = getContext();

    mlir::RewritePatternSet patterns(&ctx);
    patterns.add<ViewLikeOpsChainRewriter>(&ctx, _log);

    if (mlir::failed(applyPatternsAndFoldGreedily(func, std::move(patterns), getDefaultGreedyRewriteConfig()))) {
        signalPassFailure();
    }
}

}  // namespace

//
// createRemoveViewLikeOpsChainPass
//

std::unique_ptr<mlir::Pass> vpux::IE::createRemoveViewLikeOpsChainPass(Logger log) {
    return std::make_unique<RemoveViewLikeOpsChainPass>(log);
}
