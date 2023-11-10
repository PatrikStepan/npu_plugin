//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPURT/task.hpp"

#include "vpux/compiler/dialect/IE/utils/resources.hpp"
#include "vpux/compiler/dialect/VPURT/ops.hpp"
#include "vpux/compiler/dialect/VPURT/types.hpp"

#include "vpux/compiler/utils/dma.hpp"
#include "vpux/compiler/utils/error.hpp"
#include "vpux/utils/core/format.hpp"

using namespace vpux;

mlir::Operation* vpux::VPURT::TaskOp::getInnerTaskOp() {
    return &getBody().front().front();
}

void vpux::VPURT::TaskOp::build(::mlir::OpBuilder& odsBuilder, ::mlir::OperationState& odsState,
                                mlir::ValueRange waitBarriers, mlir::ValueRange updateBarriers) {
    build(odsBuilder, odsState, nullptr, waitBarriers, updateBarriers);
}

VPUIP::BlobWriter::SpecificTask vpux::VPURT::TaskOp::serialize(VPUIP::BlobWriter& writer) {
    auto task = mlir::dyn_cast<VPUIP::TaskOpInterface>(getInnerTaskOp());
    VPUX_THROW_UNLESS(task != nullptr, "Inner task  does not implement TaskOpInterface");
    writer.setAliasForSerializedTensors(task);
    return task.serialize(writer);
}

VPU::ExecutorKind vpux::VPURT::TaskOp::getExecutorKind() {
    auto innerTaskOp = getInnerTaskOp();
    if (auto clusterTilingOp = mlir::dyn_cast<VPUIP::NCEClusterTilingOp>(innerTaskOp)) {
        innerTaskOp = clusterTilingOp.getInnerTaskOp();
    }
    auto task = mlir::dyn_cast<VPUIP::TaskOpInterface>(innerTaskOp);
    VPUX_THROW_UNLESS(task != nullptr, "Inner task  does not implement TaskOpInterface");

    return task.getExecutorKind();
}

mlir::LogicalResult vpux::VPURT::TaskOp::verify() {
    const auto task = getOperation();
    if (getBody().getBlocks().size() != 1) {
        return errorAt(task, "The task body should contain exactly one block");
    }

    auto numOps = getBody().front().getOperations().size();
    if (numOps != 1) {
        return errorAt(task, "The task body should contain exactly one operation. Got: {0}", numOps);
    }

    auto& innerOp = getBody().front().front();
    if (!mlir::isa<mlir::MemoryEffectOpInterface>(innerOp)) {
        return errorAt(task, "The task body should contain operation with memory effects");
    }

    if (getIsTrailingSWLayer()) {
        for (auto updateBarrier : getUpdateBarriers()) {
            for (auto* depOp : updateBarrier.getUsers()) {
                auto depTask = mlir::dyn_cast<VPURT::TaskOp>(depOp);

                if (depTask == nullptr) {
                    return errorAt(task, "Trailing UPA Task has non-SW dependency : '{0}'", depOp->getLoc());
                }

                if (depTask.getExecutorKind() != VPU::ExecutorKind::SHAVE_UPA) {
                    return errorAt(task, "Trailing UPA Task has non-SW dependency : '{0}'", depOp->getLoc());
                }
            }
        }
    }
    return mlir::success();
}

void vpux::VPURT::TaskOp::getEffects(SmallVectorImpl<MemoryEffect>& effects) {
    for (const auto waitBarrier : getWaitBarriers()) {
        effects.emplace_back(mlir::MemoryEffects::Read::get(), waitBarrier, VPURT::BarrierResource::get());
    }

    for (const auto updateBarrier : getUpdateBarriers()) {
        effects.emplace_back(mlir::MemoryEffects::Write::get(), updateBarrier, VPURT::BarrierResource::get());
    }

    auto bodyEffects = mlir::cast<mlir::MemoryEffectOpInterface>(getInnerTaskOp());
    bodyEffects.getEffects(effects);
}

