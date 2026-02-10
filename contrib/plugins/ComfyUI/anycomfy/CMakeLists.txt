# Copyright OpenFX and contributors to the OpenFX project.
# SPDX-License-Identifier: BSD-3-Clause

# ComfyUI AnyComfy Plugin - Generic workflow executor

# Create the plugin as a MODULE (shared library with .ofx extension)
if(APPLE)
    add_library(AnyComfy MODULE
        anycomfy_plugin.cpp
        open_url.mm
    )
else()
    add_library(AnyComfy SHARED
        anycomfy_plugin.cpp
    )
endif()

# Set output name and properties
set_target_properties(AnyComfy PROPERTIES
    SUFFIX ".ofx"
    PREFIX ""
    C_VISIBILITY_PRESET hidden
    CXX_VISIBILITY_PRESET hidden
)

# Architecture will be set by CMAKE_OSX_ARCHITECTURES at configure time
# For universal builds, use build-macos-universal-plugin.sh which builds
# each architecture separately and combines with lipo

# Include directories
target_include_directories(AnyComfy PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_SOURCE_DIR}/Support/include
)

# Link libraries
target_link_libraries(AnyComfy PRIVATE
    ComfyUICommon
    OfxSupport
    nlohmann_json::nlohmann_json
    httplib::httplib
    ixwebsocket::ixwebsocket
    OpenSSL::SSL
    OpenSSL::Crypto
    tinyexr::tinyexr
    miniz::miniz
)

# Link macOS system frameworks
if(APPLE)
    target_link_libraries(AnyComfy PRIVATE "-framework CoreFoundation" "-framework AppKit")
endif()

# For macOS, create .ofx.bundle structure at build time
if(APPLE)
    set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/AnyComfy.ofx.bundle/Contents/MacOS")
    set(RESOURCES_DIR "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/AnyComfy.ofx.bundle/Contents/Resources")

    # Configure Info.plist
    set(PLUGIN_NAME "AnyComfy")
    set(PLUGIN_EXE "AnyComfy.ofx")
    set(BUNDLE_IDENTIFIER "com.comfyui.AnyComfy")
    set(BUNDLE_SIGNATURE "????")
    set(PLUGIN_VERSION "1.0.0")

    configure_file(${CMAKE_SOURCE_DIR}/Examples/Info.plist.in
                   "${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/AnyComfy.ofx.bundle/Contents/Info.plist")

    add_custom_command(TARGET AnyComfy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:AnyComfy> "${BUNDLE_DIR}/"

        # Copy resources
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "${RESOURCES_DIR}"

        COMMENT "Creating AnyComfy.ofx.bundle structure with resources"
    )
endif()

# For Linux, create .ofx.bundle structure at build time
if(UNIX AND NOT APPLE)
    set(BUNDLE_DIR "${CMAKE_BINARY_DIR}/AnyComfy.ofx.bundle/Contents/Linux-${CMAKE_SYSTEM_PROCESSOR}")
    set(RESOURCES_DIR "${CMAKE_BINARY_DIR}/AnyComfy.ofx.bundle/Resources")

    add_custom_command(TARGET AnyComfy POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${BUNDLE_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:AnyComfy> "${BUNDLE_DIR}/"

        # Copy resources (Linux uses Resources/ at bundle root level)
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCES_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/resources"
            "${RESOURCES_DIR}"

        COMMENT "Creating AnyComfy.ofx.bundle structure with resources (Linux)"
    )
endif()

message(STATUS "ComfyUI AnyComfy plugin configured with bundled resources")
