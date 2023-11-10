//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch%" --propagate-op-through-batch-concat %s | FileCheck %s
// REQUIRES: arch-VPUX37XX

// CHECK-LABEL: @PropagateSoftmaxThroughBatchUnrolledMatmul
func.func @PropagateSoftmaxThroughBatchUnrolledMatmul(%arg0: tensor<16x2xf32>, %arg1: tensor<16x2xf32>) -> tensor<2x16x2xf32> {
    %cst = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    %1 = IE.MatMul(%arg0, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %2 = IE.Reshape(%1) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %3 = IE.MatMul(%arg1, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %4 = IE.Reshape(%3) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %5 = IE.Concat(%2, %4) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    %6 = IE.SoftMax(%5) {axisInd = -1 : i64} : tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    return %6 : tensor<2x16x2xf32>

    // CHECK:       [[MATMUL_1:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_1:%.*]] = IE.Reshape([[MATMUL_1]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[MATMUL_2:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_2:%.*]] = IE.Reshape([[MATMUL_2]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[SOFTMAX_1:%.*]] = IE.SoftMax([[RESHAPE_1]]) {axisInd = -1 : i64} : tensor<1x16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[SOFTMAX_2:%.*]] = IE.SoftMax([[RESHAPE_2]]) {axisInd = -1 : i64} : tensor<1x16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[CONCAT:%.*]] = IE.Concat([[SOFTMAX_1]], [[SOFTMAX_2]]) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       return [[CONCAT]] : tensor<2x16x2xf32>
}

// CHECK-LABEL: @NoPropagateSoftmaxForConcatHasNonMatmulInput
func.func @NoPropagateSoftmaxForConcatHasNonMatmulInput(%arg0: tensor<16x2xf32>, %arg1: tensor<16x2xf32>) -> tensor<32x2xf32> {
    %cst = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    %cst_1 = const.Declare tensor<16x2xf32> = dense<1.000000e+00> : tensor<16x2xf32>
    %1 = IE.MatMul(%arg0, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %2 = IE.Add(%arg1, %cst_1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<16x2xf32>, tensor<16x2xf32> -> tensor<16x2xf32>
    %3 = IE.Concat(%1, %2) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<16x2xf32>, tensor<16x2xf32> -> tensor<32x2xf32>
    %4 = IE.SoftMax(%3) {axisInd = -1 : i64} : tensor<32x2xf32> -> tensor<32x2xf32>
    return %4 : tensor<32x2xf32>

    // CHECK:       [[MATMUL:%.*]] = IE.MatMul
    // CHECK:       [[ADD:%.*]] = IE.Add
    // CHECK:       [[CONCAT:%.*]] = IE.Concat([[MATMUL]], [[ADD]]) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<16x2xf32>, tensor<16x2xf32> -> tensor<32x2xf32>
    // CHECK:       [[SOFTMAX:%.*]] = IE.SoftMax([[CONCAT]]) {axisInd = -1 : i64} : tensor<32x2xf32> -> tensor<32x2xf32>
    // CHECK:       return [[SOFTMAX]] : tensor<32x2xf32>
}

// CHECK-LABEL: @NoPropagateForConcatWithoutPerAxisAttr
func.func @NoPropagateForConcatWithoutPerAxisAttr(%arg0: tensor<16x2xf32>, %arg1: tensor<16x2xf32>) -> tensor<2x16x2xf32> {
    %cst = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    %1 = IE.MatMul(%arg0, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %2 = IE.Reshape(%1) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %3 = IE.MatMul(%arg1, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %4 = IE.Reshape(%3) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %5 = IE.Concat(%2, %4) {static_offsets = [[0, 0, 0], [1, 0, 0]]} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    %6 = IE.SoftMax(%5) {axisInd = -1 : i64} : tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    return %6 : tensor<2x16x2xf32>

    // CHECK:       [[MATMUL_1:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_1:%.*]] = IE.Reshape([[MATMUL_1]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[MATMUL_2:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_2:%.*]] = IE.Reshape([[MATMUL_2]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[CONCAT:%.*]] = IE.Concat([[RESHAPE_1]], [[RESHAPE_2]]) {static_offsets = {{\[\[}}0, 0, 0], [1, 0, 0]]} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       [[SOFTMAX:%.*]] = IE.SoftMax([[CONCAT]]) {axisInd = -1 : i64} : tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       return [[SOFTMAX]] : tensor<2x16x2xf32>
}

// CHECK-LABEL: @NoPropagateWhenAxisConflict
func.func @NoPropagateWhenAxisConflict(%arg0: tensor<16x2xf32>, %arg1: tensor<16x2xf32>) -> tensor<2x16x2xf32> {
    %cst = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    %1 = IE.MatMul(%arg0, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %2 = IE.Reshape(%1) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %3 = IE.MatMul(%arg1, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %4 = IE.Reshape(%3) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %5 = IE.Concat(%2, %4) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    %6 = IE.SoftMax(%5) {axisInd = 0 : i64} : tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    return %6 : tensor<2x16x2xf32>

    // CHECK:       [[MATMUL_1:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_1:%.*]] = IE.Reshape([[MATMUL_1]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[MATMUL_2:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_2:%.*]] = IE.Reshape([[MATMUL_2]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[CONCAT:%.*]] = IE.Concat([[RESHAPE_1]], [[RESHAPE_2]]) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       [[SOFTMAX:%.*]] = IE.SoftMax([[CONCAT]]) {axisInd = 0 : i64} : tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       return [[SOFTMAX]] : tensor<2x16x2xf32>
}

// CHECK-LABEL: @PropagateAddSoftmaxThroughBatchUnrolledMatmul
func.func @PropagateAddSoftmaxThroughBatchUnrolledMatmul(%arg0: tensor<16x2xf32>, %arg1: tensor<16x2xf32>) -> tensor<2x16x2xf32> {
    %cst = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    %cst_1 = const.Declare tensor<1x1x1xf32> = dense<1.000000e+00> : tensor<1x1x1xf32>
    %1 = IE.MatMul(%arg0, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %2 = IE.Reshape(%1) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %3 = IE.MatMul(%arg1, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %4 = IE.Reshape(%3) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %5 = IE.Concat(%2, %4) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    %6 = IE.Add(%5, %cst_1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<2x16x2xf32>, tensor<1x1x1xf32> -> tensor<2x16x2xf32>
    %7 = IE.SoftMax(%6) {axisInd = -1 : i64} : tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    return %7 : tensor<2x16x2xf32>

    // CHECK-DAG:   [[CST:%.*]] = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    // CHECK-DAG:   [[CST_1:%.*]] = const.Declare tensor<1x1x1xf32> = dense<1.000000e+00> : tensor<1x1x1xf32>
    // CHECK:       [[MATMUL_1:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_1:%.*]] = IE.Reshape([[MATMUL_1]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[MATMUL_2:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_2:%.*]] = IE.Reshape([[MATMUL_2]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[ADD_1:%.*]] = IE.Add([[RESHAPE_1]], [[CST_1]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x2xf32>, tensor<1x1x1xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[SOFTMAX_1:%.*]] = IE.SoftMax([[ADD_1]]) {axisInd = -1 : i64} : tensor<1x16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[ADD_2:%.*]] = IE.Add([[RESHAPE_2]], [[CST_1]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x2xf32>, tensor<1x1x1xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[SOFTMAX_2:%.*]] = IE.SoftMax([[ADD_2]]) {axisInd = -1 : i64} : tensor<1x16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[CONCAT:%.*]] = IE.Concat([[SOFTMAX_1]], [[SOFTMAX_2]]) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       return [[CONCAT]] : tensor<2x16x2xf32>
}

// CHECK-LABEL: @NoPropagateAddSoftmaxWithInvalidAddShape
func.func @NoPropagateAddSoftmaxWithInvalidAddShape(%arg0: tensor<16x2xf32>, %arg1: tensor<16x2xf32>) -> tensor<2x16x2xf32> {
    %cst = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    %cst1 = const.Declare tensor<2x16x2xf32> = dense<1.000000e+00> : tensor<2x16x2xf32>
    %1 = IE.MatMul(%arg0, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %2 = IE.Reshape(%1) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %3 = IE.MatMul(%arg1, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %4 = IE.Reshape(%3) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %5 = IE.Concat(%2, %4) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    %6 = IE.Add(%5, %cst1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<2x16x2xf32>, tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    %7 = IE.SoftMax(%6) {axisInd = -1 : i64} : tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    return %7 : tensor<2x16x2xf32>

    // CHECK-DAG:   [[CST:%.*]] = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    // CHECK-DAG:   [[CST_1:%.*]] = const.Declare tensor<2x16x2xf32> = dense<1.000000e+00> : tensor<2x16x2xf32>
    // CHECK:       [[MATMUL_1:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_1:%.*]] = IE.Reshape([[MATMUL_1]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[MATMUL_2:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_2:%.*]] = IE.Reshape([[MATMUL_2]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[CONCAT:%.*]] = IE.Concat([[RESHAPE_1]], [[RESHAPE_2]]) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       [[ADD:%.*]] = IE.Add([[CONCAT]], [[CST_1]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<2x16x2xf32>, tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       [[SOFTMAX:%.*]] = IE.SoftMax([[ADD]]) {axisInd = -1 : i64} : tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       return [[SOFTMAX]] : tensor<2x16x2xf32>
}

// CHECK-LABEL: @NoPropagateAddSoftmaxWithInvalidAddSource
func.func @NoPropagateAddSoftmaxWithInvalidAddSource(%arg0: tensor<16x2xf32>, %arg1: tensor<16x2xf32>, %arg2: tensor<1x1x1xf32>) -> tensor<2x16x2xf32> {
    %cst = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    %1 = IE.MatMul(%arg0, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %2 = IE.Reshape(%1) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %3 = IE.MatMul(%arg1, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %4 = IE.Reshape(%3) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    %5 = IE.Concat(%2, %4) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    %6 = IE.Add(%5, %arg2) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<2x16x2xf32>, tensor<1x1x1xf32> -> tensor<2x16x2xf32>
    %7 = IE.SoftMax(%6) {axisInd = -1 : i64} : tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    return %7 : tensor<2x16x2xf32>

    // CHECK:       [[MATMUL_1:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_1:%.*]] = IE.Reshape([[MATMUL_1]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[MATMUL_2:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_2:%.*]] = IE.Reshape([[MATMUL_2]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[CONCAT:%.*]] = IE.Concat([[RESHAPE_1]], [[RESHAPE_2]]) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       [[ADD:%.*]] = IE.Add([[CONCAT]], %arg2) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<2x16x2xf32>, tensor<1x1x1xf32> -> tensor<2x16x2xf32>
    // CHECK:       [[SOFTMAX:%.*]] = IE.SoftMax([[ADD]]) {axisInd = -1 : i64} : tensor<2x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       return [[SOFTMAX]] : tensor<2x16x2xf32>
}

// CHECK-LABEL: @PropagateReshapeThroughBatchUnrolledMatmul
func.func @PropagateReshapeThroughBatchUnrolledMatmul(%arg0: tensor<16x2xf32>, %arg1: tensor<16x2xf32>) -> tensor<2x16x2xf32> {
    %cst = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    %1 = IE.MatMul(%arg0, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %2 = IE.MatMul(%arg1, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %3 = IE.Concat(%1, %2) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<16x2xf32>, tensor<16x2xf32> -> tensor<32x2xf32>
    %4 = IE.Reshape(%3) {shape_value = [2, 16, 2]} : tensor<32x2xf32> -> tensor<2x16x2xf32>
    return %4 : tensor<2x16x2xf32>

    // CHECK:       [[MATMUL_1:%.*]] = IE.MatMul
    // CHECK:       [[MATMUL_2:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_1:%.*]] = IE.Reshape([[MATMUL_1]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[RESHAPE_2:%.*]] = IE.Reshape([[MATMUL_2]]) {shape_value = [1, 16, 2]} : tensor<16x2xf32> -> tensor<1x16x2xf32>
    // CHECK:       [[CONCAT:%.*]] = IE.Concat([[RESHAPE_1]], [[RESHAPE_2]]) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<1x16x2xf32>, tensor<1x16x2xf32> -> tensor<2x16x2xf32>
    // CHECK:       return [[CONCAT]] : tensor<2x16x2xf32>
}

// CHECK-LABEL: @NoPropagateReshapeWithInvalidConcatInputs
func.func @NoPropagateReshapeWithInvalidConcatInputs(%arg0: tensor<16x2xf32>, %arg1: tensor<18x2xf32>) -> tensor<2x17x2xf32> {
    %cst = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    %1 = IE.MatMul(%arg0, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %2 = IE.MatMul(%arg1, %cst) : tensor<18x2xf32>, tensor<2x2xf32> -> tensor<18x2xf32>
    %3 = IE.Concat(%1, %2) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<16x2xf32>, tensor<18x2xf32> -> tensor<34x2xf32>
    %4 = IE.Reshape(%3) {shape_value = [2, 17, 2]} : tensor<34x2xf32> -> tensor<2x17x2xf32>
    return %4 : tensor<2x17x2xf32>

    // CHECK:       [[MATMUL_1:%.*]] = IE.MatMul
    // CHECK:       [[MATMUL_2:%.*]] = IE.MatMul
    // CHECK:       [[CONCAT:%.*]] = IE.Concat([[MATMUL_1]], [[MATMUL_2]]) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<16x2xf32>, tensor<18x2xf32> -> tensor<34x2xf32>
    // CHECK:       [[RESHAPE:%.*]] = IE.Reshape([[CONCAT]]) {shape_value = [2, 17, 2]} : tensor<34x2xf32> -> tensor<2x17x2xf32>
    // CHECK:       return [[RESHAPE]] : tensor<2x17x2xf32>
}

// CHECK-LABEL: @Propagate4DReshapeSoftMaxThroughBatchUnrolledMatmul
func.func @Propagate4DReshapeSoftMaxThroughBatchUnrolledMatmul(%arg0: tensor<16x2xf32>, %arg1: tensor<16x2xf32>) -> tensor<1x2x16x2xf32> {
    %cst = const.Declare tensor<2x2xf32> = dense<1.000000e+00> : tensor<2x2xf32>
    %1 = IE.MatMul(%arg0, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %2 = IE.MatMul(%arg1, %cst) : tensor<16x2xf32>, tensor<2x2xf32> -> tensor<16x2xf32>
    %3 = IE.Concat(%1, %2) {per_axis = #IE.Concat<axis = 0 : i64>} : tensor<16x2xf32>, tensor<16x2xf32> -> tensor<32x2xf32>
    %4 = IE.Reshape(%3) {shape_value = [1, 2, 16, 2]} : tensor<32x2xf32> -> tensor<1x2x16x2xf32>
    %5 = IE.SoftMax(%4) {axisInd = -1 : i64} : tensor<1x2x16x2xf32> -> tensor<1x2x16x2xf32>
    return %5 : tensor<1x2x16x2xf32>

    // CHECK:       [[MATMUL_1:%.*]] = IE.MatMul
    // CHECK:       [[MATMUL_2:%.*]] = IE.MatMul
    // CHECK:       [[RESHAPE_1:%.*]] = IE.Reshape([[MATMUL_1]]) {shape_value = [1, 1, 16, 2]} : tensor<16x2xf32> -> tensor<1x1x16x2xf32>
    // CHECK:       [[RESHAPE_2:%.*]] = IE.Reshape([[MATMUL_2]]) {shape_value = [1, 1, 16, 2]} : tensor<16x2xf32> -> tensor<1x1x16x2xf32>
    // CHECK:       [[SOFTMAX_1:%.*]] = IE.SoftMax([[RESHAPE_1]]) {axisInd = -1 : i64} : tensor<1x1x16x2xf32> -> tensor<1x1x16x2xf32>
    // CHECK:       [[SOFTMAX_2:%.*]] = IE.SoftMax([[RESHAPE_2]]) {axisInd = -1 : i64} : tensor<1x1x16x2xf32> -> tensor<1x1x16x2xf32>
    // CHECK:       [[CONCAT:%.*]] = IE.Concat([[SOFTMAX_1]], [[SOFTMAX_2]]) {per_axis = #IE.Concat<axis = 1 : i64>} : tensor<1x1x16x2xf32>, tensor<1x1x16x2xf32> -> tensor<1x2x16x2xf32>
    // CHECK:       return [[CONCAT]] : tensor<1x2x16x2xf32>
}