SmallVector<int64_t> vpux::VPURT::getDMATaskPorts(TaskOp task) {
    VPUX_THROW_UNLESS(task.getExecutorKind() == VPU::ExecutorKind::DMA_NN, "Unexpected task type find at '{0}'",
                      task->getLoc());
    SmallVector<int64_t> ports;
    auto* wrappedTaskOp = task.getInnerTaskOp();
    if (auto clusterTilingOp = mlir::dyn_cast<VPUIP::NCEClusterTilingOp>(wrappedTaskOp)) {
        auto module = clusterTilingOp->getParentOfType<mlir::ModuleOp>();
        auto dmaOp = IE::getAvailableExecutor(module, VPU::ExecutorKind::DMA_NN);
        auto dmaPortCount = dmaOp.count();

        const auto input = *clusterTilingOp.getInputs().begin();
        const auto output = *clusterTilingOp.getOutputs().begin();
        auto inputType = input.getType().cast<vpux::NDTypeInterface>();
        auto outputType = output.getType().cast<vpux::NDTypeInterface>();

        const auto distributedType = inputType.isa<VPUIP::DistributedBufferType>()
                                             ? inputType.dyn_cast<VPUIP::DistributedBufferType>()
                                             : outputType.dyn_cast<VPUIP::DistributedBufferType>();

        VPUX_THROW_UNLESS(distributedType != nullptr, "At least one of operands must have DistributedBuffer type");

        int64_t numClusters = 0;
        const auto checkSegmentedOrOverlapped = [&](vpux::NDTypeInterface type) {
            const auto distType = type.dyn_cast<VPUIP::DistributedBufferType>();
            if (distType == nullptr) {
                return false;
            }
            const auto distributionAttr = distType.getDistribution();
            VPUX_THROW_WHEN(distributionAttr == nullptr, "Failed to get distribution attribute.");
            const auto mode = distributionAttr.getMode().getValue();
            if (mode == VPU::DistributionMode::SEGMENTED || mode == VPU::DistributionMode::OVERLAPPED) {
                numClusters = distributionAttr.getNumClusters().getInt();
                return true;
            }
            return false;
        };

        if (checkSegmentedOrOverlapped(inputType) || checkSegmentedOrOverlapped(inputType)) {
            ports.resize(std::min(numClusters, dmaPortCount));
            std::iota(ports.begin(), ports.end(), 0);
            return ports;
        }
        wrappedTaskOp = clusterTilingOp.getInnerTaskOp();
    }

    ports.push_back(vpux::getDMAPortValue(wrappedTaskOp));
    return ports;
}
// Encode DMA port and channel setting into a single integer for convenient usage during barrier scheduling
int64_t vpux::VPURT::getDMAQueueIdEncoding(int64_t port, int64_t channelIdx) {
    return port * (VPUIP::getMaxEnumValForDmaChannelType() + 1) + channelIdx;
}
int64_t vpux::VPURT::getDMAQueueIdEncoding(int64_t port, llvm::Optional<vpux::VPUIP::DmaChannelType> channel) {
    return VPURT::getDMAQueueIdEncoding(port, static_cast<int64_t>(channel.value_or(VPUIP::DmaChannelType::DDR)));
}

Optional<SmallVector<VPURT::TaskQueueType>> vpux::VPURT::getDMATaskQueueType(TaskOp taskOp) {
    if (taskOp.getExecutorKind() != VPU::ExecutorKind::DMA_NN) {
        return None;
    }
    SmallVector<VPURT::TaskQueueType> queueTypes;

    mlir::Operation* innerOp = taskOp.getInnerTaskOp();

    if (auto clusterTilingOp = mlir::dyn_cast<VPUIP::NCEClusterTilingOp>(taskOp.getInnerTaskOp())) {
        innerOp = clusterTilingOp.getInnerTaskOp();
    }

    auto ports = VPURT::getDMATaskPorts(taskOp);
    auto dmaTask = mlir::dyn_cast<VPUIP::DMATypeOpInterface>(innerOp);
    VPUX_THROW_WHEN(dmaTask == nullptr, "Not a DMA task");

    const auto channelType = dmaTask.getChannelType();

    for (auto port : ports) {
        TaskQueueType queueType;
        queueType.type = VPU::ExecutorKind::DMA_NN;
        queueType.id = VPURT::getDMAQueueIdEncoding(port, channelType);
        queueTypes.push_back(queueType);
    }
    return queueTypes;
}

VPURT::TaskQueueType vpux::VPURT::getTaskQueueType(TaskOp taskOp, bool ignoreIndexForNce) {
    TaskQueueType queueType;
    queueType.type = taskOp.getExecutorKind();
    if (queueType.type == VPU::ExecutorKind::NCE && !ignoreIndexForNce) {
        auto* wrappedTaskOp = taskOp.getInnerTaskOp();
        if (auto clusterTilingOp = mlir::dyn_cast<VPUIP::NCEClusterTilingOp>(wrappedTaskOp)) {
            wrappedTaskOp = clusterTilingOp.getInnerTaskOp();
        }
        auto nceTask = mlir::dyn_cast<VPUIP::NCEClusterTaskOp>(wrappedTaskOp);
        VPUX_THROW_WHEN(nceTask == nullptr || nceTask.variants().getOps<VPUIP::DPUTaskOp>().empty(),
                        "Could not get DPU task");
        auto dpuTask = *(nceTask.variants().getOps<VPUIP::DPUTaskOp>().begin());
        queueType.id = dpuTask.cluster_id().value_or(0);
    } else if (queueType.type == VPU::ExecutorKind::DMA_NN) {
        auto* wrappedTaskOp = taskOp.getInnerTaskOp();
        if (auto clusterTilingOp = mlir::dyn_cast<VPUIP::NCEClusterTilingOp>(wrappedTaskOp)) {
            wrappedTaskOp = clusterTilingOp.getInnerTaskOp();
        }

        auto dmaTask = mlir::dyn_cast<VPUIP::DMATypeOpInterface>(wrappedTaskOp);
        VPUX_THROW_WHEN(dmaTask == nullptr, "Not a DMA task");
        queueType.id = getDMAQueueIdEncoding(vpux::getDMAPortValue(wrappedTaskOp), dmaTask.getChannelType());
    } else {
        queueType.id = 0;
    }
    return queueType;
}
