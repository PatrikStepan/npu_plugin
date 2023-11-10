//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPU/passes.hpp"
#include "vpux/compiler/utils/rewriter.hpp"

#include <mlir/Pass/PassManager.h>
#include <mlir/Transforms/Passes.h>

using namespace vpux;

namespace {

VPU::ActivationSparsityProfile getActSparsityProfile(const StrOption& actProfile) {
    VPUX_THROW_UNLESS(actProfile.hasValue(),
                      "Activation sparsity profile is not provided. Please try 'act-sparsity-profile=S1'");
    const auto actProfileStr = actProfile.getValue();
    const auto parsed = VPU::symbolizeActivationSparsityProfile(actProfileStr);
    VPUX_THROW_UNLESS(parsed.has_value(), "Unsupported activation sparsity profile '{0}'", actProfileStr);
    return parsed.value();
}

template <VPU::ActivationSparsityProfile PROFILE>
Optional<VPU::ActivationSparsityProfile> getSparsityProfile(StringRef) {
    return PROFILE;
}

auto getSparsityProfileCallback(VPU::ActivationSparsityProfile actSparsityProfile) {
    switch (actSparsityProfile) {
    case VPU::ActivationSparsityProfile::S0:
        return getSparsityProfile<VPU::ActivationSparsityProfile::S0>;
    case VPU::ActivationSparsityProfile::S1:
        return getSparsityProfile<VPU::ActivationSparsityProfile::S1>;
    default:
        VPUX_THROW("Unknown ActSparsityProfile");
    }
}

VPU::WeightsSparsityHeuristic getWeightsSparsityHeuristic(const StrOption& weightsSparsityHeuristic) {
    VPUX_THROW_UNLESS(weightsSparsityHeuristic.hasValue(),
                      "Weights sparsity heuristic is not provided. Please try 'weights-sparsity-heuristic=RATIO'");
    const auto weightsSparsityHeuristicStr = weightsSparsityHeuristic.getValue();
    const auto parsed = VPU::symbolizeWeightsSparsityHeuristic(weightsSparsityHeuristicStr);
    VPUX_THROW_UNLESS(parsed.has_value(), "Unsupported weights sparsity heuristic '{0}'", weightsSparsityHeuristicStr);
    return parsed.value();
}

Optional<double> getWeightsSparsityThreshold(const DoubleOption& weightsSparsityThreshold) {
    if (weightsSparsityThreshold.hasValue()) {
        const auto threshold = weightsSparsityThreshold.getValue();
        if (threshold >= 0.0) {
            return threshold;
        }
    }
    return None;
}

}  // namespace

//
// buildActivationSparsityPipeline
//

void vpux::VPU::buildActivationSparsityPipeline(mlir::OpPassManager& pm, const VPU::ActivationSparsityOptions& options,
                                                Logger log) {
    const auto grc = getDefaultGreedyRewriteConfig();
    const auto actSparsityProfile = getActSparsityProfile(options.actSparsityProfile);
    const auto profileCallback = getSparsityProfileCallback(actSparsityProfile);

    pm.addPass(VPU::createWrapOpsInSparsifyDesparsifyPairsPass(
            VPU::getActSparsityMode(options.enableActivationSparsity), actSparsityProfile, log));

    if (actSparsityProfile == VPU::ActivationSparsityProfile::S1) {
        pm.addPass(VPU::createFuseSparsityOpsPass(/*fuseSparsify=*/false, log));
    }

    pm.addPass(VPU::createOptimizeSparsifyDesparsifyPairsPass(profileCallback, log));
    pm.addPass(VPU::createFuseSparsityOpsPass(/*fuseSparsify=*/true, log));
    pm.addPass(VPU::createOptimizeSparsityOpsPass(profileCallback, log));
    pm.addPass(VPU::createAddSparsityMapToSparseActivationsPass(log));
    pm.addPass(mlir::createCanonicalizerPass(grc));
}

//
// buildWeightsSparsityPipeline
//

