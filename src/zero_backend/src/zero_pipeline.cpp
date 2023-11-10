//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "zero_pipeline.h"

#include <ze_api.h>
#include <ze_graph_ext.h>

#include "vpux/utils/IE/blob.hpp"
#include "vpux/utils/IE/itt.hpp"
#include "vpux/utils/IE/prefix.hpp"
#include "vpux/utils/core/logger.hpp"

using namespace vpux;

namespace vpux {
struct DiscretePipeline final : public Pipeline {
public:
    DiscretePipeline(const Config& config, const ze_device_handle_t& device_handle, const ze_context_handle_t context,
                     ze_graph_dditable_ext_t* graph_ddi_table_ext, const Executor::Ptr& executorPtr,
                     ze_graph_profiling_query_handle_t profiling_handle,
                     const std::array<std::shared_ptr<CommandQueue>, stage::COUNT>& command_queues,
                     const uint32_t& group_ordinal)
            : _config(config),
              _command_queues{command_queues},
              _command_list{{{device_handle, context, graph_ddi_table_ext, _config, group_ordinal},
                             {device_handle, context, graph_ddi_table_ext, _config, group_ordinal},
                             {device_handle, context, graph_ddi_table_ext, _config, group_ordinal}}},
              _fence{{{*_command_queues[stage::UPLOAD], _config},
                      {*_command_queues[stage::EXECUTE], _config},
                      {*_command_queues[stage::READBACK], _config}}},
              _event_pool(device_handle, context, stage::COUNT, _config),
              _event{{{_event_pool.handle(), stage::UPLOAD, _config},
                      {_event_pool.handle(), stage::EXECUTE, _config},
                      {_event_pool.handle(), stage::READBACK, _config}}} {
        const ZeroExecutor* executor = static_cast<ZeroExecutor*>(executorPtr.get());

        OV_ITT_SCOPED_TASK(itt::domains::LevelZeroBackend, "Zero_infer_request::DiscretePipeline::DiscretePipeline");
        for (const auto& desc : executor->inputs_desc_map()) {
            _inputs.appendArgument(desc.first, desc.second.info);
        }
        _inputs.allocate(device_handle, context);
        _command_list[stage::UPLOAD].appendMemoryCopy(_inputs.getDeviceMemRegion(), _inputs.getHostMemRegion(),
                                                      _inputs.getSize());
        for (const auto& desc : executor->inputs_desc_map()) {
            executor->setArgumentValue(desc.second.idx, _inputs.getDevicePtr(desc.first));
        }

        _command_list[stage::UPLOAD].appendBarrier();
        _event[stage::UPLOAD].AppendSignalEvent(_command_list[stage::UPLOAD]);

        for (const auto& desc : executor->outputs_desc_map()) {
            _outputs.appendArgument(desc.first, desc.second.info);
        }
        _outputs.allocate(device_handle, context);
        _command_list[stage::READBACK].appendMemoryCopy(_outputs.getHostMemRegion(), _outputs.getDeviceMemRegion(),
                                                        _outputs.getSize());
        for (const auto& desc : executor->outputs_desc_map()) {
            executor->setArgumentValue(desc.second.idx, _outputs.getDevicePtr(desc.first));
        }

        _event[stage::UPLOAD].AppendWaitOnEvent(_command_list[stage::EXECUTE]);

        _command_list[stage::EXECUTE].appendGraphExecute(executor->graph(), profiling_handle);

        _event[stage::UPLOAD].AppendEventReset(_command_list[stage::READBACK]);

        for (auto& commandList : _command_list) {
            commandList.close();
        }
    };

    DiscretePipeline(const DiscretePipeline&) = delete;
    DiscretePipeline& operator=(const DiscretePipeline&) = delete;
    virtual ~DiscretePipeline() = default;

    void push() override {
        OV_ITT_TASK_CHAIN(ZERO_INFER_REQUEST_DP_PUSH, itt::domains::LevelZeroBackend, "DiscretePipeline::push",
                          "UPLOAD");
        // Dispatch command to copy input data from upload heap to default heap
        _command_queues[stage::UPLOAD]->executeCommandList(_command_list[stage::UPLOAD]);

        OV_ITT_TASK_NEXT(ZERO_INFER_REQUEST_DP_PUSH, "EXECUTE");
        // Submit the command list for execute
        _command_queues[stage::EXECUTE]->executeCommandList(_command_list[stage::EXECUTE], _fence[stage::EXECUTE]);
    };

    void pull() override {
        OV_ITT_TASK_CHAIN(ZERO_INFER_REQUEST_DP_PULL, itt::domains::LevelZeroBackend, "DiscretePipeline::pull",
                          "EXECUTE");
        // Wait for execute to finish
        _fence[stage::EXECUTE].hostSynchronize();
        OV_ITT_TASK_NEXT(ZERO_INFER_REQUEST_DP_PULL, "READBACK");
        // Schedule the copy of outputs from zeDriverAllocDeviceMem to zeDriverAllocHostMem
        _command_queues[stage::READBACK]->executeCommandList(_command_list[stage::READBACK], _fence[stage::READBACK]);
        // Wait for output copy to finish execution for _fence from the host, to make sure that data
        // is available in the hostMem buffer of the output
        _fence[stage::READBACK].hostSynchronize();
    };

