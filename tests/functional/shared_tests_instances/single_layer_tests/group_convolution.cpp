//
// Copyright (C) 2022-2023 Intel Corporation
// SPDX-License-Identifier: Apache-2.0
//

#include <vector>

#include <common/functions.h>
#include "common_test_utils/test_constants.hpp"
#include "kmb_layer_test.hpp"
#include "single_layer_tests/group_convolution.hpp"

namespace LayerTestsDefinitions {

class VPUXGroupConvolutionLayerTest :
        public GroupConvolutionLayerTest,
        virtual public LayerTestsUtils::KmbLayerTestsCommon {};
class VPUXGroupConvolutionLayerTest_VPU3700 : public VPUXGroupConvolutionLayerTest {
    void SkipBeforeLoad() override {
    }

    // There is difference between actual and expected values during validation step on board:
    // KmbLayerTestsCommon::Validate()
    // LayerTestsCommon::Validate()
    // openvino/inference-engine/tests/functional/shared_test_classes/include
    // /shared_test_classes/base/layer_test_utils.hpp:173: Failure
    // Value of: max != 0 && (diff <= static_cast<float>(threshold))
    // Actual: false
    // Expected: true
    // Relative comparison of values expected: 446 and actual: 482 at index 0 with
    // threshold 0.0099999997764825821 failed
    // [Track number: S#50873]
    void SkipBeforeValidate() override {
    }

    // [Track number: E#12804]
    void SkipBeforeInfer() override {
        if (envConfig.IE_KMB_TESTS_PLATFORM == "3900") {
            throw LayerTestsUtils::KmbSkipTestException("CallVpu error: -1");
        }
    }
};

using VPUXGroupConvolutionLayerTest_VPU3720_HW = VPUXGroupConvolutionLayerTest;
using VPUXGroupConvolutionLayerTest_VPU3720_SW = VPUXGroupConvolutionLayerTest;

TEST_P(VPUXGroupConvolutionLayerTest_VPU3700, SW) {
    setPlatformVPU3700();
    setReferenceSoftwareModeMLIR();
    Run();
}

TEST_P(VPUXGroupConvolutionLayerTest_VPU3700, HW) {
    setPlatformVPU3700();
    setDefaultHardwareModeMLIR();
    Run();
}

TEST_P(VPUXGroupConvolutionLayerTest_VPU3720_HW, HW) {
    setPlatformVPU3720();
    setDefaultHardwareModeMLIR();
    Run();
}

TEST_P(VPUXGroupConvolutionLayerTest_VPU3720_SW, SW) {
    setPlatformVPU3720();
    setReferenceSoftwareModeMLIR();
    Run();
}

}  // namespace LayerTestsDefinitions

using namespace LayerTestsDefinitions;

