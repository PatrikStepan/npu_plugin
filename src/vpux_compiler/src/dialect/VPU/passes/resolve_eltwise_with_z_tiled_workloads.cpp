//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/utils/resources.hpp"
#include "vpux/compiler/dialect/VPU/ops.hpp"
#include "vpux/compiler/dialect/VPU/passes.hpp"
#include "vpux/compiler/utils/error.hpp"
#include "vpux/compiler/utils/logging.hpp"
#include "vpux/compiler/utils/rewriter.hpp"

#include <mlir/IR/BlockAndValueMapping.h>

using namespace vpux;
using namespace VPU;

namespace {

using WorkloadSplits = SmallVector<SmallVector<VPU::DPUWorkloadOp>>;

// Separates the workloads by their cluster_id attribute
WorkloadSplits separateWorkloadsByClusters(VPU::NCEEltwiseOp eltwiseOp, const int64_t numClusters) {
    WorkloadSplits clusterWorkloads(numClusters);
    const auto workloads = eltwiseOp.workloads().getOps<VPU::DPUWorkloadOp>();
    for (auto wl : workloads) {
        const auto clusterId = wl.cluster_id().value_or(0);
        clusterWorkloads[clusterId].push_back(wl);
    }

    clusterWorkloads.erase(std::remove_if(clusterWorkloads.begin(), clusterWorkloads.end(),
                                          [](ArrayRef<VPU::DPUWorkloadOp> workloads) {
                                              return workloads.empty();
                                          }),
                           clusterWorkloads.end());

    return clusterWorkloads;
}

// Verifies that all clusters have the same channel ranges for their workloads, since this is the only scenario
// supported by this pass; it is also the only multiclustering scenario supported so far.
// Also ensures that the range of channels starts from zero.
void validateClusterWorkloads(WorkloadSplits& clusterWorkloads) {
    SmallVector<int64_t> channelOffsets;
    SmallVector<int64_t> channelSizes;
    bool channelsStartsFromZero = false;
    auto firstClusterWorkloads = clusterWorkloads.front();
    for (auto wl : firstClusterWorkloads) {
        auto wlChannelOffset = parseIntArrayAttr<int64_t>(wl.outOffsets())[Dims4D::Act::C.ind()];
        auto wlChannelSize = parseIntArrayAttr<int64_t>(wl.outSizes())[Dims4D::Act::C.ind()];
        channelOffsets.push_back(wlChannelOffset);
        channelSizes.push_back(wlChannelSize);
        if (wlChannelOffset == 0) {
            channelsStartsFromZero = true;
        }
    }
    VPUX_THROW_UNLESS(channelsStartsFromZero, "No workloads in the first cluster start from offset zero");

    for (size_t i = 1; i < clusterWorkloads.size(); ++i) {
        auto workloads = clusterWorkloads[i];
        VPUX_THROW_UNLESS(workloads.size() == firstClusterWorkloads.size(),
                          "Expected all clusters to have the same number of workloads. Got cluster {0} with {1} "
                          "workloads, while first cluster has {2} workloads",
                          i, workloads.size(), firstClusterWorkloads.size());
        for (auto wl : workloads | indexed) {
            auto wlChannelOffset = parseIntArrayAttr<int64_t>(wl.value().outOffsets())[Dims4D::Act::C.ind()];
            auto wlChannelSize = parseIntArrayAttr<int64_t>(wl.value().outSizes())[Dims4D::Act::C.ind()];
            VPUX_THROW_UNLESS(
                    wlChannelOffset == channelOffsets[wl.index()] && wlChannelSize == channelSizes[wl.index()],
                    "Expected channels '{0}-{1}' for workload {2}, got '{3}-{4}'", channelOffsets[wl.index()],
                    channelSizes[wl.index()], wl.index(), wlChannelOffset, wlChannelSize);
        }
    }
}

// Sorts the workloads by the channel offset, independently for each cluster
void sortClusterWorkloads(WorkloadSplits& clusterWorkloads) {
    for (auto& workloads : clusterWorkloads) {
        llvm::sort(workloads, [](VPU::DPUWorkloadOp lhs, VPU::DPUWorkloadOp rhs) {
            auto lhsOffsets = parseIntArrayAttr<int64_t>(lhs.outOffsets());
            auto rhsOffsets = parseIntArrayAttr<int64_t>(rhs.outOffsets());
            return lhsOffsets[Dims4D::Act::C.ind()] < rhsOffsets[Dims4D::Act::C.ind()];
        });
    }
}

// Takes as a parameter the workloads separated by the clusters they execute on, sorted by the channel offset.
// Returns the workloads split by the channel offset, irrespective of the cluster.
//
// For example, having the following sorted workloads with dimensions in the NCHW format:
//   [cluster_id = 0, offset = [0, 0,  0, 0], sizes = [1, 64, 8, 16],
//    cluster_id = 0, offset = [0, 64, 0, 0], sizes = [1, 64, 8, 16]]
//   [cluster_id = 1, offset = [0, 0,  8, 0], sizes = [1, 64, 8, 16],
//    cluster_id = 1, offset = [0, 64, 8, 0], sizes = [1, 64, 8, 16]]
// The function returns:
//   [cluster_id = 0, offset = [0, 0,  0, 0], sizes = [1, 64, 8, 16],
//    cluster_id = 1, offset = [0, 0,  8, 0], sizes = [1, 64, 8, 16]]
//   [cluster_id = 0, offset = [0, 64, 0, 0], sizes = [1, 64, 8, 16],
//    cluster_id = 1, offset = [0, 64, 8, 0], sizes = [1, 64, 8, 16]]
WorkloadSplits getWorkloadSplits(const WorkloadSplits& sortedClusterWorkloads) {
    WorkloadSplits workloadSplits;

    for (auto& clusterWorkloads : sortedClusterWorkloads) {
        size_t index = 0;
        Optional<int64_t> channelOffset;
        for (auto wl : clusterWorkloads) {
            const auto offsets = parseIntArrayAttr<int64_t>(wl.outOffsets());
            const auto wlChannelOffset = offsets[Dims4D::Act::C.ind()];
            if (!channelOffset.has_value()) {
                channelOffset = wlChannelOffset;
            }

            if (wlChannelOffset != channelOffset.value()) {
                ++index;
            }
            if (workloadSplits.size() <= index) {
                workloadSplits.insert(workloadSplits.begin() + index, SmallVector<VPU::DPUWorkloadOp>());
            }

            workloadSplits[index].push_back(wl);
        }
    }

    for (auto& workloadSplit : workloadSplits) {
        const auto sameChannelSubset =
                std::adjacent_find(
                        workloadSplit.begin(), workloadSplit.end(), [](VPU::DPUWorkloadOp lhs, VPU::DPUWorkloadOp rhs) {
                            const auto lhsOffsetC = parseIntArrayAttr<int64_t>(lhs.outOffsets())[Dims4D::Act::C.ind()];
                            const auto rhsOffsetC = parseIntArrayAttr<int64_t>(rhs.outOffsets())[Dims4D::Act::C.ind()];
                            const auto lhsSizesC = parseIntArrayAttr<int64_t>(lhs.outSizes())[Dims4D::Act::C.ind()];
                            const auto rhsSizesC = parseIntArrayAttr<int64_t>(rhs.outSizes())[Dims4D::Act::C.ind()];
                            return lhsOffsetC != rhsOffsetC || lhsSizesC != rhsSizesC;
                        }) == workloadSplit.end();
        VPUX_THROW_UNLESS(sameChannelSubset, "Not all workloads in split have the same channel subset");
    }

    return workloadSplits;
}

// Finds the users of the value, separating them by their memory space.
// Direct copy operations are skipped, so that their users are returned
void findOutputValuesByMemoryKind(mlir::Value outputValue, SmallVector<mlir::Value>& ddrValues,
                                  SmallVector<mlir::Value>& cmxValues) {
    auto addValue = [&](mlir::Value value) -> void {
        auto memoryKind = value.getType().cast<vpux::NDTypeInterface>().getMemoryKind();
        if (memoryKind == VPU::MemoryKind::DDR) {
            ddrValues.push_back(value);
        } else if (memoryKind == VPU::MemoryKind::CMX_NN) {
            cmxValues.push_back(value);
        } else {
            VPUX_THROW("Unexpected memory kind: '{0}'", memoryKind);
        }
    };

    for (auto userOp : outputValue.getUsers()) {
        if (auto clusterTilingOp = mlir::dyn_cast<VPU::NCEClusterTilingOp>(userOp)) {
            if (clusterTilingOp.getInnerTaskOpOfType<VPU::CopyOp>() != nullptr) {
                addValue(clusterTilingOp->getResult(0));
            } else {
                addValue(outputValue);
            }
        } else if (auto copyOp = mlir::dyn_cast<VPU::CopyOp>(userOp)) {
            addValue(copyOp->getResult(0));
        } else {
            addValue(outputValue);
        }
    }
}

// Finds the entire slice of the data that is covered by the given workloads.
// For example, for the following workloads:
//   cluster_id = 0, offset = [0, 0, 0, 0], sizes = [1, 64, 8, 16]
//   cluster_id = 1, offset = [0, 0, 8, 0], sizes = [1, 64, 8, 16]
// It returns the pair of offsets [0, 0, 0, 0] and sizes [1, 64, 16, 16]
// This assumes that the workloads handle contiguous data regions.
std::pair<Shape, Shape> extractSplitInformation(ArrayRef<VPU::DPUWorkloadOp> workloads) {
    const int64_t workloadNumDims = 4;
    SmallVector<int64_t> offsets(workloadNumDims, std::numeric_limits<int64_t>::max());
    SmallVector<int64_t> end(workloadNumDims, 0);
    for (auto wl : workloads) {
        const auto wlOffsets = parseIntArrayAttr<int64_t>(wl.outOffsets());
        const auto wlSizes = parseIntArrayAttr<int64_t>(wl.outSizes());
        VPUX_THROW_UNLESS(wlOffsets.size() == workloadNumDims && wlSizes.size() == workloadNumDims,
                          "Expected 4D workload sizes, got offsets of {0} dims and sizes of {1} dims", wlOffsets.size(),
                          wlSizes.size());
        for (auto dim : irange(wlOffsets.size())) {
            if (wlOffsets[dim] < offsets[dim]) {
                offsets[dim] = wlOffsets[dim];
            }
            if (wlOffsets[dim] + wlSizes[dim] > end[dim]) {
                end[dim] = wlOffsets[dim] + wlSizes[dim];
            }
        }
    }

    SmallVector<int64_t> sizes(workloadNumDims, 0);
    for (auto dim : irange(workloadNumDims)) {
        if (end[dim] - offsets[dim] > sizes[dim]) {
            sizes[dim] = end[dim] - offsets[dim];
        }
    }

    return std::make_pair(Shape(offsets), Shape(sizes));
}

// Creates a new Eltwise operation with the operands and workloads given as parameters.
// The workloads have the channel offset set to zero, to satisfy hardware limitations.
mlir::Value createEltwiseSlice(mlir::OpBuilder& builder, VPU::NCEEltwiseOp eltwiseOp, mlir::ValueRange newOperands,
                               ArrayRef<VPU::DPUWorkloadOp> workloads, mlir::Location loc) {
    mlir::BlockAndValueMapping mapper;
    mapper.map(eltwiseOp.getOperands(), newOperands);
    auto newOp = builder.clone(*eltwiseOp.getOperation(), mapper);
    newOp->setLoc(loc);

    auto newEltwiseOp = mlir::cast<VPU::NCEEltwiseOp>(newOp);

    auto outputType = eltwiseOp.output().getType().cast<vpux::NDTypeInterface>();
    auto newInputType = newOperands.front().getType().cast<vpux::NDTypeInterface>();
    auto newOutputType = outputType.changeShape(newInputType.getShape());
    newEltwiseOp.output().setType(newOutputType);

    auto newEltwiseOpWorkloads = newEltwiseOp.workloads().getOps<VPU::DPUWorkloadOp>();
    for (auto wl : llvm::make_early_inc_range(newEltwiseOpWorkloads)) {
        wl.erase();
    }

    mlir::OpBuilder workloadBuilder(newEltwiseOp.workloads());
    for (auto wl : workloads) {
        auto offsets = parseIntArrayAttr<int64_t>(wl.outOffsets());
        offsets[Dims4D::Act::C.ind()] = 0;
        auto offsetsAttr = getIntArrayAttr(wl.getContext(), offsets);
        workloadBuilder.create<VPU::DPUWorkloadOp>(wl.getLoc(), offsetsAttr, wl.outSizesAttr(), wl.inOffsetsAttr(),
                                                   wl.inSizesAttr(), wl.padAttr(), wl.mpe_modeAttr(),
                                                   wl.cluster_idAttr());
    }

    return newEltwiseOp.output();
}

// Creates a copy operation that moves the data to the given memory space
mlir::Value createCopyOp(mlir::OpBuilder& builder, mlir::Value value, vpux::NDTypeInterface outputType,
                         const bool isMulticlustering, mlir::Location loc) {
    if (isMulticlustering) {
        const auto bodyBuilder = [&](mlir::OpBuilder& innerBuilder, mlir::Location loc,
                                     mlir::ValueRange innerOperands) {
            auto newCopyOp = builder.create<VPU::CopyOp>(loc, innerOperands[0], outputType.getMemSpace());
            innerBuilder.create<VPU::YieldOp>(loc, newCopyOp.output());
        };
        auto newClusterTilingOp =
                builder.create<VPU::NCEClusterTilingOp>(loc, outputType, mlir::ValueRange{value}, bodyBuilder);
        return newClusterTilingOp->getResult(0);
    }

    auto newCopyOp = builder.create<VPU::CopyOp>(loc, value, outputType.getMemSpace());
    return newCopyOp.output();
}

// Returns the input / output values of the eltwise operation. In case the operation is wrapped into NCEClusterTiling,
// the outer operands / results are returned.
// Additionally, the function can introduce spills when the input is written directly to CMX as a distributed type.
// This is necessary since Slice operations will be introduced in order to extract a part of the data for the new
// eltwise operations. Slice operations get lowered to copies during bufferization and CMX2CMX transfers where both the
// input and output are distributed are not fully supported. Therefore, the distributed CMX value is spilled to DDR, so
// that Slice is used on the DDR tensor. When this happens, the `spilledInputs` parameter is set to true and the
// returned input values are from DDR.
void getOpValues(mlir::OpBuilder& builder, VPU::NCEEltwiseOp eltwiseOp, mlir::Value& input1, mlir::Value& input2,
                 mlir::Value& output, bool& spilledInputs) {
    input1 = eltwiseOp.input1();
    input2 = eltwiseOp.input2();
    output = eltwiseOp.output();
    spilledInputs = false;

    auto clusterTilingOp = eltwiseOp->getParentOfType<VPU::NCEClusterTilingOp>();
    if (clusterTilingOp != nullptr) {
        auto input1BlockArg = input1.dyn_cast<mlir::BlockArgument>();
        auto input2BlockArg = input2.dyn_cast<mlir::BlockArgument>();
        VPUX_THROW_UNLESS(input1BlockArg != nullptr, "Input 1 is not a block argument");
        VPUX_THROW_UNLESS(input2BlockArg != nullptr, "Input 2 is not a block argument");
        input1 = clusterTilingOp->getOperand(input1BlockArg.getArgNumber());
        input2 = clusterTilingOp->getOperand(input2BlockArg.getArgNumber());
        output = clusterTilingOp->getResult(0);
    }

    const auto maybeCopyToDDR = [&](mlir::Value value) -> mlir::Value {
        if (value.getType().isa<VPU::DistributedTensorType>()) {
            auto valueType = value.getType().cast<vpux::NDTypeInterface>();
            if (valueType.getMemoryKind() == VPU::MemoryKind::CMX_NN) {
                spilledInputs = true;
                const auto ddrMemKindAttr =
                        vpux::IndexedSymbolAttr::get(builder.getContext(), stringifyEnum(VPU::MemoryKind::DDR));
                auto tensorAttr = vpux::getTensorAttr(builder.getContext(), valueType.getDimsOrder(), ddrMemKindAttr);
                auto outputType =
                        mlir::RankedTensorType::get(valueType.getShape().raw(), valueType.getElementType(), tensorAttr)
                                .cast<vpux::NDTypeInterface>();
                return createCopyOp(builder, value, outputType, clusterTilingOp != nullptr, eltwiseOp->getLoc());
            }
        }
        return value;
    };
    input1 = maybeCopyToDDR(input1);
    input2 = maybeCopyToDDR(input2);
}

mlir::Value sliceEltwise(mlir::OpBuilder& builder, VPU::NCEEltwiseOp origEltwiseOp, mlir::ValueRange operands,
                         ShapeRef offsets, ShapeRef sizes, ArrayRef<VPU::DPUWorkloadOp> workloadSplit,
                         const bool isMulticlustering, const bool spilledInputs, mlir::Location loc) {
    // Slice operands and move them to CMX if they are not already there
    SmallVector<mlir::Value> newOperands;
    for (auto operand : operands) {
        auto sliceOp = builder.create<VPU::SliceOp>(loc, operand, offsets, sizes);
        auto sliceOperand = sliceOp.result();
        auto sliceType = sliceOperand.getType().cast<vpux::NDTypeInterface>();

        if (sliceType.getMemoryKind() != VPU::MemoryKind::CMX_NN) {
            auto copyType = sliceType;
            if (spilledInputs) {
                // In case the input was manually spilled to DDR in this pass, the original CMX type is distributed
                // The new copy to CMX must also produce a distributed type with the same strategy as before
                auto sliceShape = sliceType.getShape();
                copyType = operand.getDefiningOp()->getOperand(0).getType().cast<vpux::NDTypeInterface>();
                copyType = copyType.changeShape(sliceShape);
            } else {
                const auto memSpaceCMX = origEltwiseOp.input1().getType().cast<vpux::NDTypeInterface>().getMemSpace();
                copyType = copyType.changeMemSpace(memSpaceCMX);
            }
            sliceOperand = createCopyOp(builder, sliceOperand, copyType, isMulticlustering, loc);
        }

        newOperands.push_back(sliceOperand);
    }

    // Create a new eltwise operation that contains the correct workloads
    mlir::Value sliceOutput;
    if (isMulticlustering) {
        const auto bodyBuilder = [&](mlir::OpBuilder& innerBuilder, mlir::Location loc, mlir::ValueRange newArgs) {
            auto newOutput = createEltwiseSlice(builder, origEltwiseOp, newArgs, workloadSplit, loc);
            innerBuilder.create<YieldOp>(loc, newOutput);
        };

        auto clusterTilingOp = origEltwiseOp->getParentOfType<VPU::NCEClusterTilingOp>();
        auto outputType = clusterTilingOp->getResult(0).getType().cast<vpux::NDTypeInterface>();
        auto newOutputType = outputType.changeShape(sizes);

        const auto newClusterTilingOp =
                builder.create<NCEClusterTilingOp>(loc, newOutputType, newOperands, bodyBuilder);
        sliceOutput = newClusterTilingOp->getResult(0);
    } else {
        sliceOutput = createEltwiseSlice(builder, origEltwiseOp, newOperands, workloadSplit, loc);
    }

    // Copy the outputs to DDR. This is done to avoid wrong results when the concat is done in CMX
    // Accuracy issue to be investigated in E76283
    auto sliceOutputType = sliceOutput.getType().cast<vpux::NDTypeInterface>();
    auto origEltwiseOutputType = origEltwiseOp.output().getType().cast<vpux::NDTypeInterface>();
    auto outputType =
            origEltwiseOutputType.changeShape(sliceOutputType.getShape()).changeMemSpace(VPU::MemoryKind::DDR);

    auto outputCopy = createCopyOp(builder, sliceOutput, outputType, isMulticlustering, loc);
    return outputCopy;
}

mlir::Type getConcatType(mlir::Value origOutputValue) {
    auto origOutputType = origOutputValue.getType().cast<vpux::NDTypeInterface>();
    auto concatType = origOutputType.changeMemSpace(VPU::MemoryKind::DDR);

    if (auto distType = origOutputType.dyn_cast<VPU::DistributedTypeInterface>()) {
        if (distType.containsDistributedTypes()) {
            auto distributedTypes = distType.getDistributedTypes();
            if (origOutputType.isa<VPU::SparseTensorType>()) {
                VPUX_THROW_UNLESS(distributedTypes.size() == 2,
                                  "Expected two distributed tensors for sparse output, got {0}",
                                  distributedTypes.size());
                auto compactDataType = distributedTypes[0].cast<VPU::DistributedTensorType>().getCompactType();
                auto compactSMType = distributedTypes[1].cast<VPU::DistributedTensorType>().getCompactType();

                auto dataType = compactDataType.cast<vpux::NDTypeInterface>().changeMemSpace(VPU::MemoryKind::DDR);
                auto smType = compactSMType.cast<vpux::NDTypeInterface>().changeMemSpace(VPU::MemoryKind::DDR);

                concatType = VPU::SparseTensorType::get(dataType, smType);
            } else {
                VPUX_THROW_UNLESS(distributedTypes.size() == 1, "Expected one distributed tensor for output, got {0}",
                                  distributedTypes.size());
                auto compactDataType = distributedTypes[0].cast<VPU::DistributedTensorType>().getCompactType();
                auto dataType =
                        compactDataType.cast<vpux::NDTypeInterface>().cast<vpux::NDTypeInterface>().changeMemSpace(
                                VPU::MemoryKind::DDR);
                concatType = dataType;
            }
        }
    }

    return concatType;
}

void replaceOrigUses(mlir::OpBuilder& builder, mlir::Value origOutputValue, VPU::ConcatOp concatOp,
                     bool isMulticlustering, mlir::Location loc) {
    SmallVector<mlir::Value> ddrValues;
    SmallVector<mlir::Value> cmxValues;
    findOutputValuesByMemoryKind(origOutputValue, ddrValues, cmxValues);

    if (!cmxValues.empty()) {
        const auto memSpaceCMX = cmxValues.front().getType().cast<vpux::NDTypeInterface>().getMemSpace();
        auto valueType = concatOp.output().getType().cast<vpux::NDTypeInterface>().changeMemSpace(memSpaceCMX);
        auto outputCopy = createCopyOp(builder, concatOp.output(), valueType, isMulticlustering, loc);
        outputCopy.setType(cmxValues.front().getType());
        for (auto value : cmxValues) {
            value.replaceAllUsesWith(outputCopy);
        }
    }

    for (auto value : ddrValues) {
        value.replaceAllUsesWith(concatOp.output());
    }
}

// Removes the original Eltwise operations that are given as parameters, as well as the user copy operations that have
// no uses of their own
void eraseOrigOperations(ArrayRef<mlir::Operation*> toErase) {
    const auto findOutputCopyOpsWithoutUses = [](mlir::Value result) -> SmallVector<mlir::Operation*> {
        SmallVector<mlir::Operation*> outputCopyOps;
        for (auto userOp : result.getUsers()) {
            if (auto clusterTilingOp = mlir::dyn_cast_or_null<VPU::NCEClusterTilingOp>(userOp)) {
                if (clusterTilingOp.getInnerTaskOpOfType<VPU::CopyOp>() != nullptr) {
                    if (clusterTilingOp->getResult(0).use_empty()) {
                        outputCopyOps.push_back(clusterTilingOp);
                    }
                }
            } else if (auto copyOp = mlir::dyn_cast<VPU::CopyOp>(userOp)) {
                if (copyOp.output().use_empty()) {
                    outputCopyOps.push_back(copyOp);
                }
            }
        }
        return outputCopyOps;
    };

    for (auto op : toErase) {
        auto outputCopyOps = findOutputCopyOpsWithoutUses(op->getResult(0));
        for (auto copyOp : outputCopyOps) {
            copyOp->erase();
        }
        op->erase();
    }
}

//
// CorrectNCEWorkloads
//

class ResolveEltwiseWithZTiledWorkloads final :
        public ResolveEltwiseWithZTiledWorkloadsBase<ResolveEltwiseWithZTiledWorkloads> {
public:
    explicit ResolveEltwiseWithZTiledWorkloads(Logger log): _log(log) {
        _log.setName(Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;

private:
    Logger _log;
};

void ResolveEltwiseWithZTiledWorkloads::safeRunOnFunc() {
    auto func = getOperation();
    auto module = func->getParentOfType<mlir::ModuleOp>();

    auto nceCluster = IE::getAvailableExecutor(module, VPU::ExecutorKind::NCE);
    VPUX_THROW_UNLESS(nceCluster != nullptr, "Failed to get NCE_Cluster information");
    const auto numClusters = nceCluster.count();

    SmallVector<mlir::Operation*> toErase;

    func.walk([&](VPU::NCEEltwiseOp eltwiseOp) {
        auto clusterWorkloads = separateWorkloadsByClusters(eltwiseOp, numClusters);
        if (clusterWorkloads.empty()) {
            return;
        }

        auto firstClusterWorkloads = clusterWorkloads.front();
        const auto variantsSplitOverC =
                std::adjacent_find(firstClusterWorkloads.begin(), firstClusterWorkloads.end(),
                                   [](VPU::DPUWorkloadOp lhs, VPU::DPUWorkloadOp rhs) {
                                       const auto lhsOffsets = parseIntArrayAttr<int64_t>(lhs.outOffsets());
                                       const auto rhsOffsets = parseIntArrayAttr<int64_t>(rhs.outOffsets());
                                       return lhsOffsets[Dims4D::Act::C.ind()] != rhsOffsets[Dims4D::Act::C.ind()];
                                   }) != firstClusterWorkloads.end();
        if (!variantsSplitOverC) {
            return;
        }

        _log.trace("Handling NCEEltwise operation at '{0}'", eltwiseOp->getLoc());

        validateClusterWorkloads(clusterWorkloads);
        sortClusterWorkloads(clusterWorkloads);
        auto workloadSplits = getWorkloadSplits(clusterWorkloads);

        mlir::OpBuilder builder(eltwiseOp);
        auto clusterTilingOp = eltwiseOp->getParentOfType<VPU::NCEClusterTilingOp>();
        const auto isMulticlustering = clusterTilingOp != nullptr;
        if (isMulticlustering) {
            builder.setInsertionPoint(clusterTilingOp);
        }

        mlir::Value input1 = nullptr;
        mlir::Value input2 = nullptr;
        mlir::Value output = nullptr;
        bool spilledInputs = false;
        getOpValues(builder, eltwiseOp, input1, input2, output, spilledInputs);
        if (spilledInputs) {
            _log.nest().trace("Inputs were spilled to DDR to avoid distributed CMX2CMX transfers");
        }

        SmallVector<mlir::Value> newOutputs;
        SmallVector<Shape> outputSlicesOffsets;
        for (auto& workloadSplit : workloadSplits) {
            _log.nest().trace("Creating operation split from workloads:");
            for (auto wl : workloadSplit) {
                _log.nest(2).trace("{0}", wl);
            }

            auto splitInformation = extractSplitInformation(workloadSplit);
            auto offsets = splitInformation.first;
            auto sizes = splitInformation.second;

            const auto sliceLoc = appendLoc(eltwiseOp->getLoc(), "slice {0} - {1}", offsets, sizes);

            auto eltwiseSlice = sliceEltwise(builder, eltwiseOp, mlir::ValueRange{input1, input2}, offsets, sizes,
                                             workloadSplit, isMulticlustering, spilledInputs, sliceLoc);

            newOutputs.push_back(eltwiseSlice);
            outputSlicesOffsets.push_back(offsets);
        }

        auto origOutputValue = isMulticlustering ? clusterTilingOp->getResult(0) : eltwiseOp.output();
        auto concatType = getConcatType(origOutputValue);
        auto concatOp = builder.create<VPU::ConcatOp>(eltwiseOp->getLoc(), concatType, mlir::ValueRange(newOutputs),
                                                      makeArrayRef(outputSlicesOffsets));

        auto loc = isMulticlustering ? clusterTilingOp->getLoc() : eltwiseOp->getLoc();
        replaceOrigUses(builder, origOutputValue, concatOp, isMulticlustering, loc);

        if (isMulticlustering) {
            toErase.push_back(clusterTilingOp);
        } else {
            toErase.push_back(eltwiseOp);
        }
    });

    eraseOrigOperations(toErase);
}

}  // namespace

//
// ResolveEltwiseWithZTiledWorkloads
//

std::unique_ptr<mlir::Pass> vpux::VPU::createResolveEltwiseWithZTiledWorkloadsPass(Logger log) {
    return std::make_unique<ResolveEltwiseWithZTiledWorkloads>(log);
}
