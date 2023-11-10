//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <numeric>
#include "vpux/compiler/dialect/IE/ops.hpp"

namespace vpux {
namespace IE {

SmallVector<int64_t> getBroadcastAxesNumpyBidirectional(ArrayRef<int64_t> inputShape, ArrayRef<int64_t> outputShape);
SmallVector<int64_t> getBroadcastAxesExplicit(ArrayRef<int64_t> axesMapping, ArrayRef<int64_t> outputShape);
Const::DeclareOp createShapeConstForBroadCast(mlir::PatternRewriter& rewriter, mlir::MLIRContext* ctx,
                                              mlir::Location loc, ShapeRef shape);
}  // namespace IE
}  // namespace vpux
