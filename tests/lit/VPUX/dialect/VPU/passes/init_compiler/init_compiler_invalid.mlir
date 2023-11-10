//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// RUN: vpux-opt --split-input-file --init-compiler="vpu-arch=%arch%" -verify-diagnostics %s
// REQUIRES: arch-VPUX30XX || arch-VPUX37XX

// expected-error@+1 {{Architecture is already defined, probably you run '--init-compiler' twice}}
module @test attributes {VPU.arch = #VPU.arch_kind<VPUX30XX>} {
}

// -----

// expected-error@+1 {{Available executor kind 'DMA_NN' was already added}}
module @error {
    IE.ExecutorResource 1 of @DMA_NN
}
