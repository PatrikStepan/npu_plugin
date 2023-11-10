//
// Copyright (C) 2022 Intel Corporation.
// SPDX-License-Identifier: Apache 2.0
//

#include <gtest/gtest.h>
#include "vpux/compiler/core/tiling.hpp"

using namespace vpux;

using MLIR_TilingTest_FillDividedTiles = testing::Test;
using MLIR_TilingTest_getTileDimOrderND = testing::Test;

TEST_F(MLIR_TilingTest_getTileDimOrderND, tileOverC4D) {
    MemShape shape({1, 80, 80, 80});
    DimsOrder dimOrder = DimsOrder::NCHW;
    const auto tileDimOrder = getTileDimOrderND(shape, dimOrder);

    const auto expectedTileDimOrder = DimArr({Dim(1), Dim(2), Dim(3)});

    for (auto tileInfo : zip(tileDimOrder, expectedTileDimOrder)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

TEST_F(MLIR_TilingTest_getTileDimOrderND, tileOverH4D) {
    MemShape shape({1, 80, 80, 80});
    DimsOrder dimOrder = DimsOrder::NHWC;
    const auto tileDimOrder = getTileDimOrderND(shape, dimOrder);

    const auto expectedTileDimOrder = DimArr({Dim(2), Dim(3), Dim(1)});

    for (auto tileInfo : zip(tileDimOrder, expectedTileDimOrder)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

TEST_F(MLIR_TilingTest_getTileDimOrderND, tileOverW4D) {
    MemShape shape({1, 1, 80, 80});
    DimsOrder dimOrder = DimsOrder::NHWC;
    const auto tileDimOrder = getTileDimOrderND(shape, dimOrder);

    const auto expectedTileDimOrder = DimArr({Dim(3), Dim(1)});
    for (auto tileInfo : zip(tileDimOrder, expectedTileDimOrder)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

TEST_F(MLIR_TilingTest_getTileDimOrderND, tileOverC5D) {
    MemShape shape({1, 80, 80, 80, 80});
    DimsOrder dimOrder = DimsOrder::NCDHW;
    const auto tileDimOrder = getTileDimOrderND(shape, dimOrder);

    const auto expectedTileDimOrder = DimArr({Dim(1), Dim(2), Dim(3), Dim(4)});

    for (auto tileInfo : zip(tileDimOrder, expectedTileDimOrder)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

TEST_F(MLIR_TilingTest_getTileDimOrderND, tileOverW5D) {
    MemShape shape({1, 1, 1, 80, 1});
    DimsOrder dimOrder = DimsOrder::NDHWC;
    const auto tileDimOrder = getTileDimOrderND(shape, dimOrder);

    const auto expectedTileDimOrder = DimArr({Dim(4)});

    for (auto tileInfo : zip(tileDimOrder, expectedTileDimOrder)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

// Imagine shape [1, 8, 8, 9] and divisor [1, 2, 3, 2].
// We'll end up with the following shapes and offsets.

// Shapes   {[1, 4, 3, 5], [1, 4, 3, 5], [1, 4, 3, 5], [1, 4, 3, 5], [1, 4, 2, 5], [1, 4, 2, 5],
//           [1, 4, 3, 4], [1, 4, 3, 4], [1, 4, 3, 4], [1, 4, 3, 4], [1, 4, 2, 4], [1, 4, 2, 4]}
// Offsets  {[1, 0, 0, 0], [1, 4, 0, 0], [1, 0, 3, 0], [1, 4, 3, 0], [1, 0, 6, 0], [1, 4, 6, 0],
//           [1, 0, 0, 5], [1, 4, 0, 5], [1, 0, 3, 5], [1, 4, 3, 5], [1, 0, 6, 0], [1, 4, 6, 5]}

TEST_F(MLIR_TilingTest_FillDividedTiles, NoAlignmentSingleAxisTiling) {
    Shape shape({1, 8, 8, 17});
    Shape divisor({1, 1, 1, 4});
    const auto dividedTiles = fillDividedTiles(divisor, shape, None);
    ASSERT_NE(mlir::failed(dividedTiles), true);

    const auto expectedTiles =
            SmallVector<TileInfo>({TileInfo{Shape({1, 8, 8, 5}), Shape({0, 0, 0, 0}), Shape({1, 1, 1, 4})},
                                   TileInfo{Shape({1, 8, 8, 4}), Shape({0, 0, 0, 5}), Shape({1, 1, 1, 4})},
                                   TileInfo{Shape({1, 8, 8, 4}), Shape({0, 0, 0, 9}), Shape({1, 1, 1, 4})},
                                   TileInfo{Shape({1, 8, 8, 4}), Shape({0, 0, 0, 13}), Shape({1, 1, 1, 4})}});

    for (auto tileInfo : zip(dividedTiles.value(), expectedTiles)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

TEST_F(MLIR_TilingTest_FillDividedTiles, NoAlignmentMultiAxisTiling) {
    Shape shape({1, 8, 8, 17});
    Shape divisor({1, 2, 3, 2});
    const auto dividedTiles = fillDividedTiles(divisor, shape, None);
    ASSERT_NE(mlir::failed(dividedTiles), true);

    const auto expectedTiles =
            SmallVector<TileInfo>({TileInfo{Shape({1, 4, 3, 9}), Shape({0, 0, 0, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 8}), Shape({0, 0, 0, 9}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 9}), Shape({0, 0, 3, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 8}), Shape({0, 0, 3, 9}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 2, 9}), Shape({0, 0, 6, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 2, 8}), Shape({0, 0, 6, 9}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 9}), Shape({0, 4, 0, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 8}), Shape({0, 4, 0, 9}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 9}), Shape({0, 4, 3, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 8}), Shape({0, 4, 3, 9}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 2, 9}), Shape({0, 4, 6, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 2, 8}), Shape({0, 4, 6, 9}), Shape({1, 2, 3, 2})}});

    for (auto tileInfo : zip(dividedTiles.value(), expectedTiles)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

TEST_F(MLIR_TilingTest_FillDividedTiles, DummyAlignmentMultiAxisTiling) {
    Shape shape({1, 8, 8, 17});
    Shape divisor({1, 2, 3, 2});
    auto alignment = SmallVector<int64_t>({1, 1, 1, 1});
    auto optionalAlignment = Optional<ArrayRef<int64_t>>(makeArrayRef(alignment));
    const auto dividedTiles = fillDividedTiles(divisor, shape, optionalAlignment);
    ASSERT_NE(mlir::failed(dividedTiles), true);

    const auto expectedTiles =
            SmallVector<TileInfo>({TileInfo{Shape({1, 4, 3, 9}), Shape({0, 0, 0, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 8}), Shape({0, 0, 0, 9}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 9}), Shape({0, 0, 3, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 8}), Shape({0, 0, 3, 9}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 2, 9}), Shape({0, 0, 6, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 2, 8}), Shape({0, 0, 6, 9}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 9}), Shape({0, 4, 0, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 8}), Shape({0, 4, 0, 9}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 9}), Shape({0, 4, 3, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 3, 8}), Shape({0, 4, 3, 9}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 2, 9}), Shape({0, 4, 6, 0}), Shape({1, 2, 3, 2})},
                                   TileInfo{Shape({1, 4, 2, 8}), Shape({0, 4, 6, 9}), Shape({1, 2, 3, 2})}});

    for (auto tileInfo : zip(dividedTiles.value(), expectedTiles)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

TEST_F(MLIR_TilingTest_FillDividedTiles, SingleAlignmentSingleAxisTiling) {
    Shape shape({1, 8, 8, 17});
    Shape divisor({1, 1, 1, 4});
    auto alignment = SmallVector<int64_t>({1, 1, 1, 5});
    auto optionalAlignment = Optional<ArrayRef<int64_t>>(makeArrayRef(alignment));
    const auto dividedTiles = fillDividedTiles(divisor, shape, optionalAlignment);
    ASSERT_NE(mlir::failed(dividedTiles), true);

    const auto expectedTiles =
            SmallVector<TileInfo>({TileInfo{Shape({1, 8, 8, 5}), Shape({0, 0, 0, 0}), Shape({1, 1, 1, 4})},
                                   TileInfo{Shape({1, 8, 8, 5}), Shape({0, 0, 0, 5}), Shape({1, 1, 1, 4})},
                                   TileInfo{Shape({1, 8, 8, 5}), Shape({0, 0, 0, 10}), Shape({1, 1, 1, 4})},
                                   TileInfo{Shape({1, 8, 8, 2}), Shape({0, 0, 0, 15}), Shape({1, 1, 1, 4})}});

    for (auto tileInfo : zip(dividedTiles.value(), expectedTiles)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

TEST_F(MLIR_TilingTest_FillDividedTiles, SingleAlignmentMultiAxisTiling) {
    Shape shape({1, 8, 8, 17});
    Shape divisor({1, 2, 3, 4});
    auto alignment = SmallVector<int64_t>({1, 1, 1, 5});
    auto optionalAlignment = Optional<ArrayRef<int64_t>>(makeArrayRef(alignment));
    const auto dividedTiles = fillDividedTiles(divisor, shape, optionalAlignment);
    ASSERT_NE(mlir::failed(dividedTiles), true);

    const auto expectedTiles =
            SmallVector<TileInfo>({TileInfo{Shape({1, 4, 3, 5}), Shape({0, 0, 0, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 5}), Shape({0, 0, 0, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 5}), Shape({0, 0, 0, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 2}), Shape({0, 0, 0, 15}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 5}), Shape({0, 0, 3, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 5}), Shape({0, 0, 3, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 5}), Shape({0, 0, 3, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 2}), Shape({0, 0, 3, 15}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 2, 5}), Shape({0, 0, 6, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 2, 5}), Shape({0, 0, 6, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 2, 5}), Shape({0, 0, 6, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 2, 2}), Shape({0, 0, 6, 15}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 5}), Shape({0, 4, 0, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 5}), Shape({0, 4, 0, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 5}), Shape({0, 4, 0, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 2}), Shape({0, 4, 0, 15}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 5}), Shape({0, 4, 3, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 5}), Shape({0, 4, 3, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 5}), Shape({0, 4, 3, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 3, 2}), Shape({0, 4, 3, 15}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 2, 5}), Shape({0, 4, 6, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 2, 5}), Shape({0, 4, 6, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 2, 5}), Shape({0, 4, 6, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 4, 2, 2}), Shape({0, 4, 6, 15}), Shape({1, 2, 3, 4})}});

    for (auto tileInfo : zip(dividedTiles.value(), expectedTiles)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

TEST_F(MLIR_TilingTest_FillDividedTiles, MultiAlignmentMultiAxisTiling) {
    Shape shape({1, 8, 8, 17});
    Shape divisor({1, 2, 3, 4});
    auto alignment = SmallVector<int64_t>({1, 6, 1, 5});
    auto optionalAlignment = Optional<ArrayRef<int64_t>>(makeArrayRef(alignment));
    const auto dividedTiles = fillDividedTiles(divisor, shape, optionalAlignment);
    ASSERT_NE(mlir::failed(dividedTiles), true);

    const auto expectedTiles =
            SmallVector<TileInfo>({TileInfo{Shape({1, 6, 3, 5}), Shape({0, 0, 0, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 6, 3, 5}), Shape({0, 0, 0, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 6, 3, 5}), Shape({0, 0, 0, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 6, 3, 2}), Shape({0, 0, 0, 15}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 6, 3, 5}), Shape({0, 0, 3, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 6, 3, 5}), Shape({0, 0, 3, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 6, 3, 5}), Shape({0, 0, 3, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 6, 3, 2}), Shape({0, 0, 3, 15}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 6, 2, 5}), Shape({0, 0, 6, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 6, 2, 5}), Shape({0, 0, 6, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 6, 2, 5}), Shape({0, 0, 6, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 6, 2, 2}), Shape({0, 0, 6, 15}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 3, 5}), Shape({0, 6, 0, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 3, 5}), Shape({0, 6, 0, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 3, 5}), Shape({0, 6, 0, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 3, 2}), Shape({0, 6, 0, 15}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 3, 5}), Shape({0, 6, 3, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 3, 5}), Shape({0, 6, 3, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 3, 5}), Shape({0, 6, 3, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 3, 2}), Shape({0, 6, 3, 15}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 2, 5}), Shape({0, 6, 6, 0}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 2, 5}), Shape({0, 6, 6, 5}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 2, 5}), Shape({0, 6, 6, 10}), Shape({1, 2, 3, 4})},
                                   TileInfo{Shape({1, 2, 2, 2}), Shape({0, 6, 6, 15}), Shape({1, 2, 3, 4})}});

    for (auto tileInfo : zip(dividedTiles.value(), expectedTiles)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

TEST_F(MLIR_TilingTest_FillDividedTiles, NoAlignmentSingleAxisTiling5D) {
    Shape shape({1, 1, 8, 8, 17});
    Shape divisor({1, 1, 1, 1, 4});
    const auto dividedTiles = fillDividedTiles(divisor, shape, None);
    ASSERT_NE(mlir::failed(dividedTiles), true);

    const auto expectedTiles =
            SmallVector<TileInfo>({TileInfo{Shape({1, 1, 8, 8, 5}), Shape({0, 0, 0, 0, 0}), Shape({1, 1, 1, 1, 4})},
                                   TileInfo{Shape({1, 1, 8, 8, 4}), Shape({0, 0, 0, 0, 5}), Shape({1, 1, 1, 1, 4})},
                                   TileInfo{Shape({1, 1, 8, 8, 4}), Shape({0, 0, 0, 0, 9}), Shape({1, 1, 1, 1, 4})},
                                   TileInfo{Shape({1, 1, 8, 8, 4}), Shape({0, 0, 0, 0, 13}), Shape({1, 1, 1, 1, 4})}});

    for (auto tileInfo : zip(dividedTiles.value(), expectedTiles)) {
        EXPECT_EQ(std::get<0>(tileInfo), std::get<1>(tileInfo));
    }
}

TEST_F(MLIR_TilingTest_FillDividedTiles, InValidTiling) {
    Shape shape({1, 320, 8, 8});
    Shape divisor({1, 6, 1, 1});
    auto alignment = SmallVector<int64_t>({1, 16, 1, 1});
    auto optionalAlignment = Optional<ArrayRef<int64_t>>(makeArrayRef(alignment));
    const auto dividedTiles = fillDividedTiles(divisor, shape, optionalAlignment);
    EXPECT_EQ(mlir::failed(dividedTiles), true);
}