    void reset() const override {
        // Reset the fence objects
        for (auto& fence : _fence) {
            fence.reset();
        }
    };

private:
    const Config _config;
    const std::array<std::shared_ptr<CommandQueue>, stage::COUNT>& _command_queues;
    std::array<CommandList, stage::COUNT> _command_list;
    std::array<Fence, stage::COUNT> _fence;
    EventPool _event_pool;
    std::array<Event, stage::COUNT> _event;
};

struct IntegratedPipeline final : public Pipeline {
public:
    IntegratedPipeline(const Config& config, const ze_device_handle_t& device_handle, const ze_context_handle_t context,
                       ze_graph_dditable_ext_t* graph_ddi_table_ext, const Executor::Ptr& executorPtr,
                       ze_graph_profiling_query_handle_t profiling_handle, CommandQueue& command_queue,
                       const uint32_t& group_ordinal)
            : _config(config),
              _command_queue{command_queue},
              _command_list{device_handle, context, graph_ddi_table_ext, _config, group_ordinal},
              _fence{_command_queue, _config},
              _event_pool{device_handle, context, 1, _config},
              _event{_event_pool.handle(), 0, _config} {
        const ZeroExecutor* executor = static_cast<ZeroExecutor*>(executorPtr.get());

        OV_ITT_SCOPED_TASK(itt::domains::LevelZeroBackend,
                           "Zero_infer_request::IntegratedPipeline::IntegratedPipeline");
        for (const auto& desc : executor->inputs_desc_map()) {
            _inputs.appendArgument(desc.first, desc.second.info);
        }
        _inputs.allocate(context, ZE_HOST_MEM_ALLOC_FLAG_BIAS_WRITE_COMBINED);
        for (const auto& desc : executor->inputs_desc_map()) {
            executor->setArgumentValue(desc.second.idx, _inputs.getHostPtr(desc.first));
        }

        for (const auto& desc : executor->outputs_desc_map()) {
            _outputs.appendArgument(desc.first, desc.second.info);
        }
        _outputs.allocate(context);
        for (const auto& desc : executor->outputs_desc_map()) {
            executor->setArgumentValue(desc.second.idx, _outputs.getHostPtr(desc.first));
        }

        _command_list.appendGraphExecute(executor->graph(), profiling_handle);
        // appendBarrier used in L0 as well
        if (!sync_output_with_fences_) {
            _command_list.appendBarrier();
            _event.AppendSignalEvent(_command_list);
        }
        _command_list.close();
    };

    IntegratedPipeline(const IntegratedPipeline&) = delete;
    IntegratedPipeline& operator=(const IntegratedPipeline&) = delete;
    virtual ~IntegratedPipeline() = default;

    void push() override {
        OV_ITT_TASK_CHAIN(ZERO_EXECUTOR_IP_PUSH, itt::domains::LevelZeroBackend, "IntegratedPipeline", "push");
        if (sync_output_with_fences_) {
            _command_queue.executeCommandList(_command_list, _fence);
        } else {
            _command_queue.executeCommandList(_command_list);
        }
    };

    void pull() override {
        OV_ITT_TASK_CHAIN(ZERO_EXECUTOR_IP_PULL, itt::domains::LevelZeroBackend, "IntegratedPipeline", "pull");
        if (sync_output_with_fences_) {
            _fence.hostSynchronize();
        } else {
            _event.hostSynchronize();
        }
    };

    void reset() const override {
        if (sync_output_with_fences_) {
            _fence.reset();
        } else {
            _event.reset();
        }
    };

private:
    const Config _config;
    CommandQueue& _command_queue;
    CommandList _command_list;
    Fence _fence;
    EventPool _event_pool;
    Event _event;
    bool sync_output_with_fences_ = true;
};

std::unique_ptr<Pipeline> makePipeline(const Executor::Ptr& executorPtr, const Config& config,
                                       vpux::zeroProfiling::ProfilingPool& profiling_pool,
                                       vpux::zeroProfiling::ProfilingQuery& profiling_query) {
    OV_ITT_SCOPED_TASK(itt::domains::LevelZeroBackend, "Infer_request::makePipeline");
    if (profiling_pool.create())
        profiling_query.create(profiling_pool._handle);

    const ZeroExecutor* executor = static_cast<ZeroExecutor*>(executorPtr.get());

    const ze_device_handle_t device_handle = executor->device();
    const ze_context_handle_t context = executor->context();
    ze_graph_dditable_ext_t* graph_ddi_table_ext = executor->graph_ddi_table_ext();
    auto& command_queues = executor->getCommandQueue();
    uint32_t group_ordinal = executor->get_group_ordinal();

    ze_device_properties_t properties = {};
    zeroUtils::throwOnFail("zeDeviceGetProperties", zeDeviceGetProperties(device_handle, &properties));

    if (properties.flags & ZE_DEVICE_PROPERTY_FLAG_INTEGRATED) {
        return std::make_unique<IntegratedPipeline>(config, device_handle, context, graph_ddi_table_ext, executorPtr,
                                                    profiling_query.getHandle(), *command_queues[stage::EXECUTE],
                                                    group_ordinal);
    }

    return std::make_unique<DiscretePipeline>(config, device_handle, context, graph_ddi_table_ext, executorPtr,
                                              profiling_query.getHandle(), command_queues, group_ordinal);
}

}  // namespace vpux
