# Copyright OpenFX and contributors to the OpenFX project.
# SPDX-License-Identifier: BSD-3-Clause

# ComfyUI Plugin Test Suite

message(STATUS "Configuring ComfyUI test suite")

# Test executable for ComfyUI client
add_executable(test_comfyui_client
    test_client.cpp
)

target_link_libraries(test_comfyui_client PRIVATE
    ComfyUICommon
    OfxSupport
    nlohmann_json::nlohmann_json
    httplib::httplib
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(test_comfyui_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_SOURCE_DIR}/Support/include
)

# Note: Tests are built but not added to CTest by default
# Run manually: ./build/Release/contrib/plugins/ComfyUI/tests/test_comfyui_client

message(STATUS "ComfyUI test executable configured: test_comfyui_client")
