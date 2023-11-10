//
// Copyright (C) 2018-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "behavior/plugin/configuration_tests.hpp"
#include "vpu_test_env_cfg.hpp"
#include "vpux/al/config/common.hpp"

using namespace BehaviorTestsDefinitions;
namespace {
INSTANTIATE_TEST_SUITE_P(
        smoke_Basic, DefaultConfigurationTest,
        ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_KEEMBAY),
                           ::testing::Values(DefaultParameter{
                                   InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS,
                                   InferenceEngine::Parameter{std::string{"1"}}})),
        DefaultConfigurationTest::getTestCaseName);

IE_SUPPRESS_DEPRECATED_START
auto inconfigs = []() {
    return std::vector<std::map<std::string, std::string>>{
            {{InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, "DOESN'T EXIST"}},
            {{InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::LATENCY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "-1"}},
            {{InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT,
              InferenceEngine::PluginConfigParams::THROUGHPUT},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "should be int"}},
            {{InferenceEngine::PluginConfigParams::KEY_PERF_COUNT, "ON"}},
            {{InferenceEngine::PluginConfigParams::KEY_CONFIG_FILE, "unknown_file"}},
            {{InferenceEngine::PluginConfigParams::KEY_DEVICE_ID, "DEVICE_UNKNOWN"}}};
};

auto multiinconfigs = []() {
    return std::vector<std::map<std::string, std::string>>{
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, "DOESN'T EXIST"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::LATENCY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "-1"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERF_COUNT, "ON"}}};
};

auto autoinconfigs = []() {
    return std::vector<std::map<std::string, std::string>>{
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, "DOESN'T EXIST"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::LATENCY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "-1"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERF_COUNT, "ON"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_CONFIG_FILE, "unknown_file"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_DEVICE_ID, "DEVICE_UNKNOWN"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_LOG_LEVEL, "NAN"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_MODEL_PRIORITY, "-1"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_MODEL_PRIORITY, "ABC"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, "DOESN'T EXIST"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::LATENCY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "-1"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_PERF_COUNT, "ON"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_CONFIG_FILE, "unknown_file"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_DEVICE_ID, "DEVICE_UNKNOWN"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_LOG_LEVEL, "NAN"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_MODEL_PRIORITY, "-1"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_MODEL_PRIORITY, "ABC"}}};
};

auto auto_batch_inconfigs = []() {
    return std::vector<std::map<std::string, std::string>>{
            {{CONFIG_KEY(AUTO_BATCH_DEVICE_CONFIG), CommonTestUtils::DEVICE_KEEMBAY},
             {CONFIG_KEY(AUTO_BATCH_TIMEOUT), "-1"}},
            {{CONFIG_KEY(AUTO_BATCH_DEVICE_CONFIG), CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, "DOESN'T EXIST"}},
            {{CONFIG_KEY(AUTO_BATCH_DEVICE_CONFIG), CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::LATENCY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "-1"}},
            {{CONFIG_KEY(AUTO_BATCH_DEVICE_CONFIG), CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERF_COUNT, "ON"}},
            {{CONFIG_KEY(AUTO_BATCH_DEVICE_CONFIG), CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_CONFIG_FILE, "unknown_file"}},
            {{CONFIG_KEY(AUTO_BATCH_DEVICE_CONFIG), CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_DEVICE_ID, "DEVICE_UNKNOWN"}}};
};

IE_SUPPRESS_DEPRECATED_END

INSTANTIATE_TEST_SUITE_P(smoke_BehaviorTests, IncorrectConfigTests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_KEEMBAY),
                                            ::testing::ValuesIn(inconfigs())),
                         IncorrectConfigTests::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Multi_BehaviorTests, IncorrectConfigTests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_MULTI),
                                            ::testing::ValuesIn(multiinconfigs())),
                         IncorrectConfigTests::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Auto_BehaviorTests, IncorrectConfigTests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_AUTO),
                                            ::testing::ValuesIn(autoinconfigs())),
                         IncorrectConfigTests::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_AutoBatch_BehaviorTests, IncorrectConfigTests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_BATCH),
                                            ::testing::ValuesIn(auto_batch_inconfigs())),
                         IncorrectConfigTests::getTestCaseName);

const std::vector<std::map<std::string, std::string>> conf = {{}};

auto autoConfigs = []() {
    return std::vector<std::map<std::string, std::string>>{
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT,
              InferenceEngine::PluginConfigParams::THROUGHPUT}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::LATENCY}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::LATENCY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "1"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_MODEL_PRIORITY,
              InferenceEngine::PluginConfigParams::MODEL_PRIORITY_HIGH}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_MODEL_PRIORITY,
              InferenceEngine::PluginConfigParams::MODEL_PRIORITY_MED}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_MODEL_PRIORITY,
              InferenceEngine::PluginConfigParams::MODEL_PRIORITY_LOW}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT,
              InferenceEngine::PluginConfigParams::THROUGHPUT}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::LATENCY}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::LATENCY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "1"}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_LOG_LEVEL, InferenceEngine::PluginConfigParams::LOG_NONE}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_LOG_LEVEL, InferenceEngine::PluginConfigParams::LOG_ERROR}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_LOG_LEVEL, InferenceEngine::PluginConfigParams::LOG_WARNING}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_LOG_LEVEL, InferenceEngine::PluginConfigParams::LOG_INFO}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_LOG_LEVEL, InferenceEngine::PluginConfigParams::LOG_DEBUG}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_LOG_LEVEL, InferenceEngine::PluginConfigParams::LOG_TRACE}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_MODEL_PRIORITY,
              InferenceEngine::PluginConfigParams::MODEL_PRIORITY_HIGH}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_MODEL_PRIORITY,
              InferenceEngine::PluginConfigParams::MODEL_PRIORITY_MED}},
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES,
              CommonTestUtils::DEVICE_KEEMBAY + std::string(",") + CommonTestUtils::DEVICE_CPU},
             {InferenceEngine::PluginConfigParams::KEY_MODEL_PRIORITY,
              InferenceEngine::PluginConfigParams::MODEL_PRIORITY_LOW}}};
};

