//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/IE/passes.hpp"

#include "vpux/compiler/dialect/IE/ops.hpp"
#include "vpux/compiler/dialect/IE/utils/quantization.hpp"
#include "vpux/compiler/dialect/IE/utils/shape_infer.hpp"
#include "vpux/compiler/utils/attributes.hpp"

#include "vpux/utils/core/error.hpp"
#include "vpux/utils/core/format.hpp"
#include "vpux/utils/core/range.hpp"

#include <mlir/IR/BlockAndValueMapping.h>
#include <mlir/IR/BuiltinTypes.h>
#include <mlir/Transforms/DialectConversion.h>

#include <numeric>

using namespace vpux;

namespace {

constexpr int64_t TARGET_TENSOR_DIM = 4;

using MergeMapItem = SmallVector<int64_t>;
using MergeMap = SmallVector<SmallVector<int64_t>>;

void alignShapeToReferenceShapeSize(size_t refSisze, SmallVector<int64_t>& shape) {
    VPUX_THROW_UNLESS(refSisze >= shape.size(), "The reference shape size({0}) < shape size({1})", refSisze,
                      shape.size());
    size_t diff = refSisze - shape.size();
    if (diff) {
        shape.insert(shape.begin(), diff, 1);
    }
}

int64_t getBalancedDimIndexFromShape(SmallVector<int64_t> shape) {
    int64_t dimH = 1;
    int64_t dimW = 1;
    int64_t dimIndex = 0;
    while (!shape.empty()) {
        if (dimW < dimH) {
            dimW *= shape.back();
            shape.pop_back();
        } else {
            dimH *= shape.front();
            shape.erase(shape.begin());
            dimIndex++;
        }
    }
    return dimIndex;
}

SmallVector<int64_t> alignShapeWithDimMap(ArrayRef<int64_t> originShape, const MergeMap& mapper) {
    SmallVector<int64_t> retNewShape;
    for (auto& dims : mapper) {
        int64_t dimSize = 1;
        for (auto i : dims) {
            dimSize *= originShape[i];
        }
        retNewShape.push_back(dimSize);
    }
    return retNewShape;
}

SmallVector<int64_t> alignShapeTo4D(ArrayRef<int64_t> originShape, const MergeMap& mapper) {
    auto newShape = alignShapeWithDimMap(originShape, mapper);
    alignShapeToReferenceShapeSize(TARGET_TENSOR_DIM, newShape);
    return newShape;
}

MergeMap getTrivialMap(size_t size) {
    auto mapper = MergeMap(size);
    std::generate(mapper.begin(), mapper.end(), [counter = 0]() mutable {
        return SmallVector<int64_t>{counter++};
    });
    return mapper;
}

MergeMap getDimMapWithFirstGreater1DimAsC(SmallVector<int64_t> shape) {
    const int64_t maxDim = checked_cast<int64_t>(shape.size());
    // Try to convert great than 4D shape to 3D.
    // In this way, to promise
    //   N always = 1
    //   C always > 1 unless the shape size is 1.
    // eg.
    //   1x1x1x1x1  -> 1x1x1
    //   1x3x9x16x1 -> 3x9x16
    //   3x9x16x1x1 -> 3x9x16
    //   3x9x1x1x16 -> 3x9x16
    //   2x3x4x5    -> 2x12x5
    //   2x3x4x5x6  -> 2x12x30
    //   2x3x4x5x6x7-> 2x60x42
    const auto moreThanOnePredicate = [](const int64_t dim) -> bool {
        return dim > 1;
    };
    const auto firstMoreThanOneIt = std::find_if(shape.begin(), shape.end(), moreThanOnePredicate);
    if (firstMoreThanOneIt == shape.end()) {
        return {};
    }

    MergeMap retMapper;
    const int64_t nextDimCIndex = std::distance(shape.begin(), firstMoreThanOneIt) + 1;
    retMapper.push_back(irange(nextDimCIndex));

    shape.erase(shape.begin(), shape.begin() + nextDimCIndex);
    // Convert shape to 2D, and make the value of 2 Dims close to each other
    const auto splitDimIndex = getBalancedDimIndexFromShape(std::move(shape)) + nextDimCIndex;
    retMapper.push_back(irange(nextDimCIndex, splitDimIndex));
    retMapper.push_back(irange(splitDimIndex, maxDim));
    return retMapper;
}

MergeMap getDimMapGeneric(ArrayRef<int64_t> shape) {
    MergeMap dimMapper;
    if (shape.size() > TARGET_TENSOR_DIM) {
        return getDimMapWithFirstGreater1DimAsC(to_small_vector(shape));
    }
    return getTrivialMap(shape.size());
}

MergeMap getDimMergeMapWith2Inputs(ArrayRef<int64_t> input1, ArrayRef<int64_t> input2) {
    auto shapeSize1 = std::accumulate(input1.begin(), input1.end(), (int64_t)1, std::multiplies<int64_t>());
    auto shapeSize2 = std::accumulate(input2.begin(), input2.end(), (int64_t)1, std::multiplies<int64_t>());
    // Find the origin input and broadcast shape
    //  The large size shape is the origin input
    //  The small size shape is the shape that needs to be broadcast in some planes
    auto maxShape = (shapeSize1 > shapeSize2) ? input1 : input2;
    auto planeShape = (shapeSize1 > shapeSize2) ? input2 : input1;

    auto getMergeMap = [](ArrayRef<int64_t> fullShape, ArrayRef<int64_t> planeShape, auto condition) {
        MergeMap dimMap;
        SmallVector<int64_t> inputDimsTmp;
        for (size_t i = 0; i < fullShape.size(); i++) {
            auto compareVal = condition(i, fullShape);
            if (compareVal == planeShape[i]) {
                inputDimsTmp.push_back(i);
            } else {
                if (inputDimsTmp.size() > 1) {
                    dimMap.push_back(inputDimsTmp);
                }
                inputDimsTmp.clear();
            }
        }
        if (inputDimsTmp.size() > 1) {
            dimMap.push_back(inputDimsTmp);
        }
        return dimMap;
    };

    auto sameDimCondition = [](size_t i, ArrayRef<int64_t> shape) {
        return shape[i];
    };
    auto planeDimCondition = [](size_t, ArrayRef<int64_t>) {
        return 1;
    };

    // Examples:
    //  Merge in plane:
    //      Inputs: tensor<4x3x13x13x2xf16>, tensor<1x1x1x1x1xf16>
    //       Dim(0, 1, 2, 3, 4) can merge together.
    //  Merge in same Dim:
    //      Inputs: tensor<4x3x13x13x2xf16>, tensor<4x3x13x13x2xf16>
    //       Dim(0, 1, 2, 3, 4) can merge together.
    //  Mixed:
    //      Inputs: tensor<4x3x13x13x2xf16>, tensor<1x1x13x13x2xf16>
    //       Dim(0, 1) 4x3 and Dim(2, 3, 4) 13x13x2 can merge together.
    //      Inputs: tensor<1x2x3x4x5x6xf16>, tensor<1x2x1x4x5x1xf16>
    //       Dim(0, 1) 1x2,  Dim(2) 3, Dim(3, 4) 4x5 and Dim(5) 6 can merge together.
    auto calculateMergeMap = [&](ArrayRef<int64_t> fullShape, ArrayRef<int64_t> planeShape) {
        auto mergeInSameDims = getMergeMap(fullShape, planeShape, sameDimCondition);
        auto mergeInPlaneDims = getMergeMap(fullShape, planeShape, planeDimCondition);
        MergeMap dimsCanMerge;
        auto fullShapeSize = checked_cast<int64_t>(fullShape.size());
        for (int64_t dimIndex = 0; dimIndex < fullShapeSize; dimIndex++) {
            auto minIndex = fullShapeSize;
            MergeMap* minVector = nullptr;

            auto getMinimumIndex = [&](MergeMap& dimMapper) {
                if (!dimMapper.empty()) {
                    if (dimMapper.front()[0] < minIndex) {
                        minVector = &dimMapper;
                        minIndex = dimMapper.front()[0];
                    }
                }
            };
            getMinimumIndex(mergeInPlaneDims);
            getMinimumIndex(mergeInSameDims);

            if (dimIndex < minIndex) {
                dimsCanMerge.push_back({dimIndex});
            } else {
                auto& currentDims = minVector->front();
                while (!currentDims.empty() && (currentDims.front() < dimIndex)) {
                    currentDims.erase(currentDims.begin());
                }
                if (!currentDims.empty()) {
                    dimsCanMerge.push_back(currentDims);
                    dimIndex = currentDims.back();
                }
                minVector->erase(minVector->begin());
            }
        }
        return dimsCanMerge;
    };

    auto getSubShape = [](ArrayRef<int64_t> shape, ArrayRef<int64_t> map) {
        SmallVector<int64_t> retShape;
        for (auto& dims : map) {
            retShape.push_back(shape[dims]);
        }
        return retShape;
    };

    MergeMap dimsCanMerge;
    // Corner case:
    //  %4 = IE.Operator(%3, %cst) : tensor<f16>, tensor<f16> -> tensor<f16>
    //  The shape size is 0, and the empty merge map will be 1.
    if (maxShape.empty() && planeShape.empty()) {
        dimsCanMerge.resize(4);
        return dimsCanMerge;
    }

    if (maxShape == planeShape) {
        dimsCanMerge.push_back(irange(static_cast<int64_t>(maxShape.size())));
    } else {
        dimsCanMerge = calculateMergeMap(maxShape, planeShape);
    }
    switch (dimsCanMerge.size()) {
    case 1: {
        dimsCanMerge = getDimMapGeneric(maxShape);
        break;
    }
    case 2: {
        auto expandMapTo3D = [&](auto mapIt) {
            auto newReshapeDim = getBalancedDimIndexFromShape(getSubShape(maxShape, *mapIt));
            SmallVector<int64_t> dimTmp(mapIt->begin(), mapIt->begin() + newReshapeDim);
            mapIt->erase(mapIt->begin(), mapIt->begin() + newReshapeDim);
            dimsCanMerge.insert(mapIt, dimTmp);
        };
        // N always 1 to avoid unroll
        if (dimsCanMerge[1].size() > 1) {
            expandMapTo3D(dimsCanMerge.begin() + 1);
        } else {
            expandMapTo3D(dimsCanMerge.begin());
        }
        break;
    }
    case 4:
        // Direct convert
        break;
    case 3:
        // Add 1 at dim N
        break;
    default:
        VPUX_THROW("The input shape {0}, {1} can't convert to 4D", input1, input2);
        break;
    }
    return dimsCanMerge;
}

MergeMap getDimMergeMapWith3Inputs(ArrayRef<int64_t> input1, ArrayRef<int64_t> inputLow, ArrayRef<int64_t> outLow) {
    // Handle 3 input shapes
    //  input:   AxBxCxDxF
    //  in_low:  1xBx1x1x1
    //  out_low: 1x1xCx1x1
    //  To: (A, B, C, [DxF])
    // vs
    //  input:   AxBxCxDxF
    //  in_low:  1xBx1x1x1
    //  out_low: 1x1x1xDx1
    //  To: (A, B, C, D, F) can't convert to 4D, unsupported.
    const auto moreThanOnePredicate = [](const int64_t dim) -> bool {
        return dim > 1;
    };

    auto getDimIdx = [&](ArrayRef<int64_t> dims) -> int64_t {
        auto firstMoreThanOneIt = std::find_if(dims.begin(), dims.end(), moreThanOnePredicate);
        VPUX_THROW_WHEN(firstMoreThanOneIt == dims.end(), "The shape size is 1, should not enter this case.");
        return std::distance(dims.begin(), firstMoreThanOneIt);
    };
    int64_t inDimIndex = getDimIdx(inputLow);
    int64_t outDimIndex = getDimIdx(outLow);

    auto generateDimMap = [](int64_t minIndex, int64_t maxIndex, int64_t size) {
        MergeMap mergeMap;
        if (minIndex > 0) {
            mergeMap.push_back(irange(minIndex));
        }
        mergeMap.push_back({minIndex});
        minIndex++;
        if (minIndex < maxIndex) {
            mergeMap.push_back(irange(minIndex, maxIndex));
        }
        mergeMap.push_back({maxIndex});
        maxIndex++;
        if (maxIndex < size) {
            mergeMap.push_back(irange(maxIndex, size));
        }
        return mergeMap;
    };

    auto fullShapeSize = checked_cast<int64_t>(input1.size());
    MergeMap mergeMapTmp;
    if (inDimIndex < outDimIndex) {
        mergeMapTmp = generateDimMap(inDimIndex, outDimIndex, fullShapeSize);
    } else {
        mergeMapTmp = generateDimMap(outDimIndex, inDimIndex, fullShapeSize);
    }
    auto newShape = alignShapeWithDimMap(input1, mergeMapTmp);
    MergeMap mergeMapRet;
    MergeMapItem item;
    for (int64_t dimIdx = 0; dimIdx < checked_cast<int64_t>(newShape.size()); dimIdx++) {
        item.append(mergeMapTmp[dimIdx]);
        if (newShape[dimIdx] > 1) {
            mergeMapRet.push_back(item);
            item.clear();
        }
    }
    if (!item.empty()) {
        mergeMapRet.back().append(item);
    }
    VPUX_THROW_WHEN(mergeMapRet.size() > 4, "Can't convert the shape to 4D, the converted shape is {0}D",
                    mergeMapRet.size());
    return mergeMapRet;
}

MergeMap extendInputShapeTo4D(IE::FakeQuantizeOp origOp) {
    auto inputLowScaleShape = to_small_vector(getShape(origOp.input_low()));
    auto outputLowScaleShape = to_small_vector(getShape(origOp.output_low()));
    const auto inputShape = to_small_vector(getShape(origOp.input()));
    const auto ref1ElemShape = SmallVector<int64_t>(inputShape.size(), 1);

    alignShapeToReferenceShapeSize(inputShape.size(), inputLowScaleShape);
    alignShapeToReferenceShapeSize(inputShape.size(), outputLowScaleShape);

    if (inputLowScaleShape == outputLowScaleShape) {
        return getDimMergeMapWith2Inputs(inputShape, inputLowScaleShape);
    }
    if (ref1ElemShape == inputLowScaleShape) {
        return getDimMergeMapWith2Inputs(inputShape, outputLowScaleShape);
    }
    if (ref1ElemShape == outputLowScaleShape) {
        return getDimMergeMapWith2Inputs(inputShape, inputLowScaleShape);
    }
    return getDimMergeMapWith3Inputs(inputShape, inputLowScaleShape, outputLowScaleShape);
}

mlir::Value reshapeInputWithMergeMap(mlir::PatternRewriter& rewriter, mlir::Location loc, mlir::Value origInput,
                                     const MergeMap& map) {
    const auto inShape = to_small_vector(getShape(origInput));
    if (inShape.empty()) {
        return origInput;
    }

    auto constInputShape = alignShapeTo4D(inShape, map);
    const auto constInputShapeAttr = getIntArrayAttr(rewriter.getContext(), constInputShape);

    return rewriter.createOrFold<IE::ReshapeOp>(loc, origInput, nullptr, false, constInputShapeAttr);
}

void tryAndConvert2NCEShape(SmallVector<int64_t>& shape1, SmallVector<int64_t>& shape2, MergeMap& map) {
    // 4D Multiply shape 1x1x1xM need convert Shape to 1xMx1x1
    //
    // TODO:
    // This logic is a litte same as AdaptShapesForScaleShiftPass.
    // May combine them into 1 pass and abandon the AdaptShapesForScaleShiftPass
    const auto nonTrivialDimPredicate = [](const int64_t dim) -> bool {
        return dim > 1;
    };
    const auto nonTrivialShape1Dims = std::count_if(shape1.begin(), shape1.end(), nonTrivialDimPredicate);
    const auto nonTrivialShape2Dims = std::count_if(shape2.begin(), shape2.end(), nonTrivialDimPredicate);
    // Filter out the Shape 1x1x1x1 and nonTrivialDims > 1 cases
    if ((nonTrivialShape1Dims > 1 || nonTrivialShape2Dims > 1) ||
        (nonTrivialShape1Dims == 0 && nonTrivialShape2Dims == 0)) {
        return;
    }
    auto findFirstNonTrivialIndex = [&](auto shape) {
        const auto firstIt = std::find_if(shape.begin(), shape.end(), nonTrivialDimPredicate);
        return std::distance(shape.begin(), firstIt);
    };
    int64_t firstNonTrivialIndex;
    // Find the first non-trivial index from 2 input shapes
    firstNonTrivialIndex = (findFirstNonTrivialIndex(shape1) <= findFirstNonTrivialIndex(shape2))
                                   ? findFirstNonTrivialIndex(shape1)
                                   : findFirstNonTrivialIndex(shape2);

    // Already at DimC
    if (firstNonTrivialIndex == 1) {
        return;
    }
    if (map.size() < 4) {
        map.insert(map.begin(), 4 - map.size(), {});
    }
    std::swap(shape1[1], shape1[firstNonTrivialIndex]);
    std::swap(shape2[1], shape2[firstNonTrivialIndex]);
    std::swap(map[1], map[firstNonTrivialIndex]);
}

//
// ConvertShapeTo4DPass
//

class ConvertShapeTo4DPass final : public IE::ConvertShapeTo4DBase<ConvertShapeTo4DPass> {
public:
    explicit ConvertShapeTo4DPass(Logger log) {
        Base::initLogger(log, Base::getArgumentName());
    }

private:
    void safeRunOnFunc() final;
};

//
// GenericConverter
//

mlir::LogicalResult convertGeneric(mlir::Operation* origOp, mlir::ValueRange operands,
                                   mlir::ConversionPatternRewriter& rewriter, mlir::TypeConverter& typeConverter,
                                   Logger log) {
    log.trace("Process Operation '{0}' at '{1}", origOp->getName(), origOp->getLoc());

    const auto origOperands = origOp->getOperands();
    VPUX_THROW_UNLESS(origOperands.size() == operands.size(), "Wrong operands size : {0}", operands.size());

    mlir::BlockAndValueMapping mapper;
    mapper.map(origOperands, operands);

    auto* newOp = rewriter.clone(*origOp, mapper);
    for (auto result : newOp->getResults()) {
        result.setType(typeConverter.convertType(result.getType()));
    }

    rewriter.replaceOp(origOp, newOp->getResults());
    return mlir::success();
}

template <class ConcreteOp>
class GenericConverter final : public mlir::OpConversionPattern<ConcreteOp> {
    using OpAdaptor = typename mlir::OpConversionPattern<ConcreteOp>::OpAdaptor;

public:
    GenericConverter(mlir::TypeConverter& typeConverter, mlir::MLIRContext* ctx, Logger log)
            : mlir::OpConversionPattern<ConcreteOp>(typeConverter, ctx), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(ConcreteOp origOp, OpAdaptor newArgs,
                                        mlir::ConversionPatternRewriter& rewriter) const final {
        auto* typeConverter = this->getTypeConverter();
        VPUX_THROW_UNLESS(typeConverter != nullptr, "TypeConverter was not set");

        if (origOp->getOperands().size() == 2) {
            return convertWith2Inputs(origOp, newArgs.getOperands(), rewriter);
        }
        return convertGeneric(origOp, newArgs.getOperands(), rewriter, *typeConverter, _log);
    }

private:
    mlir::LogicalResult convertWith2Inputs(ConcreteOp origOp, mlir::ValueRange operands,
                                           mlir::ConversionPatternRewriter& rewriter) const;

private:
    Logger _log;
};

template <class ConcreteOp>
mlir::LogicalResult GenericConverter<ConcreteOp>::convertWith2Inputs(ConcreteOp origOp, mlir::ValueRange operands,
                                                                     mlir::ConversionPatternRewriter& rewriter) const {
    _log.trace("Found '{0}' Operation at '{1}'", origOp->getName(), origOp->getLoc());

    mlir::Value input1 = origOp->getOperand(0);
    mlir::Value input2 = origOp->getOperand(1);

    const auto shapeOne = input1.getType().template cast<vpux::NDTypeInterface>().getShape();
    const auto shapeTwo = input2.getType().template cast<vpux::NDTypeInterface>().getShape();

    auto shapeOneVector = to_small_vector(shapeOne);
    auto shapeTwoVector = to_small_vector(shapeTwo);

    // Align dims
    if (shapeOneVector.size() != shapeTwoVector.size()) {
        auto maxSize = std::max(shapeOneVector.size(), shapeTwoVector.size());
        auto& smallShape = (shapeOneVector.size() > shapeTwoVector.size()) ? shapeTwoVector : shapeOneVector;
        auto& bigShape = (shapeOneVector.size() > shapeTwoVector.size()) ? shapeOneVector : shapeTwoVector;
        SmallVector<int64_t> expanedShape(maxSize, 1);
        if (origOp->hasAttr("auto_broadcast")) {
            alignShapeToReferenceShapeSize(bigShape.size(), smallShape);
        } else {
            // Some operations need to map their channels first. e.g. PRelu
            if ((smallShape.size() == 1) && (smallShape[0] == bigShape[1])) {
                expanedShape[1] = smallShape[0];
                smallShape.swap(expanedShape);
            } else {
                alignShapeToReferenceShapeSize(bigShape.size(), smallShape);
            }
        }
    }

    auto dimsCanMerge = getDimMergeMapWith2Inputs(shapeOneVector, shapeTwoVector);

    auto newInputShape1 = alignShapeTo4D(shapeOneVector, dimsCanMerge);
    auto newInputShape2 = alignShapeTo4D(shapeTwoVector, dimsCanMerge);
    if (std::is_same<IE::MultiplyOp, ConcreteOp>::value) {
        tryAndConvert2NCEShape(newInputShape1, newInputShape2, dimsCanMerge);
    }
    auto newIn1 = rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), operands[0], nullptr, false,
                                                       getIntArrayAttr(this->getContext(), newInputShape1));
    auto newIn2 = rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), operands[1], nullptr, false,
                                                       getIntArrayAttr(this->getContext(), newInputShape2));

    SmallVector<mlir::Value> newOperands;
    newOperands.push_back(newIn1);
    newOperands.push_back(newIn2);
    mlir::BlockAndValueMapping mapper;
    mapper.map(origOp->getOperands(), newOperands);

    auto* newOp = rewriter.clone(*origOp, mapper);
    SmallVector<mlir::Value> newResults;
    for (auto result : newOp->getResults()) {
        auto resultNDI = result.getType().template cast<vpux::NDTypeInterface>();
        auto resultShape = to_small_vector(resultNDI.getShape());
        result.setType(resultNDI.changeShape(ShapeRef(alignShapeTo4D(resultShape, dimsCanMerge))));
        const auto outputShapeAttr = getIntArrayAttr(rewriter.getContext(), resultShape);
        auto resultReshapeOp =
                rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), result, nullptr, false, outputShapeAttr);
        if (result == resultReshapeOp) {
            newResults.push_back(result);
        } else {
            newResults.push_back(resultReshapeOp.template getDefiningOp<IE::ReshapeOp>().output());
        }
    }

    rewriter.replaceOp(origOp, newResults);
    return mlir::success();
}

