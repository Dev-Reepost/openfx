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
    common/comfyui_image_io.cpp
    common/async_job_manager.cpp
)

# Set position-independent code for static library (required for linking into shared libraries)
set_target_properties(ComfyUICommon PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Architecture will be set by CMAKE_OSX_ARCHITECTURES at configure time
# For universal builds, use build-macos-universal-plugin.sh which builds
# each architecture separately and combines with lipo

target_link_libraries(ComfyUICommon PUBLIC
    OfxSupport
    nlohmann_json::nlohmann_json
    httplib::httplib
    ixwebsocket::ixwebsocket
    tinyexr::tinyexr
    miniz::miniz
    OpenSSL::SSL
    OpenSSL::Crypto
    spdlog::spdlog_header_only
)

# Link macOS system frameworks required by httplib
if(APPLE)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation REQUIRED)
    target_link_libraries(ComfyUICommon PUBLIC ${COREFOUNDATION_FRAMEWORK})
endif()

target_include_directories(ComfyUICommon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/Support/include
)

# Test suite
add_subdirectory(tests)

# Concrete plugin implementations
add_subdirectory(segmentation)
add_subdirectory(anycomfy)

# Future plugins:
# add_subdirectory(upscale)
# add_subdirectory(inpaint)

message(STATUS "ComfyUI common library created. Ready for plugin implementations.")
