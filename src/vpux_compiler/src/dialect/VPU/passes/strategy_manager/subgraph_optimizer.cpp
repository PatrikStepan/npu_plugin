//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPU/subgraph_optimizer.hpp"
#include "vpux/compiler/dialect/VPU/strategy_manager.hpp"
#include "vpux/compiler/dialect/VPU/utils/const_utils.hpp"
#include "vpux/compiler/utils/strings.hpp"

#include <llvm/ADT/TypeSwitch.h>

using namespace vpux;
using namespace VPU;

SubgraphOptimizer::SubgraphOptimizer(mlir::func::FuncOp func, Logger log)
        : _func(func), _log(log), _layerCostModel(LayerCostModel(func, log)) {
}

VPU::MultiClusterStrategy SubgraphOptimizer::getRollbackStrategy(VPU::ClusteredOpInterface clusteredOp) {
    auto it = layersWithRollbackStrategy.find(clusteredOp);
    if (it != layersWithRollbackStrategy.end()) {
        return it->second;
    }

    return _layerCostModel.getMultiClusterStrategyValue(clusteredOp);
}

bool SubgraphOptimizer::isStrategySOKLike(VPU::ClusteredOpInterface op) {
    auto strategy = getRollbackStrategy(op);

    if (strategy == VPU::MultiClusterStrategy::SplitOverKernel || strategy == VPU::MultiClusterStrategy::Clustering ||
        strategy == VPU::MultiClusterStrategy::HKSwitch) {
        return true;
    }
    return false;
}

bool SubgraphOptimizer::isStrategySOHLike(VPU::ClusteredOpInterface op) {
    auto strategy = getRollbackStrategy(op);

    if (strategy == VPU::MultiClusterStrategy::SplitOverHeight ||
        strategy == VPU::MultiClusterStrategy::SplitOverHeightOverlapped) {
        return true;
    }
    return false;
}

bool SubgraphOptimizer::isValidStrategy(VPU::ClusteredOpInterface clusteredOp, VPU::MultiClusterStrategy strategy) {
    if (!clusteredOp.checkStrategyCompatibility(strategy)) {
        return false;
    }

    auto layerStrategyChecker = LayerStrategyCheckerFactory::instance().get(clusteredOp->getName());
    auto isCompatibleStrategy = [&](VPU::ClusteredOpInterface op, VPU::MultiClusterStrategy targetStrategy) {
        const auto arch = VPU::getArch(clusteredOp);
        const auto isChannelMajor = (DimsOrder::fromValue(clusteredOp->getOperand(0)) == DimsOrder::NCHW) &&
                                    VPU::NCEInvariant::isChannelMajorCompatible(
                                            arch, clusteredOp->getOperand(0).getType().cast<vpux::NDTypeInterface>());
        const auto isCompressConv = mlir::isa<vpux::VPU::NCECompressConvolutionOp>(clusteredOp);
        auto isCompatible = false;
        switch (targetStrategy) {
        case MultiClusterStrategy::SplitOverHeightOverlapped:
            isCompatible = (isChannelMajor || isCompressConv) &&
                           layerStrategyChecker->isOperationSplitOverHeightCompatible(op);
            break;
        case MultiClusterStrategy::SplitOverHeight:
            isCompatible = !isChannelMajor && !isCompressConv &&
                           layerStrategyChecker->isOperationSplitOverHeightCompatible(op);
            break;
        case MultiClusterStrategy::SplitOverKernel:
            isCompatible = layerStrategyChecker->isOperationSplitOverKernelCompatible(op);
            break;
        case MultiClusterStrategy::HKSwitch:
            isCompatible = layerStrategyChecker->isOperationSplitOverHeightCompatible(op) &&
                           VPU::doesLayerFitIntoCMX(op, MultiClusterStrategy::HKSwitch, Byte(0), _log);
            break;
        case MultiClusterStrategy::Clustering:
            isCompatible = true;
            break;
        default:
            VPUX_THROW("Unsupported strategy {0} for check nce op compatibility", targetStrategy);
        }
        return isCompatible;
    };

    return isCompatibleStrategy(clusteredOp, strategy);
}

/// @brief Return SOK-like strategie with least cost
/// @details SOK-like strategy may still have input/output spilling in some cases,
/// So we need to consider spilling costs when rollback
VPU::MultiClusterStrategy SubgraphOptimizer::getBestInSOKLikeStrategies(VPU::ClusteredOpInterface clusteredOp) {
    double HKCost = _layerCostModel.COST_MAX;

    if (isValidStrategy(clusteredOp, VPU::MultiClusterStrategy::HKSwitch)) {
        HKCost = _layerCostModel.getLayerCost(clusteredOp, VPU::MultiClusterStrategy::HKSwitch);
        _log.trace("HKSwitch has compute cost {0}", HKCost);
        auto spillingCost = getInputSpillingCostToMultiClusterLayer(clusteredOp, VPU::MultiClusterStrategy::HKSwitch,
                                                                    _configForFindingRollbackStrategy) +
                            getOutputSpillingCostToMultiClusterLayer(clusteredOp, VPU::MultiClusterStrategy::HKSwitch,
                                                                     _configForFindingRollbackStrategy);
        HKCost += spillingCost;
        _log.trace("HKSwitch has spilling cost {0}", spillingCost);
    }

    double SOKCost = _layerCostModel.COST_MAX;
    if (isValidStrategy(clusteredOp, VPU::MultiClusterStrategy::SplitOverKernel)) {
        SOKCost = _layerCostModel.getLayerCost(clusteredOp, VPU::MultiClusterStrategy::SplitOverKernel);
        _log.trace("SplitOverKernel has compute cost {0}", SOKCost);
        auto spillingCost =
                getInputSpillingCostToMultiClusterLayer(clusteredOp, VPU::MultiClusterStrategy::SplitOverKernel,
                                                        _configForFindingRollbackStrategy) +
                getOutputSpillingCostToMultiClusterLayer(clusteredOp, VPU::MultiClusterStrategy::SplitOverKernel,
                                                         _configForFindingRollbackStrategy);
        SOKCost += spillingCost;
        _log.trace("SplitOverKernel has spilling cost {0}", spillingCost);
    }

    double clusteringCost = _layerCostModel.COST_MAX;
    if (isValidStrategy(clusteredOp, VPU::MultiClusterStrategy::Clustering)) {
        clusteringCost = _layerCostModel.getLayerCost(clusteredOp, VPU::MultiClusterStrategy::Clustering);
        _log.trace("Clustering has compute cost {0}", clusteringCost);
        auto spillingCost = getInputSpillingCostToMultiClusterLayer(clusteredOp, VPU::MultiClusterStrategy::Clustering,
                                                                    _configForFindingRollbackStrategy) +
                            getOutputSpillingCostToMultiClusterLayer(clusteredOp, VPU::MultiClusterStrategy::Clustering,
                                                                     _configForFindingRollbackStrategy);
        clusteringCost += spillingCost;
        _log.trace("Clustering has spilling cost {0}", spillingCost);
    }

    if ((HKCost < SOKCost) && (HKCost <= clusteringCost)) {
        _log.trace("HKSwitch is selected with cost {0}", HKCost);
        return VPU::MultiClusterStrategy::HKSwitch;
    }

    // Sometimes Clustering strategy can avoid spilling, which makes it has less overall cost than SOK
    // But spilling could be overlapped with DPU Task during scheduling, then actually SOK gives better performance.
    // Meanwhile inaccurate cost calcution may mislead the strategy selection. We can compare the cost between SOK and
    // Clustering again after intergrating VPUNN.
    if (SOKCost != _layerCostModel.COST_MAX) {
        _log.trace("SplitOverKernel is selected with cost {0}", SOKCost);
        return VPU::MultiClusterStrategy::SplitOverKernel;
    }
    if (clusteringCost != _layerCostModel.COST_MAX) {
        _log.trace("Clustering is selected with cost {0}", clusteringCost);
        return VPU::MultiClusterStrategy::Clustering;
    }

    // Here means no SOK-like strategy available
    // return original strategy as result
    return _layerCostModel.getMultiClusterStrategyValue(clusteredOp);
}