//
// FakeQuantizeConverter
//

class FakeQuantizeConverter final : public mlir::OpConversionPattern<IE::FakeQuantizeOp> {
public:
    FakeQuantizeConverter(mlir::TypeConverter& typeConverter, mlir::MLIRContext* ctx, Logger log)
            : mlir::OpConversionPattern<IE::FakeQuantizeOp>(typeConverter, ctx), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::FakeQuantizeOp origOp, OpAdaptor newArgs,
                                        mlir::ConversionPatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult FakeQuantizeConverter::matchAndRewrite(IE::FakeQuantizeOp origOp, OpAdaptor,
                                                           mlir::ConversionPatternRewriter& rewriter) const {
    _log.trace("[{0}] Found IE::FakeQuantize Operation '{1}'", getDebugName(), origOp->getLoc());

    const auto mergeMap = extendInputShapeTo4D(origOp);

    const auto inputLow = reshapeInputWithMergeMap(rewriter, origOp->getLoc(), origOp.input_low(), mergeMap);
    const auto inputHigh = reshapeInputWithMergeMap(rewriter, origOp->getLoc(), origOp.input_high(), mergeMap);
    const auto outputLow = reshapeInputWithMergeMap(rewriter, origOp->getLoc(), origOp.output_low(), mergeMap);
    const auto outputHigh = reshapeInputWithMergeMap(rewriter, origOp->getLoc(), origOp.output_high(), mergeMap);

    auto inputReshape = reshapeInputWithMergeMap(rewriter, origOp->getLoc(), origOp.input(), mergeMap);

    auto newFakeQuantizeOp =
            rewriter.create<IE::FakeQuantizeOp>(origOp->getLoc(), inputReshape, inputLow, inputHigh, outputLow,
                                                outputHigh, origOp.levels(), origOp.auto_broadcast());

    const auto outputShapeAttr = getIntArrayAttr(getContext(), getShape(origOp.output()));
    rewriter.replaceOpWithNewOp<IE::ReshapeOp>(origOp, newFakeQuantizeOp.output(), nullptr, false, outputShapeAttr);
    _log.trace("[{0}] Replaced with 'IE::FakeQuantize'", getDebugName());

    return mlir::success();
}

//
// TopKOpConverter
//

class TopKOpConverter final : public mlir::OpConversionPattern<IE::TopKOp> {
    using OpAdaptor = typename mlir::OpConversionPattern<IE::TopKOp>::OpAdaptor;

public:
    TopKOpConverter(mlir::TypeConverter& typeConverter, mlir::MLIRContext* ctx, Logger log)
            : mlir::OpConversionPattern<IE::TopKOp>(typeConverter, ctx), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::TopKOp origOp, OpAdaptor newArgs,
                                        mlir::ConversionPatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult TopKOpConverter::matchAndRewrite(IE::TopKOp origOp, OpAdaptor,
                                                     mlir::ConversionPatternRewriter& rewriter) const {
    _log.trace("Found '{0}' Operation at '{1}'", origOp->getName(), origOp->getLoc());

    const auto origInType = origOp.input().getType().cast<vpux::NDTypeInterface>();
    const int64_t origInRank = origInType.getRank();
    int64_t axis = origOp.axis();
    if (axis < 0) {
        axis += origInRank;
    }

    // Deduce the new TopK aix from map table
    const auto inShape = to_small_vector(getShape(origOp.input()));

    MergeMap mergeMap;
    SmallVector<int64_t> tempMap;
    int64_t newAxis = 0;
    if (axis > 0) {
        mergeMap.push_back(irange(axis));
        newAxis = 1;
    }
    mergeMap.push_back({axis});
    if (axis < origInRank - 1) {
        mergeMap.push_back(irange(axis + 1, origInRank));
    }
    // The mergeMap's Max Size is 3
    auto delta4D = 4 - mergeMap.size();
    mergeMap.insert(mergeMap.begin(), delta4D, {});
    newAxis += delta4D;

    const auto newAxisAttr = getIntAttr(origOp->getContext(), newAxis);

    const auto newInShapeAttr = getIntArrayAttr(this->getContext(), alignShapeTo4D(inShape, mergeMap));
    const auto newInReshape =
            rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), origOp.input(), nullptr, false, newInShapeAttr);

