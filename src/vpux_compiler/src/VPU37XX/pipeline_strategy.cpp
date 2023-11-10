//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/compiler.hpp"

#include "vpux/al/config/common.hpp"
#include "vpux/al/config/compiler.hpp"

#include "vpux/compiler/VPU37XX/conversion.hpp"
#include "vpux/compiler/VPU37XX/pipeline_strategy.hpp"
#include "vpux/compiler/VPU37XX/pipelines.hpp"

#include "vpux/compiler/options_mapper.hpp"

#include "vpux/compiler/dialect/VPU/attributes.hpp"
#include "vpux/compiler/pipelines.hpp"

#include "vpux/compiler/dialect/VPU/passes.hpp"

using namespace vpux;

//
// PipelineStrategy37XX::buildPipeline
//

void PipelineStrategy37XX::buildPipeline(mlir::PassManager& pm, const Config& config, mlir::TimingScope& rootTiming,
                                         Logger log) {
    auto buildTiming = rootTiming.nest("Build compilation pipeline");

    const auto archKind = getArchKind(config);
    const auto compilationMode = getCompilationMode(config);
    const auto enableProfiling = config.get<PERF_COUNT>();
    const auto numOfDPUGroups = getNumberOfDPUGroups(config);
    const auto numOfDMAPorts = getNumberOfDMAEngines(config);

    VPUX_THROW_WHEN(
            numOfDPUGroups.has_value() && numOfDMAPorts.has_value() && numOfDMAPorts.value() > numOfDPUGroups.value(),
            "Requested configuration not supported by runtime. Number of DMA ports ({0}) larger than NCE clusters "
            "({1})",
            numOfDMAPorts.value(), numOfDPUGroups.value());

    pm.addPass(VPU::createInitCompilerPass(archKind, compilationMode, numOfDPUGroups, numOfDMAPorts, log.nest()));

    if (compilationMode == VPU::CompilationMode::ReferenceSW) {
        const auto options = ReferenceSWOptions37XX::createFromString(config.get<COMPILATION_MODE_PARAMS>());
        VPUX_THROW_UNLESS(options != nullptr, "buildPipeline failed to parse COMPILATION_MODE_PARAMS");
        options->enableProfiling = enableProfiling;
        buildReferenceSWModePipeline(pm, *options, log.nest());
    } else if (compilationMode == VPU::CompilationMode::ReferenceHW) {
        const auto options = ReferenceHWOptions37XX::createFromString(config.get<COMPILATION_MODE_PARAMS>());
        VPUX_THROW_UNLESS(options != nullptr, "buildPipeline failed to parse COMPILATION_MODE_PARAMS");
        options->enableProfiling = enableProfiling;
        buildReferenceHWModePipeline(pm, *options, log.nest());
    } else if (compilationMode == VPU::CompilationMode::DefaultHW) {
        const auto options = DefaultHWOptions37XX::createFromString(config.get<COMPILATION_MODE_PARAMS>());
        VPUX_THROW_UNLESS(options != nullptr, "buildPipeline failed to parse COMPILATION_MODE_PARAMS");
        options->enableProfiling = enableProfiling;
        options->enableConvertAvgPoolToDWConv = false;
        options->enableHandleAsymmetricStrides = false;

        buildDefaultHWModePipeline(pm, *options, log.nest());
    } else if (compilationMode == VPU::CompilationMode::ShaveCodeGen) {
        buildShaveCodeGenPipeline37XX(pm, log.nest());
    } else {
        VPUX_THROW("Unsupported compilation mode '{0}'", compilationMode);
    }

    if (isELFEnabled(config)) {
        vpux::arch37xx::buildLowerVPUIP2ELFPipeline(pm, log.nest());
    }
}
