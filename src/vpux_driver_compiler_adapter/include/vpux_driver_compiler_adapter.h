//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once
#include "iexternal_compiler.h"
#include "vpux_compiler.hpp"

#include <ze_graph_ext.h>

namespace vpux {
namespace driverCompilerAdapter {

/**
 * @brief Adapter for Compiler in driver
 * @details Wrap compiler in driver calls and do preliminary actions (like opset conversion)
 */
class LevelZeroCompilerAdapter final : public ICompiler {
public:
    LevelZeroCompilerAdapter();
    explicit LevelZeroCompilerAdapter(const IExternalCompiler::Ptr& compilerAdapter);

    std::shared_ptr<INetworkDescription> compile(std::shared_ptr<ov::Model>& model, const std::string& networkName,
                                                 const vpux::Config& config) final;

    ov::SupportedOpsMap query(const std::shared_ptr<const ov::Model>& model, const vpux::Config& config) final;

    std::shared_ptr<vpux::INetworkDescription> parse(const std::vector<char>& network, const vpux::Config& config,
                                                     const std::string& netName) final;

private:
    /**
     * @brief Separate externals calls to separate class
     */
    IExternalCompiler::Ptr apiAdapter;
    ze_driver_handle_t _driverHandle = nullptr;
    Logger _logger;
};

}  // namespace driverCompilerAdapter
}  // namespace vpux
