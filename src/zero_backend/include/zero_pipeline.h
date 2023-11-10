//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include "zero_executor.h"
#include "zero_memory.h"
#include "zero_profiling.h"
#include "zero_utils.h"
#include "zero_wrappers.h"

namespace vpux {
struct Pipeline {
public:
    Pipeline() = default;
    Pipeline(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;
    virtual ~Pipeline() = default;

    virtual void push() = 0;
    virtual void pull() = 0;
    virtual void reset() const = 0;

    inline zeroMemory::MemoryManagementUnit& inputs() {
        return _inputs;
    };
    inline zeroMemory::MemoryManagementUnit& outputs() {
        return _outputs;
    };

protected:
    zeroMemory::MemoryManagementUnit _inputs;
    zeroMemory::MemoryManagementUnit _outputs;
};

std::unique_ptr<Pipeline> makePipeline(const Executor::Ptr& executorPtr, const Config& config,
                                       vpux::zeroProfiling::ProfilingPool& profiling_pool,
                                       vpux::zeroProfiling::ProfilingQuery& profiling_query);
}  // namespace vpux
