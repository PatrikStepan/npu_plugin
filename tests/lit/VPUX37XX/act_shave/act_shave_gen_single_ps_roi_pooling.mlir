//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --init-compiler="vpu-arch=VPUX37XX" %s | vpux-translate --vpu-arch=VPUX37XX --export-VPUIP -o %t
// RUN: flatc --raw-binary --json %vpuip_schema_file% -- %t
// RUN: FileCheck %s --input-file %basename_t.json
// RUN: rm %basename_t.json
//
// This file generates a blob with PSROIPooling shave
// demonstrate that the runtime cannot handle this.  It's also a lit test to help
// check for regressions in the VPUIP dialect.
//

module @Test {
IE.CNNNetwork
    entryPoint : @main
    inputsInfo : {
        IE.DataInfo "input0" : tensor<2x200x20x20xf16>
        IE.DataInfo "input1" : tensor<1x5xf16>
  } outputsInfo : {
        IE.DataInfo "output" : tensor<1x50x2x2xf16>
  }

VPURT.SW.Runtime
    entryPoint: @VPU.SW::@runtime
    stack_configuration: [
        4096,
        4096,
        4096,
        4096
    ]


// Sub-module, which holds SW kernel declarations and optional implementations.
// Used to group those declarations for faster access.
module @VPU.SW {
    // The declaration should match C++ params structure in decomposed form.
    // `memref` will be translated to `MemRefData`, while raw scalars will be translated as is.
    func.func private @builtin_PSROIPooling(%input0 : memref<*xf16>, %input1 : memref<*xf16>, %output : memref<*xf16>)
        attributes {
            VPU.kernel_code = "single_shave_ps_roipooling.cpp",
            VPU.kernel_entry = "single_shave_ps_roipooling"
        }

    // management kernel definition
    func.func private @runtime()
        attributes {
            VPU.kernel_code = "nnActEntry"
        }
}

func.func @main(%1: memref<2x200x20x20xf16>, %2: memref<1x5xf16>, %3: memref<1x50x2x2xf16>) -> memref<1x50x2x2xf16> {
    %in0_tile0_cmx  = VPURT.DeclareBuffer <CMX_NN> [0] <0> -> memref<2x200x20x20xf16, [@CMX_NN, 0]>
    %in1_tile0_cmx  = VPURT.DeclareBuffer <CMX_NN> [0] <4000> -> memref<1x5xf16, [@CMX_NN, 0]>
    %out_tile0_cmx = VPURT.DeclareBuffer <CMX_NN> [0] <8000> -> memref<1x50x2x2xf16, [@CMX_NN, 0]>

    %b0 = VPURT.ConfigureBarrier<0> -> !VPURT.Barrier
    %b1 = VPURT.ConfigureBarrier<1> -> !VPURT.Barrier

    VPURT.Task updates(%b0 : !VPURT.Barrier) {
        VPUIP.NNDMA inputs(%1 : memref<2x200x20x20xf16>) outputs(%in0_tile0_cmx : memref<2x200x20x20xf16, [@CMX_NN, 0]>) -> memref<2x200x20x20xf16, [@CMX_NN, 0]>
    }

    VPURT.Task updates(%b0 : !VPURT.Barrier) {
        VPUIP.NNDMA inputs(%2 : memref<1x5xf16>) outputs(%in1_tile0_cmx : memref<1x5xf16, [@CMX_NN, 0]>) -> memref<1x5xf16, [@CMX_NN, 0]>
    }

    VPURT.Task waits(%b0 : !VPURT.Barrier) updates(%b1 : !VPURT.Barrier) {
        VPUIP.SW.Kernel
                    {result_segment_sizes = dense<[1, 0]> : vector<2xi32>}
                    @VPU.SW::@builtin_PSROIPooling           // The reference to the Kernel function.
                    inputs(%in0_tile0_cmx as %arg0: memref<2x200x20x20xf16, [@CMX_NN, 0]>, %in1_tile0_cmx as %arg1: memref<1x5xf16, [@CMX_NN, 0]>)
                    outputs(%out_tile0_cmx as %arg2: memref<1x50x2x2xf16, [@CMX_NN, 0]>)   //
                    on tile 0                           // The tile index to execute on.
        -> memref<1x50x2x2xf16, [@CMX_NN, 0]> {
                VPUIP.SW.Kernel.run {attrs = [50, 1.000000e+00, 2, 1, 1, 1]}(%arg0, %arg1, %arg2)
                    : memref<2x200x20x20xf16, [@CMX_NN, 0]>
                    , memref<1x5xf16, [@CMX_NN, 0]>
                    , memref<1x50x2x2xf16, [@CMX_NN, 0]>
        }
    }

    VPURT.Task waits(%b1 : !VPURT.Barrier) {
        VPUIP.NNDMA inputs(%out_tile0_cmx : memref<1x50x2x2xf16, [@CMX_NN, 0]>) outputs(%3 : memref<1x50x2x2xf16>) -> memref<1x50x2x2xf16>
    }
    return %3: memref<1x50x2x2xf16>
}

}