/// @brief Return SOH-like strategie with least cost
/// @details SOH-like strategy may still have input/output spilling in some cases,
/// So we need to consider spilling costs when rollback
/// Currently we only consider SplitOverHeight strategy, but we still calculate the cost for debugging purpose
VPU::MultiClusterStrategy SubgraphOptimizer::getBestInSOHLikeStrategies(VPU::ClusteredOpInterface clusteredOp) {
    double SOHCost = _layerCostModel.COST_MAX;
    if (isValidStrategy(clusteredOp, VPU::MultiClusterStrategy::SplitOverHeight)) {
        SOHCost = _layerCostModel.getLayerCost(clusteredOp, VPU::MultiClusterStrategy::SplitOverHeight);
        _log.trace("SplitOverHeight has compute cost {0}", SOHCost);
        auto spillingCost =
                getInputSpillingCostToMultiClusterLayer(clusteredOp, VPU::MultiClusterStrategy::SplitOverHeight,
                                                        _configForFindingRollbackStrategy) +
                getOutputSpillingCostToMultiClusterLayer(clusteredOp, VPU::MultiClusterStrategy::SplitOverHeight,
                                                         _configForFindingRollbackStrategy);
        SOHCost += spillingCost;
        _log.trace("SplitOverHeight has spilling cost {0}", spillingCost);
    }

    if (SOHCost != _layerCostModel.COST_MAX) {
        _log.trace("SplitOverHeight is selected with cost {0}", SOHCost);
        return VPU::MultiClusterStrategy::SplitOverHeight;
    }

    // Here means no SOH-like strategy available
    // return original strategy as result
    return _layerCostModel.getMultiClusterStrategyValue(clusteredOp);
}

/// @brief Get the input spilling cost (cycles)
/// @details This function calculates input spilling cost to all parents which have multiCluster strategy. If we
/// calculate rollback spilling cost, we need to make sure the neighboring layer is using rollback strategy as well if
/// it has one
double SubgraphOptimizer::getInputSpillingCostToMultiClusterLayer(VPU::ClusteredOpInterface clusteredOp,
                                                                  VPU::MultiClusterStrategy strategy,
                                                                  SubgraphOptConfig config) {
    return llvm::TypeSwitch<mlir::Operation*, double>(clusteredOp.getOperation())
            .Case<NCEMaxPoolOp>([&](NCEMaxPoolOp origOp) {
                return getInputSpillingCostToMultiClusterLayer(origOp, origOp.input(), strategy, config);
            })
            .Case<NCEAveragePoolOp>([&](NCEAveragePoolOp origOp) {
                return getInputSpillingCostToMultiClusterLayer(origOp, origOp.input(), strategy, config);
            })
            .Case<NCEEltwiseOp>([&](NCEEltwiseOp origOp) {
                return getInputSpillingCostToMultiClusterLayer(origOp, origOp.input1(), strategy, config) +
                       getInputSpillingCostToMultiClusterLayer(origOp, origOp.input2(), strategy, config);
            })
            .Case<NCEConvolutionOp>([&](NCEConvolutionOp origOp) {
                return getInputSpillingCostToMultiClusterLayer(origOp, origOp.input(), strategy, config);
            })
            .Case<NCECompressConvolutionOp>([&](NCECompressConvolutionOp origOp) {
                return getInputSpillingCostToMultiClusterLayer(origOp, origOp.input(), strategy, config);
            })
            .Case<NCEDepthConvolutionOp>([&](NCEDepthConvolutionOp origOp) {
                return getInputSpillingCostToMultiClusterLayer(origOp, origOp.input(), strategy, config);
            })
            .Case<NCEInterpolateOp>([&](NCEInterpolateOp origOp) {
                return getInputSpillingCostToMultiClusterLayer(origOp, origOp.input(), strategy, config);
            })
            .Case<SWOpInterface>([&](SWOpInterface swOp) {
                return getInputSpillingCostToMultiClusterLayer(swOp.getOperation(), swOp->getOperand(0), strategy,
                                                               config);
            })
            .Case<ConcatOp>([&](ConcatOp origOp) {
                double inputSpillingCost = 0.0;
                for (auto concatInput : origOp.inputs()) {
                    inputSpillingCost += getInputSpillingCostToMultiClusterLayer(origOp, concatInput, strategy, config);
                }
                return inputSpillingCost;
            })
            .Default([&](mlir::Operation* origOp) {
                VPUX_THROW("Roll back strategy for op {0} at {1} is not supported", origOp->getName(),
                           origOp->getLoc());
                return 0.0;
            });
}

/// @brief Get the input spilling cost for specified input operand
double SubgraphOptimizer::getInputSpillingCostToMultiClusterLayer(VPU::ClusteredOpInterface clusteredOp,
                                                                  mlir::Value input, VPU::MultiClusterStrategy strategy,
                                                                  SubgraphOptConfig config) {
    if (!hasInputSpillingToMultiClusterLayer(clusteredOp, input, strategy, config)) {
        return 0.0;
    }

    auto targetTensorType = _layerCostModel.getDistributedInputType(clusteredOp, input.getDefiningOp(), strategy);

    auto parent = input.getDefiningOp();
    if (parent == nullptr) {
        return _layerCostModel.getSpillingReadCost(targetTensorType);
    }

    if (mlir::isa<VPU::ShapeCastOp>(parent)) {
        // propagate ShapeCast
        parent = parent->getOperand(0).getDefiningOp();
        if (parent == nullptr) {
            return _layerCostModel.getSpillingReadCost(targetTensorType);
        }
    }

    return llvm::TypeSwitch<mlir::Operation*, double>(parent)
            .Case<VPU::ClusteredOpInterface>([&](VPU::ClusteredOpInterface parentOp) {
                if (!_layerCostModel.hasMultiClusterStrategy(parentOp))
                    return 0.0;

                VPU::MultiClusterStrategy parentStrategy =
                        config.useRollbackStrategy ? getRollbackStrategy(parentOp)
                                                   : _layerCostModel.getMultiClusterStrategyValue(parentOp);
                auto currentSpillingCost =
                        _layerCostModel.calculateSpillingCost(parentOp, clusteredOp, parentStrategy, strategy);
                return currentSpillingCost.writeCost + currentSpillingCost.readCost;
            })
            .Default([&](mlir::Operation*) {
                return _layerCostModel.getSpillingReadCost(targetTensorType);
            });
}

