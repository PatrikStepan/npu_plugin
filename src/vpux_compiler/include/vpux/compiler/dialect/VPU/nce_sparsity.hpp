//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include "vpux/compiler/core/attributes/shape.hpp"
#include "vpux/compiler/dialect/IE/attributes.hpp"
#include "vpux/compiler/dialect/VPU/attributes.hpp"
#include "vpux/compiler/dialect/VPU/ops.hpp"
#include "vpux/compiler/dialect/const/attributes/content.hpp"

#include "vpux/utils/core/array_ref.hpp"
#include "vpux/utils/core/enums.hpp"
#include "vpux/utils/core/func_ref.hpp"
#include "vpux/utils/core/optional.hpp"

#include <mlir/IR/Value.h>

namespace vpux {
namespace VPU {

namespace NCESparsity {

// base_ptr is 9bits size
const int BASE_PTR_SIZE = 9;
const int MAX_BASE_POINTER_VALUE = 511;
// Max number of distinct base pointers within one Storage Element Table. Should be equal as number of clusters
const int MAX_DISTINCT_BASE_PTRS = 4;

const VPU::SparsitySupport FULLY_SUPPORTED_SPARSITY_MODE =
        SparsitySupport::SPARSE_INPUTS | SparsitySupport::SPARSE_OUTPUTS | SparsitySupport::SPARSE_WEIGHTS;

constexpr int32_t SPARSITY_PTR_WHEN_NO_SPARSITY = 0xFFFFFF;

const unsigned int DEFAULT_SPARSIFIABLE_INPUT_OPERAND_ID = 0;
const unsigned int ELTWISE_SPARSIFIABLE_SECOND_INPUT_OPERAND_ID = 1;

using BiasConverterCb = int32_t (*)(double);
using PPEConverterCb = int32_t (*)(uint8_t, uint16_t, double, mlir::Type);

extern const EnumMap<ArchKind, PPEConverterCb> ppeConvertersMap;
extern const EnumMap<ArchKind, BiasConverterCb> biasConvertersMap;

enum class Mode { CM_CONV, DW_CONV, POOL };

int64_t getBitPatternSize(Mode mode, ShapeRef kernelSize, int64_t SX, mlir::Type elemType, int64_t IC);

int64_t getActivationWindowSize(Mode mode, ShapeRef kernelSize, int64_t SX, mlir::Type elemType, int64_t IC);

Shape inferActivationWindowShape(int64_t fakeSparsitySize);
Shape inferActivationWindowShape(Mode mode, ShapeRef kernelSize, int64_t SX, mlir::Type elemType, int64_t IC);

std::vector<uint8_t> getFakeSparsity(Mode mode, ShapeRef kernelSize, int64_t SX, mlir::Type elemType, int64_t IC);

int32_t getWeightPtrStep(mlir::Value weights);

std::vector<int32_t> getWeightsTable(mlir::Type inElemType, mlir::Type outElemType, Optional<int32_t> weightsPtrOffset,
                                     int32_t weightsPtrStep, Optional<int32_t> sparsityPtrOffset,
                                     int32_t sparsityPtrStep, ArchKind arch, int64_t OC,
                                     mlir::Type weightsElemType = nullptr, Const::ContentAttr bias = nullptr,
                                     VPU::PPETaskAttr ppe = nullptr, vpux::IE::PostOpAttr postOpAttr = nullptr);
std::vector<int32_t> getWeightsTable(mlir::Type inElemType, mlir::Type outElemType, ArrayRef<int32_t> weightPtrs,
                                     ArrayRef<int32_t> sparsityPtrs, ArchKind arch, int64_t OC,
                                     mlir::Type weightsElemType = nullptr, Const::ContentAttr bias = nullptr,
                                     VPU::PPETaskAttr ppe = nullptr, vpux::IE::PostOpAttr postOpAttr = nullptr);

std::vector<int32_t> patchWeightsTableSparsityPtrs(const std::vector<std::int32_t>& weightsTableVals,
                                                   const int32_t sparsityPtrOffset, const int32_t sparsityPtrStep);

SmallVector<int32_t> getInstructionListTable(ArrayRef<int> rangeAttr, ArrayRef<int> shiftAttr, ArrayRef<int> biasAttr);

Shape inferWeightsTableShape(int64_t OC);
Shape inferWeightsSparsityMapShape(ShapeRef dataShape);

mlir::FailureOr<SmallVector<double>> getRescaledBias(Const::ContentAttr biasAttr, mlir::Type inElemType,
                                                     mlir::Type filterElemType, int64_t OC);

double getSparsityRatio(Const::DeclareOp weightsConst);

bool isSparsifiableWeightsOperand(mlir::Value operand);
bool isSuperdenseRequired(const VPU::ArchKind arch, const DimsOrder outOrder, const ShapeRef outShape,
                          const mlir::Type outElemType);
inline VPU::SparsitySupport bitwiseNot(const VPU::SparsitySupport bits) {
    static_assert(sizeof(bits) == sizeof(uint32_t), "VPU::SparsitySupport has unexpected size");
    return static_cast<VPU::SparsitySupport>(~static_cast<uint32_t>(bits));
}

//
// RuntimeSparsityStatsProvider
//

class RuntimeSparsityStatsProvider {
    const double MINIMAL_SPARSITY_THRESHOLD = 0.2;

public:
    RuntimeSparsityStatsProvider(mlir::func::FuncOp func, vpux::Logger log);

    bool containsStatistics() const;
    bool likelySparsityConsumer(mlir::Operation* op, int64_t requestedInputId) const;

private:
    vpux::Logger _logger;
    std::multimap<std::string, IE::SparsityInfoOp> _lookup;
};

}  // namespace NCESparsity

}  // namespace VPU
}  // namespace vpux
