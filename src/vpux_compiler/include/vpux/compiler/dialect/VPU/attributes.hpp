//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include "vpux/compiler/core/attributes/shape.hpp"
#include "vpux/compiler/core/attributes/strided_shape.hpp"
#include "vpux/compiler/core/tiling.hpp"
#include "vpux/compiler/dialect/VPU/attr_interfaces.hpp"
#include "vpux/utils/core/array_ref.hpp"
#include "vpux/utils/core/format.hpp"
#include "vpux/utils/core/func_ref.hpp"
#include "vpux/utils/core/mem_size.hpp"
#include "vpux/utils/core/optional.hpp"
#include "vpux/utils/core/string_ref.hpp"

#include <mlir/IR/BuiltinAttributes.h>
#include <mlir/IR/BuiltinOps.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/Interfaces/SideEffectInterfaces.h>

#include <llvm/Support/FormatVariadic.h>

namespace vpux {
namespace VPU {

class PaddingAttr;

}  // namespace VPU
}  // namespace vpux

//
// Generated
//

#include <vpux/compiler/dialect/IE/attributes.hpp.inc>
#include <vpux/compiler/dialect/VPU/enums.hpp.inc>
#include <vpux/compiler/dialect/VPU/structs.hpp.inc>

#define GET_ATTRDEF_CLASSES
#include <vpux/compiler/dialect/VPU/attributes.hpp.inc>

