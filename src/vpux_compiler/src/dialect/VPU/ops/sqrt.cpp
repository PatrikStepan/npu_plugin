//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPU/ops.hpp"

using namespace vpux;

mlir::LogicalResult vpux::VPU::SqrtOp::inferReturnTypes(mlir::MLIRContext* ctx, mlir::Optional<mlir::Location> optLoc,
                                                        mlir::ValueRange operands, mlir::DictionaryAttr attrs,
                                                        mlir::RegionRange /*regions*/,
                                                        mlir::SmallVectorImpl<mlir::Type>& inferredReturnTypes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    VPU::SqrtOpAdaptor sqrt(operands, attrs);
    if (mlir::failed(sqrt.verify(loc))) {
        return mlir::failure();
    }

    const auto inType = sqrt.input().getType();
    inferredReturnTypes.push_back(inType);

    return mlir::success();
}

//
// serialize
//

EMU::BlobWriter::SpecificTask vpux::VPU::SqrtOp::serialize(EMU::BlobWriter& writer) {
    const auto sqrt = MVCNN::CreateSqrtParams(writer);

    MVCNN::PostOpsParamsBuilder builder(writer);
    builder.add_nested_params_type(MVCNN::PostOpsNestedParams_SqrtParams);
    builder.add_nested_params(sqrt.Union());
    const auto paramsOff = builder.Finish();

    return writer.createUPALayerTask(*this, {paramsOff.Union(), MVCNN::SoftwareLayerParams_PostOpsParams});
}