    auto newTopKOp =
            rewriter.create<IE::TopKOp>(origOp->getLoc(), newInReshape, origOp.k(), origOp.k_valueAttr(), newAxisAttr,
                                        origOp.modeAttr(), origOp.sortAttr(), origOp.element_typeAttr());

    for (auto indexResult : origOp->getResults() | indexed) {
        auto idx = checked_cast<unsigned>(indexResult.index());
        const auto origResult = indexResult.value();
        const auto outputShapeAttr = getIntArrayAttr(this->getContext(), getShape(origResult));
        const auto newOutputReshape = rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), newTopKOp->getResult(idx),
                                                                           nullptr, false, outputShapeAttr);
        origResult.replaceAllUsesWith(newOutputReshape);
    }

    rewriter.eraseOp(origOp);

    return mlir::success();
}

//
// Mvn6OpConverter
//

class Mvn6Converter final : public mlir::OpConversionPattern<IE::MVN6Op> {
public:
    Mvn6Converter(mlir::TypeConverter& typeConverter, mlir::MLIRContext* ctx, Logger log)
            : mlir::OpConversionPattern<IE::MVN6Op>(typeConverter, ctx), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::MVN6Op origOp, OpAdaptor newArgs,
                                        mlir::ConversionPatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult Mvn6Converter::matchAndRewrite(IE::MVN6Op origOp, OpAdaptor,
                                                   mlir::ConversionPatternRewriter& rewriter) const {
    _log.trace("[{0}] Found IE::MVN6Op Operation '{1}'", getDebugName(), origOp->getLoc());
    const auto inType = origOp.input().getType().cast<vpux::NDTypeInterface>();
    const auto inShape = inType.getShape().raw();
    const auto inRank = inShape.size();
    const auto inAxes = parseIntArrayAttr<int64_t>(origOp.axes_value().value());

    SmallVector<int64_t> newShape;
    SmallVector<int64_t> newAxes;

    if (inRank < 4) {
        // insert leading 1s up to 4D and ajust axes accordingly
        auto newDims = static_cast<int64_t>(TARGET_TENSOR_DIM - inRank);
        newShape.insert(newShape.end(), newDims, 1);
        newShape.append(inShape.begin(), inShape.end());
        // increment 'axes'
        newAxes = inAxes;
        std::for_each(newAxes.begin(), newAxes.end(), [newDims](int64_t& axis) {
            axis += newDims;
        });
    } else if (inRank == 5) {
        // Find and merge two nearby axes of same type (either NORM or non-NORM)
        auto isNormAxis = [inAxes](auto curDim) {
            return std::find(inAxes.begin(), inAxes.end(), curDim) != inAxes.end();
        };
        SmallVector<int64_t> axes5D(inRank);
        std::iota(axes5D.begin(), axes5D.end(), 0);

        auto checkSame = [&](auto curDim, auto nxtDim) {
            auto curType = isNormAxis(curDim);
            auto nxtType = isNormAxis(nxtDim);
            return (curType == nxtType);
        };
        const auto mergeIt = std::adjacent_find(axes5D.begin(), axes5D.end(), checkSame);
        VPUX_THROW_WHEN(mergeIt == axes5D.end(), "MVN6 5D->4D failed : cannot find 2 adjacent dims of same type");
        const auto mergeDim = checked_cast<int64_t>(std::distance(axes5D.begin(), mergeIt));

        //=> new 'shape'
        newShape = decltype(newShape){inShape.begin(), inShape.end()};
        newShape[mergeDim] *= newShape[mergeDim + 1];
        newShape.erase(newShape.begin() + mergeDim + 1);

        // => new 'axes'
        newAxes = inAxes;
        newAxes.erase(std::remove(newAxes.begin(), newAxes.end(), mergeDim + 1), newAxes.end());
        std::for_each(newAxes.begin(), newAxes.end(), [mergeDim](auto& axis) {
            axis = axis > mergeDim ? (axis - 1) : axis;
        });

        VPUX_THROW_UNLESS(newShape.size() == TARGET_TENSOR_DIM, "MVN6 5D->4D conversion failed");
    } else {
        VPUX_THROW("Unimplemented {0}D->4D convert", inRank);
    }

    const auto newShapeAttr = getIntArrayAttr(getContext(), newShape);
    auto inReshape =
            rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), origOp.input(), nullptr, false, newShapeAttr);

    const auto axisAttr = getIntArrayAttr(getContext(), newAxes);
    auto newMvnOp =
            rewriter.create<IE::MVN6Op>(origOp->getLoc(), inReshape, origOp.axes(), axisAttr,
                                        origOp.normalize_varianceAttr(), origOp.epsAttr(), origOp.eps_modeAttr());

    const auto outShapeAttr = getIntArrayAttr(getContext(), getShape(origOp.output()));
    rewriter.replaceOpWithNewOp<IE::ReshapeOp>(origOp, newMvnOp.output(), nullptr, false, outShapeAttr);

    _log.trace("[{0}] Replaced with 'IE::MVN6Op'", getDebugName());

    return mlir::success();
}