namespace vpux {
namespace VPU {

// This one represents a CMX_NN memory space with fragmentation consideration
constexpr StringLiteral CMX_NN_FragmentationAware = "CMX_NN_FragmentationAware";

//
// Run-time resources
//

StringLiteral getMemoryDerateAttrName();
StringLiteral getMemoryBandwidthAttrName();

/**
 * @brief Get DPU frequency
 *
 * @param arch - architecture
 * @return DPU clock frequency [MHz]
 *
 * @note Provides processor frequency values (ie. dpu_clk) that get exported to values under
 * header>resources>processor_frequencies>number in the blob.
 *
 * @note Note the difference between the vpu and dpu clock frequencies.
 *
 * @note Values returned by this function are tight to definitions provided by
 * vpucostmodel.
 */
unsigned int getDpuFrequency(vpux::VPU::ArchKind arch);

/**
 * @brief Get maximal DMA bandwidth for a given architecture
 *
 * @param module
 * @return bandwidth in GB/s
 *
 * The BW value depends on platform, number of DMA channels and DPU clock frequency.
 * The function uses vpuperformance specifications and typically
 * corresponds to the maximal DPU frequencies (and dual DMA if available) for given architecture.
 *
 * The value is serialized into blob header fields (header>resources>memory_bandwidth>number).
 */
double getDmaBandwidthGBps(mlir::ModuleOp module);

/**
 * @brief Get maximal DMA bandwidth for a given architecture
 *
 * @param arch - architectire
 *
 * See getDmaBandwidthGBps(mlir::ModuleOp module)
 */
double getDmaBandwidthGBps(ArchKind arch);

uint32_t getMaxDPUClusterNum(ArchKind arch);
uint32_t getMaxDPUClusterNum(mlir::Operation* op);
uint32_t getMaxDMAPorts(ArchKind arch);

/**
 * @brief return DMA bandwidth
 *
 * @param arch
 * @return DMA bandwidth in bytes per DPU clock cycle
 */
double getDMABandwidth(ArchKind arch);

/**
 * @brief NCE troughput
 *
 * @param arch
 * @return return NCE troughtput in MOPS (millions of operations per second)
 */
double getNCEThroughput(ArchKind arch);

Byte getTotalCMXSize(mlir::Operation* op);
Byte getTotalCMXSize(mlir::ModuleOp module);
Byte getTotalCMXFragmentationAwareSize(mlir::Operation* op);
Byte getTotalCMXFragmentationAwareSize(mlir::ModuleOp module);

//
// ArchKind
//

void setArch(mlir::ModuleOp module, ArchKind kind, Optional<int> numOfDPUGroups = None,
             Optional<int> numOfDMAPorts = None, bool allowCustomChanges = false);

ArchKind getArch(mlir::Operation* op);
bool isArchVPUX3XXX(VPU::ArchKind arch);

//
// CompilationMode
//

void setCompilationMode(mlir::ModuleOp module, CompilationMode compilationMode);
bool hasCompilationMode(mlir::ModuleOp module);
CompilationMode getCompilationMode(mlir::Operation* op);

//
// PaddingAttr
//

PaddingAttr getPaddingAttr(mlir::MLIRContext* ctx, int64_t left, int64_t right, int64_t top, int64_t bottom);
PaddingAttr getPaddingAttr(mlir::MLIRContext* ctx, ArrayRef<int64_t> padsBegin, ArrayRef<int64_t> padsEnd);
PaddingAttr getPaddingAttr(mlir::MLIRContext* ctx, const PadInfo& pad);

PadInfo toPadInfo(PaddingAttr attr);

//
// PPETaskAttr
//

PPETaskAttr getPPETaskAttr(mlir::MLIRContext* ctx, VPU::PPEMode mode);

PPETaskAttr getPPETaskAttr(mlir::MLIRContext* ctx, VPU::PPEMode mode, int64_t clampLow, int64_t clampHigh,
                           int64_t lreluMult, int64_t lreluShift);

PPETaskAttr getPPETaskAttr(mlir::MLIRContext* ctx, VPU::PPEMode mode, int64_t clampLow, int64_t clampHigh,
                           int64_t lreluMult, int64_t lreluShift, ArrayRef<int64_t> quantMult,
                           ArrayRef<int64_t> quantShift, int64_t quantPostShift);

PPETaskAttr getPPETaskAttr(mlir::MLIRContext* ctx, VPU::PPEMode mode, int64_t clampLow, int64_t clampHigh,
                           int64_t lreluMult, int64_t lreluShift, ArrayRef<double> quantScale);

PPETaskAttr getPPETaskAttr(mlir::MLIRContext* ctx, VPU::PPEMode mode, int64_t clampLow, int64_t clampHigh,
                           int64_t lreluMult, int64_t lreluShift, ArrayRef<int64_t> quantMult,
                           ArrayRef<int64_t> quantShift, int64_t quantPostShift, ArrayRef<int64_t> in1QuantMult,
                           ArrayRef<int64_t> in2QuantMult, double fpPReluAlpha = 1.0);

VPU::PPEMode getPPEMode(VPU::EltwiseType type);

//
// DistributedTensorAttr
//

mlir::LogicalResult verify(FuncRef<mlir::InFlightDiagnostic()> emitError, DistributedTensorAttr distributedAttr,
                           ArrayRef<int64_t> shape);
mlir::LogicalResult areDistributionModesCompatible(DistributionMode sourceMode, DistributionMode targetMode);
mlir::LogicalResult areDistributionNumClustersCompatible(mlir::IntegerAttr sourceNumClusters,
                                                         mlir::IntegerAttr targetNumClusters);
mlir::LogicalResult areDistributionElementTypesCompatible(mlir::Type inType, mlir::Type outType);
SmallVector<Shape> getPerClusterComputeShapes(ShapeRef shapeRef, DistributedTensorAttr distributionAttr);
SmallVector<Shape> getPerClusterComputeShapeOffsets(ShapeRef shapeRef, DistributedTensorAttr distributionAttr);
SmallVector<Shape> getPerClusterMemoryShapes(ShapeRef shapeRef, DistributedTensorAttr distributionAttr);
SmallVector<Shape> getPerClusterMemoryShapeOffsets(ShapeRef shapeRef, DistributedTensorAttr distributionAttr);
SmallVector<PadInfo> getPerClusterPadding(DistributedTensorAttr distributionAttr, PadInfo kernelPadding);
SmallVector<StridedShape> getPerClusterMemoryStridedShapes(ShapeRef shape, StridesRef strides, DimsOrder dimsOrder,
                                                           DistributionModeAttr mode, ArrayRef<Shape> memoryShapes);
int64_t getDistributedTilingAxis(ArrayRef<int64_t> tilingScheme);
bool isDistributedAttrWithExplicitShapesAndOffsets(DistributedTensorAttr distributionAttr);
SmallVector<Shape> arrayAttrToVecOfShapes(mlir::ArrayAttr arr);

bool isSegmentedOverH(VPU::DistributedTensorAttr distAttr);
bool isSegmentedOverC(VPU::DistributedTensorAttr distAttr);
bool isSegmentedOverN(VPU::DistributedTensorAttr distAttr);
bool isOverlappedOverH(VPU::DistributedTensorAttr distAttr);

//
// CompressionSchemeAttr
//

VPU::CompressionSchemeAttr getCompressionSchemeAttr(mlir::Type type);
mlir::Type setCompressionSchemeAttr(mlir::Type type, VPU::CompressionSchemeAttr compressionSchemeAttr);

VPU::CompressionSchemeAttr tileCompressionScheme(VPU::CompressionSchemeAttr compressionScheme, ShapeRef tileOffsets,
                                                 ShapeRef tileShape);

//
// Resource kind value getter
//

template <typename ConcreteKind, typename ResourceOp>
ConcreteKind getKindValue(ResourceOp op) {
    VPUX_THROW_WHEN(!op.getKind(), "Can't find attributes for Operation");
    const auto maybeKind = vpux::VPU::symbolizeEnum<ConcreteKind>(op.getKind());
    VPUX_THROW_WHEN(!maybeKind.has_value(), "Unsupported attribute kind");
    return maybeKind.value();
}

}  // namespace VPU
}  // namespace vpux
