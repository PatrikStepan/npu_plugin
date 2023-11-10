//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch%" --adjust-input-shape-for-eltwise --canonicalize %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

// CHECK-LABEL: @ExpandAddToShapeCastAdd
// CHECK-SAME:        [[INPUT1:%arg[0-9]]]: tensor<1x3x32x32xf16>,
// CHECK-SAME:        [[INPUT2:%arg[0-9]]]: tensor<1x3x32x32xf16>
func.func @ExpandAddToShapeCastAdd(%arg0: tensor<1x3x32x32xf16>, %arg1: tensor<1x3x32x32xf16>) -> tensor<1x16x32x32xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    %1 = IE.Expand(%arg1) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    %2 = IE.Add(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x32x32xf16>, tensor<1x16x32x32xf16> -> tensor<1x16x32x32xf16>
    return %2 : tensor<1x16x32x32xf16>

    // CHECK-NOT:   IE.Expand
    // CHECK-DAG:   [[CAST1:%.+]] = IE.ShapeCast {shape = [1, 16, 16, 12]} inputs([[INPUT1]] : tensor<1x3x32x32xf16>) -> tensor<1x16x16x12xf16>
    // CHECK-DAG:   [[CAST2:%.+]] = IE.ShapeCast {shape = [1, 16, 16, 12]} inputs([[INPUT2]] : tensor<1x3x32x32xf16>) -> tensor<1x16x16x12xf16>

    // CHECK:       [[ADD:%.+]] = IE.Add([[CAST1]], [[CAST2]]) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x16x12xf16>, tensor<1x16x16x12xf16> -> tensor<1x16x16x12xf16>
    // CHECK:       [[CAST_OUTPUT:%.+]] = IE.ShapeCast {shape = [1, 3, 32, 32]} inputs([[ADD]] : tensor<1x16x16x12xf16>) -> tensor<1x3x32x32xf16>
    // CHECK:       [[EXPAND_OUTPUT:%.+]] = IE.Expand([[CAST_OUTPUT]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    // CHECK:       return [[EXPAND_OUTPUT]]
}

// -----

// CHECK-LABEL: @ExpandAddToShapeCastAddWithSlice
// CHECK-SAME:        [[INPUT1:%arg[0-9]]]: tensor<1x3x32x32xf16>,
// CHECK-SAME:        [[INPUT2:%arg[0-9]]]: tensor<1x3x32x32xf16>
func.func @ExpandAddToShapeCastAddWithSlice(%arg0: tensor<1x3x32x32xf16>, %arg1: tensor<1x3x32x32xf16>) -> tensor<1x3x32x32xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    %1 = IE.Expand(%arg1) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    %2 = IE.Add(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x32x32xf16>, tensor<1x16x32x32xf16> -> tensor<1x16x32x32xf16>
    %3 = IE.Slice %2 [0, 0, 0, 0] [1, 3, 32, 32] : tensor<1x16x32x32xf16> to tensor<1x3x32x32xf16>
    return %3 : tensor<1x3x32x32xf16>

    // CHECK-NOT:   IE.Expand
    // CHECK-DAG:   [[CAST1:%.+]] = IE.ShapeCast {shape = [1, 16, 16, 12]} inputs([[INPUT1]] : tensor<1x3x32x32xf16>) -> tensor<1x16x16x12xf16>
    // CHECK-DAG:   [[CAST2:%.+]] = IE.ShapeCast {shape = [1, 16, 16, 12]} inputs([[INPUT2]] : tensor<1x3x32x32xf16>) -> tensor<1x16x16x12xf16>

    // CHECK:       [[ADD:%.+]] = IE.Add([[CAST1]], [[CAST2]]) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x16x12xf16>, tensor<1x16x16x12xf16> -> tensor<1x16x16x12xf16>
    // CHECK:       [[CAST_OUTPUT:%.+]] = IE.ShapeCast {shape = [1, 3, 32, 32]} inputs([[ADD]] : tensor<1x16x16x12xf16>) -> tensor<1x3x32x32xf16>
    // CHECK:       [[EXPAND_OUTPUT:%.+]] = IE.Expand([[CAST_OUTPUT]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    // CHECK:       [[SLICE_OUTPUT:%.+]] = IE.Slice [[EXPAND_OUTPUT]] [0, 0, 0, 0] [1, 3, 32, 32] : tensor<1x16x32x32xf16> to tensor<1x3x32x32xf16>
    // CHECK:       return [[SLICE_OUTPUT]]
}

// -----

!qElemType0 = !quant.uniform<u8:f16, 0.5>
!qElemType1 = !quant.uniform<u8:f16, 0.25>

// CHECK-LABEL: @ExpandAddToShapeCastAddWithQuantizeCast
// CHECK-SAME:        [[INPUT1:%arg[0-9]]]: tensor<1x3x32x32x!qElemType0>,
// CHECK-SAME:        [[INPUT2:%arg[0-9]]]: tensor<1x3x32x32x!qElemType0>
func.func @ExpandAddToShapeCastAddWithQuantizeCast(%arg0: tensor<1x3x32x32x!qElemType0>, %arg1: tensor<1x3x32x32x!qElemType0>) -> tensor<1x16x32x32x!qElemType1> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32x!qElemType0> -> tensor<1x16x32x32x!qElemType0>
    %1 = IE.QuantizeCast(%0) {dstElemType = !qElemType1} :
        tensor<1x16x32x32x!qElemType0> -> tensor<1x16x32x32x!qElemType1>
    %2 = IE.Expand(%arg1) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32x!qElemType0> -> tensor<1x16x32x32x!qElemType0>
    %3 = IE.QuantizeCast(%2) {dstElemType = !qElemType1} :
        tensor<1x16x32x32x!qElemType0> -> tensor<1x16x32x32x!qElemType1>
    %4 = IE.Add(%1, %3) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x32x32x!qElemType1>, tensor<1x16x32x32x!qElemType1> -> tensor<1x16x32x32x!qElemType1>
    return %4 : tensor<1x16x32x32x!qElemType1>

    // CHECK-NOT:   IE.Expand
    // CHECK-DAG:   [[CAST1:%.+]] = IE.ShapeCast {shape = [1, 16, 16, 12]} inputs([[INPUT1]] : tensor<1x3x32x32x!qElemType0>) -> tensor<1x16x16x12x!qElemType0>
    // CHECK-DAG:   [[QUANTIZE1:%.+]] = IE.QuantizeCast([[CAST1]]) {dstElemType = !qElemType1} : tensor<1x16x16x12x!qElemType0> -> tensor<1x16x16x12x!qElemType1>
    // CHECK-DAG:   [[CAST2:%.+]] = IE.ShapeCast {shape = [1, 16, 16, 12]} inputs([[INPUT2]] : tensor<1x3x32x32x!qElemType0>) -> tensor<1x16x16x12x!qElemType0>
    // CHECK-DAG:   [[QUANTIZE2:%.+]] = IE.QuantizeCast([[CAST2]]) {dstElemType = !qElemType1} : tensor<1x16x16x12x!qElemType0> -> tensor<1x16x16x12x!qElemType1>

    // CHECK:       [[ADD:%.+]] = IE.Add([[QUANTIZE1]], [[QUANTIZE2]]) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x16x12x!qElemType1>, tensor<1x16x16x12x!qElemType1> -> tensor<1x16x16x12x!qElemType1>
    // CHECK:       [[CAST_OUTPUT:%.+]] = IE.ShapeCast {shape = [1, 3, 32, 32]} inputs([[ADD]] : tensor<1x16x16x12x!qElemType1>) -> tensor<1x3x32x32x!qElemType1>
    // CHECK:       [[EXPAND_OUTPUT:%.+]] = IE.Expand([[CAST_OUTPUT]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32x!qElemType1> -> tensor<1x16x32x32x!qElemType1>
    // CHECK:       return [[EXPAND_OUTPUT]]
}