//
// StridedSliceConverter
//

class StridedSliceConverter final : public mlir::OpConversionPattern<IE::StridedSliceOp> {
public:
    StridedSliceConverter(mlir::TypeConverter& typeConverter, mlir::MLIRContext* ctx, Logger log)
            : mlir::OpConversionPattern<IE::StridedSliceOp>(typeConverter, ctx), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::StridedSliceOp origOp, OpAdaptor newArgs,
                                        mlir::ConversionPatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult StridedSliceConverter::matchAndRewrite(IE::StridedSliceOp origOp, OpAdaptor newArgs,
                                                           mlir::ConversionPatternRewriter& rewriter) const {
    _log.trace("[{0}] Found IE::StridedSliceOp Operation '{1}'", getDebugName(), origOp->getLoc());

    SmallVector<int64_t> newInputShape;

    auto begins = parseIntArrayAttr<int64_t>(origOp.begins_attr().value());
    auto ends = parseIntArrayAttr<int64_t>(origOp.ends_attr().value());
    auto strides = parseIntArrayAttr<int64_t>(origOp.strides_attr().value());
    auto beginMask = parseIntArrayAttr<int64_t>(origOp.begin_mask());
    auto endMask = parseIntArrayAttr<int64_t>(origOp.end_mask());

    SmallVector<int64_t> newAxisMask;
    SmallVector<int64_t> shrinkAxisMask;
    SmallVector<int64_t> ellipsisMask;

    if ((!origOp.new_axis_mask().empty()) && (!origOp.shrink_axis_mask().empty()) &&
        (!origOp.ellipsis_mask().empty())) {  // in the < 4D cases, if newAxisMask, shrinkAxisMask,
                                              // ellipsisMask are nullptr, they are filled with zeros in
                                              // ResolveStridedSlice pass, but this is not happening for 5D cases.
        newAxisMask = parseIntArrayAttr<int64_t>(origOp.new_axis_mask());
        shrinkAxisMask = parseIntArrayAttr<int64_t>(origOp.shrink_axis_mask());
        ellipsisMask = parseIntArrayAttr<int64_t>(origOp.ellipsis_mask());
    }

    const auto origType = origOp.input().getType().cast<vpux::NDTypeInterface>();
    const auto origRank = origType.getRank();
    const auto origShape = origType.getShape();

    if (origRank > TARGET_TENSOR_DIM) {
        SmallVector<int64_t> newBeginAttrShape;
        SmallVector<int64_t> newEndAttrShape;
        SmallVector<int64_t> newStridesAttrShape;
        SmallVector<int64_t> newBeginMaskAttrShape;
        SmallVector<int64_t> newEndMaskAttrShape;
        SmallVector<int64_t> newAxisAttrShape;
        SmallVector<int64_t> newShrinkAxisAttrShape;
        SmallVector<int64_t> newEllipsisAttrShape;

        for (int i = 0; i < origRank - TARGET_TENSOR_DIM; i++) {
            if (origRank > TARGET_TENSOR_DIM && origShape[Dim(i)] == 1) {
                if (i == origRank - TARGET_TENSOR_DIM - 1) {
                    newInputShape.append(origShape.begin() + i + 1, origShape.end());
                    std::copy(begins.begin() + i + 1, begins.end(), std::back_inserter(newBeginAttrShape));
                    std::copy(ends.begin() + i + 1, ends.end(), std::back_inserter(newEndAttrShape));
                    std::copy(strides.begin() + i + 1, strides.end(), std::back_inserter(newStridesAttrShape));
                    std::copy(beginMask.begin() + i + 1, beginMask.end(), std::back_inserter(newBeginMaskAttrShape));
                    std::copy(endMask.begin() + i + 1, endMask.end(), std::back_inserter(newEndMaskAttrShape));
                    if ((!origOp.new_axis_mask().empty()) && (!origOp.shrink_axis_mask().empty()) &&
                        (!origOp.ellipsis_mask().empty())) {
                        std::copy(newAxisMask.begin() + i + 1, newAxisMask.end(), std::back_inserter(newAxisAttrShape));
                        std::copy(shrinkAxisMask.begin() + i + 1, shrinkAxisMask.end(),
                                  std::back_inserter(newShrinkAxisAttrShape));
                        std::copy(ellipsisMask.begin() + i + 1, ellipsisMask.end(),
                                  std::back_inserter(newEllipsisAttrShape));
                    } else {
                        newAxisAttrShape = {0, 0, 0, 0};
                        newShrinkAxisAttrShape = {0, 0, 0, 0};
                        newEllipsisAttrShape = {0, 0, 0, 0};
                    }
                }
            } else {
                VPUX_THROW("The dims from range [0, origRank - TARGET_TENSOR_DIM] are not equal to 1");
            }
        }

        origType.changeShape(ShapeRef(newInputShape));

        const auto newInputShapeAttr = getIntArrayAttr(getContext(), newInputShape);
        const auto newBeginAttrShapeAttr = getIntArrayAttr(getContext(), newBeginAttrShape);
        const auto newEndAttrShapeAttr = getIntArrayAttr(getContext(), newEndAttrShape);
        const auto newStridesAttrShapeAttr = getIntArrayAttr(getContext(), newStridesAttrShape);
        const auto newBeginMaskAttrShapeAttr = getIntArrayAttr(getContext(), newBeginMaskAttrShape);
        const auto newEndMaskAttrShapeAttr = getIntArrayAttr(getContext(), newEndMaskAttrShape);

        auto newAxisAttrShapeAttr = getIntArrayAttr(getContext(), newAxisAttrShape);
        auto newShrinkAttrShapeAttr = getIntArrayAttr(getContext(), newShrinkAxisAttrShape);
        auto newEllipsisAttrShapeAttr = getIntArrayAttr(getContext(), newEllipsisAttrShape);

        auto inputReshape = rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), newArgs.input(), nullptr, false,
                                                                 newInputShapeAttr);

