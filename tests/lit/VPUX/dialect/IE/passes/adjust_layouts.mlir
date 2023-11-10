//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch% allow-custom-values=true" --adjust-layouts --canonicalize %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

#NHCW = affine_map<(d0, d1, d2, d3) -> (d0, d2, d1, d3)>

// CHECK-LABEL: @InOutNHCW
module @InOutNHCW attributes {VPU.compilationMode = #VPU.compilation_mode<ReferenceSW>} {

IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        DataInfo "data" : tensor<1x8x4x2xf16>
    }
    outputsInfo : {
        DataInfo "prob" : tensor<1x8x4x2xf16>
    }

// CHECK: func.func @main([[ARG0:%arg[0-9]+]]: tensor<1x8x4x2xf16, {order = #NHCW}>) -> tensor<1x8x4x2xf16, {order = #NHCW}> {
func.func @main(%arg0: tensor<1x8x4x2xf16, {order = #NHCW}>) -> tensor<1x8x4x2xf16, {order = #NHCW}> {
    %0 = IE.GRN(%arg0) {bias = 1.0} : tensor<1x8x4x2xf16, {order = #NHCW}> -> tensor<1x8x4x2xf16, {order = #NHCW}>
    %1 = IE.SoftMax(%0) {axisInd = 1} : tensor<1x8x4x2xf16, {order = #NHCW}> -> tensor<1x8x4x2xf16, {order = #NHCW}>
    %2 = IE.GRN(%1) {bias = 1.0} : tensor<1x8x4x2xf16, {order = #NHCW}> -> tensor<1x8x4x2xf16, {order = #NHCW}>
    return %2 : tensor<1x8x4x2xf16, {order = #NHCW}>

    // CHECK: [[VAR0:%.+]] = IE.Reorder([[ARG0]]) {dstOrder = #NCHW} : tensor<1x8x4x2xf16, {order = #NHCW}> -> tensor<1x8x4x2xf16>
    // CHECK: [[VAR1:%.+]] = IE.GRN([[VAR0]]) {bias = 1.000000e+00 : f64} : tensor<1x8x4x2xf16> -> tensor<1x8x4x2xf16>
    // CHECK: [[VAR2:%.+]] = IE.Reorder([[VAR1]]) {dstOrder = #NHCW} : tensor<1x8x4x2xf16> -> tensor<1x8x4x2xf16, {order = #NHCW}>

    // CHECK: [[VAR3:%.+]] = IE.SoftMax([[VAR2]]) {axisInd = 1 : i64} : tensor<1x8x4x2xf16, {order = #NHCW}> -> tensor<1x8x4x2xf16, {order = #NHCW}>

    // CHECK: [[VAR4:%.+]] = IE.Reorder([[VAR3]]) {dstOrder = #NCHW} : tensor<1x8x4x2xf16, {order = #NHCW}> -> tensor<1x8x4x2xf16>
    // CHECK: [[VAR5:%.+]] = IE.GRN([[VAR4]]) {bias = 1.000000e+00 : f64} : tensor<1x8x4x2xf16> -> tensor<1x8x4x2xf16>
    // CHECK: [[VAR6:%.+]] = IE.Reorder([[VAR5]]) {dstOrder = #NHCW} : tensor<1x8x4x2xf16> -> tensor<1x8x4x2xf16, {order = #NHCW}>

    // CHECK: return [[VAR6]] : tensor<1x8x4x2xf16, {order = #NHCW}>
}

}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @DifferentOrders
module @DifferentOrders attributes {VPU.compilationMode = #VPU.compilation_mode<ReferenceSW>} {

IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        DataInfo "data" : tensor<1x8x4x2xf16>
    }
    outputsInfo : {
        DataInfo "prob" : tensor<1x8x4x2xf16>
    }

// CHECK: func.func @main([[ARG0:%arg[0-9]+]]: tensor<1x8x4x2xf16>) -> tensor<1x8x4x2xf16, {order = #NHWC}> {
func.func @main(%arg0: tensor<1x8x4x2xf16>) -> tensor<1x8x4x2xf16, {order = #NHWC}> {
    %0 = IE.GRN(%arg0) {bias = 1.0} : tensor<1x8x4x2xf16> -> tensor<1x8x4x2xf16, {order = #NHWC}>
    return %0 : tensor<1x8x4x2xf16, {order = #NHWC}>

    // CHECK: [[VAR0:%.+]] = IE.GRN([[ARG0]]) {bias = 1.000000e+00 : f64} : tensor<1x8x4x2xf16> -> tensor<1x8x4x2xf16>
    // CHECK: [[VAR1:%.+]] = IE.Reorder([[VAR0]]) {dstOrder = #NHWC} : tensor<1x8x4x2xf16> -> tensor<1x8x4x2xf16, {order = #NHWC}>
    // CHECK: return [[VAR1]] : tensor<1x8x4x2xf16, {order = #NHWC}>
}

}

// -----

// CHECK-LABEL: @HwOp
module @HwOp attributes {VPU.compilationMode = #VPU.compilation_mode<DefaultHW>} {

IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        DataInfo "data" : tensor<1x16x30x30xf16>
    }
    outputsInfo : {
        DataInfo "prob" : tensor<1x16x15x13xf16>
    }

// CHECK: func.func @main([[ARG0:%arg[0-9]+]]: tensor<1x16x30x30xf16>) -> tensor<1x16x15x13xf16> {
func.func @main(%arg0: tensor<1x16x30x30xf16>) -> tensor<1x16x15x13xf16> {
   %0 = IE.MaxPool(%arg0) {
        kernel_size = [5, 5],
        pads_begin = [2, 0],
        pads_end = [2, 0],
        rounding_type = #IE.rounding_type<FLOOR>,
        strides = [2, 2]
    } : tensor<1x16x30x30xf16> -> tensor<1x16x15x13xf16>
    return %0 : tensor<1x16x15x13xf16>

    // CHECK:       [[VAR0:%.+]] = IE.Reorder([[ARG0]]) {dstOrder = #NHWC} : tensor<1x16x30x30xf16> -> tensor<1x16x30x30xf16, {order = #NHWC}>
    // CHECK:       [[VAR1:%.+]] = IE.MaxPool([[VAR0]])
    // CHECK-SAME:      tensor<1x16x30x30xf16, {order = #NHWC}> -> tensor<1x16x15x13xf16, {order = #NHWC}>
    // CHECK:       [[VAR2:%.+]] = IE.Reorder([[VAR1]]) {dstOrder = #NCHW} : tensor<1x16x15x13xf16, {order = #NHWC}> -> tensor<1x16x15x13xf16>
    // CHECK:       return [[VAR2]] : tensor<1x16x15x13xf16>
}

}

// -----

// CHECK-LABEL: @HwOpSameInputs
module @HwOpSameInputs attributes {VPU.compilationMode =  #VPU.compilation_mode<DefaultHW>} {

IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        DataInfo "data" : tensor<1x16x30x25xf16>
    }
    outputsInfo : {
        DataInfo "prob" : tensor<1x16x30x25xf16>
    }

// CHECK-LABEL: @main
func.func @main(%arg0: tensor<1x16x30x25xf16>) -> tensor<1x16x30x25xf16> {
    %0 = IE.Add(%arg0, %arg0) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x30x25xf16>, tensor<1x16x30x25xf16> -> tensor<1x16x30x25xf16>
    %1 = IE.Add(%0, %arg0) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x30x25xf16>, tensor<1x16x30x25xf16> -> tensor<1x16x30x25xf16>
    return %1 : tensor<1x16x30x25xf16>

    // CHECK:    [[VAR0:%.+]] = IE.Reorder(%arg0) {dstOrder = #NHWC} : tensor<1x16x30x25xf16> -> tensor<1x16x30x25xf16, {order = #NHWC}>
    // CHECK:    [[VAR1:%.+]] = IE.Reorder(%arg0) {dstOrder = #NHWC} : tensor<1x16x30x25xf16> -> tensor<1x16x30x25xf16, {order = #NHWC}>

    // CHECK:    [[VAR2:%.+]] = IE.Add([[VAR0]], [[VAR1]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} :
    // CHECK-SAME:     tensor<1x16x30x25xf16, {order = #NHWC}>, tensor<1x16x30x25xf16, {order = #NHWC}> -> tensor<1x16x30x25xf16, {order = #NHWC}>

    // CHECK:    [[VAR3:%.+]] = IE.Reorder(%arg0) {dstOrder = #NHWC} : tensor<1x16x30x25xf16> -> tensor<1x16x30x25xf16, {order = #NHWC}>
    // CHECK:    [[VAR4:%.+]] = IE.Add([[VAR2]], [[VAR3]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} :
    // CHECK-SAME:     tensor<1x16x30x25xf16, {order = #NHWC}>, tensor<1x16x30x25xf16, {order = #NHWC}> -> tensor<1x16x30x25xf16, {order = #NHWC}>

    // CHECK:    [[VAR5:%.+]] = IE.Reorder([[VAR4]]) {dstOrder = #NCHW} : tensor<1x16x30x25xf16, {order = #NHWC}> -> tensor<1x16x30x25xf16>
    // CHECK:    return [[VAR5]] : tensor<1x16x30x25xf16>
}

}

// -----

// CHECK-LABEL: @SwAddOp
module @SwAddOp attributes {VPU.compilationMode = #VPU.compilation_mode<DefaultHW>} {

IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        DataInfo "data" : tensor<1x3x676x2xf16>
    }
    outputsInfo : {
        DataInfo "prob" : tensor<1x3x676x2xf16>
    }

// CHECK-LABEL: @main
func.func @main(%arg0: tensor<1x3x676x2xf16>) -> tensor<1x3x676x2xf16> {
    %cst = const.Declare tensor<1x1x676x2xf16> = dense<1.0> : tensor<1x1x676x2xf16>
    %0 = IE.Add(%arg0, %cst) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x3x676x2xf16>, tensor<1x1x676x2xf16> -> tensor<1x3x676x2xf16>
    return %0 : tensor<1x3x676x2xf16>

    // CHECK-DAG:    [[CST:%.+]] = const.Declare tensor<1x1x676x2xf16> = dense<1.000000e+00> : tensor<1x1x676x2xf16>
    // CHECK:    [[ADD:%.+]] = IE.Add(%arg0, [[CST]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} :
    // CHECK-SAME:     tensor<1x3x676x2xf16>, tensor<1x1x676x2xf16> -> tensor<1x3x676x2xf16>
    // CHECK:    return [[ADD]] : tensor<1x3x676x2xf16>
}

}

// -----

#NHCW = affine_map<(d0, d1, d2, d3) -> (d0, d2, d1, d3)>

// CHECK-LABEL: @HwOpDifferentDstOrder
module @HwOpDifferentDstOrder attributes {VPU.compilationMode = #VPU.compilation_mode<DefaultHW>} {

IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        DataInfo "data" : tensor<1x16x30x25xf16>
    }
    outputsInfo : {
        DataInfo "prob1" : tensor<1x16x30x25xf16>
        DataInfo "prob2" : tensor<1x16x30x25xf16>
    }

// CHECK-LABEL: @main
func.func @main(%arg0: tensor<1x16x30x25xf16, {order = #NHCW}>) -> (tensor<1x16x30x25xf16, {order = #NHCW}>, tensor<1x16x30x25xf16, {order = #NHCW}>) {
    %0 = IE.Add(%arg0, %arg0) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x30x25xf16, {order = #NHCW}>, tensor<1x16x30x25xf16, {order = #NHCW}> -> tensor<1x16x30x25xf16, {order = #NHCW}>
    %1 = IE.GRN(%arg0) {bias = 1.0} : tensor<1x16x30x25xf16, {order = #NHCW}> -> tensor<1x16x30x25xf16, {order = #NHCW}>
    return %0, %1 : tensor<1x16x30x25xf16, {order = #NHCW}>, tensor<1x16x30x25xf16, {order = #NHCW}>

    // CHECK:    [[VAR0:%.+]] = IE.Reorder(%arg0) {dstOrder = #NHWC} : tensor<1x16x30x25xf16, {order = #NHCW}> -> tensor<1x16x30x25xf16, {order = #NHWC}>
    // CHECK:    [[VAR1:%.+]] = IE.Reorder(%arg0) {dstOrder = #NHWC} : tensor<1x16x30x25xf16, {order = #NHCW}> -> tensor<1x16x30x25xf16, {order = #NHWC}>
    // CHECK:    [[VAR2:%.+]] = IE.Add([[VAR0]], [[VAR1]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} :
    // CHECK-SAME:     tensor<1x16x30x25xf16, {order = #NHWC}>, tensor<1x16x30x25xf16, {order = #NHWC}> -> tensor<1x16x30x25xf16, {order = #NHWC}>
    // CHECK:    [[VAR3:%.+]] = IE.Reorder([[VAR2]]) {dstOrder = #NHCW} : tensor<1x16x30x25xf16, {order = #NHWC}> -> tensor<1x16x30x25xf16, {order = #NHCW}>

    // CHECK:    [[VAR4:%.+]] = IE.Reorder(%arg0) {dstOrder = #NCHW} : tensor<1x16x30x25xf16, {order = #NHCW}> -> tensor<1x16x30x25xf16>
    // CHECK:    [[VAR5:%.+]] = IE.GRN([[VAR4]]) {bias = 1.000000e+00 : f64} : tensor<1x16x30x25xf16> -> tensor<1x16x30x25xf16>
    // CHECK:    [[VAR6:%.+]] = IE.Reorder([[VAR5]]) {dstOrder = #NHCW} : tensor<1x16x30x25xf16> -> tensor<1x16x30x25xf16, {order = #NHCW}>

    // CHECK:    return [[VAR3]], [[VAR6]] : tensor<1x16x30x25xf16, {order = #NHCW}>, tensor<1x16x30x25xf16, {order = #NHCW}>
}

}

// -----

// CHECK-LABEL: @ZMajorConv
module @ZMajorConv attributes {VPU.compilationMode = #VPU.compilation_mode<DefaultHW>} {

// CHECK: func.func @main([[ARG0:%arg[0-9]+]]: tensor<1x16x30x30xf16>) -> tensor<1x16x30x30xf16> {
func.func @main(%arg0: tensor<1x16x30x30xf16>) -> tensor<1x16x30x30xf16> {
    %cst = const.Declare tensor<16x16x1x1xf16> = dense<1.0> : tensor<16x16x1x1xf16>

    %0 = IE.Convolution(%arg0, %cst) {
        dilations = [1, 1], pads_begin = [0, 0], pads_end = [0, 0], strides = [1, 1]
    } : tensor<1x16x30x30xf16>, tensor<16x16x1x1xf16> -> tensor<1x16x30x30xf16>

    return %0 : tensor<1x16x30x30xf16>

    // CHECK-DAG:       [[CST:%.+]] = const.Declare tensor<16x16x1x1xf16, {order = #NHWC}>
    // CHECK:       [[VAR0:%.+]] = IE.Reorder([[ARG0]]) {dstOrder = #NHWC}
    // CHECK:       [[VAR1:%.+]] = IE.Convolution([[VAR0]], [[CST]])
    // CHECK-SAME:       -> tensor<1x16x30x30xf16, {order = #NHWC}>
    // CHECK:       [[VAR2:%.+]] = IE.Reorder([[VAR1]]) {dstOrder = #NCHW}
    // CHECK:       return [[VAR2]] : tensor<1x16x30x30xf16>
}

}
// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @ReorderWithScatterNDUpdate
module @ReorderWithScatterNDUpdate attributes {VPU.compilationMode = #VPU.compilation_mode<DefaultHW>} {

func.func @main(%arg0: tensor<1x50x56x56xf16, {order = #NHWC}>, %arg1: tensor<1x35x56x56xf16>) -> tensor<1x50x56x56xf16> {
    %cst = const.Declare tensor<1x35x2xsi32> = dense<1> : tensor<1x35x2xsi64>, [#const.ConvertElemType<si32>]
    %1 = IE.ScatterNDUpdate(%arg0, %cst, %arg1) : tensor<1x50x56x56xf16, {order = #NHWC}>, tensor<1x35x2xsi32>, tensor<1x35x56x56xf16> -> tensor<1x50x56x56xf16>
    return %1 : tensor<1x50x56x56xf16>

    // CHECK:       [[CST:%.+]] = const.Declare tensor<1x35x2xsi32> = dense<1> : tensor<1x35x2xsi64>, [#const.ConvertElemType<si32>]
    // CHECK:       [[VAR0:%.+]] = IE.Reorder(%arg0) {dstOrder = #NCHW} : tensor<1x50x56x56xf16, {order = #NHWC}> -> tensor<1x50x56x56xf16>
    // CHECK:       [[VAR1:%.+]] = IE.ScatterNDUpdate([[VAR0]], [[CST]], %arg1) : tensor<1x50x56x56xf16>, tensor<1x35x2xsi32>, tensor<1x35x56x56xf16> -> tensor<1x50x56x56xf16>
    // CHECK:       return [[VAR1]] : tensor<1x50x56x56xf16>
}
}

// -----

// CHECK-LABEL: @DoNotAdjustInterpolateNearestLayout
module @DoNotAdjustInterpolateNearestLayout attributes {VPU.compilationMode = #VPU.compilation_mode<DefaultHW>} {

IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        DataInfo "data" : tensor<1x3x30x30xf16>
    }
    outputsInfo : {
        DataInfo "prob" : tensor<1x3x60x60xf16>
    }

// CHECK: func.func @main([[ARG0:%arg[0-9]+]]: tensor<1x3x30x30xf16>) -> tensor<1x3x60x60xf16> {
func.func @main(%arg0: tensor<1x3x30x30xf16>) -> tensor<1x3x60x60xf16> {
    %0 = IE.Interpolate(%arg0)
         {attr = #IE.Interpolate<antialias = false, coord_mode = <ASYMMETRIC>, cube_coeff = -7.500000e-01 : f64, mode = <NEAREST>, nearest_mode = <FLOOR>,
         pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0], shape_calc_mode = <SCALES>>, axes_attr = [2, 3],
         operand_segment_sizes = dense<[1, 0, 0, 0]> : vector<4xi32>, scales_attr = [2.000000e+00, 2.000000e+00], sizes_attr = [60, 60]
         } : tensor<1x3x30x30xf16> -> tensor<1x3x60x60xf16>

    return %0 : tensor<1x3x60x60xf16>

    // CHECK:       [[INTERP:%.+]] = IE.Interpolate([[ARG0]])
    // CHECK-SAME:      attr = #IE.Interpolate<mode = <NEAREST>,
    // CHECK-SAME:                             shape_calc_mode = <SCALES>,
    // CHECK-SAME:                             coord_mode = <ASYMMETRIC>,
    // CHECK-SAME:                             nearest_mode = <FLOOR>,
    // CHECK-SAME:                             antialias = false,
    // CHECK-SAME:                             pads_begin = [0, 0, 0, 0],
    // CHECK-SAME:                             pads_end = [0, 0, 0, 0],
    // CHECK-SAME:                             cube_coeff = -7.500000e-01 : f64>,
    // CHECK-SAME:      axes_attr = [2, 3],
    // CHECK-SAME:      scales_attr = [2.000000e+00, 2.000000e+00],
    // CHECK-SAME:      sizes_attr = [60, 60]
    // CHECK-SAME:      -> tensor<1x3x60x60xf16>

    // CHECK        return [[INTERP]]
}
}

// -----

// CHECK-LABEL: @DoNotAdjustInterpolateLinearLayout
module @DoNotAdjustInterpolateLinearLayout attributes {VPU.compilationMode = #VPU.compilation_mode<DefaultHW>} {

IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        DataInfo "data" : tensor<1x3x30x30xf16>
    }
    outputsInfo : {
        DataInfo "prob" : tensor<1x3x60x60xf16>
    }

// CHECK: func.func @main([[ARG0:%arg[0-9]+]]: tensor<1x3x30x30xf16>) -> tensor<1x3x60x60xf16> {
func.func @main(%arg0: tensor<1x3x30x30xf16>) -> tensor<1x3x60x60xf16> {
    %0 = IE.Interpolate(%arg0)
         {attr = #IE.Interpolate<antialias = false, coord_mode = <ASYMMETRIC>, cube_coeff = -7.500000e-01 : f64, mode = <LINEAR>, nearest_mode = <FLOOR>,
         pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0], shape_calc_mode = <SCALES>>, axes_attr = [2, 3],
         operand_segment_sizes = dense<[1, 0, 0, 0]> : vector<4xi32>, scales_attr = [2.000000e+00, 2.000000e+00], sizes_attr = [60, 60]
         } : tensor<1x3x30x30xf16> -> tensor<1x3x60x60xf16>

    return %0 : tensor<1x3x60x60xf16>

    // CHECK:       [[INTERP:%.+]] = IE.Interpolate([[ARG0]])
    // CHECK-SAME:      attr = #IE.Interpolate<mode = <LINEAR>,
    // CHECK-SAME:                             shape_calc_mode = <SCALES>,
    // CHECK-SAME:                             coord_mode = <ASYMMETRIC>,
    // CHECK-SAME:                             nearest_mode = <FLOOR>,
    // CHECK-SAME:                             antialias = false,
    // CHECK-SAME:                             pads_begin = [0, 0, 0, 0],
    // CHECK-SAME:                             pads_end = [0, 0, 0, 0],
    // CHECK-SAME:                             cube_coeff = -7.500000e-01 : f64>,
    // CHECK-SAME:      axes_attr = [2, 3],
    // CHECK-SAME:      scales_attr = [2.000000e+00, 2.000000e+00],
    // CHECK-SAME:      sizes_attr = [60, 60]
    // CHECK-SAME:      -> tensor<1x3x60x60xf16>

    // CHECK        return [[INTERP]]
}
}
