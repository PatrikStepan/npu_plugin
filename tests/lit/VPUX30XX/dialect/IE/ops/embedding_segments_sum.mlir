//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN:vpux-opt --split-input-file --init-compiler="vpu-arch=VPUX30XX" --canonicalize %s | FileCheck %s

// CHECK-LABEL: @ConvertConstToAttrVPUX30XX
func.func @ConvertConstToAttrVPUX30XX(%arg0: tensor<5x6x4xsi32>) -> tensor<7x6x4xsi32> {
   %0 = IE.EmbeddingSegmentsSum(%arg0) {default_index_value = 4 : si32, indices_value = [0, 1, 2, 2, 3], num_segments_value = 7 : si32,
        operand_segment_sizes = dense<[1, 0, 0, 0, 0, 0]> : vector<6xi32>, per_sample_weights_value = [1.000000e+00, 5.000000e+00, 1.000000e+01, 8.000000e+00, 1.000000e+01],
        segment_ids_value = [0, 1, 2, 3, 4]} : tensor<5x6x4xsi32> -> tensor<7x6x4xsi32>
    return %0 : tensor<7x6x4xsi32>

    // CHECK: [[VAR0:%.+]] = IE.EmbeddingSegmentsSum(%arg0) {default_index_value = 4 : si32, 
    // CHECK-SAME: indices_value = [0, 1, 2, 2, 3], num_segments_value = 7 : si32, operand_segment_sizes = dense<[1, 0, 0, 0, 0, 0]> : vector<6xi32>, per_sample_weights_value = [1.000000e+00, 5.000000e+00, 1.000000e+01, 8.000000e+00, 1.000000e+01], segment_ids_value = [0, 1, 2, 3, 4]} : tensor<5x6x4xsi32> -> tensor<7x6x4xsi32>
    // CHECK: return [[VAR0]] : tensor<7x6x4xsi32>
}

