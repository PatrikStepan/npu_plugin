//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "single_layer_tests/ctc_greedy_decoder.hpp"

#include <vector>

#include <common/functions.h>
#include "vpu_ov1_layer_test.hpp"

namespace LayerTestsDefinitions {

class VPUXCTCGreedyDecoderLayerTest :
        public CTCGreedyDecoderLayerTest,
        virtual public LayerTestsUtils::VpuOv1LayerTestsCommon {
    // [Track number: E#83855]
    void SkipBeforeLoad() override {
        const std::vector<InferenceEngine::SizeVector> badInputShapesForMLIR = {InferenceEngine::SizeVector{50, 3, 3},
                                                                                InferenceEngine::SizeVector{50, 3, 128},
                                                                                InferenceEngine::SizeVector{10, 1, 16}};

        InferenceEngine::SizeVector inShape;
        std::tie(std::ignore, std::ignore, std::ignore, std::ignore, std::ignore, inShape, std::ignore, std::ignore) =
                GetParam();
        for (auto iter = badInputShapesForMLIR.cbegin(); iter != badInputShapesForMLIR.cend(); iter++) {
            if (inShape == *iter) {
                throw LayerTestsUtils::VpuSkipTestException("Comparison fails");
            }
        }
    }
};

class VPUXCTCGreedyDecoderLayerTest_VPU3700 : public VPUXCTCGreedyDecoderLayerTest {};

class VPUXCTCGreedyDecoderLayerTest_VPU3720 : public VPUXCTCGreedyDecoderLayerTest {};

TEST_P(VPUXCTCGreedyDecoderLayerTest_VPU3700, HW) {
    setPlatformVPU3700();
    setDefaultHardwareModeMLIR();
    Run();
}

TEST_P(VPUXCTCGreedyDecoderLayerTest_VPU3720, HW) {
    setPlatformVPU3720();
    setDefaultHardwareModeMLIR();
    Run();
}

}  // namespace LayerTestsDefinitions

using namespace ngraph::helpers;
using namespace LayerTestsDefinitions;

namespace {

const std::vector<InferenceEngine::Precision> netPrecisions = {
        InferenceEngine::Precision::FP32,  // Testing FP32/FP16 netPrecision functionality only for small scope of
        InferenceEngine::Precision::FP16   // tests: VPUXGRNLayerTest, VPUXSplitLayerTest, VPUXCTCGreedyDecoderLayerTest
};

const std::vector<bool> mergeRepeated = {true, false};

// Only batch = 1 is supported
const std::vector<InferenceEngine::SizeVector> inputShapes_MLIR = {
        InferenceEngine::SizeVector{88, 1, 71}, InferenceEngine::SizeVector{10, 1, 16},
        InferenceEngine::SizeVector{50, 3, 3}, InferenceEngine::SizeVector{50, 3, 128},
        InferenceEngine::SizeVector{1, 1, 16}};

const auto params_MLIR = testing::Combine(
        testing::ValuesIn(netPrecisions), testing::Values(InferenceEngine::Precision::UNSPECIFIED),
        testing::Values(InferenceEngine::Precision::UNSPECIFIED), testing::Values(InferenceEngine::Layout::ANY),
        testing::Values(InferenceEngine::Layout::ANY), testing::ValuesIn(inputShapes_MLIR),
        testing::ValuesIn(mergeRepeated), testing::Values(LayerTestsUtils::testPlatformTargetDevice()));

INSTANTIATE_TEST_SUITE_P(DISABLED_TMP_smoke_CTCGreedyDecoder, VPUXCTCGreedyDecoderLayerTest_VPU3700, params_MLIR,
                         CTCGreedyDecoderLayerTest::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_CTCGreedyDecoder, VPUXCTCGreedyDecoderLayerTest_VPU3720, params_MLIR,
                         CTCGreedyDecoderLayerTest::getTestCaseName);

}  // namespace