        auto newStridedSliceOp = rewriter.create<IE::StridedSliceOp>(
                origOp->getLoc(), inputReshape, origOp.begins(), origOp.ends(), origOp.strides(), newBeginAttrShapeAttr,
                newEndAttrShapeAttr, newStridesAttrShapeAttr, newBeginMaskAttrShapeAttr, newEndMaskAttrShapeAttr,
                newAxisAttrShapeAttr, newShrinkAttrShapeAttr, newEllipsisAttrShapeAttr);

        const auto outputShapeAttr = getIntArrayAttr(getContext(), getShape(origOp.output()));
        rewriter.replaceOpWithNewOp<IE::ReshapeOp>(origOp, newStridedSliceOp.output(), nullptr, false, outputShapeAttr);

    } else {
        newInputShape.append(origShape.begin(), origShape.end());

        for (int64_t i = 0; i < TARGET_TENSOR_DIM - origRank; ++i) {
            newInputShape.insert(newInputShape.end(), 1);
            begins.insert(begins.end(), 0);
            ends.insert(ends.end(), 1);
            strides.insert(strides.end(), 1);
            beginMask.insert(beginMask.end(), 0);
            endMask.insert(endMask.end(), 0);
            newAxisMask.insert(newAxisMask.end(), 0);
            shrinkAxisMask.insert(shrinkAxisMask.end(), 0);
            ellipsisMask.insert(ellipsisMask.end(), 0);
        }

        const auto newInputShapeAttr = getIntArrayAttr(getContext(), newInputShape);
        const auto newBeginAttrShapeAttr = getIntArrayAttr(getContext(), begins);
        const auto newEndAttrShapeAttr = getIntArrayAttr(getContext(), ends);
        const auto newStridesAttrShapeAttr = getIntArrayAttr(getContext(), strides);
        const auto newBeginMaskAttrShapeAttr = getIntArrayAttr(getContext(), beginMask);
        const auto newEndMaskAttrShapeAttr = getIntArrayAttr(getContext(), endMask);

        auto newAxisAttrShapeAttr = getIntArrayAttr(getContext(), newAxisMask);
        auto newShrinkAttrShapeAttr = getIntArrayAttr(getContext(), shrinkAxisMask);
        auto newEllipsisAttrShapeAttr = getIntArrayAttr(getContext(), ellipsisMask);

        auto inputReshape = rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), origOp.input(), nullptr, false,
                                                                 newInputShapeAttr);

        auto newStridedSliceOp = rewriter.create<IE::StridedSliceOp>(
                origOp->getLoc(), inputReshape, origOp.begins(), origOp.ends(), origOp.strides(), newBeginAttrShapeAttr,
                newEndAttrShapeAttr, newStridesAttrShapeAttr, newBeginMaskAttrShapeAttr, newEndMaskAttrShapeAttr,
                newAxisAttrShapeAttr, newShrinkAttrShapeAttr, newEllipsisAttrShapeAttr);

        const auto outputShapeAttr = getIntArrayAttr(getContext(), getShape(origOp.output()));
        rewriter.replaceOpWithNewOp<IE::ReshapeOp>(origOp, newStridedSliceOp.output(), nullptr, false, outputShapeAttr);
    }

    _log.trace("[{0}] Replaced with 'IE::StridedSlice'", getDebugName());

    return mlir::success();
}

