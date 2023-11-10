//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch% compilation-mode=DefaultHW" --vertical-fusion-tiling %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!qElemType0 = !quant.uniform<u8:f16, 0.013744638480392157:128>
!qElemType1 = !quant.uniform<u8<0:254>:f16:0, {0.003883181594488189:127,0.0031930517962598425:127,0.0036140501968503938:127,0.0036563422736220473:127,0.0035063976377952754:127,0.0039908341535433069:127,0.0036659541092519685:127,0.003196896530511811:127,0.0035217765748031494:127,0.0032622570127952754:127,0.0038408895177165355:127,0.0035256213090551179:127,0.0038332000492125986:127,0.003371831938976378:127,0.0035813699557086616:127,0.0037024790846456692:127,0.0038197434793307088:127,0.0036121278297244095:127,0.0033449187992125986:127,0.0031161571112204725:127,0.0036505751722440945:127,0.0034890963336614172:127,0.0038735697588582678:127,0.0033756766732283465:127,0.0030584860974409451:127,0.0037178580216535432:127,0.003456416092519685:127,0.0033256951279527561:127,0.0033487635334645671:127,0.0041484682578740153:127,0.0041215551181102358:127,0.0034910187007874014:127}>

func.func @VfTilingWithEltwise(%arg0: tensor<1x16x256x256x!qElemType0, {order = #NHWC}>, %wt: tensor<32x1x1x4xsi32>, %weights_1: tensor<32x16x3x3x!qElemType1, {order = #NHWC}>, %weights_2: tensor<32x32x3x3x!qElemType1, {order = #NHWC}>) -> tensor<1x32x256x256x!qElemType0, {order = #NHWC}>  {
    %0 = VPU.VerticalFusion (%arg0 as %arg1: tensor<1x16x256x256x!qElemType0, {order = #NHWC}>, %weights_1 as %arg2: tensor<32x16x3x3x!qElemType1, {order = #NHWC}>, %wt as %arg3: tensor<32x1x1x4xsi32>, %weights_2 as %arg4: tensor<32x32x3x3x!qElemType1, {order = #NHWC}>) attributes {tilingStrategy = [1, 1, 2, 1]} -> tensor<1x32x256x256x!qElemType0, {order = #NHWC}> {
      %1 = VPU.NCE.Convolution(%arg1, %arg2, %arg3) 
         {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, 
         pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 1 : i64>, 
         ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, 
         rawFilterShape = [32, 16, 3, 3], strides = [1, 1]} -> tensor<1x32x256x256x!qElemType0, {order = #NHWC}> 
      %2 = VPU.NCE.Convolution(%1, %arg4, %arg3) 
         {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, 
         pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 1 : i64>, 
         ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, 
         rawFilterShape = [32, 32, 3, 3], strides = [1, 1]} -> tensor<1x32x256x256x!qElemType0, {order = #NHWC}> 
      %3 = VPU.NCE.Eltwise(%1, %2) 
         {is_inplace = true, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, op_type = #VPU.eltwise_type<ADD>, 
         ppe = #VPU.PPETask<clamp_high = 255 : i64, clamp_low = 0 : i64, fp_prelu_alpha = 1.000000e+00 : f64, in1_quant_mult = [24118], in2_quant_mult = [25852], lrelu_mult = 1 : i64, lrelu_shift = 0 : i64, mode = <NOOP>, quant_mult = [25869], quant_post_shift = 0 : i64, quant_shift = [30]>} 
         -> tensor<1x32x256x256x!qElemType0, {order = #NHWC}> 
      VPU.Yield %3 
    }

    return %0 : tensor<1x32x256x256x!qElemType0, {order = #NHWC}> 

    // CHECK: [[SLICEARG0TILE0:%.+]] = VPU.Slice %arg0 [0, 0, 0, 0] [1, 16, 130, 256] 
    // CHECK: [[CONV0TILE0:%.+]] = VPU.NCE.Convolution([[SLICEARG0TILE0]], %arg2, %arg1) 
    // CHECK-SAME: {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, rawFilterShape = [32, 16, 3, 3], strides = [1, 1]} -> tensor<1x32x129x256x!qElemType0, {order = #NHWC}> 
    // CHECK: [[CONV1TILE0:%.+]] = VPU.NCE.Convolution([[CONV0TILE0]], %arg3, %arg1) 
    // CHECK-SAME: {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 1 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, rawFilterShape = [32, 32, 3, 3], strides = [1, 1]} -> tensor<1x32x128x256x!qElemType0, {order = #NHWC}> 
    // CHECK: [[SLICETILE0:%.+]] = VPU.Slice [[CONV0TILE0]] [0, 0, 0, 0] [1, 32, 128, 256] 
    // CHECK: [[ELTWISETILE0:%.+]] = VPU.NCE.Eltwise([[SLICETILE0]], [[CONV1TILE0]]) 
    // CHECK: [[SLICEARG0TILE1:%.+]] = VPU.Slice %arg0 [0, 0, 126, 0] [1, 16, 130, 256] 
    // CHECK: [[CONV0TILE1:%.+]] = VPU.NCE.Convolution([[SLICEARG0TILE1]], %arg2, %arg1) 
    // CHECK-SAME: {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 0 : i64, bottom = 1 : i64>, ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, rawFilterShape = [32, 16, 3, 3], strides = [1, 1]} -> tensor<1x32x129x256x!qElemType0, {order = #NHWC}> 
    // CHECK: [[CONV1TILE1:%.+]] = VPU.NCE.Convolution([[CONV0TILE1]], %arg3, %arg1) 
    // CHECK-SAME: {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 1 : i64, right = 1 : i64, top = 0 : i64, bottom = 1 : i64>, ppe = #VPU.PPETask<mode = <LPRELU>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1228 : i64, lrelu_shift = 12 : i64, fp_prelu_alpha = 0.2998046875 : f64>, rawFilterShape = [32, 32, 3, 3], strides = [1, 1]} -> tensor<1x32x128x256x!qElemType0, {order = #NHWC}> 
    // CHECK: [[SLICETILE1:%.+]] = VPU.Slice [[CONV0TILE1]] [0, 0, 1, 0] [1, 32, 128, 256] 
    // CHECK: [[ELTWISETILE1:%.+]] = VPU.NCE.Eltwise([[SLICETILE1]], [[CONV1TILE1]]) 
    // CHECK: [[CONCAT:%.+]] = VPU.Concat([[ELTWISETILE0]], [[ELTWISETILE1]]) {static_offsets = {{\[\[}}0, 0, 0, 0], [0, 0, 128, 0]]} : tensor<1x32x128x256x!qElemType0, {order = #NHWC}>, tensor<1x32x128x256x!qElemType0, {order = #NHWC}> -> tensor<1x32x256x256x!qElemType0, {order = #NHWC}>
    // CHECK: return [[CONCAT]] : tensor<1x32x256x256x!qElemType0, {order = #NHWC}>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!qElemType0 = !quant.uniform<u8<0:254>:f16, 0.003937007874015748>
!qElemType1 = !quant.uniform<u8:f16, 1.000000e+00>
!qElemType2 = !quant.uniform<u8:f16, 5.000000e-01>

func.func @VfTilingWithSwish(%arg0: tensor<1x16x176x176x!quant.uniform<u8:f16, 0.14376571505677466:128>, {order = #NHWC}>, %cst_0: tensor<1x1x1x16xui8>, %cst_1: tensor<96x16x1x1x!qElemType1, {order = #NHWC}>, %cst_2: tensor<96x1x1x4xsi32>, %cst_3: tensor<96x16x1x1xf16, {order = #NHWC}>, %cst_4: tensor<96x1x1x4xsi32>) -> tensor<1x96x176x176x!qElemType2, {order = #NHWC}>  {
   %0 = VPU.VerticalFusion (%arg0 as %arg1: tensor<1x16x176x176x!quant.uniform<u8:f16, 0.14376571505677466:128>, {order = #NHWC}>, %cst_0 as %arg2: tensor<1x1x1x16xui8>, %cst_1 as %arg3: tensor<96x16x1x1x!qElemType1, {order = #NHWC}>, %cst_2 as %arg4: tensor<96x1x1x4xsi32>, %cst_3 as %arg5: tensor<96x16x1x1xf16, {order = #NHWC}>, %cst_4 as %arg6: tensor<96x1x1x4xsi32>) attributes {tilingStrategy = [1, 1, 4, 1]} -> tensor<1x96x176x176x!qElemType2, {order = #NHWC}> {
      %1 = VPU.NCE.Convolution(%arg1, %arg3, %arg4)
         {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>,
         pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>,
         ppe = #VPU.PPETask<mode = <NOOP>, clamp_low = -2147483648 : i64, clamp_high = 2147483647 : i64, lrelu_mult = 1 : i64, lrelu_shift = 0 : i64,
         fp_prelu_alpha = 1.000000e+00 : f64>, rawFilterShape = [96, 16, 1, 1], strides = [1, 1]} -> tensor<1x96x176x176xf16, {order = #NHWC}>

      %2 = VPU.Swish(%1)
         {beta_value = 1.000000e+00 : f64, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>} : tensor<1x96x176x176xf16, {order = #NHWC}> -> tensor<1x96x176x176xf16, {order = #NHWC}>

      %3 = VPU.NCE.DepthConvolution(%2, %arg5, %arg6, %arg2)
         {activation_window_channel_length = 4 : i64, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>,
         pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>,
         ppe = #VPU.PPETask<mode = <NOOP>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1 : i64, lrelu_shift = 0 : i64,
         fp_prelu_alpha = 1.000000e+00 : f64>, rawFilterShape = [96, 1, 1, 1], strides = [1, 1]} -> tensor<1x96x176x176x!qElemType2, {order = #NHWC}>

      VPU.Yield %3
   }

   return %0 : tensor<1x96x176x176x!qElemType2, {order = #NHWC}>

   // CHECK: [[SLICE0:%.+]] = VPU.Slice %arg0 [0, 0, 0, 0] [1, 16, 44, 176] : tensor<1x16x176x176x!qElemType0, {order = #NHWC}> to tensor<1x16x44x176x!qElemType0, {order = #NHWC}>
   // CHECK: [[CONV0:%.+]] = VPU.NCE.Convolution([[SLICE0]], %arg2, %arg3) {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <NOOP>, clamp_low = -2147483648 : i64, clamp_high = 2147483647 : i64, lrelu_mult = 1 : i64, lrelu_shift = 0 : i64, fp_prelu_alpha = 1.000000e+00 : f64>, rawFilterShape = [96, 16, 1, 1], strides = [1, 1]} -> tensor<1x96x44x176xf16, {order = #NHWC}>
   // CHECK: [[SWISH0:%.+]] = VPU.Swish([[CONV0]]) {beta_value = 1.000000e+00 : f64, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>} : tensor<1x96x44x176xf16, {order = #NHWC}> -> tensor<1x96x44x176xf16, {order = #NHWC}>
   // CHECK: [[DEPTHCONV0:%.+]] = VPU.NCE.DepthConvolution([[SWISH0]], %arg4, %arg5, %arg1) {activation_window_channel_length = 4 : i64, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <NOOP>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1 : i64, lrelu_shift = 0 : i64, fp_prelu_alpha = 1.000000e+00 : f64>, rawFilterShape = [96, 1, 1, 1], strides = [1, 1]} -> tensor<1x96x44x176x!qElemType2, {order = #NHWC}>

   // CHECK: [[SLICE1:%.+]] = VPU.Slice %arg0 [0, 0, 44, 0] [1, 16, 44, 176] : tensor<1x16x176x176x!qElemType0, {order = #NHWC}> to tensor<1x16x44x176x!qElemType0, {order = #NHWC}>
   // CHECK: [[CONV1:%.+]] = VPU.NCE.Convolution([[SLICE1]], %arg2, %arg3) {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <NOOP>, clamp_low = -2147483648 : i64, clamp_high = 2147483647 : i64, lrelu_mult = 1 : i64, lrelu_shift = 0 : i64, fp_prelu_alpha = 1.000000e+00 : f64>, rawFilterShape = [96, 16, 1, 1], strides = [1, 1]} -> tensor<1x96x44x176xf16, {order = #NHWC}>
   // CHECK: [[SWISH1:%.+]] = VPU.Swish([[CONV1]]) {beta_value = 1.000000e+00 : f64, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>} : tensor<1x96x44x176xf16, {order = #NHWC}> -> tensor<1x96x44x176xf16, {order = #NHWC}>
   // CHECK: [[DEPTHCONV1:%.+]] = VPU.NCE.DepthConvolution([[SWISH1]], %arg4, %arg5, %arg1) {activation_window_channel_length = 4 : i64, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <NOOP>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1 : i64, lrelu_shift = 0 : i64, fp_prelu_alpha = 1.000000e+00 : f64>, rawFilterShape = [96, 1, 1, 1], strides = [1, 1]} -> tensor<1x96x44x176x!qElemType2, {order = #NHWC}>

   // CHECK: [[SLICE2:%.+]] = VPU.Slice %arg0 [0, 0, 88, 0] [1, 16, 44, 176] : tensor<1x16x176x176x!qElemType0, {order = #NHWC}> to tensor<1x16x44x176x!qElemType0, {order = #NHWC}>
   // CHECK: [[CONV2:%.+]] = VPU.NCE.Convolution([[SLICE2]], %arg2, %arg3) {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <NOOP>, clamp_low = -2147483648 : i64, clamp_high = 2147483647 : i64, lrelu_mult = 1 : i64, lrelu_shift = 0 : i64, fp_prelu_alpha = 1.000000e+00 : f64>, rawFilterShape = [96, 16, 1, 1], strides = [1, 1]} -> tensor<1x96x44x176xf16, {order = #NHWC}>
   // CHECK: [[SWISH2:%.+]] = VPU.Swish([[CONV2]]) {beta_value = 1.000000e+00 : f64, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>} : tensor<1x96x44x176xf16, {order = #NHWC}> -> tensor<1x96x44x176xf16, {order = #NHWC}>
   // CHECK: [[DEPTHCONV2:%.+]] = VPU.NCE.DepthConvolution([[SWISH2]], %arg4, %arg5, %arg1) {activation_window_channel_length = 4 : i64, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <NOOP>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1 : i64, lrelu_shift = 0 : i64, fp_prelu_alpha = 1.000000e+00 : f64>, rawFilterShape = [96, 1, 1, 1], strides = [1, 1]} -> tensor<1x96x44x176x!qElemType2, {order = #NHWC}>

   // CHECK: [[SLICE3:%.+]] = VPU.Slice %arg0 [0, 0, 132, 0] [1, 16, 44, 176] : tensor<1x16x176x176x!qElemType0, {order = #NHWC}> to tensor<1x16x44x176x!qElemType0, {order = #NHWC}>
   // CHECK: [[CONV3:%.+]] = VPU.NCE.Convolution([[SLICE3]], %arg2, %arg3) {multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <NOOP>, clamp_low = -2147483648 : i64, clamp_high = 2147483647 : i64, lrelu_mult = 1 : i64, lrelu_shift = 0 : i64, fp_prelu_alpha = 1.000000e+00 : f64>, rawFilterShape = [96, 16, 1, 1], strides = [1, 1]} -> tensor<1x96x44x176xf16, {order = #NHWC}>
   // CHECK: [[SWISH3:%.+]] = VPU.Swish([[CONV3]]) {beta_value = 1.000000e+00 : f64, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>} : tensor<1x96x44x176xf16, {order = #NHWC}> -> tensor<1x96x44x176xf16, {order = #NHWC}>
   // CHECK: [[DEPTHCONV3:%.+]] = VPU.NCE.DepthConvolution([[SWISH3]], %arg4, %arg5, %arg1) {activation_window_channel_length = 4 : i64, multiClusterStrategy = #VPU.multi_cluster_strategy<SplitOverHeight>, pad = #VPU.Padding<left = 0 : i64, right = 0 : i64, top = 0 : i64, bottom = 0 : i64>, ppe = #VPU.PPETask<mode = <NOOP>, clamp_low = 0 : i64, clamp_high = 255 : i64, lrelu_mult = 1 : i64, lrelu_shift = 0 : i64, fp_prelu_alpha = 1.000000e+00 : f64>, rawFilterShape = [96, 1, 1, 1], strides = [1, 1]} -> tensor<1x96x44x176x!qElemType2, {order = #NHWC}>

   // CHECK: [[CONCAT:%.+]] = VPU.Concat([[DEPTHCONV0]], [[DEPTHCONV1]], [[DEPTHCONV2]], [[DEPTHCONV3]]) {static_offsets = {{\[\[}}0, 0, 0, 0], [0, 0, 44, 0], [0, 0, 88, 0], [0, 0, 132, 0]]} : tensor<1x96x44x176x!qElemType2, {order = #NHWC}>, tensor<1x96x44x176x!qElemType2, {order = #NHWC}>, tensor<1x96x44x176x!qElemType2, {order = #NHWC}>, tensor<1x96x44x176x!qElemType2, {order = #NHWC}> -> tensor<1x96x176x176x!qElemType2, {order = #NHWC}>
   // CHECK: return [[CONCAT]] : tensor<1x96x176x176x!qElemType2, {order = #NHWC}>
}
