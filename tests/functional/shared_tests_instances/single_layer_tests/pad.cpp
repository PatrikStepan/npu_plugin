//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include <vector>

#include "single_layer_tests/pad.hpp"
#include "vpu_ov1_layer_test.hpp"

namespace LayerTestsDefinitions {

class VPUXPadLayerTest : public PadLayerTest, virtual public LayerTestsUtils::VpuOv1LayerTestsCommon {};
class VPUXPadLayerTest_VPU3700 : public VPUXPadLayerTest {};
class VPUXPadLayerTest_VPU3720 : public VPUXPadLayerTest {};

TEST_P(VPUXPadLayerTest_VPU3700, HW) {
    setPlatformVPU3700();
    setDefaultHardwareModeMLIR();
    Run();
}

TEST_P(VPUXPadLayerTest_VPU3720, HW) {
    setPlatformVPU3720();
    setDefaultHardwareModeMLIR();
    Run();
}

}  // namespace LayerTestsDefinitions

using namespace LayerTestsDefinitions;

namespace {
const std::vector<InferenceEngine::Precision> netPrecisions = {InferenceEngine::Precision::FP16};

const std::vector<std::vector<int64_t>> padsBegin4D = {{0, 0, 0, 0}, {0, 1, 1, 1}, {0, 0, 1, 0}, {0, 3, 0, 1}};
const std::vector<std::vector<int64_t>> padsEnd4D = {{0, 0, 0, 0}, {0, 1, 1, 1}, {0, 0, 0, 1}, {0, 3, 2, 0}};

const std::vector<float> argPadValue = {0.f, 1.f, 2.f, -1.f};

const std::vector<ngraph::helpers::PadMode> padMode = {
        ngraph::helpers::PadMode::EDGE, ngraph::helpers::PadMode::REFLECT, ngraph::helpers::PadMode::SYMMETRIC};

const auto pad4DConstparams = testing::Combine(
        testing::ValuesIn(padsBegin4D), testing::ValuesIn(padsEnd4D), testing::ValuesIn(argPadValue),
        testing::Values(ngraph::helpers::PadMode::CONSTANT), testing::ValuesIn(netPrecisions),
        testing::Values(InferenceEngine::Precision::FP16), testing::Values(InferenceEngine::Precision::FP16),
        testing::Values(InferenceEngine::Layout::NCHW), testing::Values(std::vector<size_t>{1, 5, 10, 11}),
        testing::Values(LayerTestsUtils::testPlatformTargetDevice()));

INSTANTIATE_TEST_SUITE_P(DISABLED_smoke_Pad4DConst, VPUXPadLayerTest_VPU3700, pad4DConstparams,
                         VPUXPadLayerTest_VPU3700::getTestCaseName);

const auto pad4Dparams = testing::Combine(
        testing::ValuesIn(padsBegin4D), testing::ValuesIn(padsEnd4D), testing::Values(0), testing::ValuesIn(padMode),
        testing::ValuesIn(netPrecisions), testing::Values(InferenceEngine::Precision::FP16),
        testing::Values(InferenceEngine::Precision::FP16), testing::Values(InferenceEngine::Layout::NCHW),
        testing::Values(std::vector<size_t>{1, 5, 10, 11}),
        testing::Values(LayerTestsUtils::testPlatformTargetDevice()));

INSTANTIATE_TEST_SUITE_P(DISABLED_TMP_smoke_Pad4D, VPUXPadLayerTest_VPU3700, pad4Dparams,
                         VPUXPadLayerTest_VPU3700::getTestCaseName);

const std::vector<std::vector<int64_t>> padsBeginForConcat = {{0, 0, 0, 0}, {4, 2, 1, 3}, {8, 0, 0, 0}, {0, 0, 2, 0}};
const std::vector<std::vector<int64_t>> padsEndForConcat = {{0, 0, 0, 0}, {5, 2, 6, 1}, {8, 0, 0, 0}, {0, 1, 0, 3}};

const auto padConvertToConcat = testing::Combine(
        testing::ValuesIn(padsBeginForConcat), testing::ValuesIn(padsEndForConcat), testing::Values(0, 1),
        testing::Values(ngraph::helpers::PadMode::CONSTANT), testing::ValuesIn(netPrecisions),
        testing::Values(InferenceEngine::Precision::FP16), testing::Values(InferenceEngine::Precision::FP16),
        testing::Values(InferenceEngine::Layout::NCHW, InferenceEngine::Layout::NHWC),
        testing::Values(std::vector<size_t>{1, 10, 20, 30}),
        testing::Values(LayerTestsUtils::testPlatformTargetDevice()));

// Tracking number [E#85137]
INSTANTIATE_TEST_SUITE_P(DISABLED_TMP_smoke_PadConvertToConcat, VPUXPadLayerTest_VPU3700, padConvertToConcat,
                         VPUXPadLayerTest_VPU3700::getTestCaseName);

//
// VPU3720 instance
//

const std::vector<std::vector<int64_t>> padsBegin4D_VPU3720 = {{0, 0, 0, 0}, {0, 3, 0, 1}};
const std::vector<std::vector<int64_t>> padsEnd4D_VPU3720 = {{0, 0, 0, 0}, {0, 3, 2, 0}};
const std::vector<float> argPadValue_VPU3720 = {0.f, -1.f};

const auto pad4DConstParams = testing::Combine(
        testing::ValuesIn(padsBegin4D_VPU3720), testing::ValuesIn(padsEnd4D_VPU3720),
        testing::ValuesIn(argPadValue_VPU3720), testing::Values(ngraph::helpers::PadMode::CONSTANT),
        testing::ValuesIn(netPrecisions), testing::Values(InferenceEngine::Precision::FP16),
        testing::Values(InferenceEngine::Precision::FP16), testing::Values(InferenceEngine::Layout::NCHW),
        testing::Values(std::vector<size_t>{1, 5, 10, 11}),
        testing::Values(LayerTestsUtils::testPlatformTargetDevice()));

const auto pad4DParams = testing::Combine(
        testing::ValuesIn(padsBegin4D_VPU3720), testing::ValuesIn(padsEnd4D_VPU3720), testing::Values(0),
        testing::ValuesIn(padMode), testing::ValuesIn(netPrecisions), testing::Values(InferenceEngine::Precision::FP16),
        testing::Values(InferenceEngine::Precision::FP16), testing::Values(InferenceEngine::Layout::NCHW),
        testing::Values(std::vector<size_t>{1, 5, 10, 11}),
        testing::Values(LayerTestsUtils::testPlatformTargetDevice()));

const auto precommit_pad4DConstParams = testing::Combine(
        testing::Values(std::vector<int64_t>({4, 2, 1, 3})), testing::Values(std::vector<int64_t>({5, 2, 6, 1})),
        testing::Values(-1.f), testing::Values(ngraph::helpers::PadMode::CONSTANT), testing::ValuesIn(netPrecisions),
        testing::Values(InferenceEngine::Precision::FP16), testing::Values(InferenceEngine::Precision::FP16),
        testing::Values(InferenceEngine::Layout::NCHW), testing::Values(std::vector<size_t>{1, 5, 10, 11}),
        testing::Values(LayerTestsUtils::testPlatformTargetDevice()));

const auto precommit_pad4DParams = testing::Combine(
        testing::Values(std::vector<int64_t>({0, 0, 2, 0})), testing::Values(std::vector<int64_t>({0, 1, 0, 3})),
        testing::Values(0), testing::Values(ngraph::helpers::PadMode::EDGE), testing::ValuesIn(netPrecisions),
        testing::Values(InferenceEngine::Precision::FP16), testing::Values(InferenceEngine::Precision::FP16),
        testing::Values(InferenceEngine::Layout::NCHW), testing::Values(std::vector<size_t>{1, 5, 10, 11}),
        testing::Values(LayerTestsUtils::testPlatformTargetDevice()));

const auto failingNHWC_pad4DConstParams = testing::Combine(
        testing::ValuesIn(padsBegin4D_VPU3720), testing::ValuesIn(padsEnd4D_VPU3720),
        testing::ValuesIn(argPadValue_VPU3720), testing::Values(ngraph::helpers::PadMode::CONSTANT),
        testing::ValuesIn(netPrecisions), testing::Values(InferenceEngine::Precision::FP16),
        testing::Values(InferenceEngine::Precision::FP16), testing::Values(InferenceEngine::Layout::NHWC),
        testing::Values(std::vector<size_t>{1, 5, 10, 11}),
        testing::Values(LayerTestsUtils::testPlatformTargetDevice()));

const auto failingNHWC_pad4DParams = testing::Combine(
        testing::ValuesIn(padsBegin4D_VPU3720), testing::ValuesIn(padsEnd4D_VPU3720), testing::Values(0),
        testing::ValuesIn(padMode), testing::ValuesIn(netPrecisions), testing::Values(InferenceEngine::Precision::FP16),
        testing::Values(InferenceEngine::Precision::FP16), testing::Values(InferenceEngine::Layout::NHWC),
        testing::Values(std::vector<size_t>{1, 5, 10, 11}),
        testing::Values(LayerTestsUtils::testPlatformTargetDevice()));

// VPU3720
INSTANTIATE_TEST_SUITE_P(smoke_Pad_Const, VPUXPadLayerTest_VPU3720, pad4DConstParams,
                         VPUXPadLayerTest_VPU3720::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Pad, VPUXPadLayerTest_VPU3720, pad4DParams, VPUXPadLayerTest_VPU3720::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_precommit_Pad_Const, VPUXPadLayerTest_VPU3720, precommit_pad4DConstParams,
                         VPUXPadLayerTest_VPU3720::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_precommit_Pad, VPUXPadLayerTest_VPU3720, precommit_pad4DParams,
                         VPUXPadLayerTest_VPU3720::getTestCaseName);

// Disabled tests
// Tracking number[E#69804]
INSTANTIATE_TEST_SUITE_P(DISABLED_TMP_smoke_Pad_Const, VPUXPadLayerTest_VPU3720, failingNHWC_pad4DConstParams,
                         VPUXPadLayerTest_VPU3720::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(DISABLED_TMP_smoke_Pad, VPUXPadLayerTest_VPU3720, failingNHWC_pad4DParams,
                         VPUXPadLayerTest_VPU3720::getTestCaseName);

}  // namespace
