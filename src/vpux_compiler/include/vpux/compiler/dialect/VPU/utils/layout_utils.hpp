//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include "vpux/compiler/dialect/VPU/ops.hpp"

namespace vpux {
namespace VPU {

mlir::LogicalResult verifyOpLayout(mlir::Operation* op);

//
// SameInOutDefaultDimsOrder
//

mlir::LogicalResult verifySameInOutDefaultDimsOrder(mlir::Operation* op);
void inferLayoutInfoSameInOutDefaultDimsOrder(IE::LayerLayoutInfo& info);

mlir::LogicalResult verifyDefaultDimsOrder(mlir::Operation* op);
void inferLayoutInfoDefaultDimsOrder(IE::LayerLayoutInfo& info);

mlir::LogicalResult verifySameAnyDimsOrder(mlir::Operation* op);
void inferLayoutInfoSameAnyDimsOrder(IE::LayerLayoutInfo& info);

mlir::LogicalResult verifySameInOutSpecificDimsOrder(mlir::Operation* op, ArrayRef<DimsOrder> supportedLayouts);
void inferLayoutInfoSameInOutSpecificDimsOrder(IE::LayerLayoutInfo& info, ArrayRef<DimsOrder> supportedLayouts);
mlir::LogicalResult verifySameMultipleInOutSpecificDimsOrder(mlir::Operation* op, ArrayRef<DimsOrder> supportedLayouts);

mlir::LogicalResult verifyReduceLayoutInfo(mlir::Operation* op);
void inferReduceLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info);

mlir::FailureOr<DimsOrder> inferAffineReshapeOutputLayout(const DimArr& inPerm, mlir::ArrayAttr dimMapAttr);
mlir::LogicalResult verifyAffineReshapeLayoutInfo(mlir::Operation* op);
void inferAffineReshapeLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info);

mlir::LogicalResult verifyRegionYoloLayoutInfo(mlir::Operation* op);
void inferRegionYoloLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info);

mlir::LogicalResult verifyInterpolateLayoutInfo(mlir::Operation* op);
void inferInterpolateLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info);

mlir::LogicalResult verifyQuantizeLayoutInfo(mlir::Operation* op);
void inferQuantizeLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info);
mlir::LogicalResult verifyDequantizeLayoutInfo(mlir::Operation* op);
void inferDequantizeLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info);

DimsOrder inferSqueezeOutputLayout(const DimArr& inPerm, const SmallVector<int64_t>& axesVec,
                                   ArrayRef<int64_t> inShape);
DimsOrder inferUnsqueezeOutputLayout(const DimArr& inPerm, const SmallVector<int64_t>& axesVec,
                                     ArrayRef<int64_t> inShape);
void inferSqueezeUnsqueezeLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info);

mlir::LogicalResult verifyNCEConvolutionLayoutInfo(mlir::Operation* op);
mlir::LogicalResult verifyTopKLayoutInfo(mlir::Operation* op);

template <class OrigOpType, class FallbackSWImplOpType, class FallbackHWImplOpType>
class LayoutInfoOpModelForHW final :
        public IE::LayoutInfoOpInterface::ExternalModel<
                LayoutInfoOpModelForHW<OrigOpType, FallbackSWImplOpType, FallbackHWImplOpType>, OrigOpType> {
public:
    void inferLayoutInfo(mlir::Operation* origOp, IE::LayerLayoutInfo& info, const bool seOpsEnabled) const {
        if (!canBeExecutedOnNCE(origOp, seOpsEnabled)) {
            FallbackSWImplOpType::inferLayoutInfo(origOp, info, seOpsEnabled);
            return;
        }

        FallbackHWImplOpType::inferLayoutInfo(origOp, info, seOpsEnabled);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return IE::verifyLayout(origOp);
    }

private:
    static bool canBeExecutedOnNCE(mlir::Operation* op, const bool seOpsEnabled) {
        if (VPU::getCompilationMode(op) == VPU::CompilationMode::ReferenceSW) {
            // We are in reference SW compilation mode
            return false;
        }

        if (!seOpsEnabled && mlir::isa<IE::SEOpInterface>(op)) {
            return false;
        }

        if (VPU::NCEInvariant::isSupported(op).failed()) {
            // Basic NCE invariants check failed, the operation will fallback to SW mode
            return false;
        }

        return true;
    }
};

//
// AnyDimsOrderOpModelForSW
//

