//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --init-compiler="vpu-arch=VPUX30XX compilation-mode=ReferenceSW" %s | FileCheck %s --strict-whitespace

// CHECK: module @test attributes {VPU.arch = #VPU.arch_kind<VPUX30XX>, VPU.compilationMode = #VPU.compilation_mode<ReferenceSW>}
module @test {

// CHECK-DAG:    {{  }}IE.ExecutorResource 1 of @DMA_NN
// CHECK-DAG:    {{  }}IE.ExecutorResource 16 of @SHAVE_UPA
// CHECK-DAG:    {{  }}IE.ExecutorResource 4 of @NCE at 7.000000e+02 MHz {
// CHECK-DAG:    {{    }}IE.ExecutorResource 5 of @DPU
// CHECK-DAG:    {{    }}IE.MemoryResource 825753 bytes of @CMX_NN_FragmentationAware
// CHECK-DAG:    {{    }}IE.MemoryResource 917504 bytes of @CMX_NN {VPU.bandwidth = 32 : i64, VPU.derateFactor = 1.000000e+00 : f64}
// CHECK-DAG:    {{  }}IE.MemoryResource 2306867200 bytes of @DDR {VPU.bandwidth = 8 : i64, VPU.derateFactor = 6.000000e-01 : f64}

}