/// @brief Get the output spilling cost (cycles)
/// @details This function calculates output spilling cost to all users which have multiCluster strategy. When we
/// calculate rollback spilling cost, we need to make sure the neighboring layer is using rollback strategy as well if
/// it has one
double SubgraphOptimizer::getOutputSpillingCostToMultiClusterLayer(VPU::ClusteredOpInterface clusteredOp,
                                                                   VPU::MultiClusterStrategy strategy,
                                                                   SubgraphOptConfig config) {
    bool hasCalculatedSpillingWriteCost = false;
    double totalSpillingCost = 0.0;
    for (auto userOp : clusteredOp->getResult(0).getUsers()) {
        if (mlir::isa_and_nonnull<VPU::QuantizeCastOp>(userOp) ||
            (mlir::isa_and_nonnull<VPU::ShapeCastOp>(userOp) && userOp->hasOneUse())) {
            // propagate cast ops
            userOp = *userOp->getResult(0).getUsers().begin();
        }

        if (userOp == nullptr || !_layerCostModel.hasMultiClusterStrategy(userOp)) {
            continue;
        }

        if (hasOutputSpillingToMultiClusterLayer(clusteredOp, userOp, strategy, config)) {
            auto userClusteredOp = mlir::dyn_cast<VPU::ClusteredOpInterface>(userOp);
            auto userStrategy = config.useRollbackStrategy
                                        ? getRollbackStrategy(userClusteredOp)
                                        : _layerCostModel.getMultiClusterStrategyValue(userClusteredOp);
            auto spillingCost =
                    _layerCostModel.calculateSpillingCost(clusteredOp, userClusteredOp, strategy, userStrategy);
            if (!hasCalculatedSpillingWriteCost) {
                totalSpillingCost += spillingCost.writeCost;
                hasCalculatedSpillingWriteCost = true;
            }
            totalSpillingCost += spillingCost.readCost;
        }
    }

    return totalSpillingCost;
}

/// @brief Check if there's spilling around a concat and ignore the excluded op
/// @details For concat, there are several scenarios which will cause spilling
/// 1. if any of input layer has incompatible strategy to concat
/// 2. if the concat has incompatible strategy to any user
/// 3. if any of input layer is non nce task. It's because currently we only enable cmx concat with nce parents
/// 4. if 2 or more inputs of concat come from the same parent layer
bool SubgraphOptimizer::hasSpillingAroundConcat(VPU::ClusteredOpInterface clusteredOp,
                                                VPU::MultiClusterStrategy clusteredStrategy,
                                                mlir::Operation* excludedOp) {
    if (auto concatOp = mlir::dyn_cast<VPU::ConcatOp>(clusteredOp.getOperation())) {
        SmallVector<mlir::Operation*> concatParents;
        for (auto concatInput : concatOp.inputs()) {
            auto currentParent = concatInput.getDefiningOp();

            if ((currentParent == nullptr) || (!_layerCostModel.hasMultiClusterStrategy(currentParent)) ||
                (!mlir::isa<VPU::NCEOpInterface>(currentParent)) ||
                (llvm::find(concatParents, currentParent) != concatParents.end())) {
                return true;
            }

            concatParents.push_back(currentParent);

            if (currentParent != excludedOp) {
                auto currentSrcTensorType =
                        _layerCostModel.getDistributedOutputType(currentParent, getRollbackStrategy(currentParent));
                auto currentDstTensorType =
                        _layerCostModel.getDistributedInputType(clusteredOp, currentParent, clusteredStrategy);
                if (_layerCostModel.hasSpilling(currentParent, currentSrcTensorType, currentDstTensorType)) {
                    return true;
                }
            }
        }

        for (auto concatUser : concatOp->getUsers()) {
            auto currentSrcTensorType = _layerCostModel.getDistributedOutputType(clusteredOp, clusteredStrategy);
            if (auto concatUserClusteredOp = mlir::dyn_cast<ClusteredOpInterface>(concatUser)) {
                auto currentDstTensorType = _layerCostModel.getDistributedInputType(
                        concatUserClusteredOp, concatOp.getOperation(), getRollbackStrategy(concatUserClusteredOp));
                if (_layerCostModel.hasSpilling(clusteredOp, currentSrcTensorType, currentDstTensorType)) {
                    return true;
                }
            } else {
                return true;
            }
        }
    }

    return false;
}

/// @brief Check if there's spilling caused by strided cmx concat
/// @details If the layer A has multiple users and one of them could be strided cmx concat. Then we have 2 options for
/// making spilling
/// 1. We enable strided cmx concat and then concat's output will propogate its stride to the output of A, which makes
/// spilling to other users of A requiring a non strided distributed tensor
/// 2. We use strided ddr concat and then A could avoid spilling to other users. But we got strided DMA from strided ddr
/// concat Here we prefer option 1 because strided DMA is usually time consuming. But in the future if we have cost
/// model for strided DMA, we can make the decision by comparing cost of option 1 vs 2
bool SubgraphOptimizer::hasSpillingCausedByStridedCMXConcat(VPU::ClusteredOpInterface clusteredOp,
                                                            VPU::MultiClusterStrategy clusteredStrategy,
                                                            SubgraphOptConfig config) {
    // check strided Concat
    const auto isStridedConcat = [&](VPU::ConcatOp op) -> bool {
        auto outputType = op.output().getType().dyn_cast<vpux::NDTypeInterface>();
        auto outputShape = outputType.getShape();
        auto input1DataType = op.inputs().front().getType().cast<NDTypeInterface>();
        auto input1Shape = input1DataType.getShape();
        auto dimOrder = input1DataType.getDimsOrder();
        int64_t shapeSize = 1;

        for (size_t idx = 0; idx < outputShape.size(); idx++) {
            if ((outputShape[dimOrder.dimAt(idx)] != input1Shape[dimOrder.dimAt(idx)]) && (shapeSize != 1)) {
                return true;
            }

            shapeSize *= outputShape[dimOrder.dimAt(idx)];
        }

        return false;
    };

    if (!clusteredOp->hasOneUse()) {
        auto users = clusteredOp->getUsers();
        for (auto currentUser : users) {
            if (auto concatOp = mlir::dyn_cast<VPU::ConcatOp>(currentUser)) {
                auto concatClusteredOp = mlir::dyn_cast<VPU::ClusteredOpInterface>(currentUser);
                auto concatStrategy = config.useRollbackStrategy
                                              ? getRollbackStrategy(concatClusteredOp)
                                              : _layerCostModel.getMultiClusterStrategyValue(concatClusteredOp);
                if (isStridedConcat(concatOp) &&
                    !hasSpillingAroundConcat(concatClusteredOp, concatStrategy, clusteredOp) &&
                    !_layerCostModel.hasSpilling(clusteredOp, clusteredStrategy, concatClusteredOp, concatStrategy)) {
                    return true;
                }
            }
        }
    }

    return false;
}

