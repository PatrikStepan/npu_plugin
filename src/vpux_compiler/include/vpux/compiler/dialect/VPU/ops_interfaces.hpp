//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include "vpux/compiler/core/attributes/shape.hpp"
#include "vpux/compiler/dialect/IE/ops_interfaces.hpp"
#include "vpux/compiler/dialect/VPU/attributes.hpp"
#include "vpux/compiler/dialect/VPU/nce_invariant.hpp"
#include "vpux/compiler/utils/error.hpp"

#include <mlir/IR/OpDefinition.h>
#include <mlir/IR/Operation.h>
#include <mlir/IR/ValueRange.h>

#include <initializer_list>
#include <numeric>

namespace vpux {
namespace VPU {

//
// SparseOpInterface
//

bool supportsSparseInputs(mlir::Operation* op);
bool supportsSparseOutputs(mlir::Operation* op);
bool supportsSparseData(mlir::Operation* op);
bool supportsSparseWeights(mlir::Operation* op);

//
// NCEOpInterface
//

template <typename ConcreteOp>
void setLayerMultiClusterStrategy(ConcreteOp mainOp, VPU::MultiClusterStrategy strategy) {
    const auto multiClusterStrategyAttr = VPU::MultiClusterStrategyAttr::get(mainOp->getContext(), strategy);
    mainOp.setMultiClusterStrategyAttr(multiClusterStrategyAttr);
}

namespace details {

mlir::LogicalResult validatePrecisionForNCE(mlir::Operation* op);
mlir::LogicalResult validateWorkloadsRegion(mlir::Location loc, mlir::Region& workloads);

mlir::Operation* addWorkload(mlir::Region& workloads, mlir::OpBuilder& builder, mlir::Location loc, ShapeRef offsets,
                             ShapeRef sizes, PaddingAttr pad, MPEMode mpeMode, mlir::IntegerAttr clusterId);

}  // namespace details

//
// LayerOpInterface
//

mlir::LogicalResult verifyLayer(mlir::Operation* op);

//
// TilingBuilderOpInterface
//

mlir::Value makeTile(mlir::OpBuilder& builder, mlir::Location baseLoc, mlir::Value origVal, const TileInfo& tile,
                     StringRef valName);

//
// TilingInfoOpInterface
//

mlir::LogicalResult verifyTilingInfo(mlir::Operation* op);

//
// EltwiseOp
//

mlir::LogicalResult verifyEltwiseOp(mlir::Operation* op);

template <typename ConcreteOp>
class EltwiseOp : public mlir::OpTrait::TraitBase<ConcreteOp, EltwiseOp> {
public:
    static mlir::LogicalResult verifyTrait(mlir::Operation* op) {
        return VPU::verifyEltwiseOp(op);
    }

    InputTiling backInferTileInfo(const vpux::TileInfo& outputTile, vpux::Logger) {
        return backInferEltwiseTile(this->getOperation(), outputTile);
    }

    void adjustAttrs(const TilingInfo&, const TileInfo&) {
        // Do nothing
    }

    mlir::FailureOr<OutputTiling> getTilingStrategy(TilingMode tilingMode, Logger log) {
        return getSWLayerTilingStrategy(this->getOperation(), tilingMode, log);
    }
};

//
// NCEOpInterface
//

mlir::LogicalResult verifyNCEOp(mlir::Operation* op);

//
// isPureViewOp
//

bool isPureViewOp(mlir::Operation* op);

//
// LimitedToArch
//

template <ArchKind... archs>
struct LimitedToArch {
    template <typename ConcreteType>
    class Impl : public mlir::OpTrait::TraitBase<ConcreteType, Impl> {
    public:
        static mlir::LogicalResult verifyTrait(mlir::Operation* op) {
            return verifyArchKind(op, {archs...});
        }

    private:
        static mlir::LogicalResult verifyArchKind(mlir::Operation* op, std::initializer_list<ArchKind> supportedArchs) {
            auto arch = getArch(op);

            if (arch != ArchKind::UNKNOWN) {
                if (std::find(cbegin(supportedArchs), cend(supportedArchs), arch) == cend(supportedArchs)) {
                    auto archStr = stringifyArchKind(arch).str();
                    auto archsStr = std::accumulate(cbegin(supportedArchs), cend(supportedArchs), std::string(),
                                                    [](const std::string& accu, const ArchKind arch) -> std::string {
                                                        return accu + (accu.length() > 0 ? "," : "") +
                                                               stringifyArchKind(arch).str();
                                                    });
                    return vpux::errorAt(op, "Operation {0} not supported in {1}; list of supported archs: {2}",
                                         op->getName(), archStr, archsStr);
                }
            }

            return mlir::success();
        }
    };
};

}  // namespace VPU
}  // namespace vpux

//
// Generated
//

#include <vpux/compiler/dialect/VPU/ops_interfaces.hpp.inc>
