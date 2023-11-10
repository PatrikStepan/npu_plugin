//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=VPUX37XX" --dma-barrier-optimization %s | FileCheck %s

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!qElemType0 = !quant.uniform<u8:f16, 0.0173492431640625:114>
!Input_DDR = memref<1x3x224x224x!qElemType0, #NHWC, @DDR>
!Output_DDR = memref<1x16x224x224x!qElemType0, #NHWC, @DDR>

//CHECK-LABEL: @DMABarrierOptimization
func.func @DMABarrierOptimization() -> !Output_DDR {

    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar1 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar2 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar3 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar4 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    %input = VPURT.DeclareBuffer <NetworkInput> [0] <0> -> !Input_DDR
    %input0 = VPURT.DeclareBuffer <NetworkInput> [0] <0> -> memref<1x1x224x224x!qElemType0, {order = #NHWC, strides = [150528, 1, 672, 3]}>

    %output = VPURT.DeclareBuffer <DDR> <0> -> !Output_DDR
    %output0 = VPURT.DeclareBuffer <DDR> <0> -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    %output1 = VPURT.DeclareBuffer <DDR> <3> -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    %output2 = VPURT.DeclareBuffer <DDR> <6> -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    %output3 = VPURT.DeclareBuffer <DDR> <9> -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    %output4 = VPURT.DeclareBuffer <DDR> <12> -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    %output5 = VPURT.DeclareBuffer <DDR> <15> -> memref<1x1x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>

    VPURT.Task updates(%bar0 : !VPURT.Barrier) attributes {cycleBegin = 1 : i64, cycleEnd = 2 : i64} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%input : !Input_DDR) outputs(%output0: memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    VPURT.Task waits(%bar0 : !VPURT.Barrier) updates(%bar1 : !VPURT.Barrier) attributes {cycleBegin = 2 : i64, cycleEnd = 3 : i64} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%input : !Input_DDR) outputs(%output1: memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    VPURT.Task waits(%bar1 : !VPURT.Barrier) updates(%bar2 : !VPURT.Barrier) attributes {cycleBegin = 3 : i64, cycleEnd = 4 : i64} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%input : !Input_DDR) outputs(%output2: memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    VPURT.Task waits(%bar2 : !VPURT.Barrier) updates(%bar3 : !VPURT.Barrier) attributes {cycleBegin = 4 : i64, cycleEnd = 5 : i64} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%input : !Input_DDR) outputs(%output3: memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    VPURT.Task waits(%bar3 : !VPURT.Barrier) updates(%bar4 : !VPURT.Barrier) attributes {cycleBegin = 5 : i64, cycleEnd = 6 : i64} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%input : !Input_DDR) outputs(%output4: memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    VPURT.Task waits(%bar4 : !VPURT.Barrier) attributes {cycleBegin = 6 : i64, cycleEnd = 7 : i64} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%input0 : memref<1x1x224x224x!qElemType0, {order = #NHWC, strides = [150528, 1, 672, 3]}>) outputs(%output5: memref<1x1x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x1x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    return %output : !Output_DDR


    // CHECK-NOT:   VPURT.DeclareVirtualBarrier

    // CHECK:    VPURT.Task attributes {cycleBegin = 1 : i64, cycleEnd = 2 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task attributes {cycleBegin = 2 : i64, cycleEnd = 3 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task attributes {cycleBegin = 3 : i64, cycleEnd = 4 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task attributes {cycleBegin = 4 : i64, cycleEnd = 5 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task attributes {cycleBegin = 5 : i64, cycleEnd = 6 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task attributes {cycleBegin = 6 : i64, cycleEnd = 7 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
}

// -----


#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

!qElemType0 = !quant.uniform<u8:f16, 0.0173492431640625:114>
!Input_DDR = memref<1x3x224x224x!qElemType0, #NHWC, @DDR>
!Output_DDR = memref<1x16x224x224x!qElemType0, #NHWC, @DDR>

//CHECK-LABEL: @NoDMABarrierOptimization
func.func @NoDMABarrierOptimization() -> !Output_DDR {

    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar1 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar2 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar3 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar4 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    %input = VPURT.DeclareBuffer <NetworkInput> [0] <0> -> !Input_DDR
    %input0 = VPURT.DeclareBuffer <NetworkInput> [0] <0> -> memref<1x1x224x224x!qElemType0, {order = #NHWC, strides = [150528, 1, 672, 3]}>

    %output = VPURT.DeclareBuffer <DDR> <0> -> !Output_DDR
    %output0 = VPURT.DeclareBuffer <DDR> <0> -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    %output1 = VPURT.DeclareBuffer <DDR> <3> -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    %output2 = VPURT.DeclareBuffer <DDR> <6> -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    %output3 = VPURT.DeclareBuffer <DDR> <9> -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    %output4 = VPURT.DeclareBuffer <DDR> <12> -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    %output5 = VPURT.DeclareBuffer <DDR> <15> -> memref<1x1x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>

    VPURT.Task updates(%bar0 : !VPURT.Barrier) attributes {cycleBegin = 1 : i64, cycleEnd = 2 : i64} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%input : !Input_DDR) outputs(%output0: memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    VPURT.Task waits(%bar0 : !VPURT.Barrier) updates(%bar1 : !VPURT.Barrier) attributes {cycleBegin = 2 : i64, cycleEnd = 3 : i64} {
      %0 = VPUIP.NNDMA {port = 1 : i64} inputs(%input : !Input_DDR) outputs(%output1: memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    VPURT.Task waits(%bar1 : !VPURT.Barrier) updates(%bar2 : !VPURT.Barrier) attributes {cycleBegin = 3 : i64, cycleEnd = 4 : i64} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%input : !Input_DDR) outputs(%output2: memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    VPURT.Task waits(%bar2 : !VPURT.Barrier) updates(%bar3 : !VPURT.Barrier) attributes {cycleBegin = 4 : i64, cycleEnd = 5 : i64} {
      %0 = VPUIP.NNDMA {port = 1 : i64} inputs(%input : !Input_DDR) outputs(%output3: memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    VPURT.Task waits(%bar3 : !VPURT.Barrier) updates(%bar4 : !VPURT.Barrier) attributes {cycleBegin = 5 : i64, cycleEnd = 6 : i64} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%input : !Input_DDR) outputs(%output4: memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x3x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    VPURT.Task waits(%bar4 : !VPURT.Barrier) attributes {cycleBegin = 6 : i64, cycleEnd = 7 : i64} {
      %0 = VPUIP.NNDMA {port = 1 : i64} inputs(%input0 : memref<1x1x224x224x!qElemType0, {order = #NHWC, strides = [150528, 1, 672, 3]}>) outputs(%output5: memref<1x1x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>) -> memref<1x1x224x224x!qElemType0, {order = #NHWC, strides = [802816, 1, 3584, 16]}, @DDR>
    }
    return %output : !Output_DDR


    // CHECK:     [[Bar0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK:     [[Bar1:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK:     [[Bar2:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK:     [[Bar3:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK:     [[Bar4:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // CHECK:    VPURT.Task updates([[Bar0]] : !VPURT.Barrier) attributes {cycleBegin = 1 : i64, cycleEnd = 2 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task waits([[Bar0]] : !VPURT.Barrier) updates([[Bar1]] : !VPURT.Barrier) attributes {cycleBegin = 2 : i64, cycleEnd = 3 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task waits([[Bar1]] : !VPURT.Barrier) updates([[Bar2]] : !VPURT.Barrier) attributes {cycleBegin = 3 : i64, cycleEnd = 4 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task waits([[Bar2]] : !VPURT.Barrier) updates([[Bar3]] : !VPURT.Barrier) attributes {cycleBegin = 4 : i64, cycleEnd = 5 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task waits([[Bar3]] : !VPURT.Barrier) updates([[Bar4]] : !VPURT.Barrier) attributes {cycleBegin = 5 : i64, cycleEnd = 6 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task waits([[Bar4]] : !VPURT.Barrier) attributes {cycleBegin = 6 : i64, cycleEnd = 7 : i64} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

module @VPU.SW {
  func.func private @builtin_MVN(memref<*xf16, @CMX_NN>, memref<*xf16, @CMX_NN>, i1, i1, f64) attributes {VPU.kernel_code = "singleShaveMVN.cpp", VPU.kernel_entry = "singleShaveMVN"}
  func.func private @runtime() attributes {VPU.kernel_code = "nnActEntry"}
}

// CHECK-LABEL: @RemoveRedundantDependenciesForProducer
func.func @RemoveRedundantDependenciesForProducer() -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]> {
    // barriers
    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar1 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // dummy buffers
    %buf0 = VPURT.DeclareBuffer <CMX_NN> [0] <0> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    %buf1 = VPURT.DeclareBuffer <CMX_NN> [0] <32> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>

    //      DMA(port=0)     
    //        /   \
    //       |    bar0
    //       |      |
    //       |  DMA(port=1)   
    //        \    /
    //         bar1
    //          |
    //       SwKernel

    VPURT.Task updates(%bar0, %bar1: !VPURT.Barrier, !VPURT.Barrier) {
         VPUIP.NNDMA {port = 0 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    }

    VPURT.Task waits(%bar0 : !VPURT.Barrier) updates(%bar1: !VPURT.Barrier) {
         VPUIP.NNDMA {port = 1 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    }

    VPURT.Task waits(%bar1 : !VPURT.Barrier) {
         VPUIP.SW.Kernel {result_segment_sizes = dense<[1, 0]> : vector<2xi32>} @VPU.SW::@builtin_MVN
            inputs(%buf0 as %arg1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1 as %arg2: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) on tile 0
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>{
                VPUIP.SW.Kernel.run {attrs = [false, true, 6.0892105102539063E-4]}(%arg1, %arg1) : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>, memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
         }
    }

    return %buf1 : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>

    //    DMA(port=0) 
    //       |
    //      bar0
    //       |
    //    DMA(port=1) 
    //       |
    //      bar1
    //       |
    //    SwKernelOp  

    // CHECK:  [[BAR0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK:  [[BAR1:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK-NOT: VPURT.DeclareVirtual
    // CHECK:  [[BUF0:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <0> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  [[BUF1:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <32> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  VPURT.Task updates([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 0 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  VPURT.Task waits([[BAR0]] : !VPURT.Barrier) updates([[BAR1]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 1 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  VPURT.Task waits([[BAR1]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.SW.Kernel {result_segment_sizes = dense<[1, 0]> : vector<2xi32>} @VPU.SW::@builtin_MVN inputs([[BUF0]] as %arg0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] as %arg1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) on tile 0 -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  return [[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

module @VPU.SW {
  func.func private @builtin_MVN(memref<*xf16, @CMX_NN>, memref<*xf16, @CMX_NN>, i1, i1, f64) attributes {VPU.kernel_code = "singleShaveMVN.cpp", VPU.kernel_entry = "singleShaveMVN"}
  func.func private @runtime() attributes {VPU.kernel_code = "nnActEntry"}
}

// CHECK-LABEL: @RemoveRedundantDependenciesForConsumer
func.func @RemoveRedundantDependenciesForConsumer() -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]> {
    // barriers
    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar1 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // dummy buffers
    %buf0 = VPURT.DeclareBuffer <CMX_NN> [0] <0> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    %buf1 = VPURT.DeclareBuffer <CMX_NN> [0] <32> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>

    //    DMA(port=0) 
    //          |
    //         bar0
    //        /    \
    // DMA(port=1)  | 
    //       |      |
    //      bar1    | 
    //        \    / 
    //       SwkernelOp  

    VPURT.Task updates(%bar0 : !VPURT.Barrier) {
         VPUIP.NNDMA {port = 0 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    }

    VPURT.Task waits(%bar0 : !VPURT.Barrier) updates(%bar1: !VPURT.Barrier) {
         VPUIP.NNDMA {port = 1 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    }

    VPURT.Task waits(%bar0, %bar1: !VPURT.Barrier, !VPURT.Barrier) {
         VPUIP.SW.Kernel {result_segment_sizes = dense<[1, 0]> : vector<2xi32>} @VPU.SW::@builtin_MVN
            inputs(%buf0 as %arg1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1 as %arg2: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) on tile 0
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>{
                VPUIP.SW.Kernel.run {attrs = [false, true, 6.0892105102539063E-4]}(%arg1, %arg1) : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>, memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
         }
    }

    return %buf1 : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>

    //    DMA(port=0) 
    //       |
    //      bar0
    //       |
    //    DMA(port=1) 
    //       |
    //      bar1
    //       |
    //    Swkernel

    // CHECK:  [[BAR0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK:  [[BAR1:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK-NOT: VPURT.DeclareVirtual
    // CHECK:  [[BUF0:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <0> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  [[BUF1:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <32> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  VPURT.Task updates([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 0 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  VPURT.Task waits([[BAR0]] : !VPURT.Barrier) updates([[BAR1]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 1 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  VPURT.Task waits([[BAR1]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.SW.Kernel {result_segment_sizes = dense<[1, 0]> : vector<2xi32>} @VPU.SW::@builtin_MVN inputs([[BUF0]] as %arg0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] as %arg1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) on tile 0 -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  return [[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @RemoveExplicitDependenciesWithSameTaskTypeProducer
func.func @RemoveExplicitDependenciesWithSameTaskTypeProducer() -> memref<1x16x1x1xf16, #NHWC, @DDR> {
    // barriers
    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // dummy buffers
    %buf0 = VPURT.DeclareBuffer <DDR> <0> -> memref<1x16x1x1xf16, #NHWC, @DDR>
    %buf1 = VPURT.DeclareBuffer <DDR> <32> -> memref<1x16x1x1xf16, #NHWC, @DDR>

    //        DMA(port=0) 
    //             |
    //            bar0
    //           /    \
    //   DMA(port=0)  DMA(port=1)

    VPURT.Task updates(%bar0: !VPURT.Barrier) {
         VPUIP.NNDMA {port = 0 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, @DDR>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, @DDR>)
            -> memref<1x16x1x1xf16, #NHWC, @DDR>
    }

    VPURT.Task waits(%bar0 : !VPURT.Barrier) {
         VPUIP.NNDMA {port = 0 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, @DDR>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, @DDR>)
            -> memref<1x16x1x1xf16, #NHWC, @DDR>
    }

    VPURT.Task waits(%bar0 : !VPURT.Barrier) {
         VPUIP.NNDMA {port = 1 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, @DDR>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, @DDR>)
            -> memref<1x16x1x1xf16, #NHWC, @DDR>
    }



    return %buf1 : memref<1x16x1x1xf16, #NHWC, @DDR>

    //        DMA(port=0) 
    //             | 
    //            bar0
    //             | 
    //        DMA(port=1)   DMA(port=0)

    // CHECK:  [[BAR0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // CHECK:  [[BUF0:%.*]] = VPURT.DeclareBuffer <DDR> <0> -> memref<1x16x1x1xf16, #NHWC, @DDR>
    // CHECK:  [[BUF1:%.*]] = VPURT.DeclareBuffer <DDR> <32> -> memref<1x16x1x1xf16, #NHWC, @DDR>
    // CHECK:  VPURT.Task updates([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 0 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, @DDR>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, @DDR>) -> memref<1x16x1x1xf16, #NHWC, @DDR>
    // CHECK:  }
    // CHECK:  VPURT.Task {
    // CHECK:                 VPUIP.NNDMA {port = 0 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, @DDR>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, @DDR>) -> memref<1x16x1x1xf16, #NHWC, @DDR>
    // CHECK:  }
    // CHECK:  VPURT.Task waits([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 1 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, @DDR>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, @DDR>) -> memref<1x16x1x1xf16, #NHWC, @DDR>
    // CHECK:  }
    // CHECK:  return [[BUF1]] : memref<1x16x1x1xf16, #NHWC, @DDR>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

// CHECK-LABEL: @RemoveExplicitDependenciesWithSameTaskTypeConsumer
func.func @RemoveExplicitDependenciesWithSameTaskTypeConsumer() -> memref<1x16x1x1xf16, #NHWC, @DDR> {
    // barriers
    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // dummy buffers
    %buf0 = VPURT.DeclareBuffer <DDR> <0> -> memref<1x16x1x1xf16, #NHWC, @DDR>
    %buf1 = VPURT.DeclareBuffer <DDR> <32> -> memref<1x16x1x1xf16, #NHWC, @DDR>

    //    DMA(port=0)  DMA(port=1)
    //           \    /
    //            bar0
    //             |
    //       DMA(port=1)

    VPURT.Task updates(%bar0: !VPURT.Barrier) {
         VPUIP.NNDMA {port = 0 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, @DDR>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, @DDR>)
            -> memref<1x16x1x1xf16, #NHWC, @DDR>
    }

    VPURT.Task updates(%bar0: !VPURT.Barrier) {
         VPUIP.NNDMA {port = 1 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, @DDR>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, @DDR>)
            -> memref<1x16x1x1xf16, #NHWC, @DDR>
    }

    VPURT.Task waits(%bar0 : !VPURT.Barrier) {
         VPUIP.NNDMA {port = 1 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, @DDR>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, @DDR>)
            -> memref<1x16x1x1xf16, #NHWC, @DDR>
    }

    return %buf1 : memref<1x16x1x1xf16, #NHWC, @DDR>

    //        DMA(port=0)  DMA(port=1)
    //             | 
    //            bar0
    //             | 
    //        DMA(port=1)

    // CHECK:  [[BAR0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // CHECK:  [[BUF0:%.*]] = VPURT.DeclareBuffer <DDR> <0> -> memref<1x16x1x1xf16, #NHWC, @DDR>
    // CHECK:  [[BUF1:%.*]] = VPURT.DeclareBuffer <DDR> <32> -> memref<1x16x1x1xf16, #NHWC, @DDR>
    // CHECK:  VPURT.Task updates([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 0 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, @DDR>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, @DDR>) -> memref<1x16x1x1xf16, #NHWC, @DDR>
    // CHECK:  }
    // CHECK:  VPURT.Task {
    // CHECK:                 VPUIP.NNDMA {port = 1 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, @DDR>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, @DDR>) -> memref<1x16x1x1xf16, #NHWC, @DDR>
    // CHECK:  }
    // CHECK:  VPURT.Task waits([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 1 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, @DDR>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, @DDR>) -> memref<1x16x1x1xf16, #NHWC, @DDR>
    // CHECK:  }
    // CHECK:  return [[BUF1]] : memref<1x16x1x1xf16, #NHWC, @DDR>
}

// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

module @VPU.SW {
  func.func private @builtin_MVN(memref<*xf16, @CMX_NN>, memref<*xf16, @CMX_NN>, i1, i1, f64) attributes {VPU.kernel_code = "singleShaveMVN.cpp", VPU.kernel_entry = "singleShaveMVN"}
  func.func private @runtime() attributes {VPU.kernel_code = "nnActEntry"}
}

// CHECK-LABEL: @MergeBarrierForCommonConsumers
func.func @MergeBarrierForCommonConsumers() -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]> {
    // barriers
    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar1 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // dummy buffers
    %buf0 = VPURT.DeclareBuffer <CMX_NN> [0] <0> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    %buf1 = VPURT.DeclareBuffer <CMX_NN> [0] <32> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>

    //        DMA(port=0)  DMA(port=1)
    //          |             |
    //         bar0          bar1
    //             \       /  
    //              Swkernel       

    VPURT.Task updates(%bar0: !VPURT.Barrier) {
         VPUIP.NNDMA {port = 0 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    }

    VPURT.Task updates(%bar1: !VPURT.Barrier) {
         VPUIP.NNDMA {port = 1 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    }

    VPURT.Task waits(%bar0, %bar1: !VPURT.Barrier, !VPURT.Barrier) {
         VPUIP.SW.Kernel {result_segment_sizes = dense<[1, 0]> : vector<2xi32>} @VPU.SW::@builtin_MVN
            inputs(%buf0 as %arg1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1 as %arg2: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) on tile 0
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>{
                VPUIP.SW.Kernel.run {attrs = [false, true, 6.0892105102539063E-4]}(%arg1, %arg1) : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>, memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
         }
    }

    return %buf1 : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>

    //        DMA(port=0)  DMA(port=1)
    //              \      /  
    //                bar0
    //                 |
    //             Swkernel

    // CHECK:  [[BAR0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK-NOT: VPURT.DeclareVirtual
    // CHECK:  [[BUF0:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <0> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  [[BUF1:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <32> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  VPURT.Task updates([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 0 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  VPURT.Task updates([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 1 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  VPURT.Task waits([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.SW.Kernel {result_segment_sizes = dense<[1, 0]> : vector<2xi32>} @VPU.SW::@builtin_MVN inputs([[BUF0]] as %arg0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] as %arg1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) on tile 0 -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  return [[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>

}


// -----

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>

module @VPU.SW {
  func.func private @builtin_MVN(memref<*xf16, @CMX_NN>, memref<*xf16, @CMX_NN>, i1, i1, f64) attributes {VPU.kernel_code = "singleShaveMVN.cpp", VPU.kernel_entry = "singleShaveMVN"}
  func.func private @runtime() attributes {VPU.kernel_code = "nnActEntry"}
}

// CHECK-LABEL: @MergeBarrierForCommonConsumersAndImplicitDependenceTaskConsumers
func.func @MergeBarrierForCommonConsumersAndImplicitDependenceTaskConsumers() -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]> {
    // barriers
    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar1 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // dummy buffers
    %buf0 = VPURT.DeclareBuffer <CMX_NN> [0] <0> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    %buf1 = VPURT.DeclareBuffer <CMX_NN> [0] <32> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>

    //        DMA(port=0)  DMA(port=1)
    //          |             |
    //         bar0          bar1
    //       /      \       /  
    //  DMA(port1)   Swkernel       

    VPURT.Task updates(%bar0: !VPURT.Barrier) {
         VPUIP.NNDMA {port = 0 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    }

    VPURT.Task updates(%bar1: !VPURT.Barrier) {
         VPUIP.NNDMA {port = 1 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    }

    VPURT.Task waits(%bar0 : !VPURT.Barrier) {
         VPUIP.NNDMA {port = 1 : i64}
            inputs(%buf0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    }

    VPURT.Task waits(%bar0, %bar1: !VPURT.Barrier, !VPURT.Barrier) {
         VPUIP.SW.Kernel {result_segment_sizes = dense<[1, 0]> : vector<2xi32>} @VPU.SW::@builtin_MVN
            inputs(%buf0 as %arg1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>)
            outputs(%buf1 as %arg2: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) on tile 0
            -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>{
                VPUIP.SW.Kernel.run {attrs = [false, true, 6.0892105102539063E-4]}(%arg1, %arg1) : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>, memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
         }
    }

    return %buf1 : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>

    //        DMA(port=0)  DMA(port=1)
    //              \      /  
    //                bar0
    //              /      \ 
    //        DMA(port=1)  Swkernel

    // CHECK:  [[BAR0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK-NOT: VPURT.DeclareVirtual
    // CHECK:  [[BUF0:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <0> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  [[BUF1:%.*]] = VPURT.DeclareBuffer <CMX_NN> [0] <32> -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  VPURT.Task updates([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 0 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  VPURT.Task updates([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 1 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  VPURT.Task waits([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.NNDMA {port = 1 : i64} inputs([[BUF0]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  VPURT.Task waits([[BAR0]] : !VPURT.Barrier) {
    // CHECK:                 VPUIP.SW.Kernel {result_segment_sizes = dense<[1, 0]> : vector<2xi32>} @VPU.SW::@builtin_MVN inputs([[BUF0]] as %arg0: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) outputs([[BUF1]] as %arg1: memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>) on tile 0 -> memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>
    // CHECK:  }
    // CHECK:  return [[BUF1]] : memref<1x16x1x1xf16, #NHWC, [@CMX_NN, 0]>

}

// -----


#NCHW = affine_map<(d0, d1, d2, d3) -> (d0, d1, d2, d3)>
#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>
#NDHWC = affine_map<(d0, d1, d2, d3, d4) -> (d0, d2, d3, d4, d1)>

!Input_CMX = memref<120x2112x1x1xf16, [@CMX_NN, 0]>
!Output_CMX = memref<1x16x66x120xf16, #NHWC , [@CMX_NN, 0]>

//CHECK-LABEL: @NotMergeBarriersForTaskOpsExecutionInParallel
func.func @NotMergeBarriersForTaskOpsExecutionInParallel() -> !Output_CMX {

    %bar0 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar1 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    %bar2 = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    %input = VPURT.DeclareBuffer <CMX_NN> [0] <1031680> -> !Input_CMX

    %1 = VPURT.DeclareBuffer <DDR> <4591744> -> memref<120x2112x1x1xf16, {order = #NCHW, strides = [4224, 1, 1, 1]}, @DDR>
    %2 = VPURT.DeclareBuffer <DDR> <4587520> -> memref<1x1x120x64x33xf16, {order = #NDHWC, strides = [506880, 1, 4224, 66, 2]}, @DDR>
    %3 = VPURT.DeclareBuffer <DDR> <4587522> -> memref<1x1x120x64x33xf16, {order = #NDHWC, strides = [506880, 1, 4224, 66, 2]}, @DDR>
    %4 = VPURT.DeclareBuffer <DDR> <5601280> -> memref<1x1x120x64x33xf16, #NDHWC, @DDR>
    %5 = VPURT.DeclareBuffer <DDR> <6108160> -> memref<1x1x120x64x33xf16, #NDHWC, @DDR>
    %6 = VPURT.DeclareBuffer <DDR> <5601280> -> memref<1x16x66x120xf16, #NHWC, @DDR>
    %7 = VPURT.DeclareBuffer <DDR> <5854720> -> memref<1x16x66x120xf16, #NHWC, @DDR>

    %output = VPURT.DeclareBuffer <CMX_NN> [0] <778240> -> !Output_CMX
    %8 = VPURT.DeclareBuffer <CMX_NN> [0] <778240> -> !Output_CMX
    %9 = VPURT.DeclareBuffer <CMX_NN> [1] <778240> -> memref<1x16x66x120xf16, #NHWC , [@CMX_NN, 1]>

    VPURT.Task updates(%bar0 : !VPURT.Barrier) attributes {cycleBegin = 1 : i64, cycleEnd = 2 : i64, isTrailingSWLayer = false} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%input : !Input_CMX) outputs(%1 : memref<120x2112x1x1xf16, {order = #NCHW, strides = [4224, 1, 1, 1]}, @DDR>) -> memref<120x2112x1x1xf16, {order = #NCHW, strides = [4224, 1, 1, 1]}, @DDR>
    }
    VPURT.Task waits(%bar0 : !VPURT.Barrier) updates(%bar1 : !VPURT.Barrier) attributes {cycleBegin = 2 : i64, cycleEnd = 3 : i64, isTrailingSWLayer = false} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%2 : memref<1x1x120x64x33xf16, {order = #NDHWC, strides = [506880, 1, 4224, 66, 2]}, @DDR>) outputs(%4 : memref<1x1x120x64x33xf16, #NDHWC, @DDR>) -> memref<1x1x120x64x33xf16, #NDHWC, @DDR>
    }
    VPURT.Task waits(%bar0 : !VPURT.Barrier) updates(%bar2 : !VPURT.Barrier) attributes {cycleBegin = 2 : i64, cycleEnd = 3 : i64, isTrailingSWLayer = false} {
      %0 = VPUIP.NNDMA {port = 1 : i64} inputs(%3 : memref<1x1x120x64x33xf16, {order = #NDHWC, strides = [506880, 1, 4224, 66, 2]}, @DDR>) outputs(%5 : memref<1x1x120x64x33xf16, #NDHWC, @DDR>) -> memref<1x1x120x64x33xf16, #NDHWC, @DDR>
    }
    VPURT.Task waits(%bar1 : !VPURT.Barrier) attributes {cycleBegin = 3 : i64, cycleEnd = 4 : i64, isTrailingSWLayer = false} {
      %0 = VPUIP.NNDMA {port = 0 : i64} inputs(%6 : memref<1x16x66x120xf16, #NHWC, @DDR>) outputs(%8 : memref<1x16x66x120xf16, #NHWC, [@CMX_NN, 0]>) -> !Output_CMX
    }
    VPURT.Task waits(%bar1 : !VPURT.Barrier) attributes {cycleBegin = 3 : i64, cycleEnd = 4 : i64, isTrailingSWLayer = false} {
      %0 = VPUIP.NNDMA {port = 1 : i64} inputs(%7 : memref<1x16x66x120xf16, #NHWC, @DDR>) outputs(%9 : memref<1x16x66x120xf16, #NHWC, [@CMX_NN, 1]>) -> memref<1x16x66x120xf16, #NHWC, [@CMX_NN, 1]>
    }

    return %output : !Output_CMX


    // CHECK:     [[Bar0:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier
    // CHECK:     [[Bar1:%.*]] = VPURT.DeclareVirtualBarrier -> !VPURT.Barrier

    // CHECK:    VPURT.Task updates([[Bar0]] : !VPURT.Barrier) attributes {cycleBegin = 1 : i64, cycleEnd = 2 : i64, isTrailingSWLayer = false} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task updates([[Bar1]] : !VPURT.Barrier) attributes {cycleBegin = 2 : i64, cycleEnd = 3 : i64, isTrailingSWLayer = false} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task waits([[Bar0]] : !VPURT.Barrier) attributes {cycleBegin = 2 : i64, cycleEnd = 3 : i64, isTrailingSWLayer = false} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task attributes {cycleBegin = 3 : i64, cycleEnd = 4 : i64, isTrailingSWLayer = false} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }
    // CHECK:    VPURT.Task waits([[Bar1]] : !VPURT.Barrier) attributes {cycleBegin = 3 : i64, cycleEnd = 4 : i64, isTrailingSWLayer = false} {
    // CHECK:        VPUIP.NNDMA
    // CHECK:    }

}
