//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include "vpux/compiler/dialect/VPU/ops.hpp"

using namespace vpux;

//
// inferReturnTypes
//

mlir::LogicalResult VPU::DetectionOutputDecodeBoxesOp::inferReturnTypes(
        mlir::MLIRContext* ctx, mlir::Optional<mlir::Location> optLoc, mlir::ValueRange operands,
        mlir::DictionaryAttr attrs, mlir::RegionRange /*regions*/,
        mlir::SmallVectorImpl<mlir::Type>& inferredReturnTypes) {
    const auto loc = optLoc.value_or(mlir::UnknownLoc::get(ctx));

    VPU::DetectionOutputDecodeBoxesOpAdaptor decodeBoxes(operands, attrs);
    if (mlir::failed(decodeBoxes.verify(loc))) {
        return mlir::failure();
    }

    const auto inputType = decodeBoxes.box_logits().getType().cast<NDTypeInterface>();
    inferredReturnTypes.push_back(inputType);

    return mlir::success();
}

//
// serialize
//

EMU::BlobWriter::SpecificTask VPU::DetectionOutputDecodeBoxesOp::serialize(EMU::BlobWriter&) {
    VPUX_THROW("VPU::DetectionOutputDecodeBoxesOp is not supported by EMU");
}

//
// TilingBuilderOpInterface
//

InputTiling vpux::VPU::DetectionOutputDecodeBoxesOp::backInferTileInfo(const vpux::TileInfo& outputTile,
                                                                       vpux::Logger /*log*/) {
    // The batch dimension of 2nd input might be equal to 1, or to N
    const auto priorBoxShape = getShape(prior_boxes());
    auto priorBoxesTile = TileInfo(priorBoxShape);

    // Use a whole priorBoxes tensor, except when we are tiling on H dimension
    priorBoxesTile.shape[Dims4D::Act::H] = outputTile.shape[Dims4D::Act::H];
    priorBoxesTile.offsets[Dims4D::Act::H] = outputTile.offsets[Dims4D::Act::H];

    return InputTiling{{outputTile, std::move(priorBoxesTile)}};
}

void vpux::VPU::DetectionOutputDecodeBoxesOp::adjustAttrs(const TilingInfo&, const TileInfo&) {
}

mlir::FailureOr<OutputTiling> vpux::VPU::DetectionOutputDecodeBoxesOp::getTilingStrategy(TilingMode tilingMode,
                                                                                         Logger log) {
    return vpux::getSWLayerTilingStrategy(this->getOperation(), tilingMode, log);
}
