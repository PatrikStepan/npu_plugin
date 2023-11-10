//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch%" --convert-grn-to-normalizel2  %s | FileCheck %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

// -----

// CHECK-LABEL: @ConvertGRNToNormalizeL2
func.func @ConvertGRNToNormalizeL2(%arg0: tensor<1x8x24x64xf16>) -> tensor<1x8x24x64xf16> {
    %0 = IE.GRN(%arg0) {bias = 0.33000001311302185 : f64} : tensor<1x8x24x64xf16> -> tensor<1x8x24x64xf16>
    return %0 : tensor<1x8x24x64xf16>

    // CHECK-DAG:   [[AXES:%.*]] = const.Declare tensor<1xsi64> = dense<1> : tensor<1xsi64>
    // CHECK:       [[NORMALIZEL2:%.*]] = IE.NormalizeL2(%arg0, [[AXES]])
    // CHECK-SAME:       {eps = 0.33000001311302185 : f64, eps_mode = #IE.eps_mode<ADD>}
    // CHECK-SAME:        : tensor<1x8x24x64xf16>, tensor<1xsi64> -> tensor<1x8x24x64xf16>

    // CHECK:       return [[NORMALIZEL2]]
}