//
// TransposeConverter
//

class TransposeConverter final : public mlir::OpConversionPattern<IE::TransposeOp> {
public:
    TransposeConverter(mlir::TypeConverter& typeConverter, mlir::MLIRContext* ctx, Logger log)
            : mlir::OpConversionPattern<IE::TransposeOp>(typeConverter, ctx), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::TransposeOp origOp, OpAdaptor newArgs,
                                        mlir::ConversionPatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult TransposeConverter::matchAndRewrite(IE::TransposeOp origOp, OpAdaptor,
                                                        mlir::ConversionPatternRewriter& rewriter) const {
    _log.trace("[{0}] Found IE::Transpose Operation '{1}'", getDebugName(), origOp->getLoc());

    const auto origOrder = DimsOrder::fromAffineMap(origOp.order_value().value());
    const auto origPermVec = origOrder.toPermutation();

    const auto origType = origOp.input().getType().cast<vpux::NDTypeInterface>();
    const auto inputShape = origType.getShape().raw();

    SmallVector<uint32_t> permVec;
    int32_t addDims = (int32_t)(TARGET_TENSOR_DIM - origType.getRank());

    for (auto d : origPermVec) {
        permVec.push_back(static_cast<uint32_t>(d.ind() + addDims));
    }

    for (auto i = 0; i < addDims; i++) {
        permVec.insert(permVec.begin(), addDims - i - 1);
    }

    SmallVector<int64_t> newInputShape(addDims, 1);

    for (auto i = 0; i < origType.getRank(); i++) {
        newInputShape.push_back(inputShape[i]);
    }

    const auto newOrderAffineMap = mlir::AffineMap::getPermutationMap(makeArrayRef(permVec), rewriter.getContext());

    const auto newInputShapeAttr = getIntArrayAttr(getContext(), newInputShape);

    auto inputReshape =
            rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), origOp.input(), nullptr, false, newInputShapeAttr);

    auto newTransposeOp = rewriter.create<IE::TransposeOp>(origOp->getLoc(), inputReshape, nullptr,
                                                           mlir::AffineMapAttr::get(newOrderAffineMap));

    const auto outputShapeAttr = getIntArrayAttr(getContext(), getShape(origOp.output()));
    rewriter.replaceOpWithNewOp<IE::ReshapeOp>(origOp, newTransposeOp.output(), nullptr, false, outputShapeAttr);

    _log.trace("[{0}] Replaced with 'IE::Tranpose'", getDebugName());

    return mlir::success();
}

//
// SoftmaxConverter
//

class SoftmaxConverter final : public mlir::OpConversionPattern<IE::SoftMaxOp> {
public:
    SoftmaxConverter(mlir::TypeConverter& typeConverter, mlir::MLIRContext* ctx, Logger log)
            : mlir::OpConversionPattern<IE::SoftMaxOp>(typeConverter, ctx), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::SoftMaxOp origOp, OpAdaptor newArgs,
                                        mlir::ConversionPatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult SoftmaxConverter::matchAndRewrite(IE::SoftMaxOp origOp, OpAdaptor,
                                                      mlir::ConversionPatternRewriter& rewriter) const {
    _log.trace("[{0}] Found IE::Softmax Operation '{1}'", getDebugName(), origOp->getLoc());

    const auto origType = origOp.input().getType().cast<vpux::NDTypeInterface>();
    const auto inputShape = origType.getShape().raw();
    // Only support dimension expansion
    if (origType.getRank() >= TARGET_TENSOR_DIM) {
        return mlir::failure();
    }

    // Support two cases:
    // when the meaningful shape (not 1) is 1 and axis is on that dimension,
    //      put the dimension to W to keep the original method
    // for other cases, put the non-1 dimensions to C and H
    //      to increase the possibility of multi-cluster and tiling
    // e.g. [32, 10] -> [1, 32, 10, 1]
    //      [1, 51] -> [1, 1, 1, 51]
    //      [1, 32, 10] -> [1, 32, 10, 1]
    int64_t axis = origOp.axisInd();
    if (axis < 0) {
        axis += origType.getRank();
    }

    auto isSingleDimSoftMax = [&]() {
        return llvm::all_of(irange(origType.getRank()), [&](int64_t ind) {
            if (inputShape[ind] != 1 && ind != axis) {
                return false;
            }
            return true;
        });
    };

    // Optimization for softmax kernel should make axis last dim.
    // Maintain axis last dim after being reshaped to 4D.
    auto isTwoDimAxisLastSoftMax = [&]() {
        const auto rank = origType.getRank();
        return rank == 2 && axis == rank - 1;
    };

    SmallVector<int64_t> newInputShape;
    auto addDims = static_cast<int32_t>(TARGET_TENSOR_DIM - origType.getRank());
    int64_t newAxis = axis;
    if (isSingleDimSoftMax() || isTwoDimAxisLastSoftMax()) {
        newInputShape = SmallVector<int64_t>(addDims, 1);
        for (auto i = 0; i < origType.getRank(); i++) {
            newInputShape.push_back(inputShape[i]);
        }
        newAxis = axis + addDims;
    } else {
        // set batch = 1 and enable more axis to split
        if (inputShape[0] != 1) {
            newInputShape.push_back(1);
            addDims--;
            newAxis++;
        }

        for (auto i = 0; i < origType.getRank(); i++) {
            newInputShape.push_back(inputShape[i]);
        }

        for (auto i = 0; i < addDims; i++) {
            newInputShape.push_back(1);
        }
    }

    const auto newInputShapeAttr = getIntArrayAttr(getContext(), newInputShape);
    auto inputReshape =
            rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), origOp.input(), nullptr, false, newInputShapeAttr);

    const auto axisAttr = getIntAttr(getContext(), newAxis);
    auto newSoftmaxOp = rewriter.create<IE::SoftMaxOp>(origOp->getLoc(), inputReshape, axisAttr, origOp.padSizeAttr());

    const auto outputShapeAttr = getIntArrayAttr(getContext(), getShape(origOp.output()));
    rewriter.replaceOpWithNewOp<IE::ReshapeOp>(origOp, newSoftmaxOp.output(), nullptr, false, outputShapeAttr);

    _log.trace("[{0}] Replaced with 'IE::SoftMaxOp'", getDebugName());

    return mlir::success();
}

