//
// Copyright (C) 2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include <vector>
#include "vpux/compiler/utils/bit_compactor_codec.hpp"
#include "vpux/utils/core/error.hpp"

namespace vpux {

std::unique_ptr<ICodec> getBitCompactorCodec(VPU::ArchKind arch) {
    switch (arch) {
#ifdef ENABLE_BITCOMPACTOR
    case VPU::ArchKind::VPUX37XX:
        return std::make_unique<vpux::BitCompactorCodec>();
#endif
    default:
        VPUX_THROW("Unsupported architecture '{0}' or codec not enabled", arch);
    }
}

std::unique_ptr<ICodec> makeCodec(const ICodec::CompressionAlgorithm algo, VPU::ArchKind arch) {
    switch (algo) {
    case ICodec::CompressionAlgorithm::BITCOMPACTOR_CODEC:
        return getBitCompactorCodec(arch);
    default:
        VPUX_THROW("vpux::makeCodec: unsupported compression algorithm");
    }
    VPUX_THROW("vpux::makeCodec: unsupported compression algorithm");
}

}  // namespace vpux
