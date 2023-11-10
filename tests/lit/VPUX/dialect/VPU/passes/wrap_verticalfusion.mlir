//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch% compilation-mode=DefaultHW" --wrap-in-vertical-fusion %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @WrapNCETiledTask(%arg0: tensor<1x32x256x256xf16, {order = #NHWC}>, %wt: tensor<32x1x1x4xsi32>, %weights: tensor<32x32x3x3xf16, {order = #NHWC}>) -> tensor<1x32x256x256xf16, {order = #NHWC}> {
       %0 = VPU.NCE.Convolution(%arg0, %weights, %wt) 
                {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, 
                pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 1 : i64>, 
                ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, 
                rawFilterShape = [32, 32, 3, 3], 
                strides = [1, 1], 
                tilingStrategy = [1, 1, 2, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}> 
    return %0 : tensor<1x32x256x256xf16, {order = #NHWC}>

    //CHECK:  VPU.VerticalFusion (%arg0 as %arg3: tensor<1x32x256x256xf16, {order = #NHWC}>, %arg2 as %arg4: tensor<32x32x3x3xf16, {order = #NHWC}>, %arg1 as %arg5: tensor<32x1x1x4xsi32>) 
    //CHECK-SAME:  attributes {tilingStrategy = [1, 1, 2, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}> {
    //CHECK:  VPU.NCE.Convolution(%arg3, %arg4, %arg5) 
    //CHECK-SAME:  {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 1 : i64>, 
    //CHECK-SAME:  ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, 
    //CHECK-SAME:  rawFilterShape = [32, 32, 3, 3], strides = [1, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}> 
    //CHECK:    VPU.Yield  

}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @WrapNCENonTiledTask(%arg0: tensor<1x32x256x256xf16, {order = #NHWC}>, %wt: tensor<32x1x1x4xsi32>, %weights: tensor<32x32x1x1xf16, {order = #NHWC}>) -> tensor<1x32x256x256xf16, {order = #NHWC}> {
       %0 = VPU.NCE.Convolution(%arg0, %weights, %wt) 
                {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, 
                pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, 
                ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, 
                rawFilterShape = [32, 32, 1, 1], 
                strides = [1, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}> 
    return %0 : tensor<1x32x256x256xf16, {order = #NHWC}>

    //CHECK:  VPU.VerticalFusion (%arg0 as %arg3: tensor<1x32x256x256xf16, {order = #NHWC}>, %arg2 as %arg4: tensor<32x32x1x1xf16, {order = #NHWC}>, %arg1 as %arg5: tensor<32x1x1x4xsi32>) 
    //CHECK-SAME:  attributes {tilingStrategy = [1, 1, 1, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}> {
    //CHECK:  VPU.NCE.Convolution(%arg3, %arg4, %arg5) 
    //CHECK-SAME:  {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, 
    //CHECK-SAME:  ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, 
    //CHECK-SAME:  rawFilterShape = [32, 32, 1, 1], strides = [1, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}> 
    //CHECK:    VPU.Yield

}

// -----

func.func @WrapActivation(%arg0: tensor<1x3x512x512xf16>) -> tensor<1x3x512x512xf16> {
    %0 = VPU.Tanh(%arg0) {multiClusterStrategy = #VPU.multi_cluster_strategy<Clustering>, tilingStrategy = [1, 1, 2, 1]} : tensor<1x3x512x512xf16> -> tensor<1x3x512x512xf16>
    return %0 : tensor<1x3x512x512xf16>

    //CHECK:  VPU.VerticalFusion (%arg0 as %arg1: tensor<1x3x512x512xf16>) attributes {tilingStrategy = [1, 1, 2, 1]} -> tensor<1x3x512x512xf16> {
    //CHECK:  VPU.Tanh(%arg1) 
    //CHECK-SAME:  multiClusterStrategy = #VPU.multi_cluster_strategy<Clustering> 
    //CHECK-SAME:  tensor<1x3x512x512xf16> -> tensor<1x3x512x512xf16>
    //CHECK:    VPU.Yield  

}

// -----
func.func @WrapSwish(%arg0: tensor<1x32x176x176xf16>) -> tensor<1x32x176x176xf16> {
    %0 = VPU.Swish(%arg0) {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, tilingStrategy = [1, 1, 2, 1]} : tensor<1x32x176x176xf16> -> tensor<1x32x176x176xf16>
    return %0 : tensor<1x32x176x176xf16>
    //CHECK:  VPU.VerticalFusion (%arg0 as %arg1: tensor<1x32x176x176xf16>) attributes {tilingStrategy = [1, 1, 2, 1]} -> tensor<1x32x176x176xf16> {
    //CHECK:  VPU.Swish(%arg1)
    //CHECK-SAME:  multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>
    //CHECK-SAME:  tensor<1x32x176x176xf16> -> tensor<1x32x176x176xf16>
    //CHECK:    VPU.Yield
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @WrapMultiply(%arg0: tensor<1x4x720x1080xf16, {order = #NHWC}>, %arg1: tensor<1x4x720x1080xf16, {order = #NHWC}>) -> tensor<1x4x720x1080xf16, {order = #NHWC}> {
    %0 = VPU.Multiply(%arg0, %arg1) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, tilingStrategy = [1, 1, 5, 1]} : tensor<1x4x720x1080xf16, {order = #NHWC}>, tensor<1x4x720x1080xf16, {order = #NHWC}> -> tensor<1x4x720x1080xf16, {order = #NHWC}>
    return %0 : tensor<1x4x720x1080xf16, {order = #NHWC}>

    //CHECK:  VPU.VerticalFusion (%arg0 as %arg2: tensor<1x4x720x1080xf16, {order = #NHWC}>, %arg1 as %arg3: tensor<1x4x720x1080xf16, {order = #NHWC}>) attributes {tilingStrategy = [1, 1, 5, 1]} -> tensor<1x4x720x1080xf16, {order = #NHWC}> {
    //CHECK:  VPU.Multiply(%arg2, %arg3) {auto_broadcast = #IE.auto_broadcast_type<NUMPY>, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>} : tensor<1x4x720x1080xf16, {order = #NHWC}>, tensor<1x4x720x1080xf16, {order = #NHWC}> -> tensor<1x4x720x1080xf16, {order = #NHWC}>
    //CHECK:    VPU.Yield
}
