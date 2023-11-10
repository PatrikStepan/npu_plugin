//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch% compilation-mode=DefaultHW" --unroll-unused-vertical-fusion %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @UnrollVFRegion(%arg0: tensor<1x32x256x256xf16, {order = #NHWC}>, %wt: tensor<32x1x1x4xsi32>, %weights: tensor<32x32x3x3xf16, {order = #NHWC}>) -> tensor<1x32x256x256xf16, {order = #NHWC}> {
    %0 = VPU.VerticalFusion (%arg0 as %arg1: tensor<1x32x256x256xf16, {order = #NHWC}>, %weights as %arg2: tensor<32x32x3x3xf16, {order = #NHWC}>, %wt as %arg3: tensor<32x1x1x4xsi32>) attributes {tilingStrategy = [1, 1, 2, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}> {
      %1 = VPU.NCE.Convolution(%arg1, %arg2, %arg3) {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 1 : i64>, ppe = #VPU.PPETask<clamp_high = 255 : i64, clamp_low = 0 : i64, fp_prelu_alpha = 0.2998046875 : f64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, mode = <LPRELU>>, rawFilterShape = [32, 32, 3, 3], strides = [1, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}> 
      VPU.Yield %1 
    }
    return %0 : tensor<1x32x256x256xf16, {order = #NHWC}>

    //CHECK-NOT: VPU.VerticalFusion
    //CHECK:     VPU.NCE.Convolution
    //CHECK-SAME: {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 1 : i64>, 
    //CHECK-SAME:  ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>,
    //CHECK-SAME:  rawFilterShape = [32, 32, 3, 3], strides = [1, 1], tilingStrategy = [1, 1, 2, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}>

}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @UnrollVFTwoOpsRegion(%arg0: tensor<1x32x256x256xf16, {order = #NHWC}>, %wt: tensor<32x1x1x4xsi32>, %weights: tensor<32x32x3x3xf16, {order = #NHWC}>) -> tensor<1x16x256x256xf16, {order = #NHWC}> {
    %0 = VPU.VerticalFusion (%arg0 as %arg1: tensor<1x32x256x256xf16, {order = #NHWC}>, %weights as %arg2: tensor<32x32x3x3xf16, {order = #NHWC}>, %wt as %arg3: tensor<32x1x1x4xsi32>) attributes {tilingStrategy = [1, 1, 2, 1]} -> tensor<1x16x256x256xf16, {order = #NHWC}> {
      %1 = VPU.NCE.Convolution(%arg1, %arg2, %arg3) {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 1 : i64>, ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, rawFilterShape = [32, 32, 3, 3], strides = [1, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}>
      %2 = VPU.Slice %1 [0, 0, 0, 0] [1, 16, 256, 256] : tensor<1x32x256x256xf16, {order = #NHWC}> to tensor<1x16x256x256xf16, {order = #NHWC}>
      VPU.Yield %2
    }
    return %0 : tensor<1x16x256x256xf16, {order = #NHWC}>

    //CHECK-NOT: VPU.VerticalFusion
    //CHECK:     VPU.NCE.Convolution
    //CHECK-SAME: {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 1 : i64>, ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>,
    //CHECK-SAME: rawFilterShape = [32, 32, 3, 3], strides = [1, 1], tilingStrategy = [1, 1, 2, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}> 
    //CHECK:     VPU.Slice
    //CHECK-SAME:  [0, 0, 0, 0] [1, 16, 256, 256] : tensor<1x32x256x256xf16, {order = #NHWC}> to tensor<1x16x256x256xf16, {order = #NHWC}>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

func.func @DontUnrollVFTwoOpsRegion(%arg0: tensor<1x32x256x256xf16, {order = #NHWC}>, %wt: tensor<32x1x1x4xsi32>, %weights1: tensor<32x32x1x1xf16, {order = #NHWC}>, %weights2: tensor<32x32x3x3xf16, {order = #NHWC}>) -> tensor<1x16x256x256xf16, {order = #NHWC}> {
    %0 = VPU.VerticalFusion (%arg0 as %arg1: tensor<1x32x256x256xf16, {order = #NHWC}>, %weights1 as %arg2: tensor<32x32x1x1xf16, {order = #NHWC}>, %weights2 as %arg3: tensor<32x32x3x3xf16, {order = #NHWC}>, %wt as %arg4: tensor<32x1x1x4xsi32>) attributes {tilingStrategy = [1, 1, 2, 1]} -> tensor<1x16x256x256xf16, {order = #NHWC}> {
      %1 = VPU.NCE.Convolution(%arg1, %arg2, %arg4) {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, rawFilterShape = [32, 32, 1, 1], strides = [1, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}>
      %2 = VPU.NCE.Convolution(%1, %arg3, %arg4) {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 1 : i64>, ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, rawFilterShape = [32, 32, 3, 3], strides = [1, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}>
      VPU.Yield %2
    }  
    return %0 : tensor<1x16x256x256xf16, {order = #NHWC}>

    //CHECK: [[VF:%.+]] = VPU.VerticalFusion (%arg0 as %arg4: tensor<1x32x256x256xf16, {order = #NHWC}>, %arg2 as %arg5: tensor<32x32x1x1xf16, {order = #NHWC}>, %arg3 as %arg6: tensor<32x32x3x3xf16, {order = #NHWC}>, %arg1 as %arg7: tensor<32x1x1x4xsi32>) attributes {tilingStrategy = [1, 1, 2, 1]} -> tensor<1x16x256x256xf16, {order = #NHWC}> {
    //CHECK: [[CONV0:%.+]] = VPU.NCE.Convolution(%arg4, %arg5, %arg7) {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, rawFilterShape = [32, 32, 1, 1], strides = [1, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}>
    //CHECK: [[CONV1:%.+]] = VPU.NCE.Convolution([[CONV0]], %arg6, %arg7) {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 1 : i64>, ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, rawFilterShape = [32, 32, 3, 3], strides = [1, 1]} -> tensor<1x32x256x256xf16, {order = #NHWC}> 
    //CHECK:   VPU.Yield [[CONV1]]  
    //CHECK: return [[VF]] : tensor<1x16x256x256xf16, {order = #NHWC}>
}