// -----

// CHECK-LABEL: @ExpandAddUnsupportedShape
// CHECK-SAME:        [[INPUT1:%arg[0-9]]]: tensor<1x3x11x11xf16>,
// CHECK-SAME:        [[INPUT2:%arg[0-9]]]: tensor<1x3x11x11xf16>
func.func @ExpandAddUnsupportedShape(%arg0: tensor<1x3x11x11xf16>, %arg1: tensor<1x3x11x11xf16>) -> tensor<1x16x11x11xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x11x11xf16> -> tensor<1x16x11x11xf16>
    %1 = IE.Expand(%arg1) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x11x11xf16> -> tensor<1x16x11x11xf16>
    %2 = IE.Add(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x11x11xf16>, tensor<1x16x11x11xf16> -> tensor<1x16x11x11xf16>
    return %2 : tensor<1x16x11x11xf16>

    // Nothing should be changed
    // the total size 3x11x11 is not divisible by the alignment 16
    // expansion is necessary
    // CHECK-NOT:   IE.ShapeCast
    // CHECK-DAG:   [[EXPAND1:%.+]] = IE.Expand([[INPUT1]])
    // CHECK-DAG:   [[EXPAND2:%.+]] = IE.Expand([[INPUT2]])

    // CHECK:       [[ADD:%.+]] = IE.Add([[EXPAND1]], [[EXPAND2]])
    // CHECK:       return [[ADD]]
}

// -----

// CHECK-LABEL: @ExpandAddUnsupportedInput
// CHECK-SAME:        [[INPUT1:%arg[0-9]]]: tensor<1x3x32x32xf16>,
// CHECK-SAME:        [[INPUT2:%arg[0-9]]]: tensor<1x16x32x32xf16>
func.func @ExpandAddUnsupportedInput(%arg0: tensor<1x3x32x32xf16>, %arg1: tensor<1x16x32x32xf16>) -> tensor<1x16x32x32xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    %2 = IE.Add(%0, %arg1) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x32x32xf16>, tensor<1x16x32x32xf16> -> tensor<1x16x32x32xf16>
    return %2 : tensor<1x16x32x32xf16>

    // Nothing should be changed
    // cases are not supported when any of the inputs is not expand
    // CHECK-NOT:   IE.ShapeCast
    // CHECK-DAG:   [[EXPAND1:%.+]] = IE.Expand([[INPUT1]])
    // CHECK:       [[ADD:%.+]] = IE.Add([[EXPAND1]], [[INPUT2]])
    // CHECK:       return [[ADD]]
}

// -----

