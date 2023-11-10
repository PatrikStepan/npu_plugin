//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include "vpux/compiler/pipeline_strategy.hpp"

namespace vpux {

//
// PipelineStrategy37XX
//

class PipelineStrategy37XX final : public IPipelineStrategy {
public:
    void buildPipeline(mlir::PassManager& pm, const Config& config, mlir::TimingScope& rootTiming, Logger log) override;
};

}  // namespace vpux
