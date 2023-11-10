//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=VPUX37XX" --convert-VPUIP-to-VPUMI37XX %s | FileCheck %s

#NHWC = affine_map<(d0, d1, d2, d3) -> (d0, d2, d3, d1)>
module @mainModule {
  IE.CNNNetwork entryPoint : @race_condition_dma_f16_f16 inputsInfo : {
    DataInfo "input_0" : tensor<1x16x16x16xf16>
  } outputsInfo : {
    DataInfo "output_0" : tensor<1x16x16x16xf16>
    DataInfo "output_1" : tensor<1x16x16x16xf16>
  }
  func.func private @race_condition_dma_f16_f16(%arg0: memref<1x16x16x16xf16, #NHWC, @DDR>, %arg1: memref<1x16x16x16xf16, #NHWC, @DDR>, %arg2: memref<1x16x16x16xf16, #NHWC, @DDR>) -> (memref<1x16x16x16xf16, #NHWC, @DDR>, memref<1x16x16x16xf16, #NHWC, @DDR>) {
    %0 = VPURT.DeclareBuffer <CMX_NN> [0] <0> -> memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 0]>
    %1 = VPURT.DeclareBuffer <CMX_NN> [1] <0> -> memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 1]>

    %2 = VPURT.ConfigureBarrier<0> -> !VPURT.Barrier
    // CHECK: %[[BAR0:.*]] = VPUMI37XX.ConfigureBarrier {consumer_count = 2 : ui8, producer_count = 2 : ui8}<0, -1> -> !VPURegMapped.Index<0:0:0>

    VPURT.Task updates(%2 : !VPURT.Barrier) attributes {isTrailingSWLayer = false} {
      %4 = VPUIP.NNDMA {port = 0 : i64} inputs(%arg0 : memref<1x16x16x16xf16, #NHWC, @DDR>) outputs(%0 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 0]>
    }
    // CHECK: %[[NNDMA0_0:.*]] = VPUMI37XX.NNDMA {port = 0 : i64} inputs(%arg0 : memref<1x16x16x16xf16, #NHWC, @DDR>) outputs(%0 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 0]>) updates(%[[BAR0]] : !VPURegMapped.Index<0:0:0>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:0>

    VPURT.Task updates(%2 : !VPURT.Barrier) attributes {isTrailingSWLayer = false} {
      %4 = VPUIP.NNDMA {port = 1 : i64} inputs(%arg0 : memref<1x16x16x16xf16, #NHWC, @DDR>) outputs(%1 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 1]>) -> memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 1]>
    }
    // CHECK: %[[NNDMA1_0:.*]] = VPUMI37XX.NNDMA {port = 1 : i64} inputs(%arg0 : memref<1x16x16x16xf16, #NHWC, @DDR>) outputs(%1 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 1]>) updates(%[[BAR0]] : !VPURegMapped.Index<0:0:0>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:0>

    %3 = VPURT.ConfigureBarrier<1> -> !VPURT.Barrier
    // CHECK: %[[BAR1:.*]] = VPUMI37XX.ConfigureBarrier {consumer_count = 2 : ui8, producer_count = 2 : ui8}<1, -1> -> !VPURegMapped.Index<0:0:1>

    VPURT.Task waits(%2 : !VPURT.Barrier) updates(%3 : !VPURT.Barrier) attributes {isTrailingSWLayer = false} {
      %4 = VPUIP.NNDMA {port = 0 : i64} inputs(%arg0 : memref<1x16x16x16xf16, #NHWC, @DDR>) outputs(%0 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 0]>) -> memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 0]>
    }
    // CHECK: %[[NNDMA0_1:.*]] = VPUMI37XX.NNDMA {port = 0 : i64} inputs(%arg0 : memref<1x16x16x16xf16, #NHWC, @DDR>) outputs(%0 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 0]>) previousDMA(%[[NNDMA0_0]] : !VPURegMapped.Index<0:0:0>) waits(%[[BAR0]] : !VPURegMapped.Index<0:0:0>) updates(%[[BAR1]] : !VPURegMapped.Index<0:0:1>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:1>

    VPURT.Task waits(%2 : !VPURT.Barrier) updates(%3 : !VPURT.Barrier) attributes {isTrailingSWLayer = false} {
      %4 = VPUIP.NNDMA {port = 1 : i64} inputs(%arg0 : memref<1x16x16x16xf16, #NHWC, @DDR>) outputs(%1 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 1]>) -> memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 1]>
    }
    // CHECK: %[[NNDMA1_1:.*]] = VPUMI37XX.NNDMA {port = 1 : i64} inputs(%arg0 : memref<1x16x16x16xf16, #NHWC, @DDR>) outputs(%1 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 1]>) previousDMA(%[[NNDMA1_0]] : !VPURegMapped.Index<0:0:0>) waits(%[[BAR0]] : !VPURegMapped.Index<0:0:0>) updates(%[[BAR1]] : !VPURegMapped.Index<0:0:1>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:1>

    VPURT.Task waits(%3 : !VPURT.Barrier) attributes {isTrailingSWLayer = false} {
      %4 = VPUIP.NNDMA {port = 0 : i64} inputs(%0 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 0]>) outputs(%arg1 : memref<1x16x16x16xf16, #NHWC, @DDR>) -> memref<1x16x16x16xf16, #NHWC, @DDR>
    }
    // CHECK: %[[NNDMA0_2:.*]] = VPUMI37XX.NNDMA {port = 0 : i64} inputs(%0 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 0]>) outputs(%arg1 : memref<1x16x16x16xf16, #NHWC, @DDR>) previousDMA(%[[NNDMA0_1]] : !VPURegMapped.Index<0:0:1>) waits(%[[BAR1]] : !VPURegMapped.Index<0:0:1>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:2>

    VPURT.Task waits(%3 : !VPURT.Barrier) attributes {isTrailingSWLayer = false} {
      %4 = VPUIP.NNDMA {port = 1 : i64} inputs(%1 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 1]>) outputs(%arg2 : memref<1x16x16x16xf16, #NHWC, @DDR>) -> memref<1x16x16x16xf16, #NHWC, @DDR>
    }
    // CHECK: %[[NNDMA1_2:.*]] = VPUMI37XX.NNDMA {port = 1 : i64} inputs(%1 : memref<1x16x16x16xf16, #NHWC, [@CMX_NN, 1]>) outputs(%arg2 : memref<1x16x16x16xf16, #NHWC, @DDR>) previousDMA(%[[NNDMA1_1]] : !VPURegMapped.Index<0:0:1>) waits(%[[BAR1]] : !VPURegMapped.Index<0:0:1>) start_after(0) clean_after(0) -> !VPURegMapped.Index<0:0:2>

    // CHECK: %[[MI:.*]] = VPUMI37XX.MappedInference dmas(%[[NNDMA0_0]], %[[NNDMA1_0]] : !VPURegMapped.Index<0:0:0>, !VPURegMapped.Index<0:0:0>) barriers(%[[BAR0]] : !VPURegMapped.Index<0:0:0>) dmaCount([3, 3]) invariantCount(0) variantCount(0) actKernelRangesCount(0) actKernelInvocationsCount(0) barrierCount(2) -> !VPURegMapped.Index<0:0:0>

    return %arg1, %arg2 : memref<1x16x16x16xf16, #NHWC, @DDR>, memref<1x16x16x16xf16, #NHWC, @DDR>
  }
}