bool SubgraphOptimizer::hasSpillingRelatedToConcat(VPU::ClusteredOpInterface parentClusteredOp,
                                                   VPU::MultiClusterStrategy parentStrategy,
                                                   VPU::ClusteredOpInterface userClusteredOp,
                                                   VPU::MultiClusterStrategy userStrategy, SubgraphOptConfig config) {
    if (mlir::isa<VPU::ConcatOp>(userClusteredOp.getOperation())) {
        // If the user is concat then exclude the parent op from the spilling check of concat. It's because we need to
        // use specified strategy to check if there's spilling from this parent to concat. And it will be checked out of
        // this function.
        if (hasSpillingAroundConcat(userClusteredOp, userStrategy, parentClusteredOp)) {
            return true;
        }
    } else if (hasSpillingCausedByStridedCMXConcat(parentClusteredOp, parentStrategy, config)) {
        return true;
    }

    // If the parent is concat then exclude the child op from the spilling check of concat. It's because we need to use
    // specified strategy to check if there's spilling from concat to this child. And it will be checked out of this
    // function.
    if (mlir::isa<VPU::ConcatOp>(parentClusteredOp.getOperation()) &&
        hasSpillingAroundConcat(parentClusteredOp, parentStrategy, userClusteredOp)) {
        return true;
    }

    return false;
}

/// @brief This one is to find the ResBlock structures in full model and record the startpoint, endpoint and middle ops
/// in ResBlock for next long-term spilling check in Subgraph optimizer.
/// @details The method starts from an eltwise or concat op and find its parent op which has multiple users,
/// then starts depth first search (DFS) from this parent op and try to find the starting eltwise or concat op in its
/// descendant.
/// @todo Generalize ResBlock structure to allow shortcut branch also has middle ops,
/// to make long-term spilling check available for more general case. Refer to E#70928.
SubgraphOptimizer::ShortcutMapTy SubgraphOptimizer::detectShortcuts() {
    const auto detectFunc = [this](mlir::Operation* origOp) {
        _log.trace("Detect above shortcut from op: {0}", origOp->getLoc());

        SmallVector<mlir::Value> layerInputs;
        if (auto eltwiseOp = mlir::dyn_cast<VPU::NCEEltwiseOp>(origOp)) {
            // exclude quantization eltwise_add
            if (eltwiseOp.input1() == eltwiseOp.input2()) {
                _log.trace(" Exclude op {0} with two same inputs", eltwiseOp.getLoc());
                return;
            }

            layerInputs.push_back(eltwiseOp.input1());
            layerInputs.push_back(eltwiseOp.input2());
        } else if (auto concatOp = mlir::dyn_cast<VPU::ConcatOp>(origOp)) {
            SmallVector<mlir::Operation*> concatParents;
            for (auto concatInput : concatOp.inputs()) {
                auto currentParent = concatInput.getDefiningOp();

                if (llvm::find(concatParents, currentParent) != concatParents.end()) {
                    _log.trace(" Exclude op {0} with two same inputs", concatOp.getLoc());
                    return;
                }

                concatParents.push_back(currentParent);
            }

            layerInputs = concatOp.inputs();
        } else {
            return;
        }

        for (auto input : layerInputs) {
            if (input.isa<mlir::BlockArgument>()) {
                continue;
            }
            auto parent = input.getDefiningOp();
            // propagate cast ops
            // TODO: support to propagate more cast/viewLike ops in Strategy Manager, refer to E#65795
            if (mlir::isa_and_nonnull<VPU::ShapeCastOp, VPU::QuantizeCastOp>(parent)) {
                if (parent->getOperand(0).isa<mlir::BlockArgument>()) {
                    continue;
                }
                parent = parent->getOperand(0).getDefiningOp();
            }

            if ((parent == nullptr) || (!_layerCostModel.hasMultiClusterStrategy(parent))) {
                continue;
            }
            if (parent->hasOneUse()) {
                continue;
            }

            _log.trace(" Found multi-user parent for detect point: {0}", origOp->getLoc());
            bool hasShortcut = false;
            auto depthCount = 0;
            const auto maxDepth = 100;  // dfs will go back when search depth is bigger than maxDepth
            std::unordered_set<mlir::Operation*> visitedOps;
            // DFS algorithm to find shortcut structure
            std::function<void(mlir::Operation*)> dfs = [&](mlir::Operation* op) {
                if (op == nullptr) {
                    return;
                }
                ++depthCount;
                visitedOps.insert(op);
                _log.trace("  DFS enter op: {0} , depth {1}", op->getLoc(), depthCount);
                if (depthCount > maxDepth) {
                    --depthCount;
                    return;
                }
                if (op == origOp) {
                    hasShortcut = true;
                    --depthCount;
                    return;
                }
                auto users = op->getUsers();
                if (users.empty()) {
                    --depthCount;
                    return;
                }

                _log.trace("   Continue dfs its users");
                for (auto user : users) {
                    if (visitedOps.count(user) == 1) {
                        _log.trace("    DFS skip visited op: {0}", user->getLoc());
                        continue;
                    }
                    dfs(user);
                    if (hasShortcut) {
                        if (_shortcutsMap.find(origOp) != _shortcutsMap.end()) {
                            auto& path = _shortcutsMap.at(origOp);
                            path.second.push_back(op);
                        } else {
                            _shortcutsMap.insert({origOp, {parent, {op}}});
                        }
                        --depthCount;
                        return;
                    }
                }
                --depthCount;
            };

            auto parentUsers = parent->getUsers();
            for (auto user : parentUsers) {
                if (user != origOp) {
                    dfs(user);
                    VPUX_THROW_UNLESS(depthCount == 0, "Depth count is not zero after finish depth first search");
                }
            }
        }
    };

    _func.walk(detectFunc);
    return _shortcutsMap;
}

