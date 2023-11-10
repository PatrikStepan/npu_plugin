//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch%" --uniquify-ops %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @UniquifyReorders(%arg0: tensor<1x16x227x227xf16>) -> tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}> {
    %0 = IE.Reorder(%arg0) {dstOrder = #NHWC} : tensor<1x16x227x227xf16> -> tensor<1x16x227x227xf16, {order = #NHWC}>
    %1 = IE.Reorder(%arg0) {dstOrder = #NHWC} : tensor<1x16x227x227xf16> -> tensor<1x16x227x227xf16, {order = #NHWC}>
    %2 = IE.And(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x227x227xf16, {order = #NHWC}>, tensor<1x16x227x227xf16, {order = #NHWC}> -> tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>
    
    return %2 : tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>

    // CHECK: [[REORDER:%.*]] = IE.Reorder
    // CHECK-NOT: IE.Reorder
    // CHECK: [[RESULT:%.*]] = IE.And([[REORDER]], [[REORDER]])
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @ReordersWithDifferentConsumerOps(%arg0: tensor<1x16x112x112xf16>) -> tensor<1x16x112x112xf16, {order = #NHWC}> {
    %cst_69 = const.Declare tensor<16x1x3x3xf16, {order = #NHWC}> = dense<0.0> : tensor<16x1x3x3xf16>, [#const.Reorder<#NHWC>]
    %cst_90 = const.Declare tensor<1x16x1x1xf16> = dense<0.0> : tensor<1x16x1x1xf16>

    %8 = IE.HSwish(%arg0) : tensor<1x16x112x112xf16> -> tensor<1x16x112x112xf16>
    %9 = IE.Reorder(%8) {dstOrder = #NHWC} : tensor<1x16x112x112xf16> -> tensor<1x16x112x112xf16, {order = #NHWC}>
    %10 = IE.GroupConvolution(%9, %cst_69, %cst_90) {dilations = [1, 1], groups = 16 : i64, pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x16x112x112xf16, {order = #NHWC}>, tensor<16x1x3x3xf16, {order = #NHWC}>, tensor<1x16x1x1xf16> -> tensor<1x16x112x112xf16, {order = #NHWC}>
    %14 = IE.Reorder(%8) {dstOrder = #NHWC} : tensor<1x16x112x112xf16> -> tensor<1x16x112x112xf16, {order = #NHWC}>
    %15 = IE.Add(%10, %14) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x112x112xf16, {order = #NHWC}>, tensor<1x16x112x112xf16, {order = #NHWC}> -> tensor<1x16x112x112xf16, {order = #NHWC}>
    
    return %15 : tensor<1x16x112x112xf16, {order = #NHWC}>
    
    // CHECK-DAG:   [[CST0:%.*]] = const.Declare tensor<16x1x3x3xf16, {order = #NHWC}>
    // CHECK-DAG:   [[CST1:%.*]] = const.Declare tensor<1x16x1x1xf16>

    // CHECK: IE.HSwish
    // CHECK: [[REORDER:%.*]] = IE.Reorder
    // CHECK: [[CONV:%.*]] = IE.GroupConvolution([[REORDER]], [[CST0]], [[CST1]])
    // CHECK-NOT: IE.Reorder
    // CHECK: IE.Add([[CONV]], [[REORDER]])
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @TripleReordersWithDifferentConsumerOps
func.func @TripleReordersWithDifferentConsumerOps(%arg0: tensor<1x16x112x112xf16>) -> tensor<1x16x112x112x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>  {
    %cst_69 = const.Declare tensor<16x1x3x3xf16, {order = #NHWC}> = dense<0.0> : tensor<16x1x3x3xf16>, [#const.Reorder<#NHWC>]
    %cst_90 = const.Declare tensor<1x16x1x1xf16> = dense<0.0> : tensor<1x16x1x1xf16>

    %8 = IE.HSwish(%arg0) : tensor<1x16x112x112xf16> -> tensor<1x16x112x112xf16>
    %9 = IE.Reorder(%8) {dstOrder = #NHWC} : tensor<1x16x112x112xf16> -> tensor<1x16x112x112xf16, {order = #NHWC}>
    %10 = IE.GroupConvolution(%9, %cst_69, %cst_90) {dilations = [1, 1], groups = 16 : i64, pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x16x112x112xf16, {order = #NHWC}>, tensor<16x1x3x3xf16, {order = #NHWC}>, tensor<1x16x1x1xf16> -> tensor<1x16x112x112xf16, {order = #NHWC}>
    %14 = IE.Reorder(%8) {dstOrder = #NHWC} : tensor<1x16x112x112xf16> -> tensor<1x16x112x112xf16, {order = #NHWC}>
    %15 = IE.Add(%10, %14) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x112x112xf16, {order = #NHWC}>, tensor<1x16x112x112xf16, {order = #NHWC}> -> tensor<1x16x112x112xf16, {order = #NHWC}>
    %16 = IE.Reorder(%8) {dstOrder = #NHWC} : tensor<1x16x112x112xf16> -> tensor<1x16x112x112xf16, {order = #NHWC}>
    %17 = IE.And(%15, %16) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x112x112xf16, {order = #NHWC}>, tensor<1x16x112x112xf16, {order = #NHWC}> -> tensor<1x16x112x112x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>

    return %17 : tensor<1x16x112x112x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>

    // CHECK-DAG:   [[CST0:%.*]] = const.Declare tensor<16x1x3x3xf16, {order = #NHWC}>
    // CHECK-DAG:   [[CST1:%.*]] = const.Declare tensor<1x16x1x1xf16>

    // CHECK: IE.HSwish
    // CHECK: [[REORDER:%.*]] = IE.Reorder
    // CHECK: [[CONV:%.*]] = IE.GroupConvolution([[REORDER]], [[CST0]], [[CST1]])
    // CHECK-NOT: IE.Reorder
    // CHECK: [[ADD:%.*]] = IE.Add([[CONV]], [[REORDER]])
    // CHECK-NOT: IE.Reorder
    // CHECK: IE.And([[ADD]], [[REORDER]])
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @UniquifyExpands(%arg0: tensor<1x13x227x227xf16, {order = #NHWC}>) -> tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}> {
    %0 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 3, 0, 0]} : tensor<1x13x227x227xf16, {order = #NHWC}> -> tensor<1x16x227x227xf16, {order = #NHWC}>
    %1 = IE.Expand(%arg0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 3, 0, 0]} : tensor<1x13x227x227xf16, {order = #NHWC}> -> tensor<1x16x227x227xf16, {order = #NHWC}>
    %2 = IE.And(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x227x227xf16, {order = #NHWC}>, tensor<1x16x227x227xf16, {order = #NHWC}> -> tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>
    
    return %2 : tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>

    // CHECK: [[EXPAND:%.*]] = IE.Expand
    // CHECK-NOT: IE.Expand
    // CHECK: [[RESULT:%.*]] = IE.And([[EXPAND]], [[EXPAND]])
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @UniquifyExpandReorders(%arg0: tensor<1x13x227x227xf16>) -> tensor<2x16x227x227xf16, {order = #NHWC}> {
    %0 = IE.Reorder(%arg0) {dstOrder = #NHWC} : tensor<1x13x227x227xf16> -> tensor<1x13x227x227xf16, {order = #NHWC}>
    %1 = IE.Expand(%0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 3, 0, 0]} : tensor<1x13x227x227xf16, {order = #NHWC}> -> tensor<1x16x227x227xf16, {order = #NHWC}>
    %2 = IE.And(%1, %1) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x227x227xf16, {order = #NHWC}>, tensor<1x16x227x227xf16, {order = #NHWC}> -> tensor<1x16x227x227xf16, {order = #NHWC}>
    
    %3 = IE.Reorder(%arg0) {dstOrder = #NHWC} : tensor<1x13x227x227xf16> -> tensor<1x13x227x227xf16, {order = #NHWC}>
    %4 = IE.Expand(%3) {pads_begin = [0, 0, 0, 0], pads_end = [0, 3, 0, 0]} : tensor<1x13x227x227xf16, {order = #NHWC}> -> tensor<1x16x227x227xf16, {order = #NHWC}>
    %5 = IE.And(%4, %4) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x227x227xf16, {order = #NHWC}>, tensor<1x16x227x227xf16, {order = #NHWC}> -> tensor<1x16x227x227xf16, {order = #NHWC}>
    
    %6 = IE.Concat(%2, %5) {static_offsets = [[0, 0, 0, 0], [1, 0, 0, 0]]} : tensor<1x16x227x227xf16, {order = #NHWC}>, tensor<1x16x227x227xf16, {order = #NHWC}> -> tensor<2x16x227x227xf16, {order = #NHWC}>

    return %6 : tensor<2x16x227x227xf16, {order = #NHWC}>

    // CHECK:       [[REORDER:%.*]] = IE.Reorder(%arg0) {dstOrder = #NHWC} : tensor<1x13x227x227xf16> -> tensor<1x13x227x227xf16, {order = #NHWC}>
    // CHECK:       [[EXPAND:%.*]] = IE.Expand(%0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 3, 0, 0]} :
    // CHECK-SAME:                      tensor<1x13x227x227xf16, {order = #NHWC}> -> tensor<1x16x227x227xf16, {order = #NHWC}>
    
    // CHECK: [[AND0:%.*]] = IE.And([[EXPAND]], [[EXPAND]])
    // CHECK: [[CONCAT:%.*]] = IE.Concat([[AND0]], [[AND0]])
    // CHECK: return [[CONCAT]] : tensor<2x16x227x227xf16, {order = #NHWC}>
}

// -----

#NCHW = affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>
#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @UniquifyPermuteCast(%arg0: tensor<1x227x227x16xf16>) -> tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}> {
    %0 = IE.PermuteCast(%arg0) {dst_order = #NHWC, mem_perm=#NCHW} : tensor<1x227x227x16xf16> -> tensor<1x16x227x227xf16, {order = #NHWC}>
    %1 = IE.PermuteCast(%arg0) {dst_order = #NHWC, mem_perm=#NCHW} : tensor<1x227x227x16xf16> -> tensor<1x16x227x227xf16, {order = #NHWC}>
    %2 = IE.And(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x227x227xf16, {order = #NHWC}>, tensor<1x16x227x227xf16, {order = #NHWC}> -> tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>
    
    return %2 : tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>

    // CHECK: [[PERMUTE_CAST:%.*]] = IE.PermuteCast
    // CHECK-NOT: IE.PermuteCast
    // CHECK: [[RESULT:%.*]] = IE.And([[PERMUTE_CAST]], [[PERMUTE_CAST]])
}

// -----

#NCHW = affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>
#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @UniquifyShapeCast(%arg0: tensor<1x16x1x51529xf16, {order = #NHWC}>) -> tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}> {
    %0 = IE.ShapeCast {shape = [1, 16, 227, 227]} inputs(%arg0 :tensor<1x16x1x51529xf16, {order = #NHWC}>) -> tensor<1x16x227x227xf16, {order = #NHWC}>
    %1 = IE.ShapeCast {shape = [1, 16, 227, 227]} inputs(%arg0 :tensor<1x16x1x51529xf16, {order = #NHWC}>) -> tensor<1x16x227x227xf16, {order = #NHWC}>
    %2 = IE.And(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x227x227xf16, {order = #NHWC}>, tensor<1x16x227x227xf16, {order = #NHWC}> -> tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>
    
    return %2 : tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>

    // CHECK: [[SHAPE_CAST:%.*]] = IE.ShapeCast
    // CHECK-NOT: IE.ShapeCast
    // CHECK: [[RESULT:%.*]] = IE.And([[SHAPE_CAST]], [[SHAPE_CAST]])
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!OutType = tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>

func.func @UniquifyAdd(%arg0: tensor<1x16x227x227xf16, {order = #NHWC}>) -> (!OutType, !OutType) {
    %0 = IE.Add(%arg0, %arg0) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x227x227xf16, {order = #NHWC}>, tensor<1x16x227x227xf16, {order = #NHWC}> -> !OutType
    %1 = IE.Add(%arg0, %arg0) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x227x227xf16, {order = #NHWC}>, tensor<1x16x227x227xf16, {order = #NHWC}> -> !OutType
   
    return %0, %1 : !OutType, !OutType

    // CHECK: [[ADD:%.*]] = IE.Add
    // CHECK-NOT: IE.Add
    // CHECK: return [[ADD]], [[ADD]]
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!OutType = tensor<1x16x227x227x!quant.uniform<u8:f16, 1.1534313725490195:128>, {order = #NHWC}>

func.func @UniquifyAnd(%arg0: tensor<1x16x227x227xf16, {order = #NHWC}>) -> (!OutType, !OutType) {
    %0 = IE.And(%arg0, %arg0) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x227x227xf16, {order = #NHWC}>, tensor<1x16x227x227xf16, {order = #NHWC}> -> !OutType
    %1 = IE.And(%arg0, %arg0) {auto_broadcast = #IE.auto_broadcast_type<NONE_OR_EXPLICIT>} : tensor<1x16x227x227xf16, {order = #NHWC}>, tensor<1x16x227x227xf16, {order = #NHWC}> -> !OutType
   
    return %0, %1 : !OutType, !OutType

    // CHECK: [[AND:%.*]] = IE.And
    // CHECK-NOT: IE.And
    // CHECK: return [[AND]], [[AND]]
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!qElemType0 = !quant.uniform<u8:f16, 0.0082835477941176471:128>
!qElemType1 = !quant.uniform<u8:f16, 0.017072610294117645:128>

!OutType = tensor<1x3x512x512x!qElemType1, {order = #NHWC}>

func.func @UniquifyQuantizeCast(%arg0: tensor<1x3x512x512x!qElemType0, {order = #NHWC}>) -> (!OutType, !OutType) {
    %0 = IE.QuantizeCast(%arg0) {dstElemType = !qElemType1} : tensor<1x3x512x512x!qElemType0, {order = #NHWC}> -> !OutType
    %1 = IE.QuantizeCast(%arg0) {dstElemType = !qElemType1} : tensor<1x3x512x512x!qElemType0, {order = #NHWC}> -> !OutType
   
    return %0, %1 : !OutType, !OutType

    // CHECK: [[CAST:%.*]] = IE.QuantizeCast
    // CHECK-NOT: IE.QuantizeCast
    // CHECK: return [[CAST]], [[CAST]]
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @UniquifyLayoutCast
func.func @UniquifyLayoutCast(%arg0: tensor<1x4x8x64xf16>) -> (tensor<1x4x8x64xf16, {order = #NHWC}>, tensor<1x4x8x64xf16, {order = #NHWC}>) {
    %0 = IE.LayoutCast(%arg0) {
        dst_order = #NHWC
    } : tensor<1x4x8x64xf16> -> tensor<1x4x8x64xf16, {order = #NHWC}>

    %1 = IE.LayoutCast(%arg0) {
        dst_order = #NHWC
    } : tensor<1x4x8x64xf16> -> tensor<1x4x8x64xf16, {order = #NHWC}>

    return %0, %1 : tensor<1x4x8x64xf16, {order = #NHWC}>, tensor<1x4x8x64xf16, {order = #NHWC}>

    // CHECK:   [[LAYOUT_CAST:%.*]] = IE.LayoutCast(%arg0) {
    // CHECK-NOT:  IE.LayoutCast
    // CHECK:   return [[LAYOUT_CAST]], [[LAYOUT_CAST]]
}

// -----

#NCHW = affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>
#NWCH = affine_map<(d0, d1, d2, d3) -> (d0, d3, d1, d2)>

// CHECK-LABEL:   @UniquifyMemPermute
func.func @UniquifyMemPermute(%arg0: tensor<1x16x2x3xf16>) ->
        (tensor<1x3x16x2xf16>, tensor<1x3x16x2xf16>) {
    %0 = IE.MemPermute(%arg0) {dst_order = #NCHW, mem_perm = #NWCH} :
        tensor<1x16x2x3xf16> -> tensor<1x3x16x2xf16>
    %1 = IE.MemPermute(%arg0) {dst_order = #NCHW, mem_perm = #NWCH} :
        tensor<1x16x2x3xf16> -> tensor<1x3x16x2xf16>

    return %0, %1 : tensor<1x3x16x2xf16>, tensor<1x3x16x2xf16>

    // CHECK:     [[PERMUTE:%.*]] = IE.MemPermute(%arg0)
    // CHECK-NOT: IE.MemPermute
    // CHECK:     return [[PERMUTE]], [[PERMUTE]] : tensor<1x3x16x2xf16>, tensor<1x3x16x2xf16>
}

// -----

// CHECK-LABEL: @UniquifyAffineReshape
func.func @UniquifyAffineReshape(%arg0: tensor<15x2xf16>) -> (tensor<30xf16>, tensor<30xf16>) {
    %0 = IE.AffineReshape(%arg0) { dim_mapping = [[0], [1]], shape_value = [30] } : tensor<15x2xf16> -> tensor<30xf16>
    %1 = IE.AffineReshape(%arg0) { dim_mapping = [[0], [1]], shape_value = [30] } : tensor<15x2xf16> -> tensor<30xf16>

    return %0, %1 : tensor<30xf16>, tensor<30xf16>

    // CHECK: [[VAL0:%.*]] = IE.AffineReshape(%arg0)
    // CHECK-NOT: IE.AffineReshape
    // CHECK: return [[VAL0]], [[VAL0]] : tensor<30xf16>, tensor<30xf16>
}

// -----

#NCHW = affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>
#NWCH = affine_map<(d0, d1, d2, d3) -> (d0, d3, d1, d2)>
#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL:   @UniquifyMemPermute
func.func @UniquifyMemPermute(%arg0: tensor<1x16x2x3xf16>) ->
        (tensor<1x16x2x3xf16, {order = #NHWC}>, tensor<1x16x2x3xf16, {order = #NHWC}>) {
    %0 = IE.PermuteQuantize(%arg0) {dstElemType = f16, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} :
         tensor<1x16x2x3xf16> -> tensor<1x16x2x3xf16, {order = #NHWC}>
    %1 = IE.PermuteQuantize(%arg0) {dstElemType = f16, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} :
         tensor<1x16x2x3xf16> -> tensor<1x16x2x3xf16, {order = #NHWC}>

    return %0, %1 : tensor<1x16x2x3xf16, {order = #NHWC}>, tensor<1x16x2x3xf16, {order = #NHWC}>

    // CHECK:     [[PERMUTE:%.*]] = IE.PermuteQuantize(%arg0)
    // CHECK-NOT: IE.PermuteQuantize
    // CHECK:     return [[PERMUTE]], [[PERMUTE]] : tensor<1x16x2x3xf16, {order = #NHWC}>, tensor<1x16x2x3xf16, {order = #NHWC}>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @CannotUniquifyAddsWithDifferentInputs
func.func @CannotUniquifyAddsWithDifferentInputs(%arg0: tensor<1x128x4x4xf16>, %arg1: tensor<1x128x4x4xf16>) -> (tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16>) {
    %cst_10 = const.Declare tensor<128x1x3x3xf16> = dense<0.0> : tensor<128x1x3x3xf16>
    %cst_11 = const.Declare tensor<1x128x1x1xf16> = dense<0.0> : tensor<1x128x1x1xf16>

    %cst_20 = const.Declare tensor<128x1x3x3xf16> = dense<1.0> : tensor<128x1x3x3xf16>
    %cst_21 = const.Declare tensor<1x128x1x1xf16> = dense<1.0> : tensor<1x128x1x1xf16>

    %0 = IE.Add(%arg0, %arg1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16> -> tensor<1x128x4x4xf16>
    %1 = IE.GroupConvolution(%0, %cst_10, %cst_11) {dilations = [1, 1], groups = 128 : i64, pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x128x4x4xf16>, tensor<128x1x3x3xf16>, tensor<1x128x1x1xf16> -> tensor<1x128x4x4xf16>
    %2 = IE.GroupConvolution(%0, %cst_20, %cst_21) {dilations = [1, 1], groups = 128 : i64, pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x128x4x4xf16>, tensor<128x1x3x3xf16>, tensor<1x128x1x1xf16> -> tensor<1x128x4x4xf16>
    %3 = IE.Add(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16> -> tensor<1x128x4x4xf16>
    %4 = IE.Add(%0, %2) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16> -> tensor<1x128x4x4xf16>

    return %3, %4 : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16>

    // CHECK-DAG: [[CST:%.*]] = const.Declare tensor<128x1x3x3xf16> = dense<0.000000e+00> : tensor<128x1x3x3xf16>
    // CHECK-DAG: [[CST0:%.*]] = const.Declare tensor<1x128x1x1xf16> = dense<0.000000e+00> : tensor<1x128x1x1xf16>
    // CHECK-DAG: [[CST1:%.*]] = const.Declare tensor<128x1x3x3xf16> = dense<1.000000e+00> : tensor<128x1x3x3xf16>
    // CHECK-DAG: [[CST2:%.*]] = const.Declare tensor<1x128x1x1xf16> = dense<1.000000e+00> : tensor<1x128x1x1xf16>
    // CHECK:     [[ADD:%.*]] = IE.Add(%arg0, %arg1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16> -> tensor<1x128x4x4xf16>
    // CHECK:     [[CONV1:%.*]] = IE.GroupConvolution([[ADD]], [[CST]], [[CST0]]) {dilations = [1, 1], groups = 128 : i64, pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x128x4x4xf16>, tensor<128x1x3x3xf16>, tensor<1x128x1x1xf16> -> tensor<1x128x4x4xf16>
    // CHECK:     [[CONV2:%.*]] = IE.GroupConvolution([[ADD]], [[CST1]], [[CST2]]) {dilations = [1, 1], groups = 128 : i64, pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x128x4x4xf16>, tensor<128x1x3x3xf16>, tensor<1x128x1x1xf16> -> tensor<1x128x4x4xf16>
    // CHECK:     [[ADD1:%.*]] = IE.Add([[ADD]], [[CONV1]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16> -> tensor<1x128x4x4xf16>
    // CHECK:     [[ADD2:%.*]] = IE.Add([[ADD]], [[CONV2]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16> -> tensor<1x128x4x4xf16>
    // CHECK:     return [[ADD1]], [[ADD2]] : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16>

}

// -----

// CHECK-LABEL: @UniquifyAddsSwappedInputs
func.func @UniquifyAddsSwappedInputs(%arg0: tensor<1x128x4x4xf16>, %arg1: tensor<1x128x4x4xf16>) -> (tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16>) {
    %cst_10 = const.Declare tensor<128x1x3x3xf16> = dense<0.0> : tensor<128x1x3x3xf16>
    %cst_11 = const.Declare tensor<1x128x1x1xf16> = dense<0.0> : tensor<1x128x1x1xf16>

    %0 = IE.Add(%arg0, %arg1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16> -> tensor<1x128x4x4xf16>
    %1 = IE.GroupConvolution(%0, %cst_10, %cst_11) {dilations = [1, 1], groups = 128 : i64, pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x128x4x4xf16>, tensor<128x1x3x3xf16>, tensor<1x128x1x1xf16> -> tensor<1x128x4x4xf16>
    %2 = IE.Add(%0, %1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16> -> tensor<1x128x4x4xf16>
    %3 = IE.Add(%1, %0) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16> -> tensor<1x128x4x4xf16>

    return %2, %3 : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16>

    // CHECK-DAG: [[CST:%.*]] = const.Declare tensor<128x1x3x3xf16> = dense<0.000000e+00> : tensor<128x1x3x3xf16>
    // CHECK-DAG: [[CST0:%.*]] = const.Declare tensor<1x128x1x1xf16> = dense<0.000000e+00> : tensor<1x128x1x1xf16>
    // CHECK:     [[ADD:%.*]] = IE.Add(%arg0, %arg1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16> -> tensor<1x128x4x4xf16>
    // CHECK:     [[CONV:%.*]] = IE.GroupConvolution([[ADD]], [[CST]], [[CST0]]) {dilations = [1, 1], groups = 128 : i64, pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x128x4x4xf16>, tensor<128x1x3x3xf16>, tensor<1x128x1x1xf16> -> tensor<1x128x4x4xf16>
    // CHECK:     [[ADD1:%.*]] = IE.Add([[ADD]], [[CONV]]) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16> -> tensor<1x128x4x4xf16>
    // CHECK-NOT: IE.ADD
    // CHECK:     return [[ADD1]], [[ADD1]] : tensor<1x128x4x4xf16>, tensor<1x128x4x4xf16>

}

// -----

// CHECK-LABEL: @UniquifyReshape
func.func @UniquifyReshape(%arg0: tensor<1x32x44x44xf16>) -> (tensor<1x32x16x121xf16>, tensor<1x32x16x121xf16>) {
    %0 = IE.Reshape(%arg0) {shape_value = [1, 32, 16, 121]} : tensor<1x32x44x44xf16> -> tensor<1x32x16x121xf16>
    %1 = IE.Reshape(%arg0) {shape_value = [1, 32, 16, 121]} : tensor<1x32x44x44xf16> -> tensor<1x32x16x121xf16>

    return %0, %1 : tensor<1x32x16x121xf16>, tensor<1x32x16x121xf16>

    // CHECK: [[VAL0:%.*]] = IE.Reshape(%arg0)
    // CHECK-NOT: IE.Reshape
    // CHECK: return [[VAL0]], [[VAL0]] : tensor<1x32x16x121xf16>, tensor<1x32x16x121xf16>
}


// -----

// CHECK-LABEL: @UniquifyConcat
func.func @UniquifyConcat(%arg0: tensor<1x32x44x44xf16>, %arg1: tensor<1x32x44x44xf16>) -> (tensor<1x64x44x44xf16>, tensor<1x64x44x44xf16>) {
    %0 = IE.Concat(%arg0, %arg1) {per_axis = #IE.Concat<axis = 1>} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x64x44x44xf16>
    %1 = IE.Concat(%arg0, %arg1) {per_axis = #IE.Concat<axis = 1>} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x64x44x44xf16>
    return %0, %1 : tensor<1x64x44x44xf16>, tensor<1x64x44x44xf16>

    //CHECK:        [[CONCAT0:%.*]] = IE.Concat(%arg0, %arg1) {per_axis = #IE.Concat<axis = 1 : i64>} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x64x44x44xf16>
    //CHECK:        return [[CONCAT0]], [[CONCAT0]] : tensor<1x64x44x44xf16>, tensor<1x64x44x44xf16>

}

// -----

// CHECK-LABEL: @NotUniquifyConcatDifferentInput
func.func @NotUniquifyConcatDifferentInput(%arg0: tensor<1x32x44x44xf16>, %arg1: tensor<1x32x44x44xf16>) -> (tensor<1x64x44x44xf16>, tensor<1x64x44x44xf16>) {
    %0 = IE.Concat(%arg0, %arg1) {per_axis = #IE.Concat<axis = 1>} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x64x44x44xf16>
    %1 = IE.Concat(%arg1, %arg0) {per_axis = #IE.Concat<axis = 1>} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x64x44x44xf16>
    return %0, %1 : tensor<1x64x44x44xf16>, tensor<1x64x44x44xf16>

    //CHECK:     [[CONCAT0:%.*]] = IE.Concat(%arg0, %arg1)
    //CHECK:     [[CONCAT1:%.*]] = IE.Concat(%arg1, %arg0)
    //CHECK:     return [[CONCAT0]], [[CONCAT1]] : tensor<1x64x44x44xf16>, tensor<1x64x44x44xf16>
}

// -----

// CHECK-LABEL: @NotUniquifyConcatDifferentInputNumber
func.func @NotUniquifyConcatDifferentInputNumber(%arg0: tensor<1x32x44x44xf16>, %arg1: tensor<1x32x44x44xf16>) -> (tensor<1x96x44x44xf16>, tensor<1x64x44x44xf16>) {
    %0 = IE.Concat(%arg0, %arg1, %arg0) {per_axis = #IE.Concat<axis = 1>} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x96x44x44xf16>
    %1 = IE.Concat(%arg1, %arg0) {per_axis = #IE.Concat<axis = 1>} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x64x44x44xf16>
    return %0, %1 : tensor<1x96x44x44xf16>, tensor<1x64x44x44xf16>

    //CHECK:     [[CONCAT0:%.*]] = IE.Concat(%arg0, %arg1, %arg0)
    //CHECK:     [[CONCAT1:%.*]] = IE.Concat(%arg1, %arg0)
    //CHECK:     return [[CONCAT0]], [[CONCAT1]] : tensor<1x96x44x44xf16>, tensor<1x64x44x44xf16>
}

// -----

// CHECK-LABEL: @NotUniquifyConcatDifferentAxis
func.func @NotUniquifyConcatDifferentAxis(%arg0: tensor<1x32x44x44xf16>, %arg1: tensor<1x32x44x44xf16>) -> (tensor<1x32x88x44xf16>, tensor<1x32x44x88xf16>) {
    %0 = IE.Concat(%arg0, %arg1) {per_axis = #IE.Concat<axis = 2>} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x32x88x44xf16>
    %1 = IE.Concat(%arg0, %arg1) {per_axis = #IE.Concat<axis = 3>}: tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x32x44x88xf16>
    return %0, %1 : tensor<1x32x88x44xf16>, tensor<1x32x44x88xf16>

    //CHECK:     [[CONCAT0:%.*]] = IE.Concat(%arg0, %arg1)
    //CHECK:     [[CONCAT1:%.*]] = IE.Concat(%arg0, %arg1)
    //CHECK:     return [[CONCAT0]], [[CONCAT1]] : tensor<1x32x88x44xf16>, tensor<1x32x44x88xf16>
}

// -----

// CHECK-LABEL: @NotUniquifyConcatDifferentOffsetStride
func.func @NotUniquifyConcatDifferentOffsetStride(%arg0: tensor<1x32x44x44xf16>, %arg1: tensor<1x32x44x44xf16>) -> (tensor<1x64x44x44xf16>, tensor<1x64x44x44xf16>) {
    %0 = IE.Concat(%arg0, %arg1) {per_axis = #IE.Concat<axis = 1, offset = 1, stride = 2>} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x64x44x44xf16>
    %1 = IE.Concat(%arg0, %arg1) {per_axis = #IE.Concat<axis = 1>} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x64x44x44xf16>
    return %0, %1 : tensor<1x64x44x44xf16>, tensor<1x64x44x44xf16>

    //CHECK:     [[CONCAT0:%.*]] = IE.Concat(%arg0, %arg1)
    //CHECK:     [[CONCAT1:%.*]] = IE.Concat(%arg0, %arg1)
    //CHECK:     return [[CONCAT0]], [[CONCAT1]] : tensor<1x64x44x44xf16>, tensor<1x64x44x44xf16>
}

// -----

// CHECK-LABEL: @NotUniquifyConcatDifferentStaticOffset
func.func @NotUniquifyConcatDifferentStaticOffset(%arg0: tensor<1x32x44x44xf16>, %arg1: tensor<1x32x44x44xf16>) -> (tensor<1x32x131x44xf16>, tensor<1x32x131x44xf16>) {
    %0 = IE.Concat(%arg0, %arg1, %arg0) {static_offsets = [[0, 0, 0, 0], [0, 0, 43, 0], [0, 0, 87, 0]]} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x32x131x44xf16>
    %1 = IE.Concat(%arg0, %arg1, %arg0) {static_offsets = [[0, 0, 0, 0], [0, 0, 44, 0], [0, 0, 87, 0]]} : tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16>, tensor<1x32x44x44xf16> -> tensor<1x32x131x44xf16>
    return %0, %1 : tensor<1x32x131x44xf16>, tensor<1x32x131x44xf16>

    //CHECK:     [[CONCAT0:%.*]] = IE.Concat(%arg0, %arg1, %arg0)
    //CHECK:     [[CONCAT1:%.*]] = IE.Concat(%arg0, %arg1, %arg0)
    //CHECK:     return [[CONCAT0]], [[CONCAT1]] : tensor<1x32x131x44xf16>, tensor<1x32x131x44xf16>
}
