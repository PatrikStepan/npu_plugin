//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/conversion.hpp"

#include "vpux/compiler/core/passes.hpp"
#include "vpux/compiler/utils/rewriter.hpp"

#include <mlir/Transforms/Passes.h>

using namespace vpux;

//
// LowerIE2IERT
//

void vpux::buildLowerIE2IERTPipeline(mlir::OpPassManager& pm, Logger log) {
    const auto grc = getDefaultGreedyRewriteConfig();

    pm.addPass(createBufferizeIEPass(log));
    pm.addPass(createBufferizeFuncAndReturnPass(log));
    pm.addPass(createAddBuffersForNetResults(log));
    pm.addPass(mlir::createCanonicalizerPass(grc));
}

//
// registerConversionPipelines
//

void vpux::registerConversionPipelines() {
    mlir::PassPipelineRegistration<>("lower-IE-to-IERT", "Performs full lowering from the IE Dialect to IERT Dialect",
                                     [](mlir::OpPassManager& pm) {
                                         buildLowerIE2IERTPipeline(pm);
                                     });
}
