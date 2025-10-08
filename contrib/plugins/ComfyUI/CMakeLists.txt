# Copyright OpenFX and contributors to the OpenFX project.
# SPDX-License-Identifier: BSD-3-Clause

# ComfyUI OFX Plugins
# This directory contains OFX plugins that integrate with ComfyUI REST server

# Only build if explicitly enabled
if(NOT BUILD_COMFYUI_PLUGINS)
    message(STATUS "ComfyUI plugins disabled. Use -DBUILD_COMFYUI_PLUGINS=ON to enable.")
    return()
endif()

message(STATUS "Building ComfyUI OFX plugins")

# Common source files shared by all ComfyUI plugins
add_library(ComfyUICommon STATIC
    common/comfyui_client.cpp
    common/comfyui_base_plugin.cpp
)

target_include_directories(ComfyUICommon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/Support/include
)

target_link_libraries(ComfyUICommon PUBLIC
    OfxSupport
    nlohmann_json::nlohmann_json
    httplib::httplib
    websocketpp::websocketpp
    # TODO: Add EXR I/O library
    # OpenImageIO::OpenImageIO
    OpenSSL::SSL
    OpenSSL::Crypto
)

# Test suite
add_subdirectory(tests)

# Future: Add individual plugin subdirectories here
# add_subdirectory(segmentation)
# add_subdirectory(upscale)
# add_subdirectory(inpaint)

message(STATUS "ComfyUI common library created. Ready for plugin implementations.")