// CHECK    identifier: "Test",
// CHECK    net_input: [
// CHECK      {
// CHECK        name: "input0",
// CHECK        dimensions: [
// CHECK          1,
// CHECK          200,
// CHECK          20,
// CHECK          20
// CHECK        ],
// CHECK        strides: [
// CHECK          2.0,
// CHECK          160000.0,
// CHECK          800.0,
// CHECK          40.0,
// CHECK          2.0
// CHECK        ],
// CHECK        data: {
// CHECK          data_index: 0
// CHECK        },
// CHECK        locale: "ProgrammableInput",
// CHECK        data_dtype: "FP16"
// CHECK      },
// CHECK      {
// CHECK        name: "input1",
// CHECK        dimensions: [
// CHECK          1,
// CHECK          5
// CHECK        ],
// CHECK        strides: [
// CHECK          2.0,
// CHECK          10.0,
// CHECK          2.0
// CHECK        ],
// CHECK        data: {
// CHECK          data_index: 0
// CHECK        },
// CHECK        locale: "ProgrammableInput",
// CHECK        data_dtype: "FP16"
// CHECK      }
// CHECK    ],

// CHECK    net_output: [
// CHECK      {
// CHECK        name: "output",
// CHECK        dimensions: [
// CHECK          1,
// CHECK          50,
// CHECK          2,
// CHECK          2
// CHECK        ],
// CHECK        strides: [
// CHECK          2.0,
// CHECK          400.0,
// CHECK          8.0,
// CHECK          4.0,
// CHECK          2.0
// CHECK        ],
// CHECK        data: {
// CHECK          data_index: 0
// CHECK        },
// CHECK        locale: "ProgrammableOutput",
// CHECK        data_dtype: "FP16"
// CHECK      }
// CHECK    ],
// CHECK    task_count: 6,
// CHECK    options: [
// CHECK    ],

// CHECK    in_tensor_desc: [
// CHECK      {
// CHECK        name: "input0",
// CHECK        dimensions: [
// CHECK          2,
// CHECK          200,
// CHECK          20,
// CHECK          20
// CHECK        ],
// CHECK        strides: [
// CHECK          2.0,
// CHECK          160000.0,
// CHECK          800.0,
// CHECK          40.0,
// CHECK          2.0
// CHECK        ],
// CHECK        data: {
// CHECK          data_index: 0
// CHECK        },
// CHECK        locale: "ProgrammableInput",
// CHECK        locale_index: [
// CHECK          0
// CHECK        ],
// CHECK        data_dtype: "FP16"
// CHECK      },

// CHECK      {
// CHECK        name: "input1",
// CHECK        dimensions: [
// CHECK          1,
// CHECK          5
// CHECK        ],
// CHECK        strides: [
// CHECK          2.0,
// CHECK          10.0,
// CHECK          2.0
// CHECK        ],
// CHECK        data: {
// CHECK          data_index: 0
// CHECK        },
// CHECK        locale: "ProgrammableInput"
// CHECK        data_dtype: "FP16"
// CHECK      }
// CHECK    ],

// CHECK    out_tensor_desc: [
// CHECK      {
// CHECK        name: "output",
// CHECK        dimensions: [
// CHECK          1,
// CHECK          50,
// CHECK          2,
// CHECK          2
// CHECK        ],
// CHECK        strides: [
// CHECK          2.0,
// CHECK          400.0,
// CHECK          8.0,
// CHECK          4.0,
// CHECK          2.0
// CHECK        ],
// CHECK        data: {
// CHECK          data_index: 0
// CHECK        },
// CHECK        locale: "ProgrammableOutput",
// CHECK        data_dtype: "FP16"
// CHECK      }
// CHECK:   ]

// CHECK:   kernel_data: [
// CHECK:      ]
