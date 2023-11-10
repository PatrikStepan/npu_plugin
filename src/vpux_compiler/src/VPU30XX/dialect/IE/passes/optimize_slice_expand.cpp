//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/passes/optimize_slice_expand.hpp"
#include "vpux/compiler/VPU30XX/dialect/IE/passes.hpp"
#include "vpux/compiler/utils/rewriter.hpp"

using namespace vpux;

namespace {

//
// OptimizeSliceExpandPass
//

class OptimizeSliceExpandPass final : public IE::arch30xx::OptimizeSliceExpandBase<OptimizeSliceExpandPass> {
public:
    explicit OptimizeSliceExpandPass(Logger log) {
        Base::initLogger(log, Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;
};

void OptimizeSliceExpandPass::safeRunOnFunc() {
    auto& ctx = getContext();

    mlir::RewritePatternSet patterns(&ctx);
    patterns.add<IE::OptimizeSliceExpand>(&ctx, _log);
    patterns.add<IE::OptimizeExpandSlice>(&ctx, _log);
    patterns.add<IE::OptimizeSliceImplicitExpand<IE::QuantizeCastOp>>(&ctx, _log);
    patterns.add<IE::OptimizeSliceImplicitExpand<IE::ConcatOp>>(&ctx, _log);
    patterns.add<IE::OptimizeSliceImplicitExpand<IE::HSwishOp>>(&ctx, _log);
    patterns.add<IE::OptimizeSliceImplicitExpand<IE::SwishOp>>(&ctx, _log);
    patterns.add<IE::OptimizeSingleSliceConcatExpand>(&ctx, _log);

    auto func = getOperation();
    if (mlir::failed(mlir::applyPatternsAndFoldGreedily(func, std::move(patterns), getDefaultGreedyRewriteConfig()))) {
        signalPassFailure();
        return;
    }
}

}  // namespace

std::unique_ptr<mlir::Pass> vpux::IE::arch30xx::createOptimizeSliceExpandPass(Logger log) {
    return std::make_unique<OptimizeSliceExpandPass>(log);
}
