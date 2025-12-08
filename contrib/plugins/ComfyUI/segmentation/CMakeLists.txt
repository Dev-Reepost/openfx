# Copyright OpenFX and contributors to the OpenFX project.
# SPDX-License-Identifier: BSD-3-Clause

# ComfyUI SAM Segmentation Plugin

# Create the plugin as a MODULE (shared library with .ofx extension)
if(APPLE)
    add_library(SAMSegmentation MODULE
        sam_segmentation_plugin.cpp
    )
else()
    add_library(SAMSegmentation SHARED
        sam_segmentation_plugin.cpp
    )
endif()

# Set output name and properties
set_target_properties(SAMSegmentation PROPERTIES
    SUFFIX ".ofx"
    PREFIX ""
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

# Architecture will be set by CMAKE_OSX_ARCHITECTURES at configure time
# For universal builds, use build-macos-universal-plugin.sh which builds
# each architecture separately and combines with lipo

# Include directories
target_include_directories(SAMSegmentation PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_SOURCE_DIR}/Support/include
)

# Link libraries
target_link_libraries(SAMSegmentation PRIVATE
    ComfyUICommon
    OfxSupport
    nlohmann_json::nlohmann_json
    httplib::httplib
    websocketpp::websocketpp
    OpenSSL::SSL
    OpenSSL::Crypto
    tinyexr::tinyexr
    miniz::miniz
)

# For macOS, create .ofx.bundle structure at build time
if(APPLE)
    set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/SAMSegmentation.ofx.bundle/Contents/MacOS")

    # Configure Info.plist
    set(PLUGIN_NAME "SAMSegmentation")
    set(PLUGIN_EXE "SAMSegmentation.ofx")
    set(BUNDLE_IDENTIFIER "com.comfyui.SAMSegmentation")
    set(BUNDLE_SIGNATURE "????")
    set(PLUGIN_VERSION "1.0.0")

    configure_file(${CMAKE_SOURCE_DIR}/Examples/Info.plist.in
                   "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/SAMSegmentation.ofx.bundle/Contents/Info.plist")

    add_custom_command(TARGET SAMSegmentation POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:SAMSegmentation> "${BUNDLE_DIR}/"
        COMMENT "Creating SAMSegmentation.ofx.bundle structure"
    )
endif()

message(STATUS "ComfyUI SAM Segmentation plugin configured")
