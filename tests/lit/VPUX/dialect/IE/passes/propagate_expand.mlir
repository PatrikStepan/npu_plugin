//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch%" --propagate-expand %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>
!qElemType0 = !quant.uniform<u8:f16, 0.010857077205882353:127>
!qElemType1 = !quant.uniform<u8:f16, 0.018435968137254902:128>
!qElemType2 = !quant.uniform<u8:f16, 0.0082261029411764708:127>

// CHECK-LABEL: @PropagateExpandThroughEltwise
func.func @PropagateExpandThroughEltwise(%arg0: tensor<1x16x256x256x!qElemType2, {order = #NHWC}>, %arg1: tensor<1x16x256x256x!qElemType0, {order = #NHWC}>) -> tensor<1x4x512x512x!qElemType1, {order = #NHWC}> {
    %0 = IE.Slice %arg0 [0, 0, 0, 0] [1, 12, 256, 256] : tensor<1x16x256x256x!qElemType2, {order = #NHWC}> to tensor<1x12x256x256x!qElemType2, {order = #NHWC}>
    %1 = IE.DepthToSpace(%0) {block_size = 2 : i64, mode = #IE.depth_to_space_mode<BLOCKS_FIRST>} : tensor<1x12x256x256x!qElemType2, {order = #NHWC}> -> tensor<1x3x512x512x!qElemType2, {order = #NHWC}>
    %2 = IE.Slice %arg1 [0, 0, 0, 0] [1, 12, 256, 256] : tensor<1x16x256x256x!qElemType0, {order = #NHWC}> to tensor<1x12x256x256x!qElemType0, {order = #NHWC}>
    %3 = IE.DepthToSpace(%2) {block_size = 2 : i64, mode = #IE.depth_to_space_mode<BLOCKS_FIRST>} : tensor<1x12x256x256x!qElemType0, {order = #NHWC}> -> tensor<1x3x512x512x!qElemType0, {order = #NHWC}>
    %4 = IE.ShapeCast {shape = [1, 16, 256, 192]} inputs(%1 : tensor<1x3x512x512x!qElemType2, {order = #NHWC}>) -> tensor<1x16x256x192x!qElemType2, {order = #NHWC}>
    %5 = IE.ShapeCast {shape = [1, 16, 256, 192]} inputs(%3 : tensor<1x3x512x512x!qElemType0, {order = #NHWC}>) -> tensor<1x16x256x192x!qElemType0, {order = #NHWC}>
    %6 = IE.Add(%4, %5) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x256x192x!qElemType2, {order = #NHWC}>, tensor<1x16x256x192x!qElemType0, {order = #NHWC}> -> tensor<1x16x256x192x!qElemType1, {order = #NHWC}>
    %7 = IE.ShapeCast {shape = [1, 3, 512, 512]} inputs(%6 : tensor<1x16x256x192x!qElemType1, {order = #NHWC}>) -> tensor<1x3x512x512x!qElemType1, {order = #NHWC}>
    %8 = IE.Expand(%7) {pads_begin = [0, 0, 0, 0], pads_end = [0, 1, 0, 0]} : tensor<1x3x512x512x!qElemType1, {order = #NHWC}> -> tensor<1x4x512x512x!qElemType1, {order = #NHWC}>
    return %8 : tensor<1x4x512x512x!qElemType1, {order = #NHWC}>


    //CHECK:    [[SLICE_0:%.*]] = IE.Slice %arg0 [0, 0, 0, 0] [1, 12, 256, 256] 
    //CHECK:    [[D2S_0:%.*]] = IE.DepthToSpace([[SLICE_0]])
    //CHECK:    [[SLICE_1:%.*]] = IE.Slice %arg1 [0, 0, 0, 0] [1, 12, 256, 256] 
    //CHECK:    [[D2S_1:%.*]] = IE.DepthToSpace([[SLICE_1]])
    //CHECK:    [[EXPAND_0:%.*]] = IE.Expand([[D2S_0]]) 
    //CHECK:    [[SHAPECAST_0:%.*]] = IE.ShapeCast {shape = [1, 16, 256, 256]} inputs([[EXPAND_0]] 
    //CHECK:    [[EXPAND_1:%.*]] = IE.Expand([[D2S_1]]) 
    //CHECK:    [[SHAPECAST_1:%.*]] = IE.ShapeCast {shape = [1, 16, 256, 256]} inputs([[EXPAND_1]] 
    //CHECK:    [[ADD:%.*]] = IE.Add([[SHAPECAST_0]], [[SHAPECAST_1]]) 
    //CHECK:    [[SHAPECAST_2:%.*]] = IE.ShapeCast {shape = [1, 4, 512, 512]} inputs([[ADD]] 
    //CHECK-NOT: IE.Expand
    //CHECK:    return [[SHAPECAST_2]] 

}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @NoPropagateExpandThroughEltwise
func.func @NoPropagateExpandThroughEltwise(%arg0: tensor<1x16x256x256xf16, {order = #NHWC}>, %arg1: tensor<1x16x256x256xf16, {order = #NHWC}>) -> tensor<1x4x512x512xf16, {order = #NHWC}> {
    %0 = IE.Slice %arg0 [0, 0, 0, 0] [1, 12, 256, 256] : tensor<1x16x256x256xf16, {order = #NHWC}> to tensor<1x12x256x256xf16, {order = #NHWC}>
    %1 = IE.DepthToSpace(%0) {block_size = 2 : i64, mode = #IE.depth_to_space_mode<BLOCKS_FIRST>} : tensor<1x12x256x256xf16, {order = #NHWC}> -> tensor<1x3x512x512xf16, {order = #NHWC}>
    %2 = IE.Slice %arg1 [0, 0, 0, 0] [1, 12, 256, 256] : tensor<1x16x256x256xf16, {order = #NHWC}> to tensor<1x12x256x256xf16, {order = #NHWC}>
    %3 = IE.DepthToSpace(%2) {block_size = 2 : i64, mode = #IE.depth_to_space_mode<BLOCKS_FIRST>} : tensor<1x12x256x256xf16, {order = #NHWC}> -> tensor<1x3x512x512xf16, {order = #NHWC}>
    %4 = IE.ShapeCast {shape = [1, 16, 256, 192]} inputs(%1 : tensor<1x3x512x512xf16, {order = #NHWC}>) -> tensor<1x16x256x192xf16, {order = #NHWC}>
    %5 = IE.ShapeCast {shape = [1, 16, 256, 192]} inputs(%3 : tensor<1x3x512x512xf16, {order = #NHWC}>) -> tensor<1x16x256x192xf16, {order = #NHWC}>
    %6 = IE.Add(%4, %5) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>} : tensor<1x16x256x192xf16, {order = #NHWC}>, tensor<1x16x256x192xf16, {order = #NHWC}> -> tensor<1x16x256x192xf16, {order = #NHWC}>
    %7 = IE.ShapeCast {shape = [1, 3, 512, 512]} inputs(%6 : tensor<1x16x256x192xf16, {order = #NHWC}>) -> tensor<1x3x512x512xf16, {order = #NHWC}>
    %8 = IE.Expand(%7) {pads_begin = [0, 0, 0, 0], pads_end = [0, 1, 0, 0]} : tensor<1x3x512x512xf16, {order = #NHWC}> -> tensor<1x4x512x512xf16, {order = #NHWC}>
    return %8 : tensor<1x4x512x512xf16, {order = #NHWC}>


    //CHECK:    [[SLICE_0:%.*]] = IE.Slice %arg0 [0, 0, 0, 0] [1, 12, 256, 256] 
    //CHECK:    [[D2S_0:%.*]] = IE.DepthToSpace([[SLICE_0]])
    //CHECK:    [[SLICE_1:%.*]] = IE.Slice %arg1 [0, 0, 0, 0] [1, 12, 256, 256] 
    //CHECK:    [[D2S_1:%.*]] = IE.DepthToSpace([[SLICE_1]])
    //CHECK-NOT: IE.Expand
    //CHECK:    [[SHAPECAST_0:%.*]] = IE.ShapeCast {shape = [1, 16, 256, 192]} inputs([[D2S_0]] 
    //CHECK:    [[SHAPECAST_1:%.*]] = IE.ShapeCast {shape = [1, 16, 256, 192]} inputs([[D2S_1]] 
    //CHECK:    [[ADD:%.*]] = IE.Add([[SHAPECAST_0]], [[SHAPECAST_1]]) 
    //CHECK:    [[SHAPECAST_2:%.*]] = IE.ShapeCast {shape = [1, 3, 512, 512]} inputs([[ADD]] 
    //CHECK:    [[EXPAND:%.*]] = IE.Expand([[SHAPECAST_2]])
    //CHECK:    return [[EXPAND]] 

}

// -----


#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>
!qElemType0 = !quant.uniform<u8:f16, 0.010857077205882353:127>
!qElemType1 = !quant.uniform<u8<0:254>:f16:0, {0.0038966381643700788:127,0.0046252153051181098:127,0.0039408526082677165:127,0.0037697619340551179:127,0.0032103531003937007:127,0.0037832185039370077:127,0.0035102423720472439:127,0.0035986712598425198:127,0.0036063607283464568:127,0.0038889486958661418:127,0.0055940883366141728:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127}>

// CHECK-LABEL: @FuseWithDepthToSpace
func.func @FuseWithDepthToSpace(%arg0: tensor<1x32x256x256x!qElemType0, {order = #NHWC}>) -> tensor<1x4x512x512x!qElemType0, {order = #NHWC}> {
    %cst_0 = const.Declare tensor<16x32x3x3x!qElemType1, {order = #NHWC}> = dense<1.0> : tensor<16x32x3x3xf16>, [#const.ConvertElemType<ui8>, #const.QuantCast<!qElemType1>, #const.Reorder<#NHWC>]
    %0 = IE.Convolution(%arg0, %cst_0) {dilations = [1, 1], pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x32x256x256x!qElemType0, {order = #NHWC}>, tensor<16x32x3x3x!qElemType1, {order = #NHWC}> -> tensor<1x16x256x256x!qElemType0, {order = #NHWC}>
    %1 = IE.Slice %0 [0, 0, 0, 0] [1, 12, 256, 256] : tensor<1x16x256x256x!qElemType0, {order = #NHWC}> to tensor<1x12x256x256x!qElemType0, {order = #NHWC}>
    %2 = IE.DepthToSpace(%1) {block_size = 2 : i64, mode = #IE.depth_to_space_mode<BLOCKS_FIRST>} : tensor<1x12x256x256x!qElemType0, {order = #NHWC}> -> tensor<1x3x512x512x!qElemType0, {order = #NHWC}>
    %3 = IE.Expand(%2) {pads_begin = [0, 0, 0, 0], pads_end = [0, 1, 0, 0]} : tensor<1x3x512x512x!qElemType0, {order = #NHWC}> -> tensor<1x4x512x512x!qElemType0, {order = #NHWC}>
    return %3 : tensor<1x4x512x512x!qElemType0, {order = #NHWC}>

    //CHECK:    [[CST:%.*]] = const.Declare tensor<1x16x256x256x!qElemType0, {order = #NHWC}> = dense<1.270000e+02> : tensor<1x16x256x256xf32>, [#const.ConvertElemType<ui8>, #const.QuantCast<!qElemType0>, #const.Reorder<#NHWC>]
    //CHECK:    [[CST_0:%.*]] = const.Declare tensor<16x16x2x2x!qElemType1, {order = #NHWC}>
    //CHECK-SAME:   [#const.ConvertElemType<ui8>, #const.QuantCast<!qElemType1>, #const.Reorder<#NHWC>]
    //CHECK:    [[CST_1:%.*]] = const.Declare tensor<16x32x3x3x!qElemType2, {order = #NHWC}> = dense<1.000000e+00> : tensor<16x32x3x3xf16>, [#const.ConvertElemType<ui8>, #const.QuantCast<!qElemType2>, #const.Reorder<#NHWC>]
    //CHECK:    [[CONV:%.*]] = IE.Convolution(%arg0, [[CST_1]]) {dilations = [1, 1], pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x32x256x256x!qElemType0, {order = #NHWC}>, tensor<16x32x3x3x!qElemType2, {order = #NHWC}> -> tensor<1x16x256x256x!qElemType0, {order = #NHWC}>
    //CHECK:    [[CONCAT:%.*]] = IE.Concat([[CONV]], [[CST]]) {per_axis = #IE.Concat<axis = 2 : i64, offset = 1 : i64, stride = 2 : i64>} : tensor<1x16x256x256x!qElemType0, {order = #NHWC}>, tensor<1x16x256x256x!qElemType0, {order = #NHWC}> -> tensor<1x16x512x256x!qElemType0, {order = #NHWC}>
    //CHECK:    [[CONV_1:%.*]] = IE.Convolution([[CONCAT]], [[CST_0]]) {dilations = [1, 1], pads_begin = [1, 0], pads_end = [0, 0], strides = [1, 2]} : tensor<1x16x512x256x!qElemType0, {order = #NHWC}>, tensor<16x16x2x2x!qElemType1, {order = #NHWC}> -> tensor<1x16x512x128x!qElemType0, {order = #NHWC}>
    //CHECK:    [[RESHAPE:%.*]] = IE.AffineReshape([[CONV_1]])
    //CHECK-SAME{LITERAL}:    {dim_mapping = [[0], [1], [2], [3]], shape_value = [1, 4, 512, 512]} : tensor<1x16x512x128x!qElemType0, {order = #NHWC}> -> tensor<1x4x512x512x!qElemType0, {order = #NHWC}>
    //CHECK:    return [[RESHAPE]] : tensor<1x4x512x512x!qElemType0, {order = #NHWC}>
}

// -----


#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>
!qElemType0 = !quant.uniform<u8:f16, 0.010857077205882353:127>
!qElemType1 = !quant.uniform<u8<0:254>:f16:0, {0.0038966381643700788:127,0.0046252153051181098:127,0.0039408526082677165:127,0.0037697619340551179:127,0.0032103531003937007:127,0.0037832185039370077:127,0.0035102423720472439:127,0.0035986712598425198:127,0.0036063607283464568:127,0.0038889486958661418:127,0.0055940883366141728:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127}>

// CHECK-LABEL: @NoFuseWithDepthToSpace
func.func @NoFuseWithDepthToSpace(%arg0: tensor<1x32x256x256x!qElemType0, {order = #NHWC}>) -> tensor<1x4x512x512x!qElemType0, {order = #NHWC}> {
    %cst_0 = const.Declare tensor<16x32x3x3x!qElemType1, {order = #NHWC}> = dense<1.0> : tensor<16x32x3x3xf16>, [#const.ConvertElemType<ui8>, #const.QuantCast<!qElemType1>, #const.Reorder<#NHWC>]
    %0 = IE.Convolution(%arg0, %cst_0) {dilations = [1, 1], pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x32x256x256x!qElemType0, {order = #NHWC}>, tensor<16x32x3x3x!qElemType1, {order = #NHWC}> -> tensor<1x16x256x256x!qElemType0, {order = #NHWC}>
    %1 = IE.Slice %0 [0, 4, 0, 0] [1, 12, 256, 256] : tensor<1x16x256x256x!qElemType0, {order = #NHWC}> to tensor<1x12x256x256x!qElemType0, {order = #NHWC}>
    %2 = IE.DepthToSpace(%1) {block_size = 2 : i64, mode = #IE.depth_to_space_mode<BLOCKS_FIRST>} : tensor<1x12x256x256x!qElemType0, {order = #NHWC}> -> tensor<1x3x512x512x!qElemType0, {order = #NHWC}>
    %3 = IE.Expand(%2) {pads_begin = [0, 0, 0, 0], pads_end = [0, 1, 0, 0]} : tensor<1x3x512x512x!qElemType0, {order = #NHWC}> -> tensor<1x4x512x512x!qElemType0, {order = #NHWC}>
    return %3 : tensor<1x4x512x512x!qElemType0, {order = #NHWC}>


    //CHECK:    [[CST:%.*]] = const.Declare tensor<16x32x3x3x!qElemType1, {order = #NHWC}> = dense<1.000000e+00> : tensor<16x32x3x3xf16>, [#const.ConvertElemType<ui8>, #const.QuantCast<!qElemType1>, #const.Reorder<#NHWC>]
    //CHECK:    [[CONV:%.*]] = IE.Convolution(%arg0, [[CST]]) {dilations = [1, 1], pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x32x256x256x!qElemType0, {order = #NHWC}>, tensor<16x32x3x3x!qElemType1, {order = #NHWC}> -> tensor<1x16x256x256x!qElemType0, {order = #NHWC}>
    //CHECK:    [[SLICE:%.*]] = IE.Slice [[CONV]] [0, 4, 0, 0] [1, 12, 256, 256] : tensor<1x16x256x256x!qElemType0, {order = #NHWC}> to tensor<1x12x256x256x!qElemType0, {order = #NHWC}>
    //CHECK:    [[D2S:%.*]] = IE.DepthToSpace([[SLICE]]) {block_size = 2 : i64, mode = #IE.depth_to_space_mode<BLOCKS_FIRST>} : tensor<1x12x256x256x!qElemType0, {order = #NHWC}> -> tensor<1x3x512x512x!qElemType0, {order = #NHWC}>
    //CHECK:    [[EXPAND:%.*]] = IE.Expand([[D2S]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 1, 0, 0]} : tensor<1x3x512x512x!qElemType0, {order = #NHWC}> -> tensor<1x4x512x512x!qElemType0, {order = #NHWC}>
    //CHECK:    return [[EXPAND]] : tensor<1x4x512x512x!qElemType0, {order = #NHWC}>

}

// -----


#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>
!qElemType0 = !quant.uniform<u8:f16, 0.010857077205882353:127>
!qElemType1 = !quant.uniform<u8<0:254>:f16:0, {0.0038966381643700788:127,0.0046252153051181098:127,0.0039408526082677165:127,0.0037697619340551179:127,0.0032103531003937007:127,0.0037832185039370077:127,0.0035102423720472439:127,0.0035986712598425198:127,0.0036063607283464568:127,0.0038889486958661418:127,0.0055940883366141728:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127}>

//CHECK: !qElemType0 = !quant.uniform<u8:f16, 0.010857077205882353:127>
//CHECK: !qElemType1 = !quant.uniform<u8:f16, 1.000000e+00>
//CHECK: !qElemType2 = !quant.uniform<u8<0:254>:f16:0, {0.0038966381643700788:127,0.0046252153051181098:127,0.0039408526082677165:127,0.0037697619340551179:127,0.0032103531003937007:127,0.0037832185039370077:127,0.0035102423720472439:127,0.0035986712598425198:127,0.0036063607283464568:127,0.0038889486958661418:127,0.0055940883366141728:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127,0.0032661017470472439:127}>

// CHECK-LABEL: @FuseWithSpaceToDepthU8
func.func @FuseWithSpaceToDepthU8(%arg0: tensor<1x3x512x512xf16>) -> tensor<1x16x128x128x!qElemType0, {order = #NHWC}> {
    %cst_0 = const.Declare tensor<16x48x3x3x!qElemType1, {order = #NHWC}> = dense<1.0> : tensor<16x48x3x3xf16>, [#const.ConvertElemType<ui8>, #const.QuantCast<!qElemType1>, #const.Reorder<#NHWC>]
    %0 = IE.PermuteQuantize(%arg0) {dstElemType = !qElemType0, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x3x512x512xf16> -> tensor<1x3x512x512x!qElemType0, {order = #NHWC}>
    %1 = IE.Expand(%0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 1, 0, 0]} : tensor<1x3x512x512x!qElemType0, {order = #NHWC}> -> tensor<1x4x512x512x!qElemType0, {order = #NHWC}>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 3, 512, 512] : tensor<1x4x512x512x!qElemType0, {order = #NHWC}> to tensor<1x3x512x512x!qElemType0, {order = #NHWC}>
    %3 = IE.SpaceToDepthOp(%2) {block_size = 4 : i64, mode = #IE.space_to_depth_mode<BLOCKS_FIRST>} : tensor<1x3x512x512x!qElemType0, {order = #NHWC}> -> tensor<1x48x128x128x!qElemType0, {order = #NHWC}>
    %4 = IE.Convolution(%3, %cst_0) {dilations = [1, 1], pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x48x128x128x!qElemType0, {order = #NHWC}>, tensor<16x48x3x3x!qElemType1, {order = #NHWC}> -> tensor<1x16x128x128x!qElemType0, {order = #NHWC}>
    return %4 : tensor<1x16x128x128x!qElemType0, {order = #NHWC}>

    //CHECK:    [[CST:%.*]] = const.Declare tensor<64x4x4x4x!qElemType1, {order = #NHWC}>
    //CHECK:    [[CST_0:%.*]] = const.Declare tensor<16x64x3x3x!qElemType2, {order = #NHWC}> = dense<1.000000e+00> : tensor<16x48x3x3xf16>, [#const.ConvertElemType<ui8>, #const.QuantCast<!qElemType2>, #const.Reorder<#NHWC>, #const.Reshape<[16, 3, 48, 3]>, #const.PadWithZero<[0, 0, 0, 0], [0, 1, 0, 0]>, #const.Reshape<[16, 64, 3, 3]>]
    //CHECK:    [[PQ:%.*]] = IE.PermuteQuantize(%arg0) {dstElemType = !qElemType0, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x3x512x512xf16> -> tensor<1x3x512x512x!qElemType0, {order = #NHWC}>
    //CHECK:    [[EXPAND:%.*]] = IE.Expand([[PQ]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 1, 0, 0]} : tensor<1x3x512x512x!qElemType0, {order = #NHWC}> -> tensor<1x4x512x512x!qElemType0, {order = #NHWC}>
    //CHECK:    [[S2D:%.*]] = IE.Convolution([[EXPAND]], [[CST]]) {dilations = [1, 1], pads_begin = [0, 0], pads_end = [0, 0], strides = [4, 4]} : tensor<1x4x512x512x!qElemType0, {order = #NHWC}>, tensor<64x4x4x4x!qElemType1, {order = #NHWC}> -> tensor<1x64x128x128x!qElemType0, {order = #NHWC}>
    //CHECK:    [[CONV:%.*]] = IE.Convolution([[S2D]], [[CST_0]]) {dilations = [1, 1], pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x64x128x128x!qElemType0, {order = #NHWC}>, tensor<16x64x3x3x!qElemType2, {order = #NHWC}> -> tensor<1x16x128x128x!qElemType0, {order = #NHWC}>
    //CHECK:    return [[CONV]] : tensor<1x16x128x128x!qElemType0, {order = #NHWC}>
}

// -----


#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @FuseWithSpaceToDepthF16
func.func @FuseWithSpaceToDepthF16(%arg0: tensor<1x3x512x512xf16>) -> tensor<1x16x128x128xf16, {order = #NHWC}> {
    %cst_0 = const.Declare tensor<16x48x3x3xf16, {order = #NHWC}> = dense<1.0> : tensor<16x48x3x3xf16>, [#const.Reorder<#NHWC>]
    %0 = IE.PermuteQuantize(%arg0) {dstElemType = f16, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x3x512x512xf16> -> tensor<1x3x512x512xf16, {order = #NHWC}>
    %1 = IE.Expand(%0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 1, 0, 0]} : tensor<1x3x512x512xf16, {order = #NHWC}> -> tensor<1x4x512x512xf16, {order = #NHWC}>
    %2 = IE.Slice %1 [0, 0, 0, 0] [1, 3, 512, 512] : tensor<1x4x512x512xf16, {order = #NHWC}> to tensor<1x3x512x512xf16, {order = #NHWC}>
    %3 = IE.SpaceToDepthOp(%2) {block_size = 4 : i64, mode = #IE.space_to_depth_mode<BLOCKS_FIRST>} : tensor<1x3x512x512xf16, {order = #NHWC}> -> tensor<1x48x128x128xf16, {order = #NHWC}>
    %4 = IE.Convolution(%3, %cst_0) {dilations = [1, 1], pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x48x128x128xf16, {order = #NHWC}>, tensor<16x48x3x3xf16, {order = #NHWC}> -> tensor<1x16x128x128xf16, {order = #NHWC}>
    return %4 : tensor<1x16x128x128xf16, {order = #NHWC}>

    //CHECK:    [[CST:%.*]] = const.Declare tensor<64x4x4x4xf16, {order = #NHWC}>
    //CHECK:    [[CST_0:%.*]] = const.Declare tensor<16x64x3x3xf16, {order = #NHWC}> = dense<1.000000e+00> : tensor<16x48x3x3xf16>, [#const.Reorder<#NHWC>, #const.Reshape<[16, 3, 48, 3]>, #const.PadWithZero<[0, 0, 0, 0], [0, 1, 0, 0]>, #const.Reshape<[16, 64, 3, 3]>]
    //CHECK:    [[PQ:%.*]] = IE.PermuteQuantize(%arg0) {dstElemType = f16, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x3x512x512xf16> -> tensor<1x3x512x512xf16, {order = #NHWC}>
    //CHECK:    [[EXPAND:%.*]] = IE.Expand([[PQ]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 1, 0, 0]} : tensor<1x3x512x512xf16, {order = #NHWC}> -> tensor<1x4x512x512xf16, {order = #NHWC}>
    //CHECK:    [[S2D:%.*]] = IE.Convolution([[EXPAND]], [[CST]]) {dilations = [1, 1], pads_begin = [0, 0], pads_end = [0, 0], strides = [4, 4]} : tensor<1x4x512x512xf16, {order = #NHWC}>, tensor<64x4x4x4xf16, {order = #NHWC}> -> tensor<1x64x128x128xf16, {order = #NHWC}>
    //CHECK:    [[CONV:%.*]] = IE.Convolution([[S2D]], [[CST_0]]) {dilations = [1, 1], pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x64x128x128xf16, {order = #NHWC}>, tensor<16x64x3x3xf16, {order = #NHWC}> -> tensor<1x16x128x128xf16, {order = #NHWC}>
    //CHECK:    return [[CONV]] : tensor<1x16x128x128xf16, {order = #NHWC}>
}

// -----


#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @NoFuseWithSpaceToDepthF16
func.func @NoFuseWithSpaceToDepthF16(%arg0: tensor<1x3x512x512xf16>) -> tensor<1x16x128x128xf16, {order = #NHWC}> {
    %cst_0 = const.Declare tensor<16x48x3x3xf16, {order = #NHWC}> = dense<1.0> : tensor<16x48x3x3xf16>, [#const.Reorder<#NHWC>]
    %0 = IE.PermuteQuantize(%arg0) {dstElemType = f16, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x3x512x512xf16> -> tensor<1x3x512x512xf16, {order = #NHWC}>
    %1 = IE.Expand(%0) {pads_begin = [0, 0, 0, 0], pads_end = [0, 1, 0, 0]} : tensor<1x3x512x512xf16, {order = #NHWC}> -> tensor<1x4x512x512xf16, {order = #NHWC}>
    %2 = IE.Slice %1 [0, 1, 0, 0] [1, 3, 512, 512] : tensor<1x4x512x512xf16, {order = #NHWC}> to tensor<1x3x512x512xf16, {order = #NHWC}>
    %3 = IE.SpaceToDepthOp(%2) {block_size = 4 : i64, mode = #IE.space_to_depth_mode<BLOCKS_FIRST>} : tensor<1x3x512x512xf16, {order = #NHWC}> -> tensor<1x48x128x128xf16, {order = #NHWC}>
    %4 = IE.Convolution(%3, %cst_0) {dilations = [1, 1], pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x48x128x128xf16, {order = #NHWC}>, tensor<16x48x3x3xf16, {order = #NHWC}> -> tensor<1x16x128x128xf16, {order = #NHWC}>
    return %4 : tensor<1x16x128x128xf16, {order = #NHWC}>

    //CHECK:    [[CST_0:%.*]] = const.Declare tensor<16x48x3x3xf16, {order = #NHWC}> = dense<1.000000e+00> : tensor<16x48x3x3xf16>, [#const.Reorder<#NHWC>]
    //CHECK:    [[PQ:%.*]] = IE.PermuteQuantize(%arg0) {dstElemType = f16, dst_order = #NHWC, mem_perm = #NHWC, pads_begin = [0, 0, 0, 0], pads_end = [0, 0, 0, 0]} : tensor<1x3x512x512xf16> -> tensor<1x3x512x512xf16, {order = #NHWC}>
    //CHECK:    [[EXPAND:%.*]] = IE.Expand([[PQ]]) {pads_begin = [0, 0, 0, 0], pads_end = [0, 1, 0, 0]} : tensor<1x3x512x512xf16, {order = #NHWC}> -> tensor<1x4x512x512xf16, {order = #NHWC}>
    //CHECK:    [[SLICE:%.*]] = IE.Slice [[EXPAND]] [0, 1, 0, 0] [1, 3, 512, 512] : tensor<1x4x512x512xf16, {order = #NHWC}> to tensor<1x3x512x512xf16, {order = #NHWC}>
    //CHECK:    [[S2D:%.*]] = IE.SpaceToDepthOp([[SLICE]]) {block_size = 4 : i64, mode = #IE.space_to_depth_mode<BLOCKS_FIRST>} : tensor<1x3x512x512xf16, {order = #NHWC}> -> tensor<1x48x128x128xf16, {order = #NHWC}>
    //CHECK:    [[CONV:%.*]] = IE.Convolution([[S2D]], [[CST_0]]) {dilations = [1, 1], pads_begin = [1, 1], pads_end = [1, 1], strides = [1, 1]} : tensor<1x48x128x128xf16, {order = #NHWC}>, tensor<16x48x3x3xf16, {order = #NHWC}> -> tensor<1x16x128x128xf16, {order = #NHWC}>
    //CHECK:    return [[CONV]] : tensor<1x16x128x128xf16, {order = #NHWC}>
}