/// @brief Long-term spilling catches the case that data spilled to DDR but not read back immediately utill it's needed
/// by its user. This situation is usually triggered in ResBlock structure.
bool SubgraphOptimizer::hasLongTermSpilling(VPU::ClusteredOpInterface origOp, VPU::ClusteredOpInterface parentOp,
                                            VPU::MultiClusterStrategy parentOpStrategy, SubgraphOptConfig config) {
    auto user = origOp.getOperation();
    auto parent = parentOp.getOperation();
    if (_shortcutsMap.find(user) == _shortcutsMap.end() || _shortcutsMap.at(user).first != parent ||
        _shortcutsMap.at(user).second.size() <= 1) {
        return false;
    }

    auto reservedMem = _layerCostModel.getDistributedOutputType(parentOp, parentOpStrategy)
                               .cast<NDTypeInterface>()
                               .getTotalAllocSize();
    SmallVector<Byte> buffersSize;
    buffersSize.push_back(reservedMem);
    reservedMem = VPU::calculateAlignedBuffersMemoryRequirement(getArch(parentOp), buffersSize);
    auto middleOpList = _shortcutsMap.at(user).second;
    SmallVector<mlir::Operation*> middleOps{&middleOpList[0], &middleOpList[middleOpList.size() - 1]};

    auto swOpFitsInCMX = [&](VPU::SWOpInterface softwareOp) {
        // single-cluster op
        SmallVector<vpux::NDTypeInterface> operandTypes;
        for (const auto& operand : softwareOp->getOperands()) {
            operandTypes.push_back(operand.getType().cast<vpux::NDTypeInterface>());
        }

        VPUX_THROW_UNLESS(softwareOp->getResults().size() == 1, "Only support SW with one output, but got '{0}'",
                          softwareOp->getResults().size());
        operandTypes.push_back(softwareOp->getResult(0).getType().cast<vpux::NDTypeInterface>());

        return softwareOp.fitIntoCMX(operandTypes, reservedMem);
    };

    auto doesMiddleOpFitCMX = [&](mlir::Operation* op) {
        return llvm::TypeSwitch<mlir::Operation*, bool>(op)
                .Case<VPU::ClusteredOpInterface>([&](VPU::ClusteredOpInterface clusteredOp) {
                    if (!_layerCostModel.hasMultiClusterStrategy(clusteredOp.getOperation())) {
                        if (auto permuteQuantize =
                                    mlir::dyn_cast<VPU::NCEPermuteQuantizeOp>(clusteredOp.getOperation())) {
                            // single-cluster NCEPermuteQuantizeOp
                            const auto inputType =
                                    permuteQuantize->getOperand(0).getType().cast<vpux::NDTypeInterface>();
                            const auto outputType =
                                    permuteQuantize->getResult(0).getType().cast<vpux::NDTypeInterface>();
                            return permuteQuantize.fitIntoCMX(inputType, outputType, reservedMem);
                        } else if (auto concat = mlir::dyn_cast<VPU::ConcatOp>(clusteredOp.getOperation())) {
                            const auto outputType = concat->getResult(0).getType().cast<vpux::NDTypeInterface>();
                            return concat.fitIntoCMX(outputType, reservedMem);
                        } else if (auto swOp = mlir::dyn_cast<VPU::SWOpInterface>(clusteredOp.getOperation())) {
                            return swOpFitsInCMX(swOp);
                        } else {
                            VPUX_THROW("Operation '{0}' at '{1}' has no MC strategy", clusteredOp->getName(),
                                       clusteredOp->getLoc());
                        }
                    }

                    auto layerStrategyChecker = LayerStrategyCheckerFactory::instance().get(clusteredOp->getName());
                    auto clusteredOpStrategy = config.useRollbackStrategy
                                                       ? getRollbackStrategy(clusteredOp)
                                                       : _layerCostModel.getMultiClusterStrategyValue(clusteredOp);
                    return layerStrategyChecker->doesLayerFitIntoCMX(clusteredOp, clusteredOpStrategy, reservedMem);
                })
                .Case<VPU::ViewLikeOpInterface>([&](VPU::ViewLikeOpInterface) {
                    // viewLike ops don't occupy memory so skip it
                    // TODO: check memory when those layers support MC strategy, refer to E#65795
                    return true;
                })
                .Case<VPU::SWOpInterface>(swOpFitsInCMX)
                .Default([&](mlir::Operation* unknownOp) {
                    _log.trace("Operation '{0}' at '{1}' is not supported when checking its CMX memory",
                               unknownOp->getName(), unknownOp->getLoc());
                    // For a unknown op, return true to keep it safe (avoid unnecessary optimization)
                    return true;
                });
    };

    auto hasSpilling = !llvm::all_of(middleOps, doesMiddleOpFitCMX);
    if (hasSpilling) {
        _log.trace(" Long term spilling {0} -> {1} happens", parent->getLoc(), user->getLoc());
    }
    return hasSpilling;
}

/// @brief Check if there's input spilling to any parent which has multiCluster strategy.
bool SubgraphOptimizer::hasInputSpillingToMultiClusterLayer(VPU::ClusteredOpInterface origClusteredOp,
                                                            VPU::MultiClusterStrategy origStrategy,
                                                            SubgraphOptConfig config) {
    SmallVector<mlir::Value> layerInputs;
    if (auto eltwiseOp = mlir::dyn_cast<VPU::NCEEltwiseOp>(origClusteredOp.getOperation())) {
        layerInputs.push_back(eltwiseOp.input1());
        layerInputs.push_back(eltwiseOp.input2());
    } else if (auto concatOp = mlir::dyn_cast<VPU::ConcatOp>(origClusteredOp.getOperation())) {
        layerInputs = concatOp.inputs();
    } else {
        layerInputs.push_back(origClusteredOp->getOperand(0));
    }

    for (auto input : layerInputs) {
        if (hasInputSpillingToMultiClusterLayer(origClusteredOp, input, origStrategy, config)) {
            return true;
        }
    }

    return false;
}

/// @brief Check if there's input spilling for a specified input operand
/// @details We make different behaviors about checking concat related spilling and it depends on the place of calling
/// this function. Please check the description of function 'hasOutputSpillingToMultiClusterLayer' for details.
bool SubgraphOptimizer::hasInputSpillingToMultiClusterLayer(VPU::ClusteredOpInterface origClusteredOp,
                                                            mlir::Value input, VPU::MultiClusterStrategy origStrategy,
                                                            SubgraphOptConfig config) {
    if (input.isa<mlir::BlockArgument>()) {
        return false;
    }

    auto parent = input.getDefiningOp();
    if (mlir::isa_and_nonnull<VPU::ShapeCastOp, VPU::QuantizeCastOp>(parent)) {
        if (parent->getOperand(0).isa<mlir::BlockArgument>()) {
            return false;
        }
        // propagate cast ops
        parent = parent->getOperand(0).getDefiningOp();
    }

    if ((parent == nullptr) || (!_layerCostModel.hasMultiClusterStrategy(parent))) {
        return false;
    }

    if (auto parentClusteredOp = mlir::dyn_cast<VPU::ClusteredOpInterface>(parent)) {
        auto parentStrategy = config.useRollbackStrategy
                                      ? getRollbackStrategy(parentClusteredOp)
                                      : _layerCostModel.getMultiClusterStrategyValue(parentClusteredOp);
        if (config.checkConcatRelatedSpilling &&
            hasSpillingRelatedToConcat(parentClusteredOp, parentStrategy, origClusteredOp, origStrategy, config)) {
            return true;
        }

        if (hasLongTermSpilling(origClusteredOp, parentClusteredOp, parentStrategy, config)) {
            return true;
        }

        if (_layerCostModel.hasSpilling(parentClusteredOp, parentStrategy, origClusteredOp, origStrategy)) {
            return true;
        }

        if (_layerCostModel.doesLayerRequireTiling(parentClusteredOp, parentStrategy) ||
            _layerCostModel.doesLayerRequireTiling(origClusteredOp, origStrategy)) {
            return true;
        }
    }

    return false;
}

