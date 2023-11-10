//
// Copyright (C) 2022-2023 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#pragma once

#include <vector>

#include "vpux/compiler/utils/codec_factory.hpp"

namespace vpux {
class BitCompactorCodec final : public ICodec {
public:
    std::vector<uint8_t> compress(std::vector<uint8_t>& data) const override;
};

}  // namespace vpux
