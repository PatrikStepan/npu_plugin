//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch%" --nn-dma-tiling %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!Input_DDR = memref<1x4x360x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>
!Output_CMX = memref<1x4x360x216xf16, #NHWC, [@CMX_NN, 0]>

func.func @SplitNNDMAWithLargePlanesNum(%input: !Input_DDR, %output: !Output_CMX) -> !Output_CMX {
    // Barriers
    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar1 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    %0 = VPURT.DeclareBuffer <DDR> <18662408> -> !Input_DDR
    %1 = VPURT.DeclareBuffer <CMX_NN> [0] <622080> -> !Output_CMX

    VPURT.Task waits(%bar0 : !VPURT.Barrier) updates(%bar1 : !VPURT.Barrier) attributes {cycleBegin = 42075751 : i64, cycleEnd = 42136605 : i64, isTrailingSWLayer = false} {
        %2 = VPUIP.NNDMA {port = 0 : i64} inputs(%0 : memref<1x4x360x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>) outputs(%1 : memref<1x4x360x216xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x4x360x216xf16, #NHWC, [@CMX_NN, 0]>
    }

    return %1: !Output_CMX

    // CHECK:    [[BAR0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK:    [[BAR1:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // CHECK:    [[INPUT_DDR_BUF0:%.*]] = VPURT.DeclareBuffer <DDR> <18662408> -> memref<1x4x180x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>
    // CHECK:    [[INPUT_DDR_BUF1:%.*]] = VPURT.DeclareBuffer <DDR> <21772808> -> memref<1x4x180x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>

    // CHECK:    [[OUTPUT_BUF:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <622080> -> memref<1x4x360x216xf16, #NHWC, [@CMX_NN, 0]>

    // CHECK:    [[OUTPUT_CMX_BUF0:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <622080> -> memref<1x4x180x216xf16, {order = #NHWC, strides = [311040, 1, 864, 4]}, [@CMX_NN, 0]>
    // CHECK:    [[OUTPUT_CMX_BUF1:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <933120> -> memref<1x4x180x216xf16, {order = #NHWC, strides = [311040, 1, 864, 4]}, [@CMX_NN, 0]>

    // CHECK:      VPURT.Task
    // CHECK:      VPUIP.NNDMA
    // CHECK-SAME    inputs([[INPUT_DDR_BUF0]] : memref<1x4x180x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>)
    // CHECK-SAME    outputs([[OUTPUT_CMX_BUF0]] : memref<1x4x180x216xf16, {order = #NHWC, strides = [311040, 1, 864, 4]}, [@CMX_NN, 0]>)

    // CHECK:      VPURT.Task
    // CHECK:      VPUIP.NNDMA
    // CHECK-SAME    inputs([[INPUT_DDR_BUF1]] : memref<1x4x180x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>)
    // CHECK-SAME    outputs([[OUTPUT_CMX_BUF1]] : memref<1x4x180x216xf16, {order = #NHWC, strides = [311040, 1, 864, 4]}, [@CMX_NN, 0]>)

    // CHECK:      return [[OUTPUT_BUF]] : memref<1x4x360x216xf16, #NHWC, [@CMX_NN, 0]>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!Input_DDR = memref<1x4x512x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>
!Output_CMX = memref<1x4x512x216xf16, #NHWC, [@CMX_NN, 0]>

func.func @UnevenSplitNNDMAWithLargePlanesNum(%input: !Input_DDR, %output: !Output_CMX) -> !Output_CMX {
    // Barriers
    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar1 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    %0 = VPURT.DeclareBuffer <DDR> <18662408> -> !Input_DDR
    %1 = VPURT.DeclareBuffer <CMX_NN> [0] <622080> -> !Output_CMX

    VPURT.Task waits(%bar0 : !VPURT.Barrier) updates(%bar1 : !VPURT.Barrier) attributes {cycleBegin = 42075751 : i64, cycleEnd = 42136605 : i64, isTrailingSWLayer = false} {
        %2 = VPUIP.NNDMA {port = 0 : i64} inputs(%0 : memref<1x4x512x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>) outputs(%1 : memref<1x4x512x216xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x4x512x216xf16, #NHWC, [@CMX_NN, 0]>
    }

    return %1: !Output_CMX

    // CHECK:    [[BAR0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK:    [[BAR1:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // CHECK:    [[INPUT_DDR_BUF0:%.*]] = VPURT.DeclareBuffer <DDR> <18662408> -> memref<1x4x171x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>
    // CHECK:    [[INPUT_DDR_BUF1:%.*]] = VPURT.DeclareBuffer <DDR> <21617288> -> memref<1x4x171x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>
    // CHECK:    [[INPUT_DDR_BUF2:%.*]] = VPURT.DeclareBuffer <DDR> <24572168> -> memref<1x4x170x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>

    // CHECK:    [[OUTPUT_BUF:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <622080> -> memref<1x4x512x216xf16, #NHWC, [@CMX_NN, 0]>

    // CHECK:    [[OUTPUT_CMX_BUF0:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <622080> -> memref<1x4x171x216xf16, {order = #NHWC, strides = [442368, 1, 864, 4]}, [@CMX_NN, 0]>
    // CHECK:    [[OUTPUT_CMX_BUF1:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <917568> -> memref<1x4x171x216xf16, {order = #NHWC, strides = [442368, 1, 864, 4]}, [@CMX_NN, 0]>
    // CHECK:    [[OUTPUT_CMX_BUF2:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <1213056> -> memref<1x4x170x216xf16, {order = #NHWC, strides = [442368, 1, 864, 4]}, [@CMX_NN, 0]>

    // CHECK:      VPURT.Task
    // CHECK:      VPUIP.NNDMA
    // CHECK-SAME    inputs([[INPUT_DDR_BUF0]] : memref<1x4x171x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>)
    // CHECK-SAME    outputs([[OUTPUT_CMX_BUF0]] : memref<1x4x171x216xf16, {order = #NHWC, strides = [442368, 1, 864, 4]}, [@CMX_NN, 0]>)

    // CHECK:      VPURT.Task
    // CHECK:      VPUIP.NNDMA
    // CHECK-SAME    inputs([[INPUT_DDR_BUF1]] : memref<1x4x171x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>)
    // CHECK-SAME    outputs([[OUTPUT_CMX_BUF1]] : memref<1x4x171x216xf16, {order = #NHWC, strides = [442368, 1, 864, 4]}, [@CMX_NN, 0]>)

    // CHECK:      VPURT.Task
    // CHECK:      VPUIP.NNDMA
    // CHECK-SAME    inputs([[INPUT_DDR_BUF2]] : memref<1x4x170x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>)
    // CHECK-SAME    outputs([[OUTPUT_CMX_BUF2]] : memref<1x4x170x216xf16, {order = #NHWC, strides = [442368, 1, 864, 4]}, [@CMX_NN, 0]>)

    // CHECK:      return [[OUTPUT_BUF]] : memref<1x4x512x216xf16, #NHWC, [@CMX_NN, 0]>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!Input_DDR = memref<1x4x180x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>
!Output_CMX = memref<1x4x180x216xf16, #NHWC, [@CMX_NN, 0]>

func.func @NotSplitNNDMAIfValidNumPlanes(%input: !Input_DDR, %output: !Output_CMX) -> !Output_CMX {
    // Barriers
    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar1 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    %0 = VPURT.DeclareBuffer <DDR> <18662408> -> !Input_DDR
    %1 = VPURT.DeclareBuffer <CMX_NN> [0] <622080> -> !Output_CMX

    VPURT.Task waits(%bar0 : !VPURT.Barrier) updates(%bar1 : !VPURT.Barrier) attributes {cycleBegin = 42075751 : i64, cycleEnd = 42136605 : i64, isTrailingSWLayer = false} {
        %2 = VPUIP.NNDMA {port = 0 : i64} inputs(%0 : memref<1x4x180x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>) outputs(%1 : memref<1x4x180x216xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x4x180x216xf16, #NHWC, [@CMX_NN, 0]>
    }

    return %1: !Output_CMX

    // CHECK:    [[BAR0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK:    [[BAR1:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // CHECK:    [[INPUT_DDR_BUF0:%.*]] = VPURT.DeclareBuffer <DDR> <18662408> -> memref<1x4x180x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>
    // CHECK:    [[OUTPUT_BUF:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <622080> -> memref<1x4x180x216xf16, #NHWC, [@CMX_NN, 0]>

    // CHECK:      VPURT.Task
    // CHECK:      VPUIP.NNDMA
    // CHECK-SAME    inputs([[INPUT_DDR_BUF0]] : memref<1x4x180x216xf16, {order = #NHWC, strides = [6220800, 1, 8640, 8]}, @DDR>)
    // CHECK-SAME    outputs([[OUTPUT_BUF]] : memref<1x4x180x216xf16, {order = #NHWC, strides = [311040, 1, 864, 4]}, [@CMX_NN, 0]>)

    // CHECK:      return [[OUTPUT_BUF]] : memref<1x4x180x216xf16, #NHWC, [@CMX_NN, 0]>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!Input_DDR = memref<1x4x360x216xf16, {order = #NHWC, strides = [3110400, 1, 8640, 4]}, @DDR>
!Output_CMX = memref<1x4x360x216xf16, #NHWC, [@CMX_NN, 0]>

func.func @NotSplitNNDMAIfSingleStrideLevel(%input: !Input_DDR, %output: !Output_CMX) -> !Output_CMX {
    // Barriers
    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar1 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    %0 = VPURT.DeclareBuffer <DDR> <18662408> -> !Input_DDR
    %1 = VPURT.DeclareBuffer <CMX_NN> [0] <622080> -> !Output_CMX

    VPURT.Task waits(%bar0 : !VPURT.Barrier) updates(%bar1 : !VPURT.Barrier) attributes {cycleBegin = 42075751 : i64, cycleEnd = 42136605 : i64, isTrailingSWLayer = false} {
        %2 = VPUIP.NNDMA {port = 0 : i64} inputs(%0 : memref<1x4x360x216xf16, {order = #NHWC, strides = [3110400, 1, 8640, 4]}, @DDR>) outputs(%1 : memref<1x4x360x216xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x4x360x216xf16, #NHWC, [@CMX_NN, 0]>
    }

    return %1: !Output_CMX

    // CHECK:    [[BAR0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK:    [[BAR1:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // CHECK:    [[INPUT_DDR_BUF0:%.*]] = VPURT.DeclareBuffer <DDR> <18662408> -> memref<1x4x360x216xf16, {order = #NHWC, strides = [3110400, 1, 8640, 4]}, @DDR>
    // CHECK:    [[OUTPUT_BUF:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <622080> -> memref<1x4x360x216xf16, #NHWC, [@CMX_NN, 0]>

    // CHECK:      VPURT.Task
    // CHECK:      VPUIP.NNDMA
    // CHECK-SAME    inputs([[INPUT_DDR_BUF0]] : memref<1x4x360x216xf16, {order = #NHWC, strides = [3110400, 1, 8640, 4]}, @DDR>)
    // CHECK-SAME    outputs([[OUTPUT_BUF]] : memref<1x4x360x216xf16, #NHWC, [@CMX_NN, 0]>

    // CHECK:      return [[OUTPUT_BUF]] : memref<1x4x360x216xf16, #NHWC, [@CMX_NN, 0]>
}