/// @brief Check if there's output spilling to any user which has multiCluster strategy.
bool SubgraphOptimizer::hasOutputSpillingToMultiClusterLayer(VPU::ClusteredOpInterface origClusteredOp,
                                                             VPU::MultiClusterStrategy origStrategy,
                                                             SubgraphOptConfig config) {
    for (auto* userOp : origClusteredOp->getResult(0).getUsers()) {
        if (hasOutputSpillingToMultiClusterLayer(origClusteredOp, userOp, origStrategy, config)) {
            return true;
        }
    }

    return false;
}

/// @brief Check if there's output spilling for a specified user
/// @details We make different behaviors about checking concat related spilling and it depends on the place of calling
/// this function.
/// 1. If it's in the stage of finding a initial node for subgraph strategy optimization, we don't check concat related
/// spilling
/// 2. If it's in the stage of finding best rollback strategy, we don't check concat related spilling. It's because we
/// only look at neighbouring layers and it will make sub-optimal strategy if checking concat related spilling. For
/// example in the following graph, NCE1 will trigger subgraph optimiztion and the traverse order is NCE1 -> VPU.Concat
/// -> NCE2 -> NCE3 -> NCE4. When we look at NCE2, it will prefer HK because SOK can't avoid spilling from concat and
/// it's caused by NCE3 with SOH.
///     NCE1 (SOH)  NCE3 (SOH)  NCE4 (SOK)                  NCE1 (HK)  NCE3 (HK)  NCE4 (SOK)
///      |           |           |                           |          |          |
///                                          ROLLBACK TO
///              VPU.Concat (SOH)                                    VPU.Concat (SOK)
///                  |                                                   |
///                 NCE2 (SOH)                                          NCE2 (HK)
///
/// If we don't check concat related spilling, only NCE4 will trigger subgraph optimiztion and the rollback will be as
/// following
///     NCE1 (SOH)  NCE3 (SOH)  NCE4 (SOK)                  NCE1 (SOH)  NCE3 (SOH)  NCE4 (SOH)
///      |           |           |                           |           |           |
///                                          ROLLBACK TO
///              VPU.Concat (SOH)                                    VPU.Concat (SOH)
///                  |                                                   |
///                 NCE2 (SOH)                                          NCE2 (SOH)
/// 3. If it's in the stage of checking whether neighbouring layers need be added into rollback list, we check concat
/// related spilling because we want to gather concat surrounding layers to make compatible strategy and cmx concat.
/// 4. If it's in the stage of comparing original cost and rollback cost, we check concat related spilling because we
/// need accurate spilling cost of concat to make the right decision
bool SubgraphOptimizer::hasOutputSpillingToMultiClusterLayer(VPU::ClusteredOpInterface origClusteredOp,
                                                             mlir::Operation* userOp,
                                                             VPU::MultiClusterStrategy origStrategy,
                                                             SubgraphOptConfig config) {
    if (mlir::isa<VPU::QuantizeCastOp>(userOp) || (mlir::isa<VPU::ShapeCastOp>(userOp) && userOp->hasOneUse())) {
        // propagate cast ops
        userOp = *userOp->getResult(0).getUsers().begin();
    }

    if (mlir::isa<VPU::SWOpInterface>(userOp) && VPU::getArch(userOp) == VPU::ArchKind::VPUX30XX) {
        return false;
    }

    if (auto userClusteredOp = mlir::dyn_cast<VPU::ClusteredOpInterface>(userOp)) {
        auto userStrategy = config.useRollbackStrategy ? getRollbackStrategy(userOp)
                                                       : _layerCostModel.getMultiClusterStrategyValue(userOp);

        if (config.checkConcatRelatedSpilling &&
            hasSpillingRelatedToConcat(origClusteredOp, origStrategy, userClusteredOp, userStrategy, config)) {
            return true;
        }

        if (hasLongTermSpilling(userClusteredOp, origClusteredOp, origStrategy, config)) {
            return true;
        }

        if (_layerCostModel.hasSpilling(origClusteredOp, origStrategy, userClusteredOp, userStrategy)) {
            return true;
        }

        if (_layerCostModel.doesLayerRequireTiling(origClusteredOp, origStrategy) ||
            _layerCostModel.doesLayerRequireTiling(userClusteredOp, userStrategy)) {
            return true;
        }
    }

    return false;
}

