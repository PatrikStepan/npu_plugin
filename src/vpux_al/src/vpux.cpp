//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux.hpp"

#include <memory>
#include <openvino/util/shared_object.hpp>

#include "vpux/utils/IE/itt.hpp"

// TODO: the creation of backend is not scalable,
// it needs to be refactored in order to simplify
// adding other backends into static build config
#if defined(OPENVINO_STATIC_LIBRARY) && defined(ENABLE_ZEROAPI_BACKEND)
#include <zero_backend.h>
#endif

namespace ie = InferenceEngine;
namespace vpux {

//------------------------------------------------------------------------------
#ifdef OPENVINO_STATIC_LIBRARY
EngineBackend::EngineBackend(std::shared_ptr<IEngineBackend> impl): _impl{impl} {};
#else
EngineBackend::EngineBackend(const std::string& pathToLib, const Config& config) {
    OV_ITT_TASK_CHAIN(ENGINE_BACKEND, itt::domains::VPUXPlugin, "EngineBackend", "IEngineBackend");
    using CreateFuncT = void (*)(std::shared_ptr<IEngineBackend>&, const Config&);
    static constexpr auto CreateFuncName = "CreateVPUXEngineBackend";

    // Due to exception is an object, we have to destroy it properly. Library could be unloaded before we handle
    // the exception. To avoid such case we have to catch and handle this exception and only after that unload the
    // library.
    OV_ITT_TASK_NEXT(ENGINE_BACKEND, "load_shared_object");
    bool successLoaded = false;
    std::string errorMessage = "Unexpected exception from backend library: " + pathToLib;
    try {
        _so = ov::util::load_shared_object(pathToLib.c_str());

        const auto createFunc = reinterpret_cast<CreateFuncT>(ov::util::get_symbol(_so, CreateFuncName));
        createFunc(_impl, config);
        successLoaded = true;
    } catch (const std::exception& ex) {
        errorMessage = ex.what();
    } catch (...) {
    }

    if (!successLoaded) {
        IE_THROW() << errorMessage;
    }
}
#endif

inline const std::shared_ptr<Device> wrapDeviceWithImpl(const std::shared_ptr<IDevice>& device,
                                                        const std::shared_ptr<void>& so) {
    if (device == nullptr) {
        return nullptr;
    }
    return std::make_shared<Device>(device, so);
}

const std::shared_ptr<Device> EngineBackend::getDevice() const {
    return wrapDeviceWithImpl(_impl->getDevice(), _so);
}

const std::shared_ptr<Device> EngineBackend::getDevice(const std::string& specificDeviceName) const {
    return wrapDeviceWithImpl(_impl->getDevice(specificDeviceName), _so);
}

const std::shared_ptr<Device> EngineBackend::getDevice(const ie::ParamMap& paramMap) const {
    return wrapDeviceWithImpl(_impl->getDevice(paramMap), _so);
}

const std::shared_ptr<IDevice> IEngineBackend::getDevice() const {
    IE_THROW() << "Default getDevice() not implemented";
}
const std::shared_ptr<IDevice> IEngineBackend::getDevice(const std::string&) const {
    IE_THROW() << "Specific device search not implemented";
}
const std::shared_ptr<IDevice> IEngineBackend::getDevice(const ie::ParamMap&) const {
    IE_THROW() << "Get device based on params not implemented";
}
const std::vector<std::string> IEngineBackend::getDeviceNames() const {
    IE_THROW() << "Get all device names not implemented";
}

void IEngineBackend::registerOptions(OptionsDesc&) const {
}

void* Allocator::wrapRemoteMemory(const ie::ParamMap&) noexcept {
    std::cerr << "Wrapping remote memory not implemented" << std::endl;
    return nullptr;
}
std::shared_ptr<Allocator> IDevice::getAllocator(const ie::ParamMap&) const {
    IE_THROW() << "Not supported";
}

Uuid IDevice::getUuid() const {
    IE_THROW() << "Get UUID not supported";
}

uint64_t IDevice::getTotalMemSize() const {
    IE_THROW() << "Get TotalMemSize is not supported";
}

uint32_t IDevice::getDriverVersion() const {
    IE_THROW() << "Get VPU driver version is not supported with this backend";
}

}  // namespace vpux
