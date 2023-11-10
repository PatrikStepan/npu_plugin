//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include <signal.h>
#include <functional_test_utils/summary/op_summary.hpp>
#include <iostream>
#include <sstream>
#include <vpux/utils/core/logger.hpp>
#include "gtest/gtest.h"
#include "vpu_test_report.hpp"
#include "vpu_test_tool.hpp"
#include "vpux/utils/IE/config.hpp"
#include "vpux/vpux_metrics.hpp"

// Headers below are just for Yocto.
#ifdef __aarch64__
#include <stdlib.h>
#include <unistd.h>
#include <exception>
#endif

// Handler for signal SIGINT. Just for running on Yocto.
#ifdef __aarch64__
void sigint_handler(int num) {
    std::cerr << "\nSIGINT signal (Ctrl+C) has been gotten.\n";
    std::cerr << "Exit from program.\n";
    std::cerr << "You may check open/close channels in XLinkUtils.\n";
    exit(1);
}
#endif

namespace testing {
namespace internal {
extern bool g_help_flag;
}  // namespace internal
}  // namespace testing

void sigsegv_handler(int errCode) {
    auto& s = ov::test::utils::OpSummary::getInstance();
    s.saveReport();
    std::cerr << "Unexpected application crash with code: " << errCode << std::endl;
    std::abort();
}

int main(int argc, char** argv, char** envp) {
// Register handler for signal SIGINT. Just for running on Yocto.
#ifdef __aarch64__
    struct sigaction act;
    sigemptyset(&act.sa_mask);
    act.sa_handler = &sigint_handler;
    act.sa_flags = 0;

    if (sigaction(SIGINT, &act, NULL) == -1) {
        std::cerr << "sigaction() error - can't register handler for SIGINT.\n";
    }
#endif

    // register crashHandler for SIGSEGV signal
    signal(SIGSEGV, sigsegv_handler);

    std::ostringstream oss;
    oss << "Command line args (" << argc << "): ";
    for (int c = 0; c < argc; ++c) {
        oss << " " << argv[c];
    }
    oss << std::endl;

    oss << "Process id: " << getpid() << std::endl;
    std::cout << oss.str();
    oss.str("");

    oss << "Environment variables: ";
    for (char** env = envp; *env != 0; env++) {
        oss << *env << "; ";
    }

    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new LayerTestsUtils::VpuTestReportEnvironment());

    const bool dryRun = ::testing::GTEST_FLAG(list_tests) || ::testing::internal::g_help_flag;

    if (!dryRun) {
        const std::string noFetch{"<not fetched>"};
        std::string backend{noFetch}, arch{noFetch}, full{noFetch};
        try {
            LayerTestsUtils::VpuTestTool kmbTestTool(LayerTestsUtils::VpuTestEnvConfig::getInstance());
            backend = kmbTestTool.getDeviceMetric(VPUX_METRIC_KEY(BACKEND_NAME));
            arch = kmbTestTool.getDeviceMetric(METRIC_KEY(DEVICE_ARCHITECTURE));
            full = kmbTestTool.getDeviceMetric(METRIC_KEY(FULL_DEVICE_NAME));
        } catch (const std::exception& e) {
            std::cerr << "Exception while trying to determine device characteristics: " << e.what() << std::endl;
        }
        std::cout << "Tests run with: Backend name: '" << backend << "'; Device arch: '" << arch
                  << "'; Full device name: '" << full << "'" << std::endl;
    }

    std::string dTest = ::testing::internal::GTEST_FLAG(internal_run_death_test);
    if (dTest.empty()) {
        std::cout << oss.str() << std::endl;
    } else {
        std::cout << "gtest death test process is running" << std::endl;
    }

    auto& log = vpux::Logger::global();
    auto& level = LayerTestsUtils::VpuTestEnvConfig::getInstance().IE_NPU_TESTS_LOG_LEVEL;
    log.setLevel(level.empty() ? vpux::LogLevel::Info : vpux::OptionParser<vpux::LogLevel>::parse(level));

    return RUN_ALL_TESTS();
}