/// @brief The idea of this algorithm is to avoid spilling by optimizing strategy
/// @details We execute following steps for each NCE task from input to output through a topological sort of the Op
/// Model.
/// 1. Check if a source NCE Task needs rollback strategy
/// 2. Find the best rollback strategy for the source task
/// 3. Check neighboring NCE Task. Add to queue if it needs rollback strategy.
/// 4. Execute step 2-3 for each task in queue until the queue is empty
/// @example List some typical scenarios
/// 1. {SOH -> SOH -> SOK} convert to {SOH -> HK -> SOK}
/// 2. {SOK -> SOK -> SOH} convert to {SOH -> SOH -> SOH}
/// 3. {SOH -> SOK -> SOH} convert to {SOH -> SOH -> SOH}
/// 4. {SOK -> SOH -> SOK} convert to {SOH -> HK -> SOK}
/// @todo This algorithm is a local strategy optimization for subgraphs. The final strategy maybe not global optimized.
/// For example, {SOK -> SOK -> SOH} maybe better converting to {SOK -> SOK -> SOK}
void SubgraphOptimizer::optimizeStrategyAvoidSpillingOnSubgraph(VPU::ClusteredOpInterface clusteredOp) {
    // There is no VPUNN cost for NCEPermuteQuantizeOp.
    // Even if NCEPermuteQuantizeOp cost can be mapped to NCEEltwiseOp,
    // But NCEPermuteQuantizeOp only support SOW that VPUNN cannot support.
    // #E80251 is to refactor NCEPermuteQuantize, after that this filter can be removed.
    if (mlir::isa<VPU::NCEPermuteQuantizeOp>(clusteredOp.getOperation())) {
        return;
    }

    if (!_layerCostModel.hasMultiClusterStrategy(clusteredOp) ||
        (!hasOutputSpillingToMultiClusterLayer(clusteredOp, _layerCostModel.getMultiClusterStrategyValue(clusteredOp),
                                               _configForFindingStartNode) &&
         !hasInputSpillingToMultiClusterLayer(clusteredOp, _layerCostModel.getMultiClusterStrategyValue(clusteredOp),
                                              _configForFindingStartNode))) {
        return;
    }

    // A layer will have spilling regardless of multi-cluster strategy if current layer needs be tiled because CMX
    // memory is not enough. The situation will change when we enable vertical fusion. There is no benefit for
    // performance to do subgraph optimization in such case because spilling can't be removed anyways.
    if (_layerCostModel.doesLayerRequireTiling(clusteredOp,
                                               _layerCostModel.getMultiClusterStrategyValue(clusteredOp))) {
        return;
    }

    _log.trace("Subgraph opt: beginning node '{0}'", clusteredOp->getLoc());

    layersNeedRollback.clear();
    layersWithRollbackStrategy.clear();
    layersWithConvergedStrategy.clear();
    opsWithSpillWriteCounted.clear();
    // If the source NCE task has SOK like strategy, we consider SOH rollback strategy because SOK usually has spilling
    // to SOH. And SOH - SOH can avoid spilling.
    // If the source NCE task has SOH like strategy, we consider SOK/Clustering/HKSwitch rollback strategy because it's
    // more possible to avoid spilling.
    bool rollbackToSOH = isStrategySOKLike(clusteredOp);

    // Processing SOH -> SOK/Clustering
    VPU::ClusteredOpInterface currentOp = clusteredOp;
    VPU::MultiClusterStrategy rollbackStrategy;
    layersNeedRollback.push_back(currentOp);
    while (!layersNeedRollback.empty()) {
        currentOp = layersNeedRollback.front();
        auto clusteredCurrentOp = mlir::dyn_cast<ClusteredOpInterface>(currentOp.getOperation());
        layersNeedRollback.erase(layersNeedRollback.begin());
        rollbackStrategy =
                rollbackToSOH ? getBestInSOHLikeStrategies(currentOp) : getBestInSOKLikeStrategies(currentOp);
        // Sometimes a layer triggers rollback several times in a subgraph because we allows rollback strategy be
        // changed during optimization. It makes sense when we find rollback strategy A first but later a better
        // rollback strategy B is found. However if B is equal to A, it means the rollback strategy is converged. We
        // need to stop the iteration in such case otherwise it could be infinite loop.
        if ((layersWithRollbackStrategy.find(currentOp) != layersWithRollbackStrategy.end()) &&
            (rollbackStrategy == layersWithRollbackStrategy[currentOp])) {
            _log.trace("  Layer '{0} has been processed twice with same strategy {1}", currentOp->getLoc(),
                       rollbackStrategy);
            layersWithConvergedStrategy.insert(currentOp);
        } else {
            layersWithRollbackStrategy[currentOp] = rollbackStrategy;
            _log.trace("  Layer '{0} has been processed, candidate strategy {1}", currentOp->getLoc(),
                       rollbackStrategy);
        }

        // A layer doesn't need rollback strategy, if
        // 1. it has no multi-cluster strategy
        // 2. it has SOK like strategy and we want SOK like rollback
        // 3. it has SOH like strategy and we want SOH like rollback
        // 4. it requires tiling
        // 5. it doesn't support cost calculation
        const auto doesLayerNeedRollbackStrategy = [&](mlir::Operation* targetOp) -> bool {
            if (auto swOp = mlir::dyn_cast<VPU::SWOpInterface>(targetOp)) {
                if (!swOp.supportCycleCostCalculation()) {
                    return false;
                }
            }
            if (!_layerCostModel.hasMultiClusterStrategy(targetOp)) {
                _log.trace("Find edge node {0} without strategy", targetOp->getLoc());
                return false;
            }
            auto clusteredTargetOp = mlir::dyn_cast<ClusteredOpInterface>(targetOp);
            if (_layerCostModel.doesLayerRequireTiling(targetOp, getRollbackStrategy(clusteredTargetOp))) {
                _log.trace("Find edge node {0} requires tiling with strategy {1}", targetOp->getLoc(),
                           getRollbackStrategy(clusteredTargetOp));
                return false;
            }

            return true;
        };

        // Check if the child layer needs rollback strategy
        for (auto child : currentOp->getResult(0).getUsers()) {
            if (mlir::isa_and_nonnull<VPU::QuantizeCastOp>(child) ||
                (mlir::isa_and_nonnull<VPU::ShapeCastOp>(child) && child->hasOneUse())) {
                // propagate cast ops
                child = *child->getResult(0).getUsers().begin();
            }

            if ((child == nullptr) ||
                (std::find(layersNeedRollback.begin(), layersNeedRollback.end(), child) != layersNeedRollback.end()) ||
                (layersWithConvergedStrategy.find(child) != layersWithConvergedStrategy.end())) {
                continue;
            }

            // If there's a spilling to the current child and it's caused by another child that is a strided cmx concat,
            // we don't add the current child into rollback list. That's because the spilling is caused by incompatible
            // stride, we can't avoid spilling even with compatible strategy.
            if (!mlir::isa_and_nonnull<VPU::ConcatOp>(child) &&
                hasSpillingCausedByStridedCMXConcat(clusteredCurrentOp, rollbackStrategy,
                                                    _configForFindingNeighbourNodes)) {
                continue;
            }

            // There is no VPUNN cost for NCEPermuteQuantizeOp.
            // Even if NCEPermuteQuantizeOp cost can be mapped to NCEEltwiseOp,
            // But NCEPermuteQuantizeOp only support SOW that VPUNN cannot support.
            // #E80251 is to refactor NCEPermuteQuantize, after that this filter can be removed.
            if (mlir::isa_and_nonnull<VPU::NCEPermuteQuantizeOp>(child)) {
                continue;
            }

            auto clusteredChildOp = mlir::dyn_cast<ClusteredOpInterface>(child);
            if ((clusteredChildOp != nullptr) && doesLayerNeedRollbackStrategy(child) &&
                hasOutputSpillingToMultiClusterLayer(clusteredCurrentOp, child, rollbackStrategy,
                                                     _configForFindingNeighbourNodes)) {
                layersNeedRollback.push_back(child);
                _log.trace("    Push child '{0} to layersNeedRollback queue", child->getLoc());
            }
        }

        // Check if the parent layer needs rollback strategy
        SmallVector<mlir::Value> layerInputs;
        if (auto eltwiseOp = mlir::dyn_cast<VPU::NCEEltwiseOp>(currentOp.getOperation())) {
            layerInputs.push_back(eltwiseOp.input1());
            layerInputs.push_back(eltwiseOp.input2());
        } else if (auto concatOp = mlir::dyn_cast<VPU::ConcatOp>(currentOp.getOperation())) {
            layerInputs = concatOp.inputs();
        } else {
            layerInputs.push_back(currentOp->getOperand(0));
        }

        for (auto input : layerInputs) {
            auto parent = input.getDefiningOp();
            if (mlir::isa_and_nonnull<VPU::ShapeCastOp, VPU::QuantizeCastOp>(parent)) {
                if (parent->getOperand(0).isa<mlir::BlockArgument>()) {
                    continue;
                }
                // propagate cast ops
                parent = parent->getOperand(0).getDefiningOp();
            }

            if ((parent == nullptr) ||
                (std::find(layersNeedRollback.begin(), layersNeedRollback.end(), parent) != layersNeedRollback.end()) ||
                (layersWithConvergedStrategy.find(parent) != layersWithConvergedStrategy.end())) {
                continue;
            }

            if (mlir::isa<SWOpInterface>(parent) && VPU::getArch(parent) == VPU::ArchKind::VPUX30XX) {
                continue;
            }

            auto clusteredParentOp = mlir::dyn_cast<ClusteredOpInterface>(parent);
            if (clusteredParentOp == nullptr) {
                continue;
            }

            // If there's a spilling from the current parent and it's caused by another child of parent as strided cmx
            // concat, we don't add the current parent into rollback list. That's because the spilling is caused by
            // incompatible stride, we can't avoid spilling even with compatible strategy.
            if (!mlir::isa_and_nonnull<VPU::ConcatOp>(currentOp) &&
                hasSpillingCausedByStridedCMXConcat(clusteredParentOp, getRollbackStrategy(clusteredParentOp),
                                                    _configForFindingNeighbourNodes)) {
                continue;
            }

            // There is no VPUNN cost for NCEPermuteQuantizeOp.
            // Even if NCEPermuteQuantizeOp cost can be mapped to NCEEltwiseOp,
            // But NCEPermuteQuantizeOp only support SOW that VPUNN cannot support.
            // #E80251 is to refactor NCEPermuteQuantize, after that this filter can be removed.
            if (mlir::isa_and_nonnull<VPU::NCEPermuteQuantizeOp>(parent)) {
                continue;
            }

            if ((clusteredParentOp != nullptr) && doesLayerNeedRollbackStrategy(parent) &&
                hasInputSpillingToMultiClusterLayer(clusteredCurrentOp, input, rollbackStrategy,
                                                    _configForFindingNeighbourNodes)) {
                layersNeedRollback.push_back(parent);
                _log.trace("    Push parent '{0} to layersNeedRollback queue", parent->getLoc());
            }
        }
    }

    // Calculate original cost and rollback cost
    double originalCost = 0;
    double rollbackCost = 0;
    std::set<mlir::Operation*> opsWithOutputSpillingCounted;
    for (auto opWithStrategy : layersWithRollbackStrategy) {
        auto clusteredTask = opWithStrategy.first;
        auto newStrategy = opWithStrategy.second;
        auto oldStrategy = _layerCostModel.getMultiClusterStrategyValue(clusteredTask);

        // compute cost + weights dma cost
        auto originalBasicCost = _layerCostModel.getLayerCost(clusteredTask, oldStrategy);
        auto rollbackBasicCost = _layerCostModel.getLayerCost(clusteredTask, newStrategy);
        _log.trace("add originalCost cost {0} for op {1} with strategy {2}", originalBasicCost, clusteredTask->getLoc(),
                   oldStrategy);
        _log.trace("add rollback cost {0} for op {1} with strategy {2}", rollbackBasicCost, clusteredTask->getLoc(),
                   newStrategy);

        originalCost += originalBasicCost;
        rollbackCost += rollbackBasicCost;

        // input spilling cost is calculated by the output spilling cost of parent
        SmallVector<mlir::Value> layerInputs;
        if (auto eltwiseOp = mlir::dyn_cast<VPU::NCEEltwiseOp>(clusteredTask.getOperation())) {
            layerInputs.push_back(eltwiseOp.input1());
            layerInputs.push_back(eltwiseOp.input2());
        } else if (auto concatOp = mlir::dyn_cast<VPU::ConcatOp>(clusteredTask.getOperation())) {
            layerInputs = concatOp.inputs();
        } else {
            layerInputs.push_back(clusteredTask->getOperand(0));
        }

        for (auto input : layerInputs) {
            auto parent = input.getDefiningOp();
            if ((parent != nullptr) && (_layerCostModel.hasMultiClusterStrategy(parent)) &&
                (!opsWithOutputSpillingCounted.count(parent))) {
                auto parentOldStrategy = _layerCostModel.getMultiClusterStrategyValue(parent);
                auto clusteredParent = mlir::dyn_cast<ClusteredOpInterface>(parent);
                auto parentNewStrategy = getRollbackStrategy(clusteredParent);

                auto originalInputSpillingCost =
                        getOutputSpillingCostToMultiClusterLayer(parent, parentOldStrategy, _configForCalcOrigCost);
                auto rollbackInputSpillingCost =
                        getOutputSpillingCostToMultiClusterLayer(parent, parentNewStrategy, _configForCalcRollbackCost);
                _log.trace("add originalCost input Spilling {0} from op {1} with strategy {2}",
                           originalInputSpillingCost, parent->getLoc(), parentOldStrategy);
                _log.trace("add rollback input Spilling {0} from op {1} with strategy {2}", rollbackInputSpillingCost,
                           parent->getLoc(), parentNewStrategy);

                originalCost += originalInputSpillingCost;
                rollbackCost += rollbackInputSpillingCost;
                opsWithOutputSpillingCounted.insert(parent);
            }
        }

        // output spilling cost
        if (!opsWithOutputSpillingCounted.count(clusteredTask)) {
            auto originalOutputSpillingCost =
                    getOutputSpillingCostToMultiClusterLayer(clusteredTask, oldStrategy, _configForCalcOrigCost);
            auto rollbackOutputSpillingCost =
                    getOutputSpillingCostToMultiClusterLayer(clusteredTask, newStrategy, _configForCalcRollbackCost);
            _log.trace("add originalCost output Spilling {0} for op {1} with strategy {2}", originalOutputSpillingCost,
                       clusteredTask->getLoc(), oldStrategy);
            _log.trace("add rollback output Spilling {0} for op {1} with strategy {2}", rollbackOutputSpillingCost,
                       clusteredTask->getLoc(), newStrategy);

            originalCost += originalOutputSpillingCost;
            rollbackCost += rollbackOutputSpillingCost;
            opsWithOutputSpillingCounted.insert(clusteredTask);
        }
    }

    if (rollbackCost < originalCost) {
        for (auto& opWithStrategy : layersWithRollbackStrategy) {
            auto clusteredTask = opWithStrategy.first;
            auto newStrategy = opWithStrategy.second;
            clusteredTask.setMultiClusterStrategy(newStrategy);
            _log.trace("  [rollback] '{0}' : set strategy as {1}", clusteredTask->getLoc(), newStrategy);
        }
    } else {
        _log.trace("Subgraph opt: rollback unneccessary! Strategies no change");
    }
    _log.trace("  Rollback cost: {0} , Current cost: {1}", rollbackCost, originalCost);
}

void SubgraphOptimizer::optimizeStrategyAvoidSpillingOnModel() {
    // detect shortcuts in model like resnet
    detectShortcuts();

    const auto callback = [this](VPU::ClusteredOpInterface clusteredOp) {
        //  TODO extend to support SW layers E#68740
        //  The spilling judgement for SEGMENTED SOK is not supported yet
        //  Enabling SW subgraph optimization leads to performance regression
        if (mlir::isa<SWOpInterface>(clusteredOp.getOperation())) {
            return;
        }

        optimizeStrategyAvoidSpillingOnSubgraph(clusteredOp);
    };

    /// @brief Traversing nodes with preOrder to execute subgraph optimization
    _func.walk(callback);
}
