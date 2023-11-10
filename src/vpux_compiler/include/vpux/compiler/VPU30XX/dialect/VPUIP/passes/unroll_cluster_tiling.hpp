//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include "vpux/compiler/dialect/VPUIP/passes/unroll_cluster_tiling.hpp"

namespace vpux {
namespace VPUIP {
namespace arch30xx {

//
// ClusterNCERewriter
//

class ClusterNCERewriter final : public ClusterNCEBaseRewriter {
public:
    ClusterNCERewriter(mlir::MLIRContext* ctx, Logger log): ClusterNCEBaseRewriter(ctx, log) {
        setDebugName("ClusterNCERewriterVPUX3XXX");
    }

private:
    void getOutputBuffers(SmallVector<mlir::Value>& parentOutputBuffs, SmallVector<mlir::Value>& outputBuffs,
                          SmallVector<mlir::Value>& parentOutputSparsityMap,
                          SmallVector<mlir::Value>& outputSparsityMapBuffs, mlir::Location loc,
                          VPUIP::NCEClusterTilingOp clusterOp, VPUIP::NCEClusterTaskOp nceTask,
                          const int64_t numClusters, mlir::PatternRewriter& rewriter) const override;

    void getInputBuffers(SmallVector<mlir::Value>& parentInputBuffs, SmallVector<mlir::Value>& inputBuffs,
                         SmallVector<mlir::Value>& parentInputSparsityMap,
                         SmallVector<mlir::Value>& inputSparsityMapBuffs, SmallVector<mlir::Value>& parentInputSETable,
                         SmallVector<mlir::Value>& inputSETableBuffs, mlir::Location loc,
                         VPUIP::NCEClusterTilingOp clusterOp, VPUIP::NCEClusterTaskOp nceTask,
                         const int64_t numClusters, mlir::PatternRewriter& rewriter) const override;

    mlir::UnitAttr isSegmentedNCETask(VPUIP::DistributedBufferType inputType) const override;
};

}  // namespace arch30xx
}  // namespace VPUIP
}  // namespace vpux