class AnyDimsOrderOpModelForSW final : public IE::LayoutInfoOpInterface::FallbackModel<AnyDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& /*info*/, const bool /*seOpsEnabled*/) {
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return IE::verifyLayout(origOp);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// SameAnyDimsOrderOpModelForSW
//

class SameAnyDimsOrderOpModelForSW final :
        public IE::LayoutInfoOpInterface::FallbackModel<SameAnyDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferLayoutInfoSameAnyDimsOrder(info);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifySameAnyDimsOrder(origOp);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// SameInOutDefaultDimsOrderOpModelForSW
//

class SameInOutDefaultDimsOrderOpModelForSW final :
        public IE::LayoutInfoOpInterface::FallbackModel<SameInOutDefaultDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferLayoutInfoSameInOutDefaultDimsOrder(info);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifySameInOutDefaultDimsOrder(origOp);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// DefaultDimsOrderOpModelForSW
//

class DefaultDimsOrderOpModelForSW final :
        public IE::LayoutInfoOpInterface::FallbackModel<DefaultDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation*, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferLayoutInfoDefaultDimsOrder(info);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifyDefaultDimsOrder(origOp);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// SameInOutDimsOrderOpModelForSW_CHW_HWC_NCHW_NHWC
//

class SameInOutDimsOrderOpModelForSW_CHW_HWC_NCHW_NHWC final :
        public IE::LayoutInfoOpInterface::FallbackModel<SameInOutDimsOrderOpModelForSW_CHW_HWC_NCHW_NHWC> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferLayoutInfoSameInOutSpecificDimsOrder(
                info, {DimsOrder::CHW, DimsOrder::HWC, DimsOrder::NCHW, DimsOrder::NHWC});
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifySameInOutSpecificDimsOrder(
                origOp, {DimsOrder::CHW, DimsOrder::HWC, DimsOrder::NCHW, DimsOrder::NHWC});
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// SameInOutDimsOrderOpModelForSW_NC_CHW_HWC_NCHW_NHWC
//

class SameInOutDimsOrderOpModelForSW_NC_CHW_HWC_NCHW_NHWC final :
        public IE::LayoutInfoOpInterface::FallbackModel<SameInOutDimsOrderOpModelForSW_NC_CHW_HWC_NCHW_NHWC> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferLayoutInfoSameInOutSpecificDimsOrder(
                info, {DimsOrder::NC, DimsOrder::CHW, DimsOrder::HWC, DimsOrder::NCHW, DimsOrder::NHWC});
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifySameInOutSpecificDimsOrder(
                origOp, {DimsOrder::NC, DimsOrder::CHW, DimsOrder::HWC, DimsOrder::NCHW, DimsOrder::NHWC});
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// SameInOutDimsOrderOpModelForSW_NCHW_NHWC
//

class SameInOutDimsOrderOpModelForSW_NCHW_NHWC final :
        public IE::LayoutInfoOpInterface::FallbackModel<SameInOutDimsOrderOpModelForSW_NCHW_NHWC> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferLayoutInfoSameInOutSpecificDimsOrder(info, {DimsOrder::NCHW, DimsOrder::NHWC});
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifySameInOutSpecificDimsOrder(origOp, {DimsOrder::NCHW, DimsOrder::NHWC});
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// SameInOutDimsOrderOpModelForSW_NCHW
//

class SameInOutDimsOrderOpModelForSW_NCHW final :
        public IE::LayoutInfoOpInterface::FallbackModel<SameInOutDimsOrderOpModelForSW_NCHW> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferLayoutInfoSameInOutSpecificDimsOrder(info, {DimsOrder::NCHW});
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifySameInOutSpecificDimsOrder(origOp, {DimsOrder::NCHW});
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// SameInOutDimsOrderOpModelForSW_NCHW_NCWH_NHWC_NWHC
//

class SameInOutDimsOrderOpModelForSW_NCHW_NCWH_NHWC_NWHC final :
        public IE::LayoutInfoOpInterface::FallbackModel<SameInOutDimsOrderOpModelForSW_NCHW_NCWH_NHWC_NWHC> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferLayoutInfoSameInOutSpecificDimsOrder(
                info, {DimsOrder::NCHW, DimsOrder::NCWH, DimsOrder::NHWC, DimsOrder::NWHC});
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifySameInOutSpecificDimsOrder(
                origOp, {DimsOrder::NCHW, DimsOrder::NCWH, DimsOrder::NHWC, DimsOrder::NWHC});
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// SameInOutDimsOrderOpModelForSW_NHWC
//

class SameInOutDimsOrderOpModelForSW_NHWC final :
        public IE::LayoutInfoOpInterface::FallbackModel<SameInOutDimsOrderOpModelForSW_NHWC> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferLayoutInfoSameInOutSpecificDimsOrder(info, {DimsOrder::NHWC});
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifySameInOutSpecificDimsOrder(origOp, {DimsOrder::NHWC});
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// ReduceDimsOrderOpModelForSW
//

class ReduceDimsOrderOpModelForSW final : public IE::LayoutInfoOpInterface::FallbackModel<ReduceDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferReduceLayoutInfo(op, info);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifyReduceLayoutInfo(origOp);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// AffineReshapeDimsOrderOpModelForSW
//

class AffineReshapeDimsOrderOpModelForSW final :
        public IE::LayoutInfoOpInterface::FallbackModel<AffineReshapeDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferAffineReshapeLayoutInfo(op, info);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifyAffineReshapeLayoutInfo(origOp);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// QuantizeDimsOrderOpModelForSW
//

class QuantizeDimsOrderOpModelForSW final :
        public IE::LayoutInfoOpInterface::FallbackModel<QuantizeDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferQuantizeLayoutInfo(op, info);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* op) const {
        return VPU::verifyQuantizeLayoutInfo(op);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// DequantizeDimsOrderOpModelForSW
//

class DequantizeDimsOrderOpModelForSW final :
        public IE::LayoutInfoOpInterface::FallbackModel<DequantizeDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferDequantizeLayoutInfo(op, info);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* op) const {
        return VPU::verifyDequantizeLayoutInfo(op);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// SqueezeUnsqueezeDimsOrderOpModelForSW
//

class SqueezeUnsqueezeDimsOrderOpModelForSW final :
        public IE::LayoutInfoOpInterface::FallbackModel<SqueezeUnsqueezeDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferSqueezeUnsqueezeLayoutInfo(op, info);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation*) const {
        return mlir::success();
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// RegionYoloDimsOrderOpModelForSW
//

class RegionYoloDimsOrderOpModelForSW final :
        public IE::LayoutInfoOpInterface::FallbackModel<RegionYoloDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferRegionYoloLayoutInfo(op, info);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* op) const {
        return VPU::verifyRegionYoloLayoutInfo(op);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// InterpolateDimsOrderOpModelForSW
//

class InterpolateDimsOrderOpModelForSW final :
        public IE::LayoutInfoOpInterface::FallbackModel<InterpolateDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* op, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        VPU::inferInterpolateLayoutInfo(op, info);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* op) const {
        return VPU::verifyInterpolateLayoutInfo(op);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// NCEConvolutionDimsOrderOpModelForHW
//

class NCEConvolutionDimsOrderOpModelForHW final :
        public IE::LayoutInfoOpInterface::FallbackModel<NCEConvolutionDimsOrderOpModelForHW> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        info.setInput(0, DimsOrder::NHWC);
        info.setInput(1, DimsOrder::OYXI);
        info.setOutput(0, DimsOrder::NHWC);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* op) const {
        return VPU::verifyNCEConvolutionLayoutInfo(op);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// PermuteQuantizeDimsOrderOpModelForSW
//

class PermuteQuantizeDimsOrderOpModelForSW final :
        public IE::LayoutInfoOpInterface::FallbackModel<PermuteQuantizeDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        info.setInput(0, DimsOrder::NHWC);
        info.setOutput(0, DimsOrder::NWCH);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation*) const {
        // Tracking number [E#86928]
        return mlir::success();
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// SameMultipleInOutDimsOrderOpModelForHW_NHWC
//

class SameMultipleInOutDimsOrderOpModelForHW_NHWC final :
        public IE::LayoutInfoOpInterface::FallbackModel<SameMultipleInOutDimsOrderOpModelForHW_NHWC> {
public:
    static void inferLayoutInfo(mlir::Operation*, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        info.fill(DimsOrder::NHWC);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifySameMultipleInOutSpecificDimsOrder(origOp, {DimsOrder::NHWC});
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// SameInOutDimsOrderOpModelForHW_NHWC
//

class SameInOutDimsOrderOpModelForHW_NHWC final :
        public IE::LayoutInfoOpInterface::FallbackModel<SameInOutDimsOrderOpModelForHW_NHWC> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        info.fill(DimsOrder::NHWC);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* origOp) const {
        return VPU::verifySameInOutSpecificDimsOrder(origOp, {DimsOrder::NHWC});
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

//
// TopKSameInOutDimsOrderOpModelForSW
//

class TopKSameInOutDimsOrderOpModelForSW final :
        public IE::LayoutInfoOpInterface::FallbackModel<TopKSameInOutDimsOrderOpModelForSW> {
public:
    static void inferLayoutInfo(mlir::Operation* /*op*/, IE::LayerLayoutInfo& info, const bool /*seOpsEnabled*/) {
        const auto inOrder = info.getInput(0);

        info.setInput(0, inOrder);
        info.setOutput(0, inOrder);
        info.setOutput(1, inOrder);
    }

    mlir::LogicalResult verifyLayout(mlir::Operation* op) const {
        return VPU::verifyTopKLayoutInfo(op);
    }

    IE::LayerLayoutInfo getLayoutInfo(mlir::Operation* origOp) const {
        return IE::getLayoutInfo(origOp);
    }
};

}  // namespace VPU
}  // namespace vpux
