//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

/**
 * @file vcl_common.hpp
 * @brief The helper functions to check time and parse build info
 */

#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>

#include <openvino/openvino.hpp>
#include "vcl_logger.hpp"
#include "vpux_compiler.hpp"
#include "vpux_driver_compiler.h"

namespace VPUXDriverCompiler {

/**
 * @name Limitation of modelIRData
 * @see vcl_exectuble_desc_t for the structure
 * @{
 */
const uint32_t maxNumberOfElements = 10;
/// Use offset to get the location of xml and weight from memory, shall not exceed uint64_t now
const uint64_t maxSizeOfXML = std::numeric_limits<uint64_t>::max() / 3;
const uint64_t maxSizeOfWeights = maxSizeOfXML * 2;
/** @} */

/**
 * @brief Calc the time cost
 *
 */
struct StopWatch {
    using fp_milliseconds = std::chrono::duration<double, std::chrono::milliseconds::period>;

    void start() {
        startTime = std::chrono::steady_clock::now();
    }

    void stop() {
        stopTime = std::chrono::steady_clock::now();
    }

    double delta_ms() const {
        return std::chrono::duration_cast<fp_milliseconds>(stopTime - startTime).count();
    }

    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point stopTime;
};

class VPUXCompilerL0;

/**
 * @brief Parse and store all the info which is used to create blob
 *
 */
struct BuildInfo {
    explicit BuildInfo(VPUXCompilerL0* pvc);

    /**
     * @brief Convert value to ov defined precision, not all precisions are supported
     *
     * @param value The string represents precison. For example: FP32
     * @param matched If the value is not supported in the inside table, assign false
     */
    static InferenceEngine::Precision getPrecisionIE(std::string value, bool& matched);

    /**
     * @brief Convert value to ov defined layout, not all layouts are supported
     *
     * @param value The string represents layout. For example: NCHW
     * @param matched If the value is not supported in the inside table, assign false
     */
    static InferenceEngine::Layout getLayoutIE(std::string value, bool& matched);

    /**
     * @brief Parse the ioInfo string to real values and store them
     *
     * @param ioInfoOptions the input and output config which is created by prepareBuildFlags
     *
     * @see prepareBuildFlags()
     */
    vcl_result_t parseIOOption(const std::vector<std::string>& ioInfoOptions);

    /**
     * @brief Parse the build flags from vcl_executable_desc_t and store the results
     *
     * @param descOptions The info includes input and output info of model, runtime configuration of compiler
     * @return vcl_result_t
     */
    vcl_result_t prepareBuildFlags(const std::string& descOptions);

    /**
     * @brief Parse the model and weight from modelIR, store the results
     *
     * @param modelIR The memory which contains the model xml and bin data
     * @param modelIRSize The size of the memory which is pointed by modelIR
     * @return vcl_result_t
     */
    vcl_result_t prepareModel(const uint8_t* modelIR, uint64_t modelIRSize);

    /**
     * @brief The model deserialized by prepareMode()
     *
     * @todo Update to ov::Model once we drop CNNNetwork
     */
    InferenceEngine::CNNNetwork cnnNet;

    /**
     * @name Input and output setting from user
     * @{
     */
    std::unordered_map<std::string, InferenceEngine::Precision> inPrcsIE;
    std::unordered_map<std::string, InferenceEngine::Layout> inLayoutsIE;
    std::unordered_map<std::string, InferenceEngine::Precision> outPrcsIE;
    std::unordered_map<std::string, InferenceEngine::Layout> outLayoutsIE;
    /** @} */

    /// The runtime compilation config from user
    vpux::Config parsedConfig;

    /// Calc time cost on VCL level
    bool enableProfiling = false;
    VPUXCompilerL0* pvc = nullptr;
    VCLLogger* logger = nullptr;
};  // BuildInfo

}  // namespace VPUXDriverCompiler