auto auto_batch_configs = []() {
    return std::vector<std::map<std::string, std::string>>{
            {{CONFIG_KEY(AUTO_BATCH_DEVICE_CONFIG), CommonTestUtils::DEVICE_KEEMBAY}},
            {{CONFIG_KEY(AUTO_BATCH_DEVICE_CONFIG), CommonTestUtils::DEVICE_KEEMBAY},
             {CONFIG_KEY(AUTO_BATCH_TIMEOUT), "1"}}};
};

static std::string getTestCaseName(testing::TestParamInfo<CorrectConfigParams> obj) {
    return CorrectConfigTests::getTestCaseName(obj) +
           "_targetPlatform=" + LayerTestsUtils::getTestsPlatformFromEnvironmentOr(CommonTestUtils::DEVICE_KEEMBAY);
}

INSTANTIATE_TEST_SUITE_P(smoke_BehaviorTests, DefaultValuesConfigTests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_KEEMBAY),
                                            ::testing::ValuesIn(conf)),
                         getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_BehaviorTests, IncorrectConfigAPITests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_KEEMBAY),
                                            ::testing::ValuesIn(inconfigs())),
                         getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Multi_BehaviorTests, IncorrectConfigAPITests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_MULTI),
                                            ::testing::ValuesIn(multiinconfigs())),
                         getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Auto_BehaviorTests, IncorrectConfigAPITests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_AUTO),
                                            ::testing::ValuesIn(autoinconfigs())),
                         getTestCaseName);
INSTANTIATE_TEST_SUITE_P(smoke_AutoBatch_BehaviorTests, IncorrectConfigAPITests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_BATCH),
                                            ::testing::ValuesIn(auto_batch_inconfigs())),
                         getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_AutoBatch_BehaviorTests, CorrectConfigTests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_BATCH),
                                            ::testing::ValuesIn(auto_batch_configs())),
                         getTestCaseName);

const std::vector<std::map<std::string, std::string>> vpu_prop_config = {{
        {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::THROUGHPUT},
        {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "8"},
        {InferenceEngine::PluginConfigParams::KEY_PERF_COUNT, InferenceEngine::PluginConfigParams::NO},
}};

const std::vector<std::map<std::string, std::string>> vpu_loadNetWork_config = {{
        {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::LATENCY},
        //{InferenceEngine::PluginConfigParams::KEY_EXCLUSIVE_ASYNC_REQUESTS,
        // InferenceEngine::PluginConfigParams::NO},
        {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "10"},
        {InferenceEngine::PluginConfigParams::KEY_PERF_COUNT, InferenceEngine::PluginConfigParams::YES},
}};

auto auto_multi_prop_config = []() {
    return std::vector<std::map<std::string, std::string>>{
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT,
              InferenceEngine::PluginConfigParams::THROUGHPUT},
             //{InferenceEngine::PluginConfigParams::KEY_EXCLUSIVE_ASYNC_REQUESTS,
             // InferenceEngine::PluginConfigParams::YES},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "2"},
             {InferenceEngine::PluginConfigParams::KEY_ALLOW_AUTO_BATCHING, InferenceEngine::PluginConfigParams::NO},
             {InferenceEngine::PluginConfigParams::KEY_PERF_COUNT, InferenceEngine::PluginConfigParams::NO}}};
};

auto auto_multi_loadNetWork_config = []() {
    return std::vector<std::map<std::string, std::string>>{
            {{InferenceEngine::MultiDeviceConfigParams::KEY_MULTI_DEVICE_PRIORITIES, CommonTestUtils::DEVICE_KEEMBAY},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT, InferenceEngine::PluginConfigParams::LATENCY},
             //{InferenceEngine::PluginConfigParams::KEY_EXCLUSIVE_ASYNC_REQUESTS,
             // InferenceEngine::PluginConfigParams::NO},
             {InferenceEngine::PluginConfigParams::KEY_PERFORMANCE_HINT_NUM_REQUESTS, "10"},
             {InferenceEngine::PluginConfigParams::KEY_ALLOW_AUTO_BATCHING, InferenceEngine::PluginConfigParams::YES},
             {InferenceEngine::PluginConfigParams::KEY_PERF_COUNT, InferenceEngine::PluginConfigParams::YES}}};
};

INSTANTIATE_TEST_SUITE_P(smoke_BehaviorTests, SetPropLoadNetWorkGetPropTests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_KEEMBAY),
                                            ::testing::ValuesIn(vpu_prop_config),
                                            ::testing::ValuesIn(vpu_loadNetWork_config)),
                         SetPropLoadNetWorkGetPropTests::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Multi_BehaviorTests, SetPropLoadNetWorkGetPropTests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_MULTI),
                                            ::testing::ValuesIn(auto_multi_prop_config()),
                                            ::testing::ValuesIn(auto_multi_loadNetWork_config())),
                         SetPropLoadNetWorkGetPropTests::getTestCaseName);

INSTANTIATE_TEST_SUITE_P(smoke_Auto_BehaviorTests, SetPropLoadNetWorkGetPropTests,
                         ::testing::Combine(::testing::Values(CommonTestUtils::DEVICE_AUTO),
                                            ::testing::ValuesIn(auto_multi_prop_config()),
                                            ::testing::ValuesIn(auto_multi_loadNetWork_config())),
                         SetPropLoadNetWorkGetPropTests::getTestCaseName);
}  // namespace
