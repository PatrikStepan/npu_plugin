//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include "vpux/utils/IE/config.hpp"
#include "vpux/utils/core/logger.hpp"

#include "vpux/compiler/dialect/VPU/attributes.hpp"

#include <mlir/Pass/PassManager.h>

namespace vpux {
//
// IPipelineStrategy
//

class IPipelineStrategy {
public:
    virtual void buildPipeline(mlir::PassManager& pm, const Config& config, mlir::TimingScope& rootTiming,
                               Logger log) = 0;

    virtual ~IPipelineStrategy() = default;
};

}  // namespace vpux
