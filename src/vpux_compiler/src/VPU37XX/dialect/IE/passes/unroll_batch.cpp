//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/passes/unroll_batch.hpp"
#include "vpux/compiler/VPU37XX/dialect/IE/passes.hpp"

#include "vpux/compiler/dialect/IE/ops.hpp"
#include "vpux/compiler/utils/attributes.hpp"
#include "vpux/compiler/utils/rewriter.hpp"
#include "vpux/compiler/utils/types.hpp"

#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/DialectConversion.h>

#include <mlir/IR/BlockAndValueMapping.h>

using namespace vpux;

namespace {

//
// UnrollBatchPass
//

class UnrollBatchPass final : public IE::arch37xx::UnrollBatchBase<UnrollBatchPass> {
public:
    explicit UnrollBatchPass(Logger log) {
        Base::initLogger(log, Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;
};

//
// safeRunOnFunc
//

void UnrollBatchPass::safeRunOnFunc() {
    auto& ctx = getContext();

    mlir::ConversionTarget target(ctx);
    target.addDynamicallyLegalOp<IE::FullyConnectedOp>([&](IE::FullyConnectedOp op) -> bool {
        return vpux::IE::isShapeRankEqualToZero(op.input()) || vpux::IE::isBatchEqualToOne(op.input());
    });
    target.addDynamicallyLegalOp<IE::ConvolutionOp>([&](IE::ConvolutionOp op) -> bool {
        return vpux::IE::isShapeRankEqualToZero(op.input()) || vpux::IE::isBatchEqualToOne(op.input());
    });
    target.addDynamicallyLegalOp<IE::GroupConvolutionOp>([&](IE::GroupConvolutionOp op) -> bool {
        return vpux::IE::isShapeRankEqualToZero(op.input()) || vpux::IE::isBatchEqualToOne(op.input());
    });
    target.addDynamicallyLegalOp<IE::ExpOp>([&](IE::ExpOp op) -> bool {
        return vpux::IE::isShapeRankEqualToZero(op.input()) || vpux::IE::isBatchEqualToOne(op.input());
    });
    target.addDynamicallyLegalOp<IE::SigmoidOp>([&](IE::SigmoidOp op) -> bool {
        return vpux::IE::isShapeRankEqualToZero(op.input()) || vpux::IE::isBatchEqualToOne(op.input());
    });
    target.addDynamicallyLegalOp<IE::AndOp>([&](IE::AndOp op) -> bool {
        return (vpux::IE::isShapeRankEqualToZero(op.input1()) || vpux::IE::isShapeRankEqualToZero(op.input2())) ||
               !vpux::IE::areShapeRanksEqual(op.input1(), op.input2()) ||
               (vpux::IE::isBatchEqualToOne(op.input1()) || vpux::IE::isBatchEqualToOne(op.input2()));
    });
    target.addDynamicallyLegalOp<IE::AddOp>([&](IE::AddOp op) -> bool {
        return (vpux::IE::isShapeRankEqualToZero(op.input1()) || vpux::IE::isShapeRankEqualToZero(op.input2())) ||
               !vpux::IE::areShapeRanksEqual(op.input1(), op.input2()) ||
               (vpux::IE::isBatchEqualToOne(op.input1()) || vpux::IE::isBatchEqualToOne(op.input2()));
    });
    target.addDynamicallyLegalOp<IE::AvgPoolOp>([&](IE::AvgPoolOp op) -> bool {
        return vpux::IE::isShapeRankEqualToZero(op.input()) || vpux::IE::isBatchEqualToOne(op.input());
    });
    target.addLegalOp<IE::ReshapeOp>();
    target.addLegalOp<IE::ConcatOp>();
    target.addLegalOp<IE::SliceOp>();
    target.addLegalOp<Const::DeclareOp>();

    mlir::RewritePatternSet patterns(&ctx);
    patterns.add<vpux::IE::BatchUnrollConverter<IE::ConvolutionOp>>(&ctx, _log, 1);
    patterns.add<vpux::IE::BatchUnrollConverter<IE::FullyConnectedOp>>(&ctx, _log, 1);
    patterns.add<vpux::IE::BatchUnrollConverter<IE::GroupConvolutionOp>>(&ctx, _log, 1);
    patterns.add<vpux::IE::BatchUnrollConverter<IE::ExpOp>>(&ctx, _log, 1);
    patterns.add<vpux::IE::BatchUnrollConverter<IE::SigmoidOp>>(&ctx, _log, 1);
    patterns.add<vpux::IE::BatchUnrollConverter<IE::AndOp>>(&ctx, _log, 2);
    patterns.add<vpux::IE::BatchUnrollConverter<IE::AddOp>>(&ctx, _log, 2);
    patterns.add<vpux::IE::BatchUnrollConverter<IE::AvgPoolOp>>(&ctx, _log, 1);

    auto func = getOperation();
    if (mlir::failed(mlir::applyPartialConversion(func, target, std::move(patterns)))) {
        signalPassFailure();
    }
}

}  // namespace

//
// createUnrollBatchPass
//

std::unique_ptr<mlir::Pass> vpux::IE::arch37xx::createUnrollBatchPass(Logger log) {
    return std::make_unique<UnrollBatchPass>(log);
}
