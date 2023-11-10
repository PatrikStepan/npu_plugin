//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vcl_tests_common.h"

#include <regex>

namespace VCLTestsUtils {

vcl_result_t VCLTestsCommon::initModelData(const char* netName, const char* weightName) {
    vcl_result_t ret = VCL_RESULT_SUCCESS;

    vcl_version_info_t version;
    vcl_compiler_desc_t compilerDesc = {VCL_PLATFORM_VPU3720, VCL_LOG_ERROR};
    vcl_compiler_handle_t compiler = nullptr;
    ret = vclCompilerCreate(compilerDesc, &compiler, nullptr);
    if (ret) {
        std::cerr << "Failed to create compiler! Result:0x" << std::hex << uint64_t(ret) << std::dec << std::endl;
        return ret;
    }

    vcl_compiler_properties_t compilerProp;
    ret = vclCompilerGetProperties(compiler, &compilerProp);
    if (ret) {
        std::cerr << "Failed to query compiler props! Result:0x" << std::hex << uint64_t(ret) << std::dec << std::endl;
        vclCompilerDestroy(compiler);
        return ret;
    } else {
        version.major = compilerProp.version.major;
        version.minor = compilerProp.version.minor;
        vclCompilerDestroy(compiler);
    }

    /// Read buffer, add net.xml
    std::ifstream xmlFileStream(netName, std::ios::binary);
    if (!xmlFileStream.is_open()) {
        std::cerr << "Cannot open file " << netName << std::endl;
        return VCL_RESULT_ERROR_IO;
    }
    xmlFileStream.seekg(0, xmlFileStream.end);
    uint64_t xmlSize = xmlFileStream.tellg();
    xmlFileStream.seekg(0, xmlFileStream.beg);

    /// Read weights size, add weight.bin
    std::ifstream weightFileStream(weightName, std::ios::binary);

    if (!weightFileStream.is_open()) {
        std::cerr << "Cannot open file " << weightName << std::endl;
        return VCL_RESULT_ERROR_IO;
    }

    weightFileStream.seekg(0, weightFileStream.end);
    uint64_t weightsSize = weightFileStream.tellg();

    /// Prepare modelIRData for compiler
    uint32_t numberOfInputData = 2;
    modelIRSize =
            sizeof(version) + sizeof(numberOfInputData) + sizeof(xmlSize) + xmlSize + sizeof(weightsSize) + weightsSize;
    modelIR.resize(modelIRSize);

    uint64_t offset = 0;
    memcpy(modelIR.data(), &version, sizeof(version));
    offset += sizeof(version);
    memcpy(modelIR.data() + offset, &numberOfInputData, sizeof(numberOfInputData));
    offset += sizeof(numberOfInputData);
    memcpy(modelIR.data() + offset, &xmlSize, sizeof(xmlSize));
    offset += sizeof(xmlSize);
    uint8_t* xmlData = modelIR.data() + offset;
    xmlFileStream.read(reinterpret_cast<char*>(xmlData), xmlSize);

    if (xmlFileStream.fail()) {
        std::cerr << "Short read on network buffer!" << std::endl;
        return VCL_RESULT_ERROR_IO;
    }

    offset += xmlSize;
    memcpy(modelIR.data() + offset, &weightsSize, sizeof(weightsSize));
    offset += sizeof(weightsSize);
    uint8_t* weights = nullptr;
    if (weightsSize != 0) {
        weights = modelIR.data() + offset;
        weightFileStream.seekg(0, weightFileStream.beg);
        weightFileStream.read(reinterpret_cast<char*>(weights), weightsSize);
        if (weightFileStream.fail()) {
            std::cerr << "Short read on weights file!" << std::endl;
            return VCL_RESULT_ERROR_IO;
        }
    }
    return VCL_RESULT_SUCCESS;
}

std::shared_ptr<ngraph::Function> VCLTestsCommon::createSimpleFunction() {
    /**
     * This example shows how to create ngraph::Function
     * Parameter--->Multiply--->Add--->Result
     *     Constant---'          /
     *              Constant---'
     */

    /// Create opset3::Parameter operation with static shape
    auto data = std::make_shared<ngraph::opset3::Parameter>(ngraph::element::f16, ngraph::Shape{3, 2});

    auto mul_constant = ngraph::opset3::Constant::create(ngraph::element::f16, ngraph::Shape{1}, {1.5});
    auto mul = std::make_shared<ngraph::opset3::Multiply>(data, mul_constant);

    auto add_constant = ngraph::opset3::Constant::create(ngraph::element::f16, ngraph::Shape{1}, {0.5});
    auto add = std::make_shared<ngraph::opset3::Add>(mul, add_constant);

    /// Create opset3::Result operation
    auto res = std::make_shared<ngraph::opset3::Result>(mul);

    /// Create nGraph function
    return std::make_shared<ngraph::Function>(ngraph::ResultVector{std::move(res)},
                                              ngraph::ParameterVector{std::move(data)});
}

std::string VCLTestsCommon::getTestModelsBasePath() {
    /// User shall set POR_PATH to show the location of model pacakge
    if (const auto envVar = std::getenv("POR_PATH")) {
        return envVar;
    }
    return {};
}

std::string VCLTestsCommon::getCidToolPath() {
    /// User shall set CID_TOOL to show the location of config files
    if (const auto envVar = std::getenv("CID_TOOL")) {
        return envVar;
    }
    std::cerr << "CID_TOOL empty! You need to export CID_TOOL to load config!" << std::endl;
    return {};
}

void VCLTestsCommon::postProcessNetOptions(const std::string& device) {
    netOptions += std::string{" NPU_PLATFORM=\""} + device + std::string{"\""};
    netOptions += std::string{" DEVICE_ID=\""} + std::string{"NPU."} + device + std::string{"\""};
}

IRInfoTestType VCLTestsCommon::readJson2Vec(std::string fileName) {
    /// The map that contains model name and it configuration
    IRInfoTestType irInfos;
    std::ifstream configFile(fileName);
    std::string line;
    while (std::getline(configFile, line)) {
        std::unordered_map<std::string, std::string> irInfo;
        auto res = VCLTestsUtils::Json::parse(line);
        for (auto it = res.begin(); it != res.end(); ++it) {
            irInfo.emplace(it.key(), it.value());
        }

        /// If the test config is enabled
        bool enabled;
        auto find = irInfo.find("enabled");
        std::istringstream valueOfEnabled;
        if (find != irInfo.end()) {
            /// Store the user setting
            valueOfEnabled.str(find->second);
        } else {
            /// Skip this test config by default
            std::cout << "Not found enabled entry, using false as its default\n";
            valueOfEnabled.str("false");
        }
        valueOfEnabled >> std::boolalpha >> enabled;

        if (enabled) {
            /// Only add a config to test if we enable it in config file
            irInfos.push_back(irInfo);
        }
    }
    return irInfos;
}

void VCLTestsCommon::SetUp() {
    auto ir = GetParam();
    auto netInfo = std::get<0>(ir);
    /// Model name
    const std::string net = netInfo.at("network");
    /// Model build flags
    netOptions = netInfo.at("info");
    /// Add device info to build flags
    postProcessNetOptions(netInfo.at("device"));
    if (net != "simple_function") {
        /// Load model data from disk
        std::string PATH = getTestModelsBasePath();
        if (PATH.empty()) {
            GTEST_SKIP() << "POR_PATH is empty. Skipping this test";
        }
        /// The relative path of model to model package
        const std::string interPath = netInfo.at("path");
        std::string netName = PATH + "/" + interPath + "/" + net + ".xml";
        std::string weightName = PATH + "/" + interPath + "/" + net + ".bin";
        std::cout << "netName:" << netName << " weightName:" << weightName << std::endl;

#if defined(_WIN32)
        bool netExist = PathFileExists(netName.c_str());
        EXPECT_EQ(netExist, true) << "The network " << netName.c_str() << " does not exist!" << std::endl;
#else
        struct stat buffer;
        int netExist = stat(netName.c_str(), &buffer);
        EXPECT_EQ(netExist, 0) << "The network " << netName.c_str() << " does not exist!" << std::endl;
#endif

        EXPECT_EQ(initModelData(netName.c_str(), weightName.c_str()), VCL_RESULT_SUCCESS);
    } else {
        auto function = createSimpleFunction();
        InferenceEngine::CNNNetwork cnnSimpleNet(function);
        auto inputs = cnnSimpleNet.getInputsInfo();
        auto outputs = cnnSimpleNet.getOutputsInfo();

        // Get input/output names dynamically.
        // In VCLTestsCommon::createSimpleFunction, the created simple network only has one
        // input/output, so no need to iterate through iterators.
        std::string inputName = inputs.begin()->first;
        std::string outputName = outputs.begin()->first;

        // Update netOptions by replacing input/output placeholders with dynamic names.
        netOptions = std::regex_replace(netOptions, std::regex("SIMPLE_IN_PLACEHOLDER"), inputName);
        netOptions = std::regex_replace(netOptions, std::regex("SIMPLE_OUT_PLACEHOLDER"), outputName);

        std::string mOutXmlPath = "./simple_func_multiple.xml";
        std::string mOutBinPath = "./simple_func_multiple.bin";

        ngraph::pass::Manager passManager;
        passManager.register_pass<ngraph::pass::Serialize>(mOutXmlPath, mOutBinPath);
        passManager.run_passes(std::move(function));

        EXPECT_EQ(initModelData(mOutXmlPath.c_str(), mOutBinPath.c_str()), VCL_RESULT_SUCCESS);

        int xmlStatus = std::remove(mOutXmlPath.c_str());
        int binStatus = std::remove(mOutBinPath.c_str());
        if (xmlStatus == 0 && binStatus == 0) {
            std::cout << "Temp IRs" << mOutXmlPath << " and " << mOutBinPath << " deleting succeeded!" << std::endl;
        } else {
            std::cerr << "Temp IRs" << mOutXmlPath << " and " << mOutBinPath << " deleting failed!" << std::endl;
        }
    }
}

}  // namespace VCLTestsUtils