// CHECK-LABEL: @ExpandGroupConvToShapeCastGroupConv
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x3x32x32xf16>
func.func @ExpandGroupConvToShapeCastGroupConv(%arg0: tensor<1x3x32x32xf16>) -> tensor<1x16x32x32xf16> {
    %filters = const.Declare tensor<16x1x1x1xf32> = dense<1.0> : tensor<1x1x1x1xf32>, [#const.Broadcast<0 : i64, 16 : i64>]
    %bias = const.Declare tensor<1x16x1x1xf32> = dense<0.0> : tensor<1x1x1x1xf32>, [#const.Broadcast<1 : i64, 16 : i64>]
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    %2 = IE.GroupConvolution(%0, %filters, %bias) {dilations = [1, 1], groups = 16 : i64, pads_begin = [0, 0], pads_end = [0, 0], strides = [1, 1]} : tensor<1x16x32x32xf16>, tensor<16x1x1x1xf32>, tensor<1x16x1x1xf32> -> tensor<1x16x32x32xf16>
    return %2 : tensor<1x16x32x32xf16>

    // CHECK-DAG:   [[BIAS:%.+]] = const.Declare tensor<1x16x1x1xf32> = dense<0.000000e+00> : tensor<1x1x1x1xf32>, [#const.Broadcast<1 : i64, 16 : i64>, #const.Reshape<[1, 16, 1, 1]>]
    // CHECK-DAG:   [[FILTER:%.+]] = const.Declare tensor<16x1x1x1xf32> = dense<1.000000e+00> : tensor<1x1x1x1xf32>, [#const.Broadcast<0 : i64, 16 : i64>, #const.Reshape<[16, 1, 1, 1]>]

    // CHECK-NOT:   IE.Expand
    // CHECK:       [[SHAPE_CAST_IN:%.+]] = IE.ShapeCast {shape = [1, 16, 16, 12]} inputs([[INPUT]] : tensor<1x3x32x32xf16>) -> tensor<1x16x16x12xf16>
    // CHECK:       [[GROUP_CONV:%.+]] = IE.GroupConvolution([[SHAPE_CAST_IN]], [[FILTER]], [[BIAS]])
    // CHECK-SAME:      dilations = [1, 1], groups = 16 : i64, pads_begin = [0, 0], pads_end = [0, 0], strides = [1, 1]
    // CHECK-SAME:          -> tensor<1x16x16x12xf16>
    // CHECK:       [[SHAPE_CAST_OUT:%.+]] = IE.ShapeCast {shape = [1, 3, 32, 32]} inputs([[GROUP_CONV]] : tensor<1x16x16x12xf16>) -> tensor<1x3x32x32xf16>
    // CHECK:       [[EXPAND:%.+]] = IE.Expand([[SHAPE_CAST_OUT]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>

    // CHECK:       return [[EXPAND]]
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @ExpandOverWAddToShapeCastAdd
// CHECK-SAME:        [[INPUT1:%arg[0-9]]]: tensor<1x16x32x22xf16, {order = #NHWC}>
// CHECK-SAME:        [[INPUT2:%arg[0-9]]]: tensor<1x16x32x22xf16, {order = #NHWC}>
func.func @ExpandOverWAddToShapeCastAdd(%arg0: tensor<1x16x32x22xf16, {order = #NHWC}>, %arg1: tensor<1x16x32x22xf16, {order = #NHWC}>) -> tensor<1x16x32x32xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 10]} : tensor<1x16x32x22xf16, {order = #NHWC}> -> tensor<1x16x32x32xf16, {order = #NHWC}>
    %1 = IE.Expand(%arg1) {pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 10]} : tensor<1x16x32x22xf16, {order = #NHWC}> -> tensor<1x16x32x32xf16, {order = #NHWC}>
    %2 = IE.Add(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x32x32xf16, {order = #NHWC}>, tensor<1x16x32x32xf16, {order = #NHWC}>
            -> tensor<1x16x32x32xf16>
    return %2 : tensor<1x16x32x32xf16>

    // Nothing should be changed
    // Eltwise ops with different input and output layouts are not supported
    // CHECK:       [[EXPAND_1:%.+]] = IE.Expand([[INPUT1]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 10]} : tensor<1x16x32x22xf16, {order = #NHWC}> -> tensor<1x16x32x32xf16, {order = #NHWC}>
    // CHECK:       [[EXPAND_2:%.+]] = IE.Expand([[INPUT2]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 10]} : tensor<1x16x32x22xf16, {order = #NHWC}> -> tensor<1x16x32x32xf16, {order = #NHWC}>
    // CHECK:       [[ADD:%.+]] = IE.Add([[EXPAND_1]], [[EXPAND_2]]) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x32x32xf16, {order = #NHWC}>, tensor<1x16x32x32xf16, {order = #NHWC}> -> tensor<1x16x32x32xf16>
    // CHECK:       return [[ADD]] : tensor<1x16x32x32xf16>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @AddWithConstInput
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x512xf16, {order = #NHWC}>
func.func @AddWithConstInput(%arg0: tensor<1x1x1x512xf16, {order = #NHWC}>) -> tensor<1x16x1x512xf16, {order = #NHWC}> {
    %cst = const.Declare tensor<1x16x1x512xf16, {order = #NHWC}> = dense<1.0> : tensor<512xf16>, [#const.Reshape<[1, 1, 1, 512]>, #const.Reorder<#NHWC>, #const.PadWithZero<[0, 0, 0, 0], [0, 15, 0, 0]>]
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    %1 = IE.Add(%0, %cst) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x1x512xf16, {order = #NHWC}>, tensor<1x16x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    return %1 : tensor<1x16x1x512xf16, {order = #NHWC}>

    // CHECK-DAG:       [[CST:%.+]] = const.Declare tensor<1x16x8x4xf16, {order = #NHWC}> = dense<1.000000e+00> : tensor<512xf16>, [#const.Reshape<[1, 1, 1, 512]>, #const.Reorder<#NHWC>, #const.Reshape<[1, 16, 8, 4]>]
    // CHECK:       [[SHAPE_CAST:%.+]] = IE.ShapeCast {shape = [1, 16, 8, 4]} inputs([[INPUT]] : tensor<1x1x1x512xf16, {order = #NHWC}>) -> tensor<1x16x8x4xf16, {order = #NHWC}>
    // CHECK:       [[ADD:%.+]] = IE.Add([[SHAPE_CAST]], [[CST]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x8x4xf16, {order = #NHWC}>, tensor<1x16x8x4xf16, {order = #NHWC}> -> tensor<1x16x8x4xf16, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_OUT:%.+]] = IE.ShapeCast {shape = [1, 1, 1, 512]} inputs([[ADD]] : tensor<1x16x8x4xf16, {order = #NHWC}>) -> tensor<1x1x1x512xf16, {order = #NHWC}>
    // CHECK:       [[EXPAND:%.+]] = IE.Expand([[SHAPE_CAST_OUT]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    // CHECK:       return [[EXPAND]] : tensor<1x16x1x512xf16, {order = #NHWC}>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @AddWithUnsupportedConstInput
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x512xf16, {order = #NHWC}>
func.func @AddWithUnsupportedConstInput(%arg0: tensor<1x1x1x512xf16, {order = #NHWC}>) -> tensor<1x16x1x512xf16, {order = #NHWC}> {
    %cst = const.Declare tensor<1x16x1x512xf16, {order = #NHWC}> = dense<1.0> : tensor<1x8x1x512xf16>, [#const.Reorder<#NHWC>, #const.PadWithZero<[0, 0, 0, 0], [0, 8, 0, 0]>]
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    %1 = IE.Add(%0, %cst) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x1x512xf16, {order = #NHWC}>, tensor<1x16x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    return %1 : tensor<1x16x1x512xf16, {order = #NHWC}>

    // Nothing should be changed
    // cases are not supported when the constant input's real size doesn't equal to another input's size
    // CHECK-DAG:       [[CST:%.+]] = const.Declare tensor<1x16x1x512xf16, {order = #NHWC}> = dense<1.000000e+00> : tensor<1x8x1x512xf16>, [#const.Reorder<#NHWC>, #const.PadWithZero<[0, 0, 0, 0], [0, 8, 0, 0]>]
    // CHECK:       [[EXPAND:%.+]] = IE.Expand([[INPUT]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    // CHECK:       [[ADD:%.+]] = IE.Add([[EXPAND]], [[CST]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x1x512xf16, {order = #NHWC}>, tensor<1x16x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    // CHECK:       return [[ADD]] : tensor<1x16x1x512xf16, {order = #NHWC}>
}

// -----

// CHECK-LABEL: @SharedWeightsGroupConv
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x3x32x32xf16>
// CHECK-SAME:        [[INPUT_1:%arg[0-9]]]: tensor<1x16x32x32xf16>
func.func @SharedWeightsGroupConv(%arg0: tensor<1x3x32x32xf16>, %arg1: tensor<1x16x32x32xf16>) -> (tensor<1x16x32x32xf16>, tensor<1x16x32x32xf16>) {
    %filters = const.Declare tensor<16x1x1x1xf32> = dense<1.0> : tensor<1x1x1x1xf32>, [#const.Broadcast<0 : i64, 16 : i64>]
    %bias = const.Declare tensor<1x16x1x1xf32> = dense<0.0> : tensor<1x1x1x1xf32>, [#const.Broadcast<1 : i64, 16 : i64>]
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    %2 = IE.GroupConvolution(%0, %filters, %bias) {dilations = [1, 1], groups = 16 : i64, pads_begin = [0, 0], pads_end = [0, 0], strides = [1, 1]} : tensor<1x16x32x32xf16>, tensor<16x1x1x1xf32>, tensor<1x16x1x1xf32> -> tensor<1x16x32x32xf16>
    %3 = IE.GroupConvolution(%arg1, %filters, %bias) {dilations = [1, 1], groups = 16 : i64, pads_begin = [0, 0], pads_end = [0, 0], strides = [1, 1]} : tensor<1x16x32x32xf16>, tensor<16x1x1x1xf32>, tensor<1x16x1x1xf32> -> tensor<1x16x32x32xf16>
    return %2, %3 : tensor<1x16x32x32xf16>, tensor<1x16x32x32xf16>

    // CHECK-DAG:   [[BIAS_0:%.+]] = const.Declare tensor<1x16x1x1xf32> = dense<0.000000e+00> : tensor<1x1x1x1xf32>, [#const.Broadcast<1 : i64, 16 : i64>, #const.Reshape<[1, 16, 1, 1]>]
    // CHECK-DAG:   [[FILTER_0:%.+]] = const.Declare tensor<16x1x1x1xf32> = dense<1.000000e+00> : tensor<1x1x1x1xf32>, [#const.Broadcast<0 : i64, 16 : i64>, #const.Reshape<[16, 1, 1, 1]>]

    // CHECK-DAG:   [[BIAS_1:%.+]] = const.Declare tensor<1x16x1x1xf32> = dense<0.000000e+00> : tensor<1x1x1x1xf32>, [#const.Broadcast<1 : i64, 16 : i64>]
    // CHECK-DAG:   [[FILTER_1:%.+]] = const.Declare tensor<16x1x1x1xf32> = dense<1.000000e+00> : tensor<1x1x1x1xf32>, [#const.Broadcast<0 : i64, 16 : i64>]

    // CHECK-NOT:   IE.Expand
    // CHECK:       [[SHAPE_CAST_IN:%.+]] = IE.ShapeCast {shape = [1, 16, 16, 12]} inputs([[INPUT]] : tensor<1x3x32x32xf16>) -> tensor<1x16x16x12xf16>
    // CHECK:       [[GROUP_CONV:%.+]] = IE.GroupConvolution([[SHAPE_CAST_IN]], [[FILTER_0]], [[BIAS_0]])
    // CHECK-SAME:      dilations = [1, 1], groups = 16 : i64, pads_begin = [0, 0], pads_end = [0, 0], strides = [1, 1]
    // CHECK-SAME:          -> tensor<1x16x16x12xf16>
    // CHECK:       [[SHAPE_CAST_OUT:%.+]] = IE.ShapeCast {shape = [1, 3, 32, 32]} inputs([[GROUP_CONV]] : tensor<1x16x16x12xf16>) -> tensor<1x3x32x32xf16>
    // CHECK:       [[EXPAND:%.+]] = IE.Expand([[SHAPE_CAST_OUT]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>

    // CHECK:       [[GROUP_CONV_1:%.+]] = IE.GroupConvolution([[INPUT_1]], [[FILTER_1]], [[BIAS_1]])
    // CHECK-SAME:      dilations = [1, 1], groups = 16 : i64, pads_begin = [0, 0], pads_end = [0, 0], strides = [1, 1]
    // CHECK-SAME:          -> tensor<1x16x32x32xf16>

    // CHECK:       return [[EXPAND]], [[GROUP_CONV_1:%.+]]
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @AdjustInputShapeForPermuteQuantize
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1280x8x8xf16>
func.func @AdjustInputShapeForPermuteQuantize(%arg0: tensor<1x1280x8x8xf16>) -> tensor<1x1280x8x8xf16, {order = #NHWC}> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 8]} : tensor<1x1280x8x8xf16> -> tensor<1x1280x8x16xf16>
    %1 = IE.PermuteQuantize(%0) {dstElemType = f16, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x1280x8x16xf16> -> tensor<1x1280x8x16xf16, {order = #NHWC}>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 1280, 8, 8] : tensor<1x1280x8x16xf16, {order = #NHWC}> to tensor<1x1280x8x8xf16, {order = #NHWC}>
    return %2 : tensor<1x1280x8x8xf16, {order = #NHWC}>

    // CHECK-NOT:   IE.Expand
    // CHECK-DAG:   [[CAST:%.+]] = IE.ShapeCast {shape = [1, 1280, 4, 16]} inputs(%arg0 : tensor<1x1280x8x8xf16>) -> tensor<1x1280x4x16xf16>
    // CHECK:       [[PERMUTEQUANTIZE:%.+]] = IE.PermuteQuantize([[CAST]]) {dstElemType = f16, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x1280x4x16xf16> -> tensor<1x1280x4x16xf16, {order = #NHWC}>
    // CHECK:       [[CAST_OUTPUT:%.+]] = IE.ShapeCast {shape = [1, 1280, 8, 8]} inputs([[PERMUTEQUANTIZE]] : tensor<1x1280x4x16xf16, {order = #NHWC}>) -> tensor<1x1280x8x8xf16, {order = #NHWC}>
    // CHECK:       [[EXPAND_OUTPUT:%.+]] = IE.Expand([[CAST_OUTPUT]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 8]} : tensor<1x1280x8x8xf16, {order = #NHWC}> -> tensor<1x1280x8x16xf16, {order = #NHWC}>
    // CHECK:       [[SLICE_OUTPUT:%.+]] = IE.Slice [[EXPAND_OUTPUT]] [0, 0, 0, 0] [1, 1280, 8, 8] : tensor<1x1280x8x16xf16, {order = #NHWC}> to tensor<1x1280x8x8xf16, {order = #NHWC}>
    // CHECK:       return [[SLICE_OUTPUT]]
}

// -----

#NHCW = affine_map<(d0, d1, d2, d3) -> (d0, d2, d1, d3)>

// CHECK-LABEL: @NotAdjustInputShapeForPermuteQuantizeWithInvalidLayouts
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1280x8x8xf16>
func.func @NotAdjustInputShapeForPermuteQuantizeWithInvalidLayouts(%arg0: tensor<1x1280x8x8xf16>) -> tensor<1x1280x8x8xf16, {order = #NHCW}> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 8]} : tensor<1x1280x8x8xf16> -> tensor<1x1280x8x16xf16>
    %1 = IE.PermuteQuantize(%0) {dstElemType = f16, dst_order = #NHCW, mem_perm = #NHCW, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x1280x8x16xf16> -> tensor<1x1280x8x16xf16, {order = #NHCW}>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 1280, 8, 8] : tensor<1x1280x8x16xf16, {order = #NHCW}> to tensor<1x1280x8x8xf16, {order = #NHCW}>
    return %2 : tensor<1x1280x8x8xf16, {order = #NHCW}>

    // Nothing should be changed
    // cases are not supported when the layouts are invalid
    // CHECK-NOT:   IE.ShapeCast
    // CHECK-DAG:   [[EXPAND:%.+]] = IE.Expand(%arg0)
    // CHECK:       [[PERMUTEQUANTIZE:%.+]] = IE.PermuteQuantize([[EXPAND]]) {dstElemType = f16, dst_order = #NHCW, mem_perm = #NHCW, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x1280x8x16xf16> -> tensor<1x1280x8x16xf16, {order = #NHCW}>
    // CHECK:       [[SLICE:%.+]] = IE.Slice [[PERMUTEQUANTIZE]] [0, 0, 0, 0] [1, 1280, 8, 8] : tensor<1x1280x8x16xf16, {order = #NHCW}> to tensor<1x1280x8x8xf16, {order = #NHCW}>
    // CHECK:       return [[SLICE]]
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @NotAdjustInputShapeForPermuteQuantizeWithInvalidExpand
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x3x384x672xf16>
func.func @NotAdjustInputShapeForPermuteQuantizeWithInvalidExpand(%arg0: tensor<1x3x384x672xf16>) -> tensor<1x16x384x672xf16, {order = #NHWC}> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x384x672xf16> -> tensor<1x16x384x672xf16>
    %1 = IE.PermuteQuantize(%0) {dstElemType = f16, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x16x384x672xf16> -> tensor<1x16x384x672xf16, {order = #NHWC}>
    return %1 : tensor<1x16x384x672xf16, {order = #NHWC}>

    // Nothing should be changed as it should only handle width expanding
    // CHECK-NOT:   IE.ShapeCast
    // CHECK-DAG:   [[EXPAND:%.+]] = IE.Expand(%arg0)
    // CHECK:       [[PERMUTEQUANTIZE:%.+]] = IE.PermuteQuantize([[EXPAND]]) {dstElemType = f16, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x16x384x672xf16> -> tensor<1x16x384x672xf16, {order = #NHWC}>
    // CHECK:       return [[PERMUTEQUANTIZE]]
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @AddWithConstInputSingleConst
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x512xf16, {order = #NHWC}>
func.func @AddWithConstInputSingleConst(%arg0: tensor<1x1x1x512xf16, {order = #NHWC}>) -> tensor<1x16x1x512xf16, {order = #NHWC}> {
    %cst = const.Declare tensor<1x16x1x512xf16, {order = #NHWC}> = dense<1.0> : tensor<1x1x1x1xf16>, [#const.Broadcast<3 : i64, 512 : i64>, #const.Reorder<#NHWC>, #const.PadWithZero<[0, 0, 0, 0], [0, 15, 0, 0]>]
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    %1 = IE.Add(%0, %cst) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x1x512xf16, {order = #NHWC}>, tensor<1x16x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    return %1 : tensor<1x16x1x512xf16, {order = #NHWC}>

    // CHECK-DAG:       [[CST:%.+]] = const.Declare tensor<1x16x8x4xf16, {order = #NHWC}> = dense<1.000000e+00> : tensor<1x1x1x1xf16>, [#const.Broadcast<1 : i64, 16 : i64>, #const.Broadcast<2 : i64, 8 : i64>, #const.Broadcast<3 : i64, 4 : i64>, #const.Reorder<#NHWC>]
    // CHECK:       [[SHAPE_CAST:%.+]] = IE.ShapeCast {shape = [1, 16, 8, 4]} inputs(%arg0 : tensor<1x1x1x512xf16, {order = #NHWC}>) -> tensor<1x16x8x4xf16, {order = #NHWC}>
    // CHECK:       [[ADD:%.+]] = IE.Add([[SHAPE_CAST]], [[CST]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x8x4xf16, {order = #NHWC}>, tensor<1x16x8x4xf16, {order = #NHWC}> -> tensor<1x16x8x4xf16, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_OUT:%.+]] = IE.ShapeCast {shape = [1, 1, 1, 512]} inputs([[ADD]] : tensor<1x16x8x4xf16, {order = #NHWC}>) -> tensor<1x1x1x512xf16, {order = #NHWC}>
    // CHECK:       [[EXPAND:%.+]] = IE.Expand([[SHAPE_CAST_OUT]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    // CHECK:       return [[EXPAND]] : tensor<1x16x1x512xf16, {order = #NHWC}>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @AddWithConstInputSingleConst1
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x512xf16, {order = #NHWC}>
func.func @AddWithConstInputSingleConst1(%arg0: tensor<1x1x1x512xf16, {order = #NHWC}>) -> tensor<1x16x1x512xf16, {order = #NHWC}> {
    %cst = const.Declare tensor<1x16x1x512xf16, {order = #NHWC}> = dense<1.0> : tensor<1xf16>, [#const.Reshape<[1, 1, 1, 1]>, #const.Broadcast<3 : i64, 512 : i64>, #const.Reorder<#NHWC>, #const.PadWithZero<[0, 0, 0, 0], [0, 15, 0, 0]>]
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    %1 = IE.Add(%0, %cst) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x1x512xf16, {order = #NHWC}>, tensor<1x16x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    return %1 : tensor<1x16x1x512xf16, {order = #NHWC}>

    // CHECK-DAG:       [[CST:%.+]] = const.Declare tensor<1x16x8x4xf16, {order = #NHWC}> = dense<1.000000e+00> : tensor<1xf16>, [#const.Reshape<[1, 1, 1, 1]>, #const.Broadcast<1 : i64, 16 : i64>, #const.Broadcast<2 : i64, 8 : i64>, #const.Broadcast<3 : i64, 4 : i64>, #const.Reorder<#NHWC>]
    // CHECK:       [[SHAPE_CAST:%.+]] = IE.ShapeCast {shape = [1, 16, 8, 4]} inputs(%arg0 : tensor<1x1x1x512xf16, {order = #NHWC}>) -> tensor<1x16x8x4xf16, {order = #NHWC}>
    // CHECK:       [[ADD:%.+]] = IE.Add([[SHAPE_CAST]], [[CST]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x8x4xf16, {order = #NHWC}>, tensor<1x16x8x4xf16, {order = #NHWC}> -> tensor<1x16x8x4xf16, {order = #NHWC}>
    // CHECK:       [[SHAPE_CAST_OUT:%.+]] = IE.ShapeCast {shape = [1, 1, 1, 512]} inputs([[ADD]] : tensor<1x16x8x4xf16, {order = #NHWC}>) -> tensor<1x1x1x512xf16, {order = #NHWC}>
    // CHECK:       [[EXPAND:%.+]] = IE.Expand([[SHAPE_CAST_OUT]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x512xf16, {order = #NHWC}> -> tensor<1x16x1x512xf16, {order = #NHWC}>
    // CHECK:       return [[EXPAND]] : tensor<1x16x1x512xf16, {order = #NHWC}>
}

// -----

// CHECK-LABEL: @NotAdjustGroupConvToShapeCastGroupConv
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x3x32x32xf16>
func.func @NotAdjustGroupConvToShapeCastGroupConv(%arg0: tensor<1x3x32x32xf16>) -> tensor<1x16x31x31xf16> {
    %filters = const.Declare tensor<16x1x2x2xf32> = dense<1.0> : tensor<1x1x1x1xf32>, [#const.Broadcast<0 : i64, 16 : i64>, #const.Broadcast<2 : i64, 2 : i64>, #const.Broadcast<3 : i64, 2 : i64>]
    %bias = const.Declare tensor<1x16x1x1xf32> = dense<0.0> : tensor<1x1x1x1xf32>, [#const.Broadcast<1 : i64, 16 : i64>]
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    %2 = IE.GroupConvolution(%0, %filters, %bias) {dilations = [1, 1], groups = 16 : i64, pads_begin = [0, 0], pads_end = [0, 0], strides = [1, 1]} : tensor<1x16x32x32xf16>, tensor<16x1x2x2xf32>, tensor<1x16x1x1xf32> -> tensor<1x16x31x31xf16>
    return %2 : tensor<1x16x31x31xf16>

    // CHECK-DAG:   [[FILTER:%.+]] = const.Declare tensor<16x1x2x2xf32> = dense<1.000000e+00> : tensor<1x1x1x1xf32>, [#const.Broadcast<0 : i64, 16 : i64>, #const.Broadcast<2 : i64, 2 : i64>, #const.Broadcast<3 : i64, 2 : i64>]
    // CHECK-DAG:   [[BIAS:%.+]] = const.Declare tensor<1x16x1x1xf32> = dense<0.000000e+00> : tensor<1x1x1x1xf32>, [#const.Broadcast<1 : i64, 16 : i64>]
    // CHECK:       [[EXPAND:%.+]] = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    // CHECK:       [[GROUP_CONV:%.+]] = IE.GroupConvolution([[EXPAND]], [[FILTER]], [[BIAS]]) {dilations = [1, 1], groups = 16 : i64, pads_begin = [0, 0], pads_end = [0, 0], strides = [1, 1]} : tensor<1x16x32x32xf16>, tensor<16x1x2x2xf32>, tensor<1x16x1x1xf32> -> tensor<1x16x31x31xf16>
    // CHECK:       return [[GROUP_CONV]] : tensor<1x16x31x31xf16>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!qElemType0 = !quant.uniform<u8:f16, 0.5>
!qElemType1 = !quant.uniform<u8:f16, 0.25>

// CHECK-LABEL: @ExpandWithQuantizeDequantizeSequence
// CHECK-SAME:        [[INPUT1:%arg[0-9]]]: tensor<1x1x44x44xf16, {order = #NHWC}>
func.func @ExpandWithQuantizeDequantizeSequence(%arg0: tensor<1x1x44x44xf16, {order = #NHWC}>) -> tensor<1x1x44x44xf16, {order = #NHWC}> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x44x44xf16, {order = #NHWC}> -> tensor<1x16x44x44xf16, {order = #NHWC}>
    %1 = IE.Add(%0, %0) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x44x44xf16, {order = #NHWC}>, tensor<1x16x44x44xf16, {order = #NHWC}> -> tensor<1x16x44x44x!qElemType0, {order = #NHWC}>
    %2 = IE.QuantizeCast(%1) {dstElemType = !qElemType1} : tensor<1x16x44x44x!qElemType0, {order = #NHWC}> -> tensor<1x16x44x44x!qElemType1, {order = #NHWC}>
    %3 = IE.Add(%2, %2) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x44x44x!qElemType1, {order = #NHWC}>, tensor<1x16x44x44x!qElemType1, {order = #NHWC}> -> tensor<1x16x44x44xf16, {order = #NHWC}>
    %4 = IE.Slice %3 [0, 0, 0, 0] [1, 1, 44, 44] : tensor<1x16x44x44xf16, {order = #NHWC}> to tensor<1x1x44x44xf16, {order = #NHWC}>
    return %4 : tensor<1x1x44x44xf16, {order = #NHWC}>


    // CHECK-NOT:   IE.Expand
    // CHECK-DAG:   [[CAST1:%.+]] = IE.ShapeCast {shape = [1, 16, 11, 11]} inputs(%arg0 : tensor<1x1x44x44xf16, {order = #NHWC}>) -> tensor<1x16x11x11xf16, {order = #NHWC}>
    // CHECK-DAG:   [[ADD1:%.+]] = IE.Add([[CAST1]], [[CAST1]]) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x11x11xf16, {order = #NHWC}>, tensor<1x16x11x11xf16, {order = #NHWC}> -> tensor<1x16x11x11x!qElemType0, {order = #NHWC}>
    // CHECK-DAG:   [[QUANTIZE:%.+]] = IE.QuantizeCast([[ADD1]]) {dstElemType = !qElemType1} : tensor<1x16x11x11x!qElemType0, {order = #NHWC}> -> tensor<1x16x11x11x!qElemType1, {order = #NHWC}>
    // CHECK:       [[ADD2:%.+]] = IE.Add([[QUANTIZE]], [[QUANTIZE]]) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x11x11x!qElemType1, {order = #NHWC}>, tensor<1x16x11x11x!qElemType1, {order = #NHWC}> -> tensor<1x16x11x11xf16, {order = #NHWC}>
    // CHECK:       [[CAST2:%.+]] = IE.ShapeCast {shape = [1, 1, 44, 44]} inputs([[ADD2]] : tensor<1x16x11x11xf16, {order = #NHWC}>) -> tensor<1x1x44x44xf16, {order = #NHWC}>
    // CHECK:       [[EXPAND:%.+]] = IE.Expand([[CAST2]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x44x44xf16, {order = #NHWC}> -> tensor<1x16x44x44xf16, {order = #NHWC}>
    // CHECK:       [[SLICE:%.+]] = IE.Slice [[EXPAND]] [0, 0, 0, 0] [1, 1, 44, 44] : tensor<1x16x44x44xf16, {order = #NHWC}> to tensor<1x1x44x44xf16, {order = #NHWC}>
    // CHECK:       return [[SLICE]]
}

// -----

// CHECK-LABEL: @AdjustAvgPoolingToShapeCastAvgPooling
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x2048xf16>
func.func @AdjustAvgPoolingToShapeCastAvgPooling(%arg0: tensor<1x1x1x2048xf16>) -> tensor<1x1x1x2048xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x2048xf16> -> tensor<1x16x1x2048xf16>
    %1 = IE.AvgPool(%0) {exclude_pads, kernel_size = [1, 1], pads_begin = [0, 0], pads_end = [0, 0], post_op = #IE.PostOp<name = "IE.LeakyRelu", attrs = {negative_slope = 0.10000000149011612 : f64}>, rounding_type = #IE.rounding_type<FLOOR>, strides = [1, 1]} : tensor<1x16x1x2048xf16> -> tensor<1x16x1x2048xf16>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 1, 1, 2048] : tensor<1x16x1x2048xf16> to tensor<1x1x1x2048xf16>
    return %2 : tensor<1x1x1x2048xf16>

    // CHECK:   [[SHAPECAST0:%.+]] = IE.ShapeCast {shape = [1, 16, 16, 8]} inputs(%arg0 : tensor<1x1x1x2048xf16>) -> tensor<1x16x16x8xf16>
    // CHECK:   [[POOLING:%.+]] = IE.AvgPool([[SHAPECAST0]])
    // CHECK:   [[SHAPECAST0:%.+]] = IE.ShapeCast {shape = [1, 1, 1, 2048]} inputs([[POOLING]] : tensor<1x16x16x8xf16>) -> tensor<1x1x1x2048xf16>
    // CHECK:   [[EXPAND:%.+]] = IE.Expand([[SHAPECAST0]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x2048xf16> -> tensor<1x16x1x2048xf16>
    // CHECK:   [[SLICE:%.+]] = IE.Slice [[EXPAND]] [0, 0, 0, 0] [1, 1, 1, 2048] : tensor<1x16x1x2048xf16> to tensor<1x1x1x2048xf16>
    // CHECK:       return [[SLICE]] : tensor<1x1x1x2048xf16>
}

// -----

// CHECK-LABEL: @NotAdjustAvgPoolingWrongKernel
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x2048xf16>
func.func @NotAdjustAvgPoolingWrongKernel(%arg0: tensor<1x1x1x2048xf16>) -> tensor<1x1x1x2047xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x2048xf16> -> tensor<1x16x1x2048xf16>
    %1 = IE.AvgPool(%0) {exclude_pads, kernel_size = [1, 2], pads_begin = [0, 0], pads_end = [0, 0], post_op = #IE.PostOp<name = "IE.LeakyRelu", attrs = {negative_slope = 0.10000000149011612 : f64}>, rounding_type = #IE.rounding_type<FLOOR>, strides = [1, 1]} : tensor<1x16x1x2048xf16> -> tensor<1x16x1x2047xf16>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 1, 1, 2047] : tensor<1x16x1x2047xf16> to tensor<1x1x1x2047xf16>
    return %2 : tensor<1x1x1x2047xf16>

    // CHECK:   [[EXPAND:%.+]] = IE.Expand
    // CHECK:   [[POOLING:%.+]] = IE.AvgPool
    // CHECK:   [[SLICE:%.+]] = IE.Slice
    // CHECK:       return [[SLICE]] : tensor<1x1x1x2047xf16>
}


// -----

// CHECK-LABEL: @NotAdjustAvgPoolingWrongPad
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x2048xf16>
func.func @NotAdjustAvgPoolingWrongPad(%arg0: tensor<1x1x1x2048xf16>) -> tensor<1x1x2x2049xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x2048xf16> -> tensor<1x16x1x2048xf16>
    %1 = IE.AvgPool(%0) {exclude_pads, kernel_size = [1, 1], pads_begin = [1, 0], pads_end = [0, 1], post_op = #IE.PostOp<name = "IE.LeakyRelu", attrs = {negative_slope = 0.10000000149011612 : f64}>, rounding_type = #IE.rounding_type<FLOOR>, strides = [1, 1]} : tensor<1x16x1x2048xf16> -> tensor<1x16x2x2049xf16>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 1, 2, 2049] : tensor<1x16x2x2049xf16> to tensor<1x1x2x2049xf16>
    return %2 : tensor<1x1x2x2049xf16>

    // CHECK:   [[EXPAND:%.+]] = IE.Expand
    // CHECK:   [[POOLING:%.+]] = IE.AvgPool
    // CHECK:   [[SLICE:%.+]] = IE.Slice
    // CHECK:       return [[SLICE]] : tensor<1x1x2x2049xf16>
}


// -----

// CHECK-LABEL: @NotAdjustAvgPoolingWrongStride
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x2048xf16>
func.func @NotAdjustAvgPoolingWrongStride(%arg0: tensor<1x1x1x2048xf16>) -> tensor<1x1x1x1024xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x2048xf16> -> tensor<1x16x1x2048xf16>
    %1 = IE.AvgPool(%0) {exclude_pads, kernel_size = [1, 1], pads_begin = [0, 0], pads_end = [0, 0], post_op = #IE.PostOp<name = "IE.LeakyRelu", attrs = {negative_slope = 0.10000000149011612 : f64}>, rounding_type = #IE.rounding_type<FLOOR>, strides = [1, 2]} : tensor<1x16x1x2048xf16> -> tensor<1x16x1x1024xf16>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 1, 1, 1024] : tensor<1x16x1x1024xf16> to tensor<1x1x1x1024xf16>
    return %2 : tensor<1x1x1x1024xf16>

    // CHECK:   [[EXPAND:%.+]] = IE.Expand
    // CHECK:   [[POOLING:%.+]] = IE.AvgPool
    // CHECK:   [[SLICE:%.+]] = IE.Slice
    // CHECK:       return [[SLICE]] : tensor<1x1x1x1024xf16>
}


// -----

// CHECK-LABEL: @AdjustMaxPoolingToShapeCastMaxPooling
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x2048xf16>
func.func @AdjustMaxPoolingToShapeCastMaxPooling(%arg0: tensor<1x1x1x2048xf16>) -> tensor<1x1x1x2048xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x2048xf16> -> tensor<1x16x1x2048xf16>
    %1 = IE.MaxPool(%0) {exclude_pads, kernel_size = [1, 1], pads_begin = [0, 0], pads_end = [0, 0], post_op = #IE.PostOp<name = "IE.LeakyRelu", attrs = {negative_slope = 0.10000000149011612 : f64}>, rounding_type = #IE.rounding_type<FLOOR>, strides = [1, 1]} : tensor<1x16x1x2048xf16> -> tensor<1x16x1x2048xf16>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 1, 1, 2048] : tensor<1x16x1x2048xf16> to tensor<1x1x1x2048xf16>
    return %2 : tensor<1x1x1x2048xf16>

    // CHECK:   [[SHAPECAST0:%.+]] = IE.ShapeCast {shape = [1, 16, 16, 8]} inputs(%arg0 : tensor<1x1x1x2048xf16>) -> tensor<1x16x16x8xf16>
    // CHECK:   [[POOLING:%.+]] = IE.MaxPool([[SHAPECAST0]])
    // CHECK:   [[SHAPECAST0:%.+]] = IE.ShapeCast {shape = [1, 1, 1, 2048]} inputs([[POOLING]] : tensor<1x16x16x8xf16>) -> tensor<1x1x1x2048xf16>
    // CHECK:   [[EXPAND:%.+]] = IE.Expand([[SHAPECAST0]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x2048xf16> -> tensor<1x16x1x2048xf16>
    // CHECK:   [[SLICE:%.+]] = IE.Slice [[EXPAND]] [0, 0, 0, 0] [1, 1, 1, 2048] : tensor<1x16x1x2048xf16> to tensor<1x1x1x2048xf16>
    // CHECK:       return [[SLICE]] : tensor<1x1x1x2048xf16>
}


// -----

// CHECK-LABEL: @NotAdjustMaxPoolingWrongKernel
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x2048xf16>
func.func @NotAdjustMaxPoolingWrongKernel(%arg0: tensor<1x1x1x2048xf16>) -> tensor<1x1x1x2047xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x2048xf16> -> tensor<1x16x1x2048xf16>
    %1 = IE.MaxPool(%0) {exclude_pads, kernel_size = [1, 2], pads_begin = [0, 0], pads_end = [0, 0], post_op = #IE.PostOp<name = "IE.LeakyRelu", attrs = {negative_slope = 0.10000000149011612 : f64}>, rounding_type = #IE.rounding_type<FLOOR>, strides = [1, 1]} : tensor<1x16x1x2048xf16> -> tensor<1x16x1x2047xf16>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 1, 1, 2047] : tensor<1x16x1x2047xf16> to tensor<1x1x1x2047xf16>
    return %2 : tensor<1x1x1x2047xf16>

    // CHECK:   [[EXPAND:%.+]] = IE.Expand
    // CHECK:   [[POOLING:%.+]] = IE.MaxPool
    // CHECK:   [[SLICE:%.+]] = IE.Slice
    // CHECK:       return [[SLICE]] : tensor<1x1x1x2047xf16>
}


// -----

// CHECK-LABEL: @NotAdjustMaxPoolingWrongPad
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x2048xf16>
func.func @NotAdjustMaxPoolingWrongPad(%arg0: tensor<1x1x1x2048xf16>) -> tensor<1x1x2x2049xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x2048xf16> -> tensor<1x16x1x2048xf16>
    %1 = IE.MaxPool(%0) {exclude_pads, kernel_size = [1, 1], pads_begin = [1, 0], pads_end = [0, 1], post_op = #IE.PostOp<name = "IE.LeakyRelu", attrs = {negative_slope = 0.10000000149011612 : f64}>, rounding_type = #IE.rounding_type<FLOOR>, strides = [1, 1]} : tensor<1x16x1x2048xf16> -> tensor<1x16x2x2049xf16>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 1, 2, 2049] : tensor<1x16x2x2049xf16> to tensor<1x1x2x2049xf16>
    return %2 : tensor<1x1x2x2049xf16>

    // CHECK:   [[EXPAND:%.+]] = IE.Expand
    // CHECK:   [[POOLING:%.+]] = IE.MaxPool
    // CHECK:   [[SLICE:%.+]] = IE.Slice
    // CHECK:       return [[SLICE]] : tensor<1x1x2x2049xf16>
}


// -----

// CHECK-LABEL: @NotAdjustMaxPoolingWrongStride
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x1x1x2048xf16>
func.func @NotAdjustMaxPoolingWrongStride(%arg0: tensor<1x1x1x2048xf16>) -> tensor<1x1x1x1024xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 15, 0, 0]} : tensor<1x1x1x2048xf16> -> tensor<1x16x1x2048xf16>
    %1 = IE.MaxPool(%0) {exclude_pads, kernel_size = [1, 1], pads_begin = [0, 0], pads_end = [0, 0], post_op = #IE.PostOp<name = "IE.LeakyRelu", attrs = {negative_slope = 0.10000000149011612 : f64}>, rounding_type = #IE.rounding_type<FLOOR>, strides = [1, 2]} : tensor<1x16x1x2048xf16> -> tensor<1x16x1x1024xf16>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 1, 1, 1024] : tensor<1x16x1x1024xf16> to tensor<1x1x1x1024xf16>
    return %2 : tensor<1x1x1x1024xf16>

    // CHECK:   [[EXPAND:%.+]] = IE.Expand
    // CHECK:   [[POOLING:%.+]] = IE.MaxPool
    // CHECK:   [[SLICE:%.+]] = IE.Slice
    // CHECK:       return [[SLICE]] : tensor<1x1x1x1024xf16>
}


// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @NotAdjustMaxPoolingDifferentLayout
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x3x8x8xf16, {order = #NHWC}>
func.func @NotAdjustMaxPoolingDifferentLayout(%arg0: tensor<1x3x8x8xf16, {order = #NHWC}>) -> tensor<1x3x8x8xf16> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x8x8xf16, {order = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>}> -> tensor<1x16x8x8xf16, {order = #NHWC}>
    %1 = IE.MaxPool(%0) {exclude_pads, kernel_size = [1, 1], pads_begin = [0, 0], pads_end = [0, 0], post_op = #IE.PostOp<name = "IE.LeakyRelu", attrs = {negative_slope = 0.10000000149011612 : f64}>, rounding_type = #IE.rounding_type<FLOOR>, strides = [1, 1]} : tensor<1x16x8x8xf16, {order = #NHWC}> -> tensor<1x16x8x8xf16>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 3, 8, 8] : tensor<1x16x8x8xf16> to tensor<1x3x8x8xf16>
    return %2 : tensor<1x3x8x8xf16>

    // CHECK:   [[EXPAND:%.+]] = IE.Expand
    // CHECK:   [[POOLING:%.+]] = IE.MaxPool
    // CHECK:   [[SLICE:%.+]] = IE.Slice
    // CHECK:       return [[SLICE]] : tensor<1x3x8x8xf16>
}

// -----

// CHECK-LABEL: @NotAdjustNonEltwiseGroupConv
// CHECK-SAME:        [[INPUT:%arg[0-9]]]: tensor<1x3x32x32xf16>
func.func @NotAdjustNonEltwiseGroupConv(%arg0: tensor<1x3x32x32xf16>) -> tensor<1x16x16x32xf16> {
    %filters = const.Declare tensor<16x1x1x1xf32> = dense<1.0> : tensor<1x1x1x1xf32>, [#const.Broadcast<0 : i64, 16 : i64>]
    %bias = const.Declare tensor<1x16x1x1xf32> = dense<0.0> : tensor<1x1x1x1xf32>, [#const.Broadcast<1 : i64, 16 : i64>]
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    %2 = IE.GroupConvolution(%0, %filters, %bias) {dilations = [1, 1], groups = 16 : i64, pads_begin = [0, 0], pads_end = [0, 0], strides = [2, 1]} : tensor<1x16x32x32xf16>, tensor<16x1x1x1xf32>, tensor<1x16x1x1xf32> -> tensor<1x16x16x32xf16>
    return %2 : tensor<1x16x16x32xf16>

    // CHECK-DAG:   [[FILTER:%.+]] = const.Declare tensor<16x1x1x1xf32> = dense<1.000000e+00> : tensor<1x1x1x1xf32>, [#const.Broadcast<0 : i64, 16 : i64>]
    // CHECK-DAG:   [[BIAS:%.+]] = const.Declare tensor<1x16x1x1xf32> = dense<0.000000e+00> : tensor<1x1x1x1xf32>, [#const.Broadcast<1 : i64, 16 : i64>]
    // CHECK:       [[EXPAND:%.+]] = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 13, 0, 0]} : tensor<1x3x32x32xf16> -> tensor<1x16x32x32xf16>
    // CHECK:       [[GROUP_CONV:%.+]] = IE.GroupConvolution([[EXPAND]], [[FILTER]], [[BIAS]]) {
    // CHECK-SAME:      dilations = [1, 1],
    // CHECK-SAME:      groups = 16 : i64, pads_begin = [0, 0],
    // CHECK-SAME:      pads_end = [0, 0], strides = [2, 1]
    // CHECK-SAME:      } : tensor<1x16x32x32xf16>, tensor<16x1x1x1xf32>, tensor<1x16x1x1xf32>
    // CHECK-SAME:          -> tensor<1x16x16x32xf16>

    // CHECK:       return [[GROUP_CONV]] : tensor<1x16x16x32xf16>
}
