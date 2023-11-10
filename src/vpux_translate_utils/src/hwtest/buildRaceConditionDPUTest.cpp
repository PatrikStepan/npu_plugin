//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include <numeric>

#include <mlir/Dialect/Quant/QuantTypes.h>

#include "vpux/compiler/dialect/VPU/nce_sparsity.hpp"
#include "vpux/compiler/dialect/VPU/passes.hpp"
#include "vpux/compiler/dialect/VPUIP/ops.hpp"
#include "vpux/compiler/dialect/VPUIP/passes.hpp"
#include "vpux/compiler/dialect/VPURT/ops.hpp"
#include "vpux/compiler/dialect/VPURT/task.hpp"
#include "vpux/compiler/dialect/const/ops.hpp"
#include "vpux/hwtest/hwtest_utils.hpp"
#include "vpux/hwtest/test_case_json_parser.hpp"
#include "vpux/utils/core/error.hpp"

namespace vpux {
namespace hwtest {

//
//             [input]
//                |
//            (barrier)
//            |       |
//         (conv)    (conv)
//            |       |
//            (barrier)
//            |       |
//        ... (loop with conv ops and barriers)
//            |       |
//         (conv)    (conv)
//            |       |
//            (barrier)
//            |       |
//       [output0]  [output1]
//

void buildRaceConditionDPUTest(const nb::TestCaseJsonDescriptor& testDesc, mlir::ModuleOp module,
                               mlir::OpBuilder builder, Logger& log, mlir::Type inputType, mlir::Type weightsType,
                               mlir::Type outputType) {
    auto* ctx = builder.getContext();
    auto loc = builder.getUnknownLoc();
    const auto int32 = builder.getIntegerType(32, true);

    const auto input = testDesc.getInputLayerList().front();
    const auto weights = testDesc.getWeightLayers().front();
    const auto conv = testDesc.getConvLayer();
    const auto output = testDesc.getOutputLayers().front();
    const auto iterationCount = testDesc.getIterationCount();
    const auto numClusters = testDesc.getNumClusters();

    const SmallVector<std::int64_t> inputShape{input.shape.begin(), input.shape.end()};
    const SmallVector<std::int64_t> outputShape{output.shape.begin(), output.shape.end()};
    const SmallVector<std::int64_t> weightsShape{weights.shape.begin(), weights.shape.end()};
    const SmallVector<std::int64_t> weightsTableShape{weightsShape[0], 1, 1, 4};

    VPUX_THROW_UNLESS(!inputShape.empty(), "buildRaceConditionDPUTest: Got empty inputShape");
    VPUX_THROW_UNLESS(!outputShape.empty(), "buildRaceConditionDPUTest: Got empty outputShape");
    VPUX_THROW_UNLESS(!weightsShape.empty(), "buildRaceConditionDPUTest: Got empty weightsShape");
    VPUX_THROW_UNLESS(!weightsTableShape.empty(), "buildRaceConditionDPUTest: Got empty weightsTableShape");

    const char* weightsFileName = "weights.dat";

    auto inputCMXShape = inputShape;

    auto weightsCMXShape = weightsShape;
    auto outputCMXShape = outputShape;

    const auto alignmentRequirement = 16;

    const auto weightsCMXSize = vpux::hwtest::totalTensorSize(weightsCMXShape, weightsType);
    const auto outputCMXSize = vpux::hwtest::totalTensorSize(outputCMXShape, outputType);
    const auto inputCMXSize = vpux::hwtest::totalTensorSize(inputCMXShape, inputType);

    const auto alignment =
            (alignmentRequirement * static_cast<vpux::Bit>(getElemTypeSize(inputType)).count()) / CHAR_BIT;
    const auto WEIGHTS_CMX_OFFSET = 0;
    VPUX_THROW_UNLESS(WEIGHTS_CMX_OFFSET % alignment == 0, "WEIGHTS_CMX_OFFSET must be multiple of {0}, got {1}",
                      alignment, WEIGHTS_CMX_OFFSET);

    const auto OUTPUT_CMX_OFFSET = WEIGHTS_CMX_OFFSET + weightsCMXSize;
    VPUX_THROW_UNLESS(OUTPUT_CMX_OFFSET % alignment == 0, "OUTPUT_CMX_OFFSET must be multiple of {0}, got {1}",
                      alignment, OUTPUT_CMX_OFFSET);

    const auto INPUT_CMX_OFFSET = OUTPUT_CMX_OFFSET + outputCMXSize;
    VPUX_THROW_UNLESS(INPUT_CMX_OFFSET % alignment == 0, "INPUT_CMX_OFFSET must be multiple of {0}, got {1}", alignment,
                      INPUT_CMX_OFFSET);

    const auto WEIGHTSTABLE_CMX_OFFSET = INPUT_CMX_OFFSET + inputCMXSize;
    VPUX_THROW_UNLESS(WEIGHTSTABLE_CMX_OFFSET % alignment == 0,
                      "WEIGHTSTABLE_CMX_OFFSET must be multiple of {0}, got {1}", alignment, WEIGHTSTABLE_CMX_OFFSET);

    const auto inputParamType =
            getMemRefType(VPURT::BufferSection::NetworkInput, inputShape, inputType, DimsOrder::NHWC);
    const auto outputParamType =
            getMemRefType(vpux::VPURT::BufferSection::NetworkOutput, outputShape, outputType, DimsOrder::NHWC);

    SmallVector<mlir::Type> inputTypes(numClusters, outputParamType);
    inputTypes.insert(inputTypes.begin(), inputParamType);

    SmallVector<mlir::Type> outputTypes(numClusters, outputParamType);

    const auto funcType = builder.getFunctionType(makeArrayRef(inputTypes), makeArrayRef(outputTypes));

    auto function = builder.create<mlir::func::FuncOp>(
            loc, printToString("race_condition_dpu_{0}_{1}_{2}", inputType, weightsType, outputType), funcType,
            builder.getStringAttr("private"));

    auto functionBuilder = mlir::OpBuilder::atBlockBegin(function.addEntryBlock(), builder.getListener());

    auto functionInput = function.getArgument(0);

    SmallVector<mlir::BlockArgument> functionOutputs;
    for (std::size_t idx = 1; idx <= numClusters; ++idx) {
        functionOutputs.push_back(function.getArgument(idx));
    }

    const auto weightsValues = generateWeights(weightsShape, weightsType, ctx, weightsFileName);
    auto weightsAttribute = vpux::Const::ContentAttr::get(weightsValues);
    weightsAttribute = weightsAttribute.reorder(vpux::DimsOrder::OYXI);

    if (auto qty = weightsType.dyn_cast<mlir::quant::QuantizedType>()) {
        const auto quantizedType = vpux::changeStorageType(qty, weightsAttribute.getType().getElementType());
        weightsAttribute = weightsAttribute.quantCast(quantizedType);
        if (qty.getStorageType().isInteger(4)) {
            weightsAttribute = weightsAttribute.bitPack(4);
        }
    }

    const auto weightsDDRType =
            getMemRefType(VPURT::BufferSection::Constant, weightsShape, weightsType, DimsOrder::NHWC);

    auto weightsStrides = weightsDDRType.cast<vpux::NDTypeInterface>().getStrides();
    auto inputStrides = vpux::getStrides(functionInput);

    SmallVector<VPURT::DeclareBufferOp> weightsCMX;
    SmallVector<VPURT::DeclareBufferOp> inputsCMX;
    SmallVector<VPURT::DeclareBufferOp> outputsCMX;
    SmallVector<VPURT::DeclareBufferOp> weightsTablesCMX;

    for (std::size_t idx = 0; idx < numClusters; ++idx) {
        weightsCMX.push_back(createDeclareTensorOp(functionBuilder, VPURT::BufferSection::CMX_NN, weightsShape,
                                                   weightsType, vpux::DimsOrder::OYXI, weightsStrides, idx,
                                                   WEIGHTS_CMX_OFFSET));
        inputsCMX.push_back(createDeclareTensorOp(functionBuilder, VPURT::BufferSection::CMX_NN, inputShape, inputType,
                                                  vpux::DimsOrder::NHWC, inputStrides, idx, INPUT_CMX_OFFSET));
        outputsCMX.push_back(createDeclareTensorOp(functionBuilder, VPURT::BufferSection::CMX_NN, outputShape,
                                                   outputType, DimsOrder::NHWC, idx, OUTPUT_CMX_OFFSET));
        weightsTablesCMX.push_back(createDeclareTensorOp(functionBuilder, VPURT::BufferSection::CMX_NN,
                                                         weightsTableShape, int32, DimsOrder::NHWC, idx,
                                                         WEIGHTSTABLE_CMX_OFFSET));
    }

    auto weightsDDR = functionBuilder.create<vpux::Const::DeclareOp>(loc, weightsDDRType, weightsAttribute);

    auto& weightsOutputChannelsStrideInBits = weightsStrides[vpux::Dims4D::Filter::OC];

    const auto weightsTableDDRType = mlir::RankedTensorType::get(weightsTableShape, int32);
    const auto sparsityPtrStep = 0;
    const auto weightsTable = VPU::NCESparsity::getWeightsTable(
            inputType, outputType, static_cast<std::int32_t>(WEIGHTS_CMX_OFFSET),
            static_cast<std::int32_t>(weightsOutputChannelsStrideInBits.count() / CHAR_BIT),
            VPU::NCESparsity::SPARSITY_PTR_WHEN_NO_SPARSITY, sparsityPtrStep, testDesc.getArchitecture(),
            output.shape[1], weightsType);

    const auto weightsTableDDRMemRef =
            getMemRefType(VPURT::BufferSection::Constant, weightsTableShape, int32, DimsOrder::NHWC);
    const auto weightsTableValues =
            mlir::DenseElementsAttr::get(weightsTableDDRType, llvm::makeArrayRef<std::int32_t>(weightsTable));
    auto weightsTableDDR = functionBuilder.create<vpux::Const::DeclareOp>(
            loc, weightsTableDDRMemRef,
            vpux::Const::ContentAttr::get(weightsTableValues).reorder(vpux::DimsOrder::NHWC));

    auto updateBarrier = functionBuilder.create<vpux::VPURT::ConfigureBarrierOp>(loc, 0);
    VPURT::ConfigureBarrierOp waitBarrier;

    for (std::size_t idx = 0; idx < numClusters; ++idx) {
        VPURT::wrapIntoTaskOp<VPUIP::NNDMAOp>(functionBuilder, mlir::ValueRange(),
                                              mlir::ValueRange(updateBarrier.getBarrier()), loc, functionInput,
                                              inputsCMX[idx].getOperation()->getResult(0));
        VPURT::wrapIntoTaskOp<VPUIP::NNDMAOp>(
                functionBuilder, mlir::ValueRange(), mlir::ValueRange(updateBarrier.getBarrier()), loc,
                weightsDDR.getOperation()->getResult(0), weightsCMX[idx].getOperation()->getResult(0));
        VPURT::wrapIntoTaskOp<VPUIP::NNDMAOp>(
                functionBuilder, mlir::ValueRange(), mlir::ValueRange(updateBarrier.getBarrier()), loc,
                weightsTableDDR.getOperation()->getResult(0), weightsTablesCMX[idx].getOperation()->getResult(0));
    }

    waitBarrier = updateBarrier;

    const auto strides = getIntArrayAttr(ctx, conv.stride);
    std::vector<std::int64_t> paddings = convertNBPadtoNCETaskPad(conv.pad);
    SmallVector<std::int64_t> kernel = {weightsShape[2], weightsShape[3]};
    const auto kernelSize = getIntArrayAttr(ctx, kernel);

    const auto start = getIntArrayAttr(ctx, std::vector<std::int64_t>{0, 0, 0});
    const auto outEnd =
            getIntArrayAttr(ctx, std::vector<std::int64_t>{outputShape[3] - 1, outputShape[2] - 1, outputShape[1] - 1});
    const auto inEnd =
            getIntArrayAttr(ctx, std::vector<std::int64_t>{inputShape[3] - 1, inputShape[2] - 1, inputShape[1] - 1});
    const auto pad = VPU::getPaddingAttr(ctx, paddings[PAD_NCETASK_LEFT], paddings[PAD_NCETASK_RIGHT],
                                         paddings[PAD_NCETASK_TOP], paddings[PAD_NCETASK_BOTTOM]);

    for (std::size_t i = 1; i < iterationCount; ++i) {
        updateBarrier = functionBuilder.create<vpux::VPURT::ConfigureBarrierOp>(loc, i);

        for (std::size_t idx = 0; idx < numClusters; ++idx) {
            auto nceTask = VPURT::wrapIntoTaskOp<VPUIP::NCEClusterTaskOp>(
                    functionBuilder, mlir::ValueRange(waitBarrier.getBarrier()),
                    mlir::ValueRange(updateBarrier.getBarrier()), loc, inputsCMX[idx].getBuffer(),
                    weightsCMX[idx].getBuffer(), weightsTablesCMX[idx].getBuffer(), nullptr, nullptr,
                    inputsCMX[idx].getBuffer(), outputsCMX[idx].getBuffer(), outputsCMX[idx].getBuffer(),
                    vpux::VPUIP::NCETaskType::CONV, kernelSize, strides, pad, nullptr, nullptr);
            nceTask.addDPUTask(functionBuilder, start, outEnd, start, inEnd, pad, conv.cube_mode);
        }
        waitBarrier = updateBarrier;
    }

    for (std::size_t idx = 0; idx < numClusters; ++idx) {
        VPURT::wrapIntoTaskOp<VPUIP::NNDMAOp>(functionBuilder, mlir::ValueRange(waitBarrier.getBarrier()),
                                              mlir::ValueRange(), loc, outputsCMX[idx].getOperation()->getResult(0),
                                              functionOutputs[idx]);
    }
    auto outputsRef = makeArrayRef(functionOutputs);
    functionBuilder.create<mlir::func::ReturnOp>(loc, mlir::ValueRange{outputsRef});

    module.dump();

    mlir::PassManager pm(ctx, mlir::OpPassManager::Nesting::Implicit);
    pm.addPass(VPU::createInitCompilerPass(testDesc.getArchitecture(), VPU::CompilationMode::DefaultHW,
                                           /*numOfDPUGroups=*/numClusters, None, log));
    if (conv.compress) {
        pm.addPass(VPUIP::createCompressWeightsBTCPass(log));
    }

    VPUX_THROW_UNLESS(mlir::succeeded(pm.run(module)), "Compilation failed");

    SmallVector<mlir::Type> userOutputs(numClusters,
                                        getTensorType(ShapeRef(outputShape), outputType, DimsOrder::NHWC, nullptr));
    buildCNNOp(builder, function.getName(),
               {getTensorType(ShapeRef(inputShape), inputType, vpux::DimsOrder::NHWC, nullptr)},
               makeArrayRef(userOutputs));
}

}  // namespace hwtest
}  // namespace vpux