//
// InterpolateConverter
//

class InterpolateConverter final : public mlir::OpConversionPattern<IE::InterpolateOp> {
public:
    InterpolateConverter(mlir::TypeConverter& typeConverter, mlir::MLIRContext* ctx, Logger log)
            : mlir::OpConversionPattern<IE::InterpolateOp>(typeConverter, ctx), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::InterpolateOp origOp, OpAdaptor newArgs,
                                        mlir::ConversionPatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult InterpolateConverter::matchAndRewrite(IE::InterpolateOp origOp, OpAdaptor,
                                                          mlir::ConversionPatternRewriter& rewriter) const {
    _log.trace("[{0}] Found IE::Interpolate Operation '{1}'", getDebugName(), origOp->getLoc());

    const auto inputShape = getShape(origOp.input()).raw();
    const auto inputRank = inputShape.size();

    VPUX_THROW_WHEN(inputRank > TARGET_TENSOR_DIM, "Tensors with rank > 4 are not supported");

    const auto addDims = static_cast<int64_t>(TARGET_TENSOR_DIM - inputRank);

    const auto createAxesAttr = [&](Optional<mlir::ArrayAttr> axesAttr) {
        if (axesAttr.has_value()) {
            auto intArray = parseIntArrayAttr<int64_t>(axesAttr.value());
            for (auto& val : intArray) {
                val += addDims;
            }
            SmallVector<unsigned> sortIndexArray(addDims);
            std::iota(sortIndexArray.begin(), sortIndexArray.end(), 0);
            intArray.insert(intArray.begin(), sortIndexArray.begin(), sortIndexArray.end());
            return getIntArrayAttr(this->getContext(), intArray);
        }
        return mlir::ArrayAttr();
    };

    const auto extendShapeWithValue = [&](Optional<mlir::ArrayAttr> attr, int64_t value) {
        if (attr.has_value()) {
            auto intArray = parseIntArrayAttr<int64_t>(attr.value());
            intArray.insert(intArray.begin(), addDims, value);
            return getIntArrayAttr(this->getContext(), intArray);
        }
        return mlir::ArrayAttr();
    };

    const auto extendShapeWithFloatValue = [&](Optional<mlir::ArrayAttr> attr, double value) {
        if (attr.has_value()) {
            auto fpArray = parseFPArrayAttr<double>(attr.value());
            fpArray.insert(fpArray.begin(), addDims, value);
            return getFPArrayAttr(this->getContext(), fpArray);
        }
        return mlir::ArrayAttr();
    };

    SmallVector<int64_t> newInputShape(addDims, 1);
    newInputShape.insert(newInputShape.end(), inputShape.begin(), inputShape.end());
    const auto newInputShapeAttr = getIntArrayAttr(this->getContext(), newInputShape);
    auto inputReshape =
            rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), origOp.input(), nullptr, false, newInputShapeAttr);

    const auto attrs = origOp.attr();
    const auto newPadsBeginAttr = extendShapeWithValue(attrs.getPadsBegin(), 0);
    const auto newPadsEndAttr = extendShapeWithValue(attrs.getPadsEnd(), 0);
    const auto newAttr = IE::InterpolateAttr::get(this->getContext(), attrs.getMode(), attrs.getShapeCalcMode(),
                                                  attrs.getCoordMode(), attrs.getNearestMode(), attrs.getAntialias(),
                                                  newPadsBeginAttr, newPadsEndAttr, attrs.getCubeCoeff());

    const auto newAxesAttr = createAxesAttr(origOp.axes_attr());
    const auto newSizesAttr = extendShapeWithValue(origOp.sizes_attr(), 1);
    const auto newScalesAttr = extendShapeWithFloatValue(origOp.scales_attr(), 1.0);
    const auto newOffsetAttr = extendShapeWithValue(origOp.tile_offset_attr(), 0);
    const auto newInitInputDimAttr = extendShapeWithValue(origOp.initial_input_dims_attr(), 1);
    const auto newInitOutputDimAttr = extendShapeWithValue(origOp.initial_output_dims_attr(), 1);
    auto newInterpOp = rewriter.create<IE::InterpolateOp>(origOp->getLoc(), inputReshape, nullptr, nullptr, nullptr,
                                                          newSizesAttr, newScalesAttr, newAxesAttr, newOffsetAttr,
                                                          newInitInputDimAttr, newInitOutputDimAttr, newAttr);

    const auto outShape = getShape(origOp.output());
    const auto outputShapeAttr = getIntArrayAttr(this->getContext(), outShape);
    rewriter.replaceOpWithNewOp<IE::ReshapeOp>(origOp, newInterpOp.output(), nullptr, false, outputShapeAttr);

    _log.trace("[{0}] Replaced with 'IE::InterpolateOp'", getDebugName());

    return mlir::success();
}

//
// GatherConverter
//

class GatherConverter final : public mlir::OpConversionPattern<IE::GatherOp> {
public:
    GatherConverter(mlir::TypeConverter& typeConverter, mlir::MLIRContext* ctx, Logger log)
            : mlir::OpConversionPattern<IE::GatherOp>(typeConverter, ctx), _log(log) {
    }

public:
    mlir::LogicalResult matchAndRewrite(IE::GatherOp origOp, OpAdaptor newArgs,
                                        mlir::ConversionPatternRewriter& rewriter) const final;

private:
    Logger _log;
};

mlir::LogicalResult GatherConverter::matchAndRewrite(IE::GatherOp origOp, OpAdaptor,
                                                     mlir::ConversionPatternRewriter& rewriter) const {
    _log.trace("[{0}] Found IE::Gather Operation '{1}'", getDebugName(), origOp->getLoc());

    const auto origType = origOp.input().getType().cast<vpux::NDTypeInterface>();
    const auto inputShape = origType.getShape().raw();

    int64_t axis = origOp.axis_value().value();
    if (axis < 0) {
        axis += origType.getRank();
    }

    auto addDims = static_cast<int32_t>(TARGET_TENSOR_DIM - origType.getRank());
    SmallVector<int64_t> newInputShape(addDims, 1);

    for (auto i = 0; i < origType.getRank(); i++) {
        newInputShape.push_back(inputShape[i]);
    }

    const auto newInputShapeAttr = getIntArrayAttr(getContext(), newInputShape);
    auto inputReshape =
            rewriter.createOrFold<IE::ReshapeOp>(origOp->getLoc(), origOp.input(), nullptr, false, newInputShapeAttr);

    int64_t newAxis = axis + addDims;
    const auto axisAttr = getIntAttr(getContext(), newAxis);

    auto newGatherOp = rewriter.create<IE::GatherOp>(origOp.getLoc(), inputReshape, origOp.indices(), nullptr, axisAttr,
                                                     origOp.batch_dims());
    const auto outputShapeAttr = getIntArrayAttr(getContext(), getShape(origOp.output()));
    rewriter.replaceOpWithNewOp<IE::ReshapeOp>(origOp, newGatherOp.output(), nullptr, false, outputShapeAttr);

    _log.trace("[{0}] Replaced with 'IE::GatherOp'", getDebugName());

    return mlir::success();
}

