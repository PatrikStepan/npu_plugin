//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include <limits>

#include "vpux/compiler/dialect/VPURegMapped/ops_interfaces.hpp"
#include "vpux/compiler/dialect/VPURegMapped/passes.hpp"

#include "vpux/utils/core/range.hpp"

namespace vpux {

void VPURegMapped::ResolveTaskLocationPass::safeRunOnFunc() {
    auto function = getOperation();
    auto builder = mlir::OpBuilder::atBlockBegin(&function.getBody().front());
    auto context = function.getContext();

    auto populateTaskBuffers = [&, this](size_t tile, VPURegMapped::TaskType type, const auto& sizesPerTaskType) {
        // order of DeclareTaskBuffer is important as it must be aligned with firmware expectations
        // tile0: DPUInvariant -> DPUVariant -> Ranges -> Invocations -> DMA from DDR -> DMA from CMX
        // tile1: DPUInvariant -> DPUVariant -> Ranges -> Invocations -> DMA from DDR -> DMA from CMX
        // ...
        const auto sizesPerList = sizesPerTaskType.lookup(type);
        auto& metadataBuffersPerTaskType = _metadataBuffers[tile][type];
        metadataBuffersPerTaskType.resize(sizesPerList.size());
        for (const auto& entryPerList : llvm::enumerate(sizesPerList)) {
            const auto list = entryPerList.index();
            const auto sizePerList = entryPerList.value();

            for (auto i : irange(sizePerList)) {
                auto declareTaskBufferOp = builder.create<VPURegMapped::DeclareTaskBufferOp>(
                        function.getLoc(), vpux::VPURegMapped::IndexType::get(context, tile, list, i), type);
                metadataBuffersPerTaskType[list].push_back(declareTaskBufferOp);
            }
        }
    };

    _metadataBuffers.resize(_metadataBuffersSizes.size());
    for (const auto& entryPerTile : llvm::enumerate(_metadataBuffersSizes)) {
        const auto tile = entryPerTile.index();
        const auto& sizesPerTaskType = entryPerTile.value();
        populateTaskBuffers(tile, VPURegMapped::TaskType::DPUInvariant, sizesPerTaskType);
        populateTaskBuffers(tile, VPURegMapped::TaskType::DPUVariant, sizesPerTaskType);
        populateTaskBuffers(tile, VPURegMapped::TaskType::ActKernelRange, sizesPerTaskType);
        populateTaskBuffers(tile, VPURegMapped::TaskType::ActKernelInvocation, sizesPerTaskType);
        populateTaskBuffers(tile, VPURegMapped::TaskType::DMA, sizesPerTaskType);
    }

    for (auto task : function.getOps<VPURegMapped::TaskOpInterface>()) {
        const auto type = task.getTaskType();
        const auto index = task.getIndexType();
        const auto& taskBuffers = _metadataBuffers[index.getTileIdx()][type][index.getListIdx()];

        task.setTaskLocation(taskBuffers[index.getValue() % taskBuffers.size()]);
    }
}

}  // namespace vpux
