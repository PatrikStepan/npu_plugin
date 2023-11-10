//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

// System
#include <fstream>

#include <ie_icore.hpp>
#include <ie_metric_helpers.hpp>
#include <ie_ngraph_utils.hpp>
#include <ngraph/pass/constant_folding.hpp>
#include <ngraph/pass/manager.hpp>
#include <openvino/runtime/properties.hpp>
#include <threading/ie_executor_manager.hpp>
#include <transformations/utils/utils.hpp>

// Plugin
#include "vpux/utils/IE/config.hpp"
#include "vpux/utils/IE/itt.hpp"
#include "vpux/utils/IE/prefix.hpp"
#include "vpux_async_infer_request.h"
#include "vpux_exceptions.h"
#include "vpux_executable_network.h"

// Abstraction layer
#include "vpux.hpp"
#include "vpux/al/config/common.hpp"
#include "vpux/al/config/compiler.hpp"
#include "vpux/al/config/runtime.hpp"
#include "vpux/utils/core/checked_cast.hpp"
#include "vpux_compiler.hpp"

namespace vpux {
namespace ie = InferenceEngine;

//------------------------------------------------------------------------------
//      Helpers
//------------------------------------------------------------------------------
namespace {
std::vector<InferenceEngine::Blob::Ptr> CreateBlobsForStates(const vpux::NetworkIOVector& networkStatesInfo) {
    std::vector<InferenceEngine::Blob::Ptr> states;
    for (auto& stateInfo : networkStatesInfo) {
        InferenceEngine::TensorDesc desc = stateInfo.second->getTensorDesc();
        InferenceEngine::Blob::Ptr blob = make_blob_with_precision(desc);
        blob->allocate();
        std::memset(blob->buffer(), 0, blob->byteSize());

        states.push_back(blob);
    }

    return states;
}
}  // namespace

//------------------------------------------------------------------------------
//      Shared init ctor
//------------------------------------------------------------------------------
ExecutableNetwork::ExecutableNetwork(const Config& config, const Device::Ptr& device)
        : _config(config),
          _logger("ExecutableNetwork", config.get<LOG_LEVEL>()),
          _device(device),
          _compiler(Compiler::create(config)),
          _supportedMetrics({METRIC_KEY(NETWORK_NAME), METRIC_KEY(OPTIMAL_NUMBER_OF_INFER_REQUESTS),
                             METRIC_KEY(SUPPORTED_CONFIG_KEYS), METRIC_KEY(SUPPORTED_METRICS)}) {
}

//------------------------------------------------------------------------------
//      Load network
//------------------------------------------------------------------------------
ExecutableNetwork::ExecutableNetwork(const ie::CNNNetwork& orignet, const Device::Ptr& device, const Config& config,
                                     const bool& isNewAPI)
        : ExecutableNetwork(config, device) {
    OV_ITT_SCOPED_TASK(itt::domains::VPUXPlugin, "ExecutableNetwork::ExecutableNetwork[Load]");
    // FIXME: This is a copy-paste from kmb_executable_network.cpp
    // should be fixed after switching to VPUX completely
    OV_ITT_TASK_CHAIN(EXECUTABLE_NETWORK_LOAD, itt::domains::VPUXPlugin, "ExecutableNetwork::ExecutableNetwork[Load]",
                      "CloneNetwork");
    initializeProperties();
    VPUX_THROW_UNLESS(orignet.getFunction() != nullptr, "Old IR versions (prior v10) are not supported : {0}",
                      orignet.getName());

    ie::CNNNetwork network = ie::details::cloneNetwork(orignet);

    OV_ITT_TASK_NEXT(EXECUTABLE_NETWORK_LOAD, "Compile");
    if (std::shared_ptr<ov::Model> model = network.getFunction()) {
        ie::InputsDataMap inputMetadata = network.getInputsInfo();
        ie::OutputsDataMap outputMetadata = network.getOutputsInfo();

        // This shall be used later in the main compiler file in order to indicate the need of updating the I/O metadata
        // found within the "ov::Model" object
        model->set_rt_info(isNewAPI, "is_new_api");
        // Although the content of the I/O metadata is not required in the 2.0 API flow, these attibutes are stored
        // anyway in order to later satisfy the plugin-driver contract which demands some serialized metadata to be sent
        model->set_rt_info(inputMetadata, "input_metadata");
        model->set_rt_info(outputMetadata, "output_metadata");

        try {
            _networkPtr = _compiler->compile(model, network.getName(), _config);
        } catch (const std::exception& ex) {
            IE_THROW() << ex.what();
        } catch (...) {
            _logger.error("Unexpected exception");
            IE_THROW() << "NPU ExecutableNetwork got unexpected exception from compiler";
        }
    } else {
        _logger.warning("Failed to read NGraph network");
        IE_THROW() << "Failed to read NGraph network";
    }

    _networkStatesInfo = ExtractStatesFromInputsInfo();

    // TODO: Fix this WA for E#22783, E#25449
    // Precedence: 1st env var; 2nd config value;
    const bool configCreateExecutor = _config.get<CREATE_EXECUTOR>();
    static const auto envVar = std::getenv("IE_NPU_CREATE_EXECUTOR");
    const bool IE_NPU_CREATE_EXECUTOR =
            envVar ? vpux::envVarStrToBool("IE_NPU_CREATE_EXECUTOR", envVar) : configCreateExecutor;
    OV_ITT_TASK_NEXT(EXECUTABLE_NETWORK_LOAD, "createExecutor");
    if (IE_NPU_CREATE_EXECUTOR) {
        _logger.info("Creating executor at Load Network step");
        _executorPtr = createExecutor(_networkPtr, _config, device);
        ConfigureStreamsExecutor(network.getName());
    } else {
        _logger.info("Executor will not be created at Load Network step");
    }

    OV_ITT_TASK_SKIP(EXECUTABLE_NETWORK_LOAD);
}

InferenceEngine::InputsDataMap ExecutableNetwork::BeautifyInputsInfo() const {
    const vpux::NetworkIOVector& origDataMap = _networkPtr->getDeviceInputsInfo();
    vpux::NetworkIOVector inputWithoutStates;

    // If network to launch is passed as binary file for import, it comes with information about inputs,
    // containing additional items, related to states (VPUX stateful networks implementation specifics).
    // We don't want to expose these inputs to user, so we only store non-states inputs information in map.
    for (auto& inputInfo : origDataMap) {
        if (!isStateInputName(inputInfo.first)) {
            inputWithoutStates.emplace_back(inputInfo.first, inputInfo.second);
        }
    }

    return helpers::networkIOVectorIntoInputsDataMap(inputWithoutStates);
}

InferenceEngine::OutputsDataMap ExecutableNetwork::BeautifyOutputsInfo() const {
    const vpux::NetworkIOVector& origDataMap = _networkPtr->getDeviceOutputsInfo();
    vpux::NetworkIOVector outputWithoutStates;

    // If network to launch is passed as binary file for import, it comes with information about outputs,
    // containing additional items, related to states (VPUX stateful networks implementation specifics).
    // We don't want to expose these outputs to user, so we only store non-states outputs information in map.
    for (auto& outputInfo : origDataMap) {
        if (!isStateOutputName(outputInfo.first)) {
            outputWithoutStates.emplace_back(outputInfo.first, outputInfo.second);
        }
    }

    return helpers::networkIOVectorIntoOutputsDataMap(outputWithoutStates);
}

//------------------------------------------------------------------------------
//      Import network
//------------------------------------------------------------------------------
ExecutableNetwork::ExecutableNetwork(std::istream& networkModel, const Device::Ptr& device, const Config& config)
        : ExecutableNetwork(config, device) {
    OV_ITT_SCOPED_TASK(itt::domains::VPUXPlugin, "ExecutableNetwork::ExecutableNetwork[Import]");
    try {
        OV_ITT_TASK_CHAIN(EXECUTABLE_NETWORK_IMPORT, itt::domains::VPUXPlugin,
                          "ExecutableNetwork::ExecutableNetwork[Import]", "Parse");
        initializeProperties();
        const std::string networkName = "net" + std::to_string(loadBlobCounter);
        _networkPtr = _compiler->parse(networkModel, _config, networkName);
        OV_ITT_TASK_NEXT(EXECUTABLE_NETWORK_IMPORT, "createExecutor");
        _executorPtr = createExecutor(_networkPtr, _config, device);
        OV_ITT_TASK_NEXT(EXECUTABLE_NETWORK_IMPORT, "setIn/Out");
        _networkStatesInfo = ExtractStatesFromInputsInfo();
        _networkInputs = BeautifyInputsInfo();
        _networkOutputs = BeautifyOutputsInfo();
        setInputs(helpers::ovRawNodesIntoOVNodes(_networkPtr->getOVParameters(), false));
        setOutputs(helpers::ovRawNodesIntoOVNodes(_networkPtr->getOVResults(), true));
        OV_ITT_TASK_NEXT(EXECUTABLE_NETWORK_IMPORT, "ConfigureStreamsExecutor");
        ConfigureStreamsExecutor(networkName);
        OV_ITT_TASK_SKIP(EXECUTABLE_NETWORK_IMPORT);
    } catch (const std::exception& ex) {
        IE_THROW() << ex.what();
    } catch (...) {
        _logger.error("Unexpected exception");
        IE_THROW() << "NPU ExecutableNetwork got unexpected exception from compiler";
    }
}

void ExecutableNetwork::ConfigureStreamsExecutor(const std::string& networkName) {
    size_t maxTaskExecutorGetResultCount = 1;
    if (_config.get<EXCLUSIVE_ASYNC_REQUESTS>()) {
        _taskExecutor = ie::executorManager()->getExecutor("NPU");
        maxTaskExecutorGetResultCount = 1;
    } else {
        _taskExecutor = std::make_shared<ie::CPUStreamsExecutor>(ie::IStreamsExecutor::Config{"NPUPlugin executor", 1});
        maxTaskExecutorGetResultCount = 1;
    }

    for (size_t i = 0; i < maxTaskExecutorGetResultCount; i++) {
        std::stringstream idStream;
        idStream << networkName << "_VPUXResultExecutor" << i;
        _taskExecutorGetResultIds.emplace(idStream.str());
    }
}

ie::ITaskExecutor::Ptr ExecutableNetwork::GetNextTaskExecutor() {
    std::string id = _taskExecutorGetResultIds.front();

    _taskExecutorGetResultIds.pop();
    _taskExecutorGetResultIds.push(id);

    ie::ITaskExecutor::Ptr taskExecutor = ie::executorManager()->getExecutor(id);

    return taskExecutor;
}

//------------------------------------------------------------------------------
//      Extract information about states
//------------------------------------------------------------------------------
vpux::NetworkIOVector ExecutableNetwork::ExtractStatesFromInputsInfo() const {
    vpux::NetworkIOVector networkStatesInfo;

    auto& outputsMap = _networkPtr->getDeviceOutputsInfo();
    for (auto& inputInfo : _networkPtr->getDeviceInputsInfo()) {
        if (!isStateInputName(inputInfo.first)) {
            continue;
        }

        const auto readValueName = inputInfo.first;
        const auto variableId = readValueName.substr(READVALUE_PREFIX.length());
        std::string assignName = ASSIGN_PREFIX + variableId;

        IE_ASSERT(1 == count_if(outputsMap.begin(), outputsMap.end(), [&assignName](auto i) {
                      return i.first.compare(assignName) == 0;
                  }));

        networkStatesInfo.emplace_back(variableId, std::make_shared<InferenceEngine::Data>(*inputInfo.second));
    }

    return networkStatesInfo;
}

//------------------------------------------------------------------------------
//      Create infer requests
//------------------------------------------------------------------------------
ie::IInferRequestInternal::Ptr ExecutableNetwork::CreateInferRequestImpl(const ie::InputsDataMap networkInputs,
                                                                         const ie::OutputsDataMap networkOutputs) {
    OV_ITT_SCOPED_TASK(itt::domains::VPUXPlugin, "ExecutableNetwork::CreateInferRequestImpl");
    if (!_executorPtr) {
        _executorPtr = createExecutor(_networkPtr, _config, _device);
        ConfigureStreamsExecutor(_networkName);
    }

    if (_executorPtr == nullptr) {
        IE_THROW() << NO_EXECUTOR_FOR_INFERENCE;
    }
    const auto inferExecutor = _executorPtr;
    const auto allocator = _device->getAllocator();
    return _device->createInferRequest(networkInputs, networkOutputs, inferExecutor, _config, _networkName, _parameters,
                                       _results, _networkStatesInfo, allocator);
}

InferenceEngine::IInferRequestInternal::Ptr ExecutableNetwork::CreateInferRequest() {
    OV_ITT_SCOPED_TASK(itt::domains::VPUXPlugin, "ExecutableNetwork::CreateInferRequest");
    if (!_executorPtr) {
        _executorPtr = createExecutor(_networkPtr, _config, _device);
        ConfigureStreamsExecutor(_networkName);
    }

    if (_executorPtr == nullptr) {
        IE_THROW() << NO_EXECUTOR_FOR_INFERENCE;
    }
    const auto inferExecutor = _executorPtr;
    const auto allocator = _device->getAllocator();

    // Since, states are implemented via additional network's inputs and outputs, it is
    // important for InferRequest to hold information about them in the
    // _networkInputs/_networkOutputs maps. These maps are used by OpenVINO as source of truth
    // when someone wants to set or get blob for specific input/output in the request.
    // By the way, these maps are not accessible to the user through request API, so it is safe to
    // keep it there.
    InferenceEngine::InputsDataMap inputsInfo(_networkInputs.begin(), _networkInputs.end());
    InferenceEngine::OutputsDataMap outputsInfo(_networkOutputs.begin(), _networkOutputs.end());
    for (auto& stateInfo : _networkStatesInfo) {
        InferenceEngine::InputInfo info;
        info.setInputData(stateInfo.second);
        inputsInfo.insert({READVALUE_PREFIX + stateInfo.first, std::make_shared<InferenceEngine::InputInfo>(info)});
        outputsInfo.insert({ASSIGN_PREFIX + stateInfo.first, stateInfo.second});
    }

    auto syncRequestImpl = _device->createInferRequest(inputsInfo, outputsInfo, inferExecutor, _config, _networkName,
                                                       _parameters, _results, _networkStatesInfo, allocator);
    syncRequestImpl->setPointerToExecutableNetworkInternal(shared_from_this());

    auto blobs = CreateBlobsForStates(_networkStatesInfo);
    int index = 0;
    for (auto& stateInfo : _networkStatesInfo) {
        syncRequestImpl->SetBlob(READVALUE_PREFIX + stateInfo.first, blobs[index]);
        syncRequestImpl->SetBlob(ASSIGN_PREFIX + stateInfo.first, blobs[index]);
        ++index;
    }

    return std::make_shared<AsyncInferRequest>(syncRequestImpl, _taskExecutor, GetNextTaskExecutor(),
                                               _callbackExecutor);
}

//------------------------------------------------------------------------------
//      Export
//------------------------------------------------------------------------------

namespace {
std::uint32_t hash(const std::vector<char>& data) {
    std::uint32_t result = 1171117u;
    for (const char& c : data)
        result = ((result << 7) + result) + static_cast<uint32_t>(c);
    return result;
}

}  // namespace

void ExecutableNetwork::Export(std::ostream& model) {
    auto graphBlob = _networkPtr->getCompiledNetwork();
    model.write(graphBlob.data(), graphBlob.size());
    std::stringstream str;
    str << "Blob size: " << graphBlob.size() << ", hash: " << std::hex << hash(graphBlob);
    _logger.info("{0}", str.str());
}

void ExecutableNetwork::Export(const std::string& modelFileName) {
    std::ofstream modelFile(modelFileName, std::ios::binary);

    if (modelFile.is_open()) {
        Export(modelFile);
    } else {
        IE_THROW() << "The " << modelFileName << " file can not be opened for export.";
    }
}

//------------------------------------------------------------------------------
//      Config and Metrics
//------------------------------------------------------------------------------

void ExecutableNetwork::initializeProperties() {
    propertiesOv2 = {
            // from GetConfig
            {ov::supported_properties.name(),
             {true, ov::PropertyMutability::RO,
              [&](const Config&) {
                  return supportedProperties;
              }}},
            {ov::device::id.name(),
             {true, ov::PropertyMutability::RO,
              [](const Config& config) {
                  return config.get<DEVICE_ID>();
              }}},
            {ov::intel_vpux::print_profiling.name(),
             {true, ov::PropertyMutability::RO,
              [](const Config& config) {
                  return config.get<PRINT_PROFILING>();
              }}},
            {ov::intel_vpux::profiling_output_file.name(),
             {true, ov::PropertyMutability::RO,
              [](const Config& config) {
                  return config.get<PROFILING_OUTPUT_FILE>();
              }}},
            {ov::intel_vpux::vpux_platform.name(),
             {true, ov::PropertyMutability::RO,
              [](const Config& config) {
                  return config.get<PLATFORM>();
              }}},
            {ov::hint::model_priority.name(),
             {true, ov::PropertyMutability::RO,
              [](const Config& config) {
                  return config.get<MODEL_PRIORITY>();
              }}},
            {ov::enable_profiling.name(),
             {true, ov::PropertyMutability::RO,
              [](const Config& config) {
                  return config.get<PERF_COUNT>();
              }}},
            {ov::hint::performance_mode.name(),
             {true, ov::PropertyMutability::RO,
              [](const Config& config) {
                  return config.get<PERFORMANCE_HINT>();
              }}},
            {ov::hint::num_requests.name(),
             {true, ov::PropertyMutability::RO,
              [](const Config& config) {
                  return config.get<PERFORMANCE_HINT_NUM_REQUESTS>();
              }}},
            {ov::intel_vpux::use_elf_compiler_backend.name(),
             {true, ov::PropertyMutability::RO,
              [](const Config& config) {
                  return config.get<USE_ELF_COMPILER_BACKEND>();
              }}},
            {ov::intel_vpux::create_executor.name(),
             {true, ov::PropertyMutability::RO,
              [](const Config& config) {
                  return config.get<CREATE_EXECUTOR>();
              }}},
            // from GetConfig

            // from GetMetric
            {ov::model_name.name(),
             {true, ov::PropertyMutability::RO,
              [&](const Config&) {
                  VPUX_THROW_WHEN(_networkPtr == nullptr, "GetMetric: network is not initialized");
                  return _networkPtr->getName();
              }}},
            {ov::optimal_number_of_infer_requests.name(),
             {true, ov::PropertyMutability::RO,
              [&](const Config& config) {
                  // value is allowed to be queried prior the network is compiled
                  return static_cast<uint32_t>(getOptimalNumberOfInferRequestsInParallel(config));
              }}},
            {ov::execution_devices.name(),
             {true, ov::PropertyMutability::RO,
              [&](const Config&) {
                  VPUX_THROW_WHEN(_device == nullptr, "GetMetric: device is not initialized");
                  return _device->getName();
              }}}
            // from GetMetric
    };

    for (auto& prop : propertiesOv2) {
        if (std::get<0>(prop.second)) {
            supportedProperties.emplace_back(ov::PropertyName(prop.first, std::get<1>(prop.second)));
        }
    }
}

ie::Parameter ExecutableNetwork::GetMetric(const std::string& name) const {
    if (_plugin->GetCore()->isNewAPI()) {
        auto&& cit = propertiesOv2.find(name);
        if (cit != propertiesOv2.cend()) {
            return std::get<2>(cit->second)(_config);
        }
    }

    if (name == METRIC_KEY(NETWORK_NAME)) {
        VPUX_THROW_WHEN(_networkPtr == nullptr, "GetMetric: network is not initialized");
        IE_SET_METRIC_RETURN(NETWORK_NAME, _networkPtr->getName());
    } else if (name == METRIC_KEY(OPTIMAL_NUMBER_OF_INFER_REQUESTS)) {
        // value is allowed to be queried prior the network is compiled
        IE_SET_METRIC_RETURN(OPTIMAL_NUMBER_OF_INFER_REQUESTS,
                             static_cast<unsigned int>(getOptimalNumberOfInferRequestsInParallel(_config)));
    } else if (name == METRIC_KEY(SUPPORTED_METRICS)) {
        IE_SET_METRIC_RETURN(SUPPORTED_METRICS, _supportedMetrics);
    } else if (name == METRIC_KEY(SUPPORTED_CONFIG_KEYS)) {
        // TODO implement retrieval the actual config keys collection
        std::vector<std::string> keys;
        IE_SET_METRIC_RETURN(SUPPORTED_CONFIG_KEYS, std::move(keys));
    }

    VPUX_THROW("Unsupported metric {0}", name);
}

ie::Parameter ExecutableNetwork::GetConfig(const std::string& name) const {
    auto&& cit = propertiesOv2.find(name);
    if (cit != propertiesOv2.cend()) {
        return std::get<2>(cit->second)(_config);
    }
    VPUX_THROW("Unsupported parameter {0}", name);
}

//------------------------------------------------------------------------------
std::atomic<int> ExecutableNetwork::loadBlobCounter{1};

Executor::Ptr ExecutableNetwork::createExecutor(const NetworkDescription::Ptr& network, const Config& config,
                                                const Device::Ptr& device) {
    loadBlobCounter++;  // increment blob static counter to make unique network ID
    if (network == nullptr) {
        IE_THROW() << "Network is null!";
    }
    // Default executor is nullptr, allow only perform export
    Executor::Ptr executor = nullptr;
    if (device != nullptr) {
        executor = device->createExecutor(network, config);
    }
    _networkName = _networkPtr->getName();
    return executor;
}

}  // namespace vpux