namespace {

const std::vector<InferenceEngine::Precision> netPrecisions = {
        InferenceEngine::Precision::FP16,
};

/* ============= 1D GroupConvolution ============= */
const std::vector<std::vector<size_t>> kernels1d = {{3}};
const std::vector<std::vector<size_t>> strides1d = {{1}};
const std::vector<std::vector<ptrdiff_t>> padBegins1d = {{0}, {2}};
const std::vector<std::vector<ptrdiff_t>> padEnds1d = {{0}, {2}};
const std::vector<std::vector<size_t>> dilations1d = {{1}, {2}};
const std::vector<size_t> numOutChannels1d = {8, 16};
const std::vector<size_t> numGroups1d = {2, 8};
const auto inputShapes1d = std::vector<size_t>({1, 16, 30});

const auto groupConv1DParams_ExplicitPadding = ::testing::Combine(
        ::testing::ValuesIn(kernels1d), ::testing::ValuesIn(strides1d), ::testing::ValuesIn(padBegins1d),
        ::testing::ValuesIn(padEnds1d), ::testing::ValuesIn(dilations1d), ::testing::ValuesIn(numOutChannels1d),
        ::testing::ValuesIn(numGroups1d), ::testing::Values(ngraph::op::PadType::EXPLICIT));
const auto groupConv1DParams_AutoPadValid = ::testing::Combine(
        ::testing::ValuesIn(kernels1d), ::testing::ValuesIn(strides1d), ::testing::Values(std::vector<ptrdiff_t>({0})),
        ::testing::Values(std::vector<ptrdiff_t>({0})), ::testing::ValuesIn(dilations1d),
        ::testing::ValuesIn(numOutChannels1d), ::testing::ValuesIn(numGroups1d),
        ::testing::Values(ngraph::op::PadType::VALID));

INSTANTIATE_TEST_SUITE_P(DISABLED_TMP_smoke_GroupConvolution1D_ExplicitPadding, VPUXGroupConvolutionLayerTest_VPU3700,
                         ::testing::Combine(groupConv1DParams_ExplicitPadding, ::testing::ValuesIn(netPrecisions),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(std::vector<size_t>(inputShapes1d)),
                                            ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                         VPUXGroupConvolutionLayerTest_VPU3700::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(DISABLED_TMP_smoke_GroupConvolution1D_AutoPadValid, VPUXGroupConvolutionLayerTest_VPU3700,
                         ::testing::Combine(groupConv1DParams_AutoPadValid, ::testing::ValuesIn(netPrecisions),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(std::vector<size_t>({1, 16, 30})),
                                            ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                         VPUXGroupConvolutionLayerTest_VPU3700::getTestCaseName);

INSTANTIATE_TEST_CASE_P(smoke_GroupConvolution1D_ExplicitPadding_VPU3720, VPUXGroupConvolutionLayerTest_VPU3720_SW,
                        ::testing::Combine(groupConv1DParams_ExplicitPadding, ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>(inputShapes1d)),
                                           ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                        GroupConvolutionLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(smoke_GroupConvolution1D_AutoPadValid_VPU3720, VPUXGroupConvolutionLayerTest_VPU3720_SW,
                        ::testing::Combine(groupConv1DParams_AutoPadValid, ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>(inputShapes1d)),
                                           ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                        GroupConvolutionLayerTest::getTestCaseName);
/* ============= 2D GroupConvolution ============= */
const std::vector<std::vector<size_t>> kernels = {{3, 3}};
const std::vector<std::vector<size_t>> strides = {{1, 1}};
const std::vector<std::vector<ptrdiff_t>> padBegins = {{0, 0}};
const std::vector<std::vector<ptrdiff_t>> padEnds = {{0, 0}};
const std::vector<std::vector<size_t>> dilations = {{1, 1}};
const std::vector<size_t> numOutChannels = {8, 32};
const std::vector<size_t> numGroups = {2, 8};
const auto inputShapes = std::vector<size_t>({1, 32, 30, 30});

const auto groupConv2DParams_ExplicitPadding = ::testing::Combine(
        ::testing::ValuesIn(kernels), ::testing::ValuesIn(strides), ::testing::ValuesIn(padBegins),
        ::testing::ValuesIn(padEnds), ::testing::ValuesIn(dilations), ::testing::ValuesIn(numOutChannels),
        ::testing::ValuesIn(numGroups), ::testing::Values(ngraph::op::PadType::EXPLICIT));
const auto groupConv2DParams_AutoPadValid = ::testing::Combine(
        ::testing::ValuesIn(kernels), ::testing::ValuesIn(strides), ::testing::Values(std::vector<ptrdiff_t>({0, 0})),
        ::testing::Values(std::vector<ptrdiff_t>({0, 0})), ::testing::ValuesIn(dilations),
        ::testing::ValuesIn(numOutChannels), ::testing::ValuesIn(numGroups),
        ::testing::Values(ngraph::op::PadType::VALID));
const auto groupConv2DParams_LargeStrides = ::testing::Combine(
        ::testing::ValuesIn(kernels), ::testing::Values(std::vector<size_t>({9, 9})),
        ::testing::Values(std::vector<ptrdiff_t>({0, 0})), ::testing::Values(std::vector<ptrdiff_t>({0, 0})),
        ::testing::ValuesIn(dilations), ::testing::ValuesIn(numOutChannels), ::testing::ValuesIn(numGroups),
        ::testing::Values(ngraph::op::PadType::VALID));

INSTANTIATE_TEST_SUITE_P(DISABLED_TMP_smoke_GroupConvolution2D_ExplicitPadding, VPUXGroupConvolutionLayerTest_VPU3700,
                         ::testing::Combine(groupConv2DParams_ExplicitPadding, ::testing::ValuesIn(netPrecisions),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(std::vector<size_t>(inputShapes)),
                                            ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                         VPUXGroupConvolutionLayerTest_VPU3700::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_GroupConvolution2D_ExplicitPadding_VPU3720, VPUXGroupConvolutionLayerTest_VPU3720_SW,
                         ::testing::Combine(groupConv2DParams_ExplicitPadding, ::testing::ValuesIn(netPrecisions),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(std::vector<size_t>(inputShapes)),
                                            ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                         GroupConvolutionLayerTest::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(DISABLED_TMP_smoke_GroupConvolution2D_AutoPadValid, VPUXGroupConvolutionLayerTest_VPU3700,
                         ::testing::Combine(groupConv2DParams_AutoPadValid, ::testing::ValuesIn(netPrecisions),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(std::vector<size_t>({1, 16, 30, 30})),
                                            ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                         VPUXGroupConvolutionLayerTest_VPU3700::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_GroupConvolution2D_AutoPadValid_VPU3720, VPUXGroupConvolutionLayerTest_VPU3720_SW,
                         ::testing::Combine(groupConv2DParams_AutoPadValid, ::testing::ValuesIn(netPrecisions),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(std::vector<size_t>({1, 16, 30, 30})),
                                            ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                         GroupConvolutionLayerTest::getTestCaseName);

INSTANTIATE_TEST_CASE_P(DISABLED_TMP_smoke_GroupConvolution2D_LargeStrides, VPUXGroupConvolutionLayerTest_VPU3700,
                        ::testing::Combine(groupConv2DParams_LargeStrides, ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::FP16),
                                           ::testing::Values(InferenceEngine::Precision::FP16),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>({1, 16, 30, 30})),
                                           ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                        VPUXGroupConvolutionLayerTest_VPU3700::getTestCaseName);

INSTANTIATE_TEST_CASE_P(smoke_GroupConvolution2D_LargeStrides_VPU3720, VPUXGroupConvolutionLayerTest_VPU3720_SW,
                        ::testing::Combine(groupConv2DParams_LargeStrides, ::testing::ValuesIn(netPrecisions),
                                           ::testing::Values(InferenceEngine::Precision::FP16),
                                           ::testing::Values(InferenceEngine::Precision::FP16),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(InferenceEngine::Layout::ANY),
                                           ::testing::Values(std::vector<size_t>({1, 16, 30, 30})),
                                           ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                        GroupConvolutionLayerTest::getTestCaseName);

/* ============= 3D GroupConvolution ============= */
const std::vector<std::vector<size_t>> kernels3d = {{3, 3, 3}};
const std::vector<std::vector<ptrdiff_t>> paddings3d = {{0, 0, 0}};
const std::vector<std::vector<size_t>> strides3d = {{1, 1, 1}};
const std::vector<std::vector<size_t>> dilations3d = {{1, 1, 1}};
const auto inputShapes3d = std::vector<size_t>({1, 4, 10, 10, 10});

const auto groupConv3DParams_ExplicitPadding = ::testing::Combine(
        ::testing::ValuesIn(kernels3d), ::testing::ValuesIn(strides3d), ::testing::ValuesIn(paddings3d),
        ::testing::ValuesIn(paddings3d), ::testing::ValuesIn(dilations3d), ::testing::Values(4), ::testing::Values(2),
        ::testing::Values(ngraph::op::PadType::EXPLICIT));
const auto groupConv3DParams_AutoPadValid =
        ::testing::Combine(::testing::ValuesIn(kernels3d), ::testing::ValuesIn(strides3d),
                           ::testing::Values(std::vector<ptrdiff_t>({0, 0, 0})),
                           ::testing::Values(std::vector<ptrdiff_t>({0, 0, 0})), ::testing::ValuesIn(dilations3d),
                           ::testing::Values(4), ::testing::Values(2), ::testing::Values(ngraph::op::PadType::VALID));

// [Track number: S#50872]
INSTANTIATE_TEST_SUITE_P(DISABLED_smoke_GroupConvolution3D_ExplicitPadding, VPUXGroupConvolutionLayerTest_VPU3700,
                         ::testing::Combine(groupConv3DParams_ExplicitPadding, ::testing::ValuesIn(netPrecisions),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(std::vector<size_t>(inputShapes3d)),
                                            ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                         VPUXGroupConvolutionLayerTest_VPU3700::getTestCaseName);

// [Track number: S#50872]
INSTANTIATE_TEST_SUITE_P(DISABLED_smoke_GroupConvolution3D_AutoPadValid, VPUXGroupConvolutionLayerTest_VPU3700,
                         ::testing::Combine(groupConv3DParams_AutoPadValid, ::testing::ValuesIn(netPrecisions),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Precision::UNSPECIFIED),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(InferenceEngine::Layout::ANY),
                                            ::testing::Values(std::vector<size_t>({1, 4, 10, 10, 10})),
                                            ::testing::Values(LayerTestsUtils::testPlatformTargetDevice)),
                         VPUXGroupConvolutionLayerTest_VPU3700::getTestCaseName);

}  // namespace
