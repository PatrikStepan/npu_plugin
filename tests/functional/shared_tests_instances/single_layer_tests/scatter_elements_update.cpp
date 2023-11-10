//
// Copyright (C) Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "single_layer_tests/scatter_elements_update.hpp"
#include <vector>

#include "vpu_ov1_layer_test.hpp"

namespace LayerTestsDefinitions {

class VPUXScatterElementsUpdateLayerTest :
        public ScatterElementsUpdateLayerTest,
        virtual public LayerTestsUtils::VpuOv1LayerTestsCommon {};

class VPUXScatterElementsUpdateLayerTest_VPU3720 : public VPUXScatterElementsUpdateLayerTest {};

TEST_P(VPUXScatterElementsUpdateLayerTest_VPU3720, HW) {
    setPlatformVPU3720();
    setDefaultHardwareModeMLIR();
    Run();
}

}  // namespace LayerTestsDefinitions

using namespace LayerTestsDefinitions;

namespace {

std::map<std::vector<size_t>, std::map<std::vector<size_t>, std::vector<int>>> axesShapeInShape{
        {{2, 3, 4}, {{{1, 3, 1}, {1, -1}}}}};

const std::vector<std::vector<size_t>> indicesValue = {{1, 0, 1}};

INSTANTIATE_TEST_SUITE_P(
        smoke_ScatterElementsUpdate, VPUXScatterElementsUpdateLayerTest_VPU3720,
        testing::Combine(testing::ValuesIn(ScatterElementsUpdateLayerTest::combineShapes(axesShapeInShape)),
                         testing::ValuesIn(indicesValue), testing::Values(InferenceEngine::Precision::FP16),
                         testing::Values(InferenceEngine::Precision::I32),
                         testing::Values(LayerTestsUtils::testPlatformTargetDevice())),
        VPUXScatterElementsUpdateLayerTest_VPU3720::getTestCaseName);

}  // namespace