void vpux::VPU::buildWeightsSparsityPipeline(mlir::OpPassManager& pm, const VPU::WeightsSparsityOptions& options,
                                             Logger log) {
    const auto weightsSparsityHeuristic = getWeightsSparsityHeuristic(options.weightsSparsityHeuristic);
    const auto weightsSparsityThreshold = getWeightsSparsityThreshold(options.weightsSparsityThreshold);
    pm.addPass(VPU::createSparsifyWeightsPass(weightsSparsityHeuristic, weightsSparsityThreshold, log));
    pm.addPass(VPU::createRecomputeSparsityPtrsPass(log));
}

void VPU::registerVPUPipelines() {
    mlir::PassPipelineRegistration<VPU::ActivationSparsityOptions>(
            "enable-act-sparsity", "Enable activation sparsity",
            [](mlir::OpPassManager& pm, const VPU::ActivationSparsityOptions& options) {
                VPU::buildActivationSparsityPipeline(pm, options);
            });
    mlir::PassPipelineRegistration<VPU::WeightsSparsityOptions>(
            "enable-weights-sparsity", "Enable weights sparsity",
            [](mlir::OpPassManager& pm, const VPU::WeightsSparsityOptions& options) {
                VPU::buildWeightsSparsityPipeline(pm, options);
            });
    mlir::PassPipelineRegistration<VPU::TilingOptions>("tiling", "Apply tiling",
                                                       [](mlir::OpPassManager& pm, const VPU::TilingOptions& options) {
                                                           VPU::buildTilingPipeline(pm, options);
                                                       });

    mlir::PassPipelineRegistration<VPU::TilingOptions>("sm-pipeline", "Apply SM Pipeline",
                                                       [](mlir::OpPassManager& pm, const VPU::TilingOptions& options) {
                                                           VPU::buildSMPipeline(pm, options);
                                                       });
}

void vpux::VPU::buildTilingPipeline(mlir::OpPassManager& pm, const VPU::TilingOptions& options, Logger log) {
    const auto grc = getDefaultGreedyRewriteConfig();

    pm.addPass(VPU::createTilingStrategyAssignmentPass(options.enablePrefetchTiling, log));
    if (options.enableVerticalFusion) {
        VPU::buildVFPipeline(pm, options, log);
    }
    pm.addPass(VPU::createApplyTilingPass(log));
    pm.addPass(mlir::createCanonicalizerPass(grc));
}

//
// Strategy Pipeline
//

void vpux::VPU::buildVFPipeline(mlir::OpPassManager& pm, const VPU::TilingOptions& options, Logger log) {
    pm.addPass(VPU::createTileOverHForVFPass(options.enablePrefetchTiling, log));
    pm.addPass(VPU::createWrapVerticalFusionRegionPass(log));
    pm.addPass(VPU::createMergeVfSubgraphsPass(log));
    pm.addPass(VPU::createUnrollUnusedVerticalFusionRegionPass(log));
    pm.addPass(VPU::createRollBackTilingStrategyPass(options.enablePrefetchTiling, log));
    pm.addPass(VPU::createAdjustVFTilingStrategyPass(log));
    pm.addPass(VPU::createVfTilingPass(log));
}

void vpux::VPU::buildSMPipeline(mlir::OpPassManager& pm, const VPU::TilingOptions& options, Logger log) {
    // TO DO - SM Assignment Optimization Pass
    // Keep enableSMpipleline Option - false till SM pipeline is built

    pm.addPass(VPU::createStrategyManagerImplPass(options.enablePrefetchTiling, log));
    if (options.enableVerticalFusion) {
        VPU::buildVFPipeline(pm, options, log);
    }
    pm.addPass(VPU::createApplyTilingPass(log));
    pm.addPass(VPU::createAdjustTilingForPermuteQuantizePass(log));
    pm.addPass(VPU::createWrapVPUOpsInNCEClusterTilingPass(options.enableExplicitDistributedTensorAttr, log));
}
