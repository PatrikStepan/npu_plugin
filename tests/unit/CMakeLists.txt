#
# Copyright (C) 2022 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0
#

if(ENABLE_LTO)
    ie_enable_lto()
endif()

set(TARGET_NAME "vpuxUnitTests")
set(EXCLUDED_UNIT_TESTS_DIR "")
set(OPTIONAL_UNIT_TESTS_INCLUDES "")
set(OPTIONAL_UNIT_TESTS_LIBS "")
set(MANDATORY_UNIT_TESTS_LIBS
    "vpux_elf"
    "IE::commonTestUtils"
    "IE::ngraph"
    "IE::ngraphFunctions"
    "IE::gmock"
    "common_tests_helpers"
    "${VPUX_PLUGIN_STATIC_TARGET}"
    "kmb_utils"
    "vpux_al"
    "vpux_utils"
)

if(ENABLE_DRIVER_COMPILER_ADAPTER)
    list(APPEND OPTIONAL_UNIT_TESTS_INCLUDES
        "${CMAKE_CURRENT_SOURCE_DIR}/vpux_driver_compiler_adapter"
    )

    list(APPEND OPTIONAL_UNIT_TESTS_LIBS
            vpux_driver_compiler_adapter_static
        )
else()
    list(APPEND EXCLUDED_UNIT_TESTS_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/vpux_driver_compiler_adapter"
    )
endif()

if(NOT ENABLE_MLIR_COMPILER)
    list(APPEND EXCLUDED_UNIT_TESTS_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/vpux_compiler"
        "${CMAKE_CURRENT_SOURCE_DIR}/vpux_translate_utils/hwtest/vpux37xx/pss"
    )
else()
    list(APPEND MANDATORY_UNIT_TESTS_LIBS
        "vpux_mlir_compiler_static"
        "vpux_translate_utils_static"
    )
endif()

#
# Add optional tests on HuffmanCodec
#
if(ENABLE_HUFFMAN_CODEC)
    list(APPEND OPTIONAL_UNIT_TESTS_INCLUDES
        "${IE_MAIN_VPUX_PLUGIN_SOURCE_DIR}/src/compression_codec/huffman_encoding/include"
    )

    list(APPEND OPTIONAL_UNIT_TESTS_LIBS
        huffman_codec
    )
else()
    list(APPEND EXCLUDED_UNIT_TESTS_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/compression_codec/huffman_encoding"
    )
endif()

add_subdirectory(vpux_plugin/test_utils)

addIeTargetTest(
    NAME ${TARGET_NAME}
    ROOT ${CMAKE_CURRENT_SOURCE_DIR}
    EXCLUDED_SOURCE_PATHS
        ${EXCLUDED_UNIT_TESTS_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/vpux_plugin/test_utils
    INCLUDES
        ${OPTIONAL_UNIT_TESTS_INCLUDES}
        "${IE_MAIN_VPUX_PLUGIN_SOURCE_DIR}/include"
        ${VPUAL_INCLUDES}
        "${IE_MAIN_VPUX_PLUGIN_SOURCE_DIR}/artifacts/vpuip_2"
    LINK_LIBRARIES
        ${OPTIONAL_UNIT_TESTS_LIBS}
        ${MANDATORY_UNIT_TESTS_LIBS}
    DEFINES
        DATA_PATH=\"${DATA_PATH}\"
        MODELS_PATH=\"${MODELS_PATH}\"
    LABELS
        VPUX_PLUGIN
)

set_target_properties(${TARGET_NAME} PROPERTIES FOLDER "tests")
add_dependencies(${TARGET_NAME} throw_test_backend vpu3700_test_backend no_devices_test_backend)

enable_warnings_as_errors(${TARGET_NAME})
vpux_enable_clang_format(${TARGET_NAME})

# FIXME: E78489 gtest does not respect this option yet in some parts
# so we cannot enable it for vpuxUnitTests

if(NOT MSVC)
    target_compile_options(${TARGET_NAME} PRIVATE -Wno-suggest-override)
endif()

install(TARGETS ${TARGET_NAME}
        RUNTIME DESTINATION tests
        COMPONENT ${VPUX_TESTS_COMPONENT}
        EXCLUDE_FROM_ALL
)

