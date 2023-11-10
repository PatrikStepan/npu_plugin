//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/core/cost_model_utils.hpp"
#include "vpux/compiler/dialect/IE/utils/resources.hpp"
#include "vpux/compiler/dialect/VPUIP/convert_to_dma_utils.hpp"
#include "vpux/compiler/dialect/VPUIP/passes.hpp"
#include "vpux/compiler/dialect/VPUIP/utils.hpp"
#include "vpux/compiler/dialect/VPURT/task.hpp"
#include "vpux/compiler/utils/rewriter.hpp"

#include <mlir/Transforms/GreedyPatternRewriteDriver.h>

using namespace vpux;

namespace {

//
// SplitNNDMARewriter
//

class SplitNNDMARewriter final : public mlir::OpRewritePattern<VPUIP::NNDMAOp> {
public:
    SplitNNDMARewriter(mlir::MLIRContext* ctx, int64_t dmaPortCount, Logger log, VPU::ArchKind arch)
            : mlir::OpRewritePattern<VPUIP::NNDMAOp>(ctx), _log(log), _dmaPortCount(dmaPortCount), _arch(arch) {
        setDebugName("SplitNNDMARewriter");

        _cmxNameAttr = mlir::FlatSymbolRefAttr::get(ctx, stringifyEnum(VPU::MemoryKind::CMX_NN));
    }