//
// safeRunOnFunc
//

void ConvertShapeTo4DPass::safeRunOnFunc() {
    auto& ctx = getContext();
    auto func = getOperation();

    const auto reshape = [](mlir::OpBuilder& builder, mlir::RankedTensorType dstType, mlir::ValueRange inputs,
                            mlir::Location loc) -> mlir::Value {
        VPUX_THROW_UNLESS(inputs.size() == 1, "Got wrong number of inputs : {0}", inputs.size());

        const auto outShapeAttr = builder.getI64ArrayAttr(dstType.getShape());
        return builder.createOrFold<IE::ReshapeOp>(loc, inputs.front(), nullptr, false, outShapeAttr);
    };

    mlir::TypeConverter typeConverter;
    typeConverter.addConversion([](vpux::NDTypeInterface type) {
        SmallVector<int64_t> shape = to_small_vector(type.getShape());
        auto dimMapper = getDimMapGeneric(shape);
        return type.changeShape(ShapeRef(alignShapeTo4D(shape, dimMapper)));
    });
    typeConverter.addSourceMaterialization(reshape);
    typeConverter.addTargetMaterialization(reshape);
    typeConverter.addArgumentMaterialization(reshape);

    const auto isLegalOp = [&](mlir::Operation* op) {
        return typeConverter.isLegal(op);
    };

    const auto isLegalFqOp = [&](IE::FakeQuantizeOp op) {
        const auto inShape = op.input().getType().cast<vpux::NDTypeInterface>().getShape();
        const auto outShape = op.output().getType().cast<vpux::NDTypeInterface>().getShape();

        VPUX_THROW_WHEN(inShape != outShape,
                        "FakeQuantize must have the same shape for input and output. Got: {0} != {1}", inShape,
                        outShape);

        return inShape.size() == TARGET_TENSOR_DIM;
    };

    const auto isLegalEltwiseOp = [&](mlir::Operation* op) {
        if (op->getNumOperands() < 2) {
            return true;
        }
        const auto inOneShape = op->getOperand(0).getType().cast<vpux::NDTypeInterface>().getShape();
        const auto inTwoShape = op->getOperand(1).getType().cast<vpux::NDTypeInterface>().getShape();
        return (inOneShape.size() == TARGET_TENSOR_DIM) && (inTwoShape.size() == TARGET_TENSOR_DIM);
    };

    const auto is4DLegalOp = [&](mlir::Operation* op) {
        const auto inShape = op->getOperand(0).getType().cast<vpux::NDTypeInterface>().getShape();
        return inShape.size() == TARGET_TENSOR_DIM;
    };

    const auto isLegalTransposeOp = [&](IE::TransposeOp op) {
        // Support non-4D tranpose
        // dimension rank >= 5 was supported and optimized by LegalizeNDMemPermutePass.
        const auto inShape = getShape(op.input());

        return (inShape.size() >= TARGET_TENSOR_DIM) || (inShape.size() == 1);
    };

    mlir::ConversionTarget target(ctx);
    target.addLegalDialect<Const::ConstDialect>();
    target.addLegalDialect<IE::IEDialect>();
    target.addLegalOp<mlir::ModuleOp>();
    target.addLegalOp<mlir::func::FuncOp>();
    target.addLegalOp<mlir::func::ReturnOp>();
    target.addDynamicallyLegalOp<IE::ClampOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::EluOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::ReLUOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::SigmoidOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::HSwishOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::SwishOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::TanhOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::SinOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::CosOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::SqrtOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::SinhOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::CoshOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::AsinhOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::AcoshOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::AtanhOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::ExpOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::GeluOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::DivideOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::MinimumOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::MaximumOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::PowerOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::AndOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::ScaleShiftOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::EqualOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::NotEqualOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::FakeQuantizeOp>(isLegalFqOp);
    target.addDynamicallyLegalOp<IE::LessOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::SelectOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::LessEqualOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::GreaterOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::GreaterEqualOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::LogicalNotOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::LogicalOrOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::LogicalXorOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::AbsOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::AtanOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::AsinOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::AcosOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::PReluOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::LeakyReluOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::AddOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::MultiplyOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::SubtractOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::TopKOp>(is4DLegalOp);
    target.addDynamicallyLegalOp<IE::MVN6Op>(is4DLegalOp);
    target.addDynamicallyLegalOp<IE::FloorModOp>(isLegalEltwiseOp);
    target.addDynamicallyLegalOp<IE::StridedSliceOp>(is4DLegalOp);
    target.addDynamicallyLegalOp<IE::TransposeOp>(isLegalTransposeOp);
    target.addDynamicallyLegalOp<IE::SoftMaxOp>(is4DLegalOp);
    target.addDynamicallyLegalOp<IE::InterpolateOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::FloorOp>(isLegalOp);
    target.addDynamicallyLegalOp<IE::SquaredDifferenceOp>(isLegalOp);

    mlir::RewritePatternSet patterns(&ctx);
    patterns.add<GenericConverter<IE::ClampOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::EluOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::ReLUOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::SigmoidOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::HSwishOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::SwishOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::TanhOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::SinOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::CosOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::SqrtOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::SinhOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::CoshOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::AsinhOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::AcoshOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::AtanhOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::ExpOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::GeluOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::DivideOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::MinimumOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::MaximumOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::PowerOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::AndOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::ScaleShiftOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::EqualOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::LessOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::SelectOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::LessEqualOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::NotEqualOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::GreaterOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::GreaterEqualOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::LogicalNotOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::LogicalOrOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::LogicalXorOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::AbsOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::AtanOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::AsinOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::AcosOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::PReluOp>>(typeConverter, &ctx, _log);
    auto module = getOperation();
    const auto arch = VPU::getArch(module);
    if (arch == VPU::ArchKind::VPUX37XX) {
        target.addDynamicallyLegalOp<IE::ConvertOp>(isLegalOp);
        patterns.add<GenericConverter<IE::ConvertOp>>(typeConverter, &ctx, _log);
    }
    if (arch == VPU::ArchKind::VPUX30XX) {
        target.addDynamicallyLegalOp<IE::GatherOp>(is4DLegalOp);
        patterns.add<GatherConverter>(typeConverter, &ctx, _log);
    }
    patterns.add<GenericConverter<IE::LeakyReluOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::FloorOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::FloorModOp>>(typeConverter, &ctx, _log);
    patterns.add<FakeQuantizeConverter>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::AddOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::MultiplyOp>>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::SubtractOp>>(typeConverter, &ctx, _log);
    patterns.add<TopKOpConverter>(typeConverter, &ctx, _log);
    patterns.add<Mvn6Converter>(typeConverter, &ctx, _log);
    patterns.add<StridedSliceConverter>(typeConverter, &ctx, _log);
    patterns.add<TransposeConverter>(typeConverter, &ctx, _log);
    patterns.add<SoftmaxConverter>(typeConverter, &ctx, _log);
    patterns.add<InterpolateConverter>(typeConverter, &ctx, _log);
    patterns.add<GenericConverter<IE::SquaredDifferenceOp>>(typeConverter, &ctx, _log);

    if (mlir::failed(mlir::applyPartialConversion(func, target, std::move(patterns)))) {
        signalPassFailure();
    }
}

}  // namespace

//
// createConvertShapeTo4DPass
//

std::unique_ptr<mlir::Pass> vpux::IE::createConvertShapeTo4DPass(Logger log) {
    return std::make_unique<ConvertShapeTo4DPass>(log);
}
