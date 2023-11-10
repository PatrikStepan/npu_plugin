//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch%" --fuse-quantized-ops="se-ops-enabled=true" %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

!qElemType0 = !quant.uniform<u8:f16, 1.1534313725490195:128>
!qElemType1 = !quant.uniform<u8:f16, 2.4627450980392158>

//CHECK:  !qElemType0 = !quant.uniform<u8:f16, 1.1534313725490195:128>
//CHECK:  !qElemType1 = !quant.uniform<u8:f16, 2.4627450980392158>

//CHECK-LABEL: @FuseQuantParamsIntoInterp
func.func @FuseQuantParamsIntoInterp(%arg0: tensor<1x3x10x10xf16>) -> tensor<1x3x20x20xf16> {
  %1 = IE.Quantize(%arg0) {dstElemType = !qElemType0} : tensor<1x3x10x10xf16> -> tensor<1x3x10x10x!qElemType0>
  %2 = IE.Dequantize(%1) {dstElemType = f16} : tensor<1x3x10x10x!qElemType0> -> tensor<1x3x10x10xf16>
  %4 = IE.Interpolate(%2) {
            attr = #IE.Interpolate<mode = <NEAREST>, 
                                   shape_calc_mode = <SCALES>, 
                                   coord_mode = <ASYMMETRIC>, 
                                   nearest_mode = <FLOOR>, 
                                   antialias = false, 
                                   pads_begin = [0, 0, 0, 0], 
                                   pads_end = [0, 0, 0, 0], 
                                   cube_coeff = -7.500000e-01 : f64>,
                                   axes_attr = [2, 3],
                                   operand_segment_sizes = dense<[1, 0, 0, 0]> : vector<4xi32>,
                                   scales_attr = [2.000000e+00, 2.000000e+00],
                                   sizes_attr = [20, 20]} :
        tensor<1x3x10x10xf16> -> tensor<1x3x20x20xf16>
  %5 = IE.Quantize(%4) {dstElemType = !qElemType1}: tensor<1x3x20x20xf16> -> tensor<1x3x20x20x!qElemType1>
  %6 = IE.Dequantize(%5) {dstElemType = f16} : tensor<1x3x20x20x!qElemType1> -> tensor<1x3x20x20xf16>

  return %6 : tensor<1x3x20x20xf16>

  //CHECK:      [[QUANT:%.*]] = IE.Quantize(%arg0) {dstElemType = !qElemType0}
  //CHECK-SAME:   tensor<1x3x10x10xf16> -> tensor<1x3x10x10x!qElemType0>

  //CHECK:      [[INTERP:%.*]] = IE.Interpolate([[QUANT]])
  //CHECK-SAME:   tensor<1x3x10x10x!qElemType0> -> tensor<1x3x20x20x!qElemType1>

  //CHECK:      [[DEQUANT:%.*]] = IE.Dequantize([[INTERP]]) {dstElemType = f16}
  //CHECK-SAME:   tensor<1x3x20x20x!qElemType1> -> tensor<1x3x20x20xf16>

  //CHECK:      return [[DEQUANT]] : tensor<1x3x20x20xf16>
}

// -----

!qElemType0 = !quant.uniform<u8:f16, 1.1534313725490195:128>
!qElemType1 = !quant.uniform<u8:f16, 2.4627450980392158>

// Do not quantize interoplate that will not be run on NCE due to non integer scales

//CHECK:  !qElemType0 = !quant.uniform<u8:f16, 1.1534313725490195:128>
//CHECK:  !qElemType1 = !quant.uniform<u8:f16, 2.4627450980392158>
//CHECK-LABEL: @DonotFuseQuantParamsIntoInterp
func.func @DonotFuseQuantParamsIntoInterp(%arg0: tensor<1x3x10x10xf16>) -> tensor<1x3x25x25xf16> {
  %1 = IE.Quantize(%arg0) {dstElemType = !qElemType0} : tensor<1x3x10x10xf16> -> tensor<1x3x10x10x!qElemType0>
  %2 = IE.Dequantize(%1) {dstElemType = f16} : tensor<1x3x10x10x!qElemType0> -> tensor<1x3x10x10xf16>
  %4 = IE.Interpolate(%2) {
            attr = #IE.Interpolate<mode = <NEAREST>, 
                                   shape_calc_mode = <SCALES>, 
                                   coord_mode = <ASYMMETRIC>, 
                                   nearest_mode = <FLOOR>, 
                                   antialias = false, 
                                   pads_begin = [0, 0, 0, 0], 
                                   pads_end = [0, 0, 0, 0], 
                                   cube_coeff = -7.500000e-01 : f64>,
                                   axes_attr = [2, 3],
                                   operand_segment_sizes = dense<[1, 0, 0, 0]> : vector<4xi32>,
                                   scales_attr = [2.500000e+00, 2.500000e+00],
                                   sizes_attr = [25, 25]} :
        tensor<1x3x10x10xf16> -> tensor<1x3x25x25xf16>
  %5 = IE.Quantize(%4) {dstElemType = !qElemType1}: tensor<1x3x25x25xf16> -> tensor<1x3x25x25x!qElemType1>
  %6 = IE.Dequantize(%5) {dstElemType = f16} : tensor<1x3x25x25x!qElemType1> -> tensor<1x3x25x25xf16>

  return %6 : tensor<1x3x25x25xf16>

  //CHECK:      [[QUANT:%.*]] = IE.Quantize(%arg0) {dstElemType = !qElemType0}
  //CHECK-SAME:   tensor<1x3x10x10xf16> -> tensor<1x3x10x10x!qElemType0>

  //CHECK:      [[DEQUANT:%.*]] = IE.Dequantize([[QUANT]]) {dstElemType = f16}
  //CHECK-SAME:   tensor<1x3x10x10x!qElemType0> -> tensor<1x3x10x10xf16>

  //CHECK:      [[INTERP:%.*]] = IE.Interpolate([[DEQUANT]])
  //CHECK-SAME:   tensor<1x3x10x10xf16> -> tensor<1x3x25x25xf16>

  //CHECK:      [[QUANT_OUT:%.*]] = IE.Quantize([[INTERP]]) {dstElemType = !qElemType1}
  //CHECK-SAME:   tensor<1x3x25x25xf16> -> tensor<1x3x25x25x!qElemType1>

  //CHECK:      [[DEQUANT_OUT:%.*]] = IE.Dequantize([[QUANT_OUT]]) {dstElemType = f16}
  //CHECK-SAME:   tensor<1x3x25x25x!qElemType1> -> tensor<1x3x25x25xf16>

  //CHECK:      return [[DEQUANT_OUT]] : tensor<1x3x25x25xf16>
}