    mlir::LogicalResult matchAndRewrite(VPUIP::NNDMAOp nndmaOp, mlir::PatternRewriter& rewriter) const final;

private:
    void createTiles(VPUIP::NNDMAOp nndmaOp, mlir::PatternRewriter& rewriter, Logger log) const;

private:
    Logger _log;
    int64_t _dmaPortCount;
    mlir::FlatSymbolRefAttr _cmxNameAttr;
    VPU::ArchKind _arch;
};

Byte getDmaSize(VPUIP::NNDMAOp nndmaOp) {
    const auto inputShape = getShape(nndmaOp.input());
    const auto outputShape = getShape(nndmaOp.output());
    VPUX_THROW_UNLESS(inputShape == outputShape,
                      "NNDMAOpTiling: NNDMAOp node's input and output have different shapes: {0} vs {1}", inputShape,
                      outputShape);

    // Sparse data is composed of multiple buffers which will later get ungrouped into individual Copy operations
    // Therefore, the maximum buffer size is selected for tiling
    if (auto sparseInput = nndmaOp.input().getType().dyn_cast<VPUIP::SparseBufferType>()) {
        auto dataSize = sparseInput.getData().cast<vpux::NDTypeInterface>().getCompactAllocSize();
        auto sparsityMapSize =
                (sparseInput.getSparsityMap() != nullptr)
                        ? sparseInput.getSparsityMap().cast<vpux::NDTypeInterface>().getCompactAllocSize()
                        : Byte(0);
        auto seTableSize =
                (sparseInput.getStorageElementTable() != nullptr)
                        ? sparseInput.getStorageElementTable().cast<vpux::NDTypeInterface>().getCompactAllocSize()
                        : Byte(0);
        return std::max({dataSize, sparsityMapSize, seTableSize});
    }

    return static_cast<Byte>(getCompactSize(nndmaOp.input()));
}

void SplitNNDMARewriter::createTiles(VPUIP::NNDMAOp nndmaOp, mlir::PatternRewriter& rewriter, Logger log) const {
    // Currently, tiling is implemented only for 4D shapes.
    const auto origInputShape = getShape(nndmaOp.input());
    const auto origOutputShape = getShape(nndmaOp.output());

    const auto fullCopySize = getDmaSize(nndmaOp);

    auto tileDim = VPUIP::getCopyDMATilingDimForLargeSize(nndmaOp);
    if (VPUIP::isSplitNeededForLargePlanesNum(nndmaOp)) {
        tileDim = VPUIP::getCopyDMATilingDimForLargePlaneNum(nndmaOp);
    }

    log.nest().trace("[{0}] tile on dim {1}", nndmaOp->getLoc(), tileDim);

    // We cannot _just_ divide the fullCopySize by sizeLimit to get the number of tiles required
    // Example: let fullCopySize=48MB, sizeLimit=16MB and IFM.C=4, then it would be 48/16=3 tiles, but it's obviously
    //          impossible to split 4 channels into 3 tiles each of those would fit the limits
    const auto numPlanesOfFullShape = origInputShape[tileDim];
    const auto singlePlaneSize = fullCopySize / numPlanesOfFullShape;
    // The number of planes DMA could process within one tile. In case of small spatial dimensions of tensor (e.g.
    // 1x2048x8x8) it can exceed CMX_DMA_MAX_NUM_PLANES, so it's necessary to limit this value
    const auto maxNumPlanes = VPUIP::getMaxNumberPlanes(_arch);
    const auto desiredPlanesPerTileAmount = (VPUIP::DMA_LIMIT.count() / singlePlaneSize.count());
    VPUX_THROW_UNLESS(desiredPlanesPerTileAmount != 0,
                      "Couldn't split a NNDMAOp with single plane size greater than DMA_LIMIT");

    auto numPlanesPerTile = std::min(desiredPlanesPerTileAmount, maxNumPlanes);

    // Adjust numPlanesPerTile for even split, which provides better performance
    auto numTiles = numPlanesOfFullShape / numPlanesPerTile;
    if (numPlanesOfFullShape % numPlanesPerTile != 0) {
        numTiles++;
    }
    numPlanesPerTile = vpux::divUp(numPlanesOfFullShape, numTiles);

    auto inputDeclBuff = nndmaOp.input().getDefiningOp<VPURT::DeclareBufferOp>();
    auto outputDeclBuff = nndmaOp.output_buff().getDefiningOp<VPURT::DeclareBufferOp>();
    VPUX_THROW_UNLESS(inputDeclBuff != nullptr && outputDeclBuff != nullptr,
                      "Can't get input or output buffer of NNDMAOp '{0}'", nndmaOp->getLoc());

    Byte inputOffset{inputDeclBuff.getByteOffset()};
    Byte outputOffset{outputDeclBuff.getByteOffset()};

    auto vpurtTask = nndmaOp->getParentOfType<VPURT::TaskOp>();
    rewriter.setInsertionPointAfter(vpurtTask);

    auto currentTileInShape = Shape(origInputShape.raw());
    auto currentTileOutShape = Shape(origOutputShape.raw());
    auto planesLeftToCopy = numPlanesOfFullShape;
    auto inputInsertionPoint = nndmaOp.input().getDefiningOp();
    auto outputInsertionPoint = nndmaOp.output_buff().getDefiningOp();

    auto spillIdAttr = nndmaOp.spillIdAttr();

    const auto getTiledBuf = [](VPURT::DeclareBufferOp origBuf, vpux::ShapeRef subShape, vpux::Byte newOffset,
                                mlir::Operation* insertionPoint,
                                mlir::PatternRewriter& rewriter) -> VPURT::DeclareBufferOp {
        auto origType = origBuf.getType().cast<NDTypeInterface>();
        auto origStrides = origType.getStrides();
        auto newType = origType.changeShape(subShape);
        newType = newType.changeStrides(origStrides);

        return VPUIP::createNewDeclareBuffer(rewriter, insertionPoint, origBuf, newType, newOffset.count());
    };

    for (int64_t tileIdx = 0; tileIdx < numTiles; ++tileIdx) {
        // Get the proper shape and a new location for the tile
        const auto tileLoc = appendLoc(nndmaOp->getLoc(), "tile {0}", tileIdx);

        // The last tile may consume less number of planes for uneven split
        // e.g. Split 512 planes into 3 tiles
        // Tile 0: 171 planes
        // Tile 1: 171 planes
        // Tile 2: 170 planes
        currentTileInShape[tileDim] = std::min(planesLeftToCopy, numPlanesPerTile);
        currentTileOutShape[tileDim] = std::min(planesLeftToCopy, numPlanesPerTile);

        // Create new input buffer
        auto inputNewBuffer =
                getTiledBuf(inputDeclBuff, currentTileInShape, inputOffset, inputInsertionPoint, rewriter);
        inputInsertionPoint = inputNewBuffer.getResult().getDefiningOp();
        auto origInputStrides = inputNewBuffer.getType().cast<NDTypeInterface>().getStrides();
        inputOffset += Byte(currentTileInShape[tileDim] * origInputStrides[tileDim]);

        // Create new output buffer
        auto outputNewBuffer =
                getTiledBuf(outputDeclBuff, currentTileOutShape, outputOffset, outputInsertionPoint, rewriter);
        outputInsertionPoint = outputNewBuffer.getResult().getDefiningOp();
        auto origOutputStrides = outputDeclBuff.getType().cast<NDTypeInterface>().getStrides();
        outputOffset += Byte(currentTileOutShape[tileDim] * origOutputStrides[tileDim]);

        // Such distribution of tasks between ports may conflict with initial assumptions at memory scheduler level and
        // may potentially negatively impact previously prepared DMA tasks distribution and prefetching.
        // See E#84706, as a follow up task, implement it before scheduler with ClusterTiling refactoring.
        auto newDMAPort = tileIdx % _dmaPortCount;
        const auto newNNDMA = VPURT::wrapIntoTaskOp<VPUIP::NNDMAOp>(
                rewriter, vpurtTask.getWaitBarriers(), vpurtTask.getUpdateBarriers(), tileLoc, inputNewBuffer,
                outputNewBuffer, newDMAPort, nndmaOp.channelTypeAttr(), false, false, spillIdAttr, nullptr);

        log.trace("New tile '{0}' NNDMA op: '{1}'", tileIdx, newNNDMA);

        planesLeftToCopy -= currentTileInShape[tileDim];

        auto newVpurtTask = nndmaOp->getParentOfType<VPURT::TaskOp>();
        if (vpurtTask->getAttr(cycleBegin)) {
            newVpurtTask->setAttr(cycleBegin, vpurtTask->getAttr(cycleBegin));
        }
        if (vpurtTask->getAttr(cycleEnd)) {
            newVpurtTask->setAttr(cycleEnd, vpurtTask->getAttr(cycleEnd));
        }
    }

    VPUX_THROW_UNLESS(planesLeftToCopy == 0, "SplitNNDMA: a part of the original shape was not covered by NNDMA tiles");

    rewriter.eraseOp(vpurtTask);
}

mlir::LogicalResult SplitNNDMARewriter::matchAndRewrite(VPUIP::NNDMAOp nndmaOp, mlir::PatternRewriter& rewriter) const {
    // Split NNDMAOp with large tensor size should no happen at the moment because NNDMAOp's data size can't be larger
    // than VPUIP::DMA_LIMIT, see NNDMAOp::verify()
    // However, keep this part of code so that it looks similar to the processing logic in copy-op-tiling pass
    bool needSplitForLargeTensorSize = getDmaSize(nndmaOp) > VPUIP::DMA_LIMIT;
    bool needSplitForLargePlanesNum = VPUIP::isSplitNeededForLargePlanesNum(nndmaOp);

    if (!needSplitForLargeTensorSize && !needSplitForLargePlanesNum) {
        return mlir::failure();
    }

    _log.trace("Process NNDMA op : {0}", nndmaOp);
    createTiles(nndmaOp, rewriter, _log);

    return mlir::success();
}

//
// NNDMATilingPass
//

class NNDMATilingPass final : public VPUIP::NNDMATilingBase<NNDMATilingPass> {
public:
    explicit NNDMATilingPass(Logger log) {
        Base::initLogger(log, Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;
};

void NNDMATilingPass::safeRunOnFunc() {
    auto& ctx = getContext();
    auto func = getOperation();
    auto module = func->getParentOfType<mlir::ModuleOp>();
    const auto arch = VPU::getArch(module);

    auto dmaOp = IE::getAvailableExecutor(module, VPU::ExecutorKind::DMA_NN);
    auto dmaPortCount = dmaOp.count();

    mlir::RewritePatternSet patterns(&ctx);
    patterns.add<SplitNNDMARewriter>(&ctx, dmaPortCount, _log, arch);

    if (mlir::failed(
                mlir::applyPatternsAndFoldGreedily(func, std::move(patterns), vpux::getDefaultGreedyRewriteConfig()))) {
        signalPassFailure();
    }
}

}  // namespace

//
// createNNDMATilingPass
//

std::unique_ptr<mlir::Pass> vpux::VPUIP::createNNDMATilingPass(Logger log) {
    return std::make_unique<NNDMATilingPass>(log);
}
