//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPURegMapped/dialect.hpp"
#include "vpux/compiler/dialect/VPURegMapped/ops.hpp"

//
// initialize
//

void vpux::VPURegMapped::VPURegMappedDialect::initialize() {
    addOperations<
#define GET_OP_LIST
#include <vpux/compiler/dialect/VPURegMapped/ops.cpp.inc>
            >();

    registerTypes();
    registerAttributes();
}

//
// Generated
//

#include <vpux/compiler/dialect/VPURegMapped/dialect.cpp.inc>
