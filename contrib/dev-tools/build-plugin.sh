#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright OpenFX and contributors to the OpenFX project.

# OpenFX Plugin Build and Install Script - Cross-Platform Version
#
# Builds any OpenFX plugin from any directory structure
# Supports Linux, macOS, and Windows (Git Bash/MSYS2)

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Detect platform and set plugin paths
detect_platform() {
    case "$OSTYPE" in
        linux-gnu*) PLATFORM="linux" ;;
        darwin*)    PLATFORM="macos" ;;
        msys*|cygwin*|mingw*) PLATFORM="windows" ;;
        *)
            log_error "Unsupported platform: $OSTYPE"
            exit 1
            ;;
    esac
}

# Get platform-specific plugin paths
get_plugin_paths() {
    detect_platform
    case "$PLATFORM" in
        macos)
            SYSTEM_PLUGIN_DIR="/Library/OFX/Plugins"
            USER_INSTALL_DIR="$HOME/Library/OFX/Plugins"
            DEV_PLUGIN_DIR="$HOME/OFX/Plugins"
            ;;
        linux)
            SYSTEM_PLUGIN_DIR="/usr/OFX/Plugins"
            USER_INSTALL_DIR="$HOME/.OFX/Plugins"
            DEV_PLUGIN_DIR="$HOME/OFX/Plugins"
            ;;
        windows)
            SYSTEM_PLUGIN_DIR="/c/Program Files/Common Files/OFX/Plugins"
            USER_INSTALL_DIR="$HOME/AppData/Roaming/OFX/Plugins"
            DEV_PLUGIN_DIR="$HOME/OFX/Plugins"
            ;;
    esac

}

# Function to show usage
show_usage() {
    echo "Usage: $0 <plugin-directory> [target-name] [options]"
    echo
    echo "Arguments:"
    echo "  plugin-directory    Path to plugin directory (relative to OpenFX root)"
    echo "  target-name         CMake target name (defaults to directory name)"
    echo
    echo "Options:"
    echo "  -d, --debug              Build in debug mode"
    echo "  -c, --clean              Clean build before building"
    echo "  -i, --install            Install to user OFX library (~/Library/OFX/Plugins on macOS)"
    echo "  --install-dir DIR        Install to custom directory (implies -i)"
    echo "  -v, --verbose            Verbose build output"
    echo "  --bundle-name NAME       Custom bundle name (defaults to target name)"
    echo "  -h, --help               Show this help message"
    echo
    echo "Examples:"
    echo "  $0 contrib/plugins/ComfyUI/depth_da3 DepthAnythingV3        # Build only"
    echo "  $0 contrib/plugins/ComfyUI/depth_da3 DepthAnythingV3 -i     # Build and install"
    echo "  $0 Examples/Invert -d -v                                     # Debug build"
    echo "  $0 MyPlugin MyTarget --install-dir /Library/OFX/Plugins      # System install"
}

# Default values
PLUGIN_DIR=""
TARGET_NAME=""
BUNDLE_NAME=""
BUILD_TYPE="Release"
CLEAN_BUILD=false
DO_INSTALL=false
INSTALL_DIR=""
VERBOSE=""
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OPENFX_ROOT="${OPENFX_ROOT:-$(cd "$SCRIPT_DIR/../.." && pwd)}"
# Initialize platform-specific path variables (SYSTEM_PLUGIN_DIR, USER_INSTALL_DIR, DEV_PLUGIN_DIR)
get_plugin_paths

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -i|--install)
            DO_INSTALL=true
            shift
            ;;
        -v|--verbose)
            VERBOSE="--verbose"
            shift
            ;;
        --bundle-name)
            BUNDLE_NAME="$2"
            shift 2
            ;;
        --install-dir)
            INSTALL_DIR="$2"
            DO_INSTALL=true
            shift 2
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        -*)
            log_error "Unknown option $1"
            show_usage
            exit 1
            ;;
        *)
            if [[ -z "$PLUGIN_DIR" ]]; then
                PLUGIN_DIR="$1"
            elif [[ -z "$TARGET_NAME" ]]; then
                TARGET_NAME="$1"
            else
                log_error "Too many arguments"
                show_usage
                exit 1
            fi
            shift
            ;;
    esac
done

# Check if plugin directory is provided
if [[ -z "$PLUGIN_DIR" ]]; then
    log_error "Plugin directory is required"
    show_usage
    exit 1
fi

# Set default target name if not provided
if [[ -z "$TARGET_NAME" ]]; then
    TARGET_NAME=$(basename "$PLUGIN_DIR")
fi

# Set default bundle name if not provided
if [[ -z "$BUNDLE_NAME" ]]; then
    BUNDLE_NAME="$TARGET_NAME"
fi

# Check if we're in the OpenFX root directory
check_openfx_root() {
    if [[ ! -f "$OPENFX_ROOT/CMakeLists.txt" ]] || [[ ! -d "$OPENFX_ROOT/include" ]]; then
        log_error "Not in OpenFX root directory or OPENFX_ROOT not set correctly"
        log_info "Current directory: $(pwd)"
        log_info "OPENFX_ROOT: $OPENFX_ROOT"
        exit 1
    fi
}

# Check if plugin directory exists
check_plugin_exists() {
    local full_path="$OPENFX_ROOT/$PLUGIN_DIR"
    
    if [[ ! -d "$full_path" ]]; then
        log_error "Plugin directory not found: $full_path"
        log_info "Make sure the path is relative to the OpenFX root directory"
        exit 1
    fi
    
    # Check if it has a CMakeLists.txt
    if [[ ! -f "$full_path/CMakeLists.txt" ]]; then
        log_warning "No CMakeLists.txt found in $full_path"
        log_info "Plugin may not be buildable independently"
    fi
    
    log_info "Found plugin directory: $full_path"
}

# Clean build directory
clean_build() {
    if [[ "$CLEAN_BUILD" == true ]]; then
        log_info "Cleaning build directory..."
        if [[ -d "$OPENFX_ROOT/build" ]]; then
            rm -rf "$OPENFX_ROOT/build/$BUILD_TYPE"
            log_success "Build directory cleaned"
        fi
    fi
}

# Configure CMake
configure_cmake() {
    log_info "Configuring CMake ($BUILD_TYPE build)..."

    cd "$OPENFX_ROOT"

    # Ensure Conan is available
    if ! command -v conan &> /dev/null; then
        # Try to find Conan in common locations
        for path in ~/.local/bin ~/Library/Python/*/bin ~/.pyenv/versions/*/bin; do
            if [[ -f "$path/conan" ]]; then
                export PATH="$PATH:$path"
                break
            fi
        done
    fi

    if ! command -v conan &> /dev/null; then
        log_error "Conan not found in PATH"
        exit 1
    fi

    # Configure with appropriate build type
    local build_type_arg=""
    if [[ "$BUILD_TYPE" == "Debug" ]]; then
        build_type_arg="Debug"
    else
        build_type_arg="Release"
    fi

    # Detect if this is a ComfyUI plugin
    local is_comfyui_plugin=false
    local conan_extra_args=()
    if [[ "$PLUGIN_DIR" == *"ComfyUI"* ]]; then
        is_comfyui_plugin=true
        conan_extra_args=(-o "&:build_comfyui_plugins=True")
        log_info "Detected ComfyUI plugin - enabling ComfyUI dependencies"
    fi

    # Install Conan dependencies and generate build files if needed
    if [[ ! -f "build/$BUILD_TYPE/generators/CMakePresets.json" ]] || [[ "$is_comfyui_plugin" == true ]]; then
        log_info "Installing Conan dependencies..."
        if ! conan install -s build_type=$BUILD_TYPE -pr:b=default "${conan_extra_args[@]}" --build=missing .; then
            log_warning "Conan install with --build=missing failed, retrying with --build='*' (builds all from source)..."
            conan install -s build_type=$BUILD_TYPE -pr:b=default "${conan_extra_args[@]}" --build="*" .
        fi
    fi

    # Additional CMake flags. Use an array so values containing spaces stay
    # in a single argument without needing embedded quotes (which CMake would
    # otherwise see as part of the value).
    local cmake_flags=(-DBUILD_EXAMPLE_PLUGINS=TRUE)
    if [[ "$is_comfyui_plugin" == true ]]; then
        cmake_flags+=(-DBUILD_COMFYUI_PLUGINS=ON)
    fi
    if [[ -n "$INSTALL_DIR" ]]; then
        cmake_flags+=("-DPLUGIN_INSTALLDIR=$INSTALL_DIR")
    fi

    cmake --preset "conan-$(echo "${build_type_arg}" | tr '[:upper:]' '[:lower:]')" "${cmake_flags[@]}"
    log_success "CMake configuration completed"
}

# Build the plugin
build_plugin() {
    log_info "Building plugin target: $TARGET_NAME"
    
    cd "$OPENFX_ROOT"
    
    # Try to build the specific target first
    if cmake --build "build/$BUILD_TYPE" --config "$BUILD_TYPE" --target "$TARGET_NAME" --parallel $VERBOSE; then
        log_success "Plugin target built successfully"
    else
        # If specific target fails, try building all targets in the plugin directory
        log_info "Direct target build failed, trying to build all targets..."
        if cmake --build "build/$BUILD_TYPE" --config "$BUILD_TYPE" --parallel $VERBOSE; then
            log_success "Build completed (all targets)"
        else
            log_error "Plugin build failed"
            exit 1
        fi
    fi
}

# Find built plugin binary
find_plugin_binary() {
    local plugin_binary=""
    local search_base="build/$BUILD_TYPE"
    
    # Define possible binary names (including bundle paths)
    local binary_names=(
        "$TARGET_NAME.ofx"
        "${TARGET_NAME%.ofx}.ofx"
        "$(basename "$PLUGIN_DIR").ofx"
        "$TARGET_NAME.ofx/Contents/MacOS/$TARGET_NAME"
        "${TARGET_NAME%.ofx}.ofx/Contents/MacOS/${TARGET_NAME%.ofx}"
    )
    
    # Define possible locations based on the plugin directory structure
    local search_dirs=(
        "$PLUGIN_DIR"
        "$(dirname "$PLUGIN_DIR")/$(basename "$PLUGIN_DIR")"
        "MyFirstPlugin"
        "Examples/$(basename "$PLUGIN_DIR")"
        "Support/Plugins/$(basename "$PLUGIN_DIR")"
    )
    
    # Search for the binary
    for dir in "${search_dirs[@]}"; do
        for name in "${binary_names[@]}"; do
            local candidate="$OPENFX_ROOT/$search_base/$dir/$name"
            if [[ -f "$candidate" ]]; then
                plugin_binary="$candidate"
                break 2
            fi
        done
    done
    
    # If not found in specific directories, search more specifically
    if [[ -z "$plugin_binary" ]]; then
        log_info "Searching for plugin binary in $PLUGIN_DIR..." >&2
        # Look specifically in the plugin's build directory first
        while IFS= read -r -d '' candidate; do
            if [[ -f "$candidate" ]]; then
                plugin_binary="$candidate"
                break
            fi
        done < <(find "$OPENFX_ROOT/$search_base/$PLUGIN_DIR" -name "*$TARGET_NAME*" -type f -print0 2>/dev/null)

        # If still not found, look for any executable in the plugin dir
        if [[ -z "$plugin_binary" ]]; then
            while IFS= read -r -d '' candidate; do
                if [[ -f "$candidate" && -x "$candidate" ]]; then
                    plugin_binary="$candidate"
                    break
                fi
            done < <(find "$OPENFX_ROOT/$search_base/$PLUGIN_DIR" -type f -executable -print0 2>/dev/null)
        fi
    fi
    
    if [[ -z "$plugin_binary" ]]; then
        log_error "Built plugin binary not found" >&2
        log_info "Searched for: ${binary_names[*]}" >&2
        log_info "In directories under $search_base/: ${search_dirs[*]}" >&2
        exit 1
    fi
    
    log_info "Found plugin binary: $plugin_binary" >&2
    echo "$plugin_binary"
}

# Locate the cmake-generated bundle or construct one from the raw binary.
# Outputs the bundle path to stdout; all other output goes to stderr.
locate_or_create_bundle() {
    local plugin_binary="$1"
    local bundle_name="${BUNDLE_NAME}.ofx.bundle"
    local binary_name
    binary_name=$(basename "$plugin_binary")

    # Prefer the cmake-generated bundle — it already has Info.plist (macOS-only,
    # generated by cmake) and Resources correctly in place.
    local cmake_bundle
    cmake_bundle=$(find "$OPENFX_ROOT/build/$BUILD_TYPE" -name "${TARGET_NAME}.ofx.bundle" -type d 2>/dev/null | head -1)

    if [[ -n "$cmake_bundle" && -d "$cmake_bundle/Contents" ]]; then
        log_info "Using cmake-generated bundle: $cmake_bundle" >&2
        { file "$(find "$cmake_bundle" -name "*.ofx" -type f | head -1)"; } >&2
        echo "$cmake_bundle"
        return
    fi

    # Fallback: construct bundle manually from the raw binary
    log_warning "No cmake bundle found, constructing bundle from binary" >&2
    local bundle_path="$OPENFX_ROOT/build/$BUILD_TYPE/$bundle_name"

    case "$PLATFORM" in
        macos)
            mkdir -p "$bundle_path/Contents/MacOS"
            cp "$plugin_binary" "$bundle_path/Contents/MacOS/"
            chmod +x "$bundle_path/Contents/MacOS/$binary_name"
            ;;
        linux|windows)
            mkdir -p "$bundle_path/Contents/Linux-x86-64"
            cp "$plugin_binary" "$bundle_path/Contents/Linux-x86-64/"
            chmod +x "$bundle_path/Contents/Linux-x86-64/$binary_name"
            ;;
    esac

    log_success "Bundle created: $bundle_path" >&2
    { file "$(find "$bundle_path" -name "*.ofx" -type f | head -1)"; } >&2
    echo "$bundle_path"
}

# Install the bundle to the target directory
install_plugin() {
    local bundle_path="$1"

    if [[ "$DO_INSTALL" != true ]]; then
        log_info "Bundle ready (not installed). Use -i to install to $USER_INSTALL_DIR"
        return
    fi

    # If --install-dir was not set, default to the user OFX library
    local target_dir="${INSTALL_DIR:-$USER_INSTALL_DIR}"
    local bundle_name
    bundle_name=$(basename "$bundle_path")

    log_info "Installing to $target_dir ..."
    mkdir -p "$target_dir"
    rm -rf "$target_dir/$bundle_name"
    cp -r "$bundle_path" "$target_dir/"
    log_success "Plugin installed: $target_dir/$bundle_name"
}

# Verify installation
verify_installation() {
    if [[ "$DO_INSTALL" != true ]]; then
        return
    fi

    local target_dir="${INSTALL_DIR:-$USER_INSTALL_DIR}"
    local bundle_name="${BUNDLE_NAME}.ofx.bundle"

    log_info "Verifying installation..."
    if [[ -d "$target_dir/$bundle_name" ]]; then
        log_success "Plugin found at: $target_dir/$bundle_name"
    else
        log_warning "Plugin not found at expected location: $target_dir/$bundle_name"
    fi
}

# Print summary
print_summary() {
    local bundle_path="$1"
    echo
    log_success "Plugin build completed!"
    echo
    echo "Summary:"
    echo "  Plugin Directory: $PLUGIN_DIR"
    echo "  Target Name:      $TARGET_NAME"
    echo "  Bundle:           $bundle_path"
    echo "  Build Type:       $BUILD_TYPE"
    if [[ "$DO_INSTALL" == true ]]; then
        local target_dir="${INSTALL_DIR:-$USER_INSTALL_DIR}"
        echo "  Installed to:     $target_dir/${BUNDLE_NAME}.ofx.bundle"
    else
        echo
        echo "To install for host apps (Flame, Nuke, etc.):"
        echo "  $0 $PLUGIN_DIR $TARGET_NAME -i"
    fi
}

# Main execution
main() {
    echo "OpenFX Plugin Builder - Cross-Platform Version"
    echo "=========================================="
    echo "Plugin Directory: $PLUGIN_DIR"
    echo "Target Name: $TARGET_NAME"
    echo "Bundle Name: $BUNDLE_NAME"
    echo "Build Type: $BUILD_TYPE"
    if [[ "$CLEAN_BUILD" == true ]]; then echo "Clean Build: Yes"; fi
    if [[ "$DO_INSTALL" == true ]]; then echo "Install to:   ${INSTALL_DIR:-$USER_INSTALL_DIR}"; fi
    echo

    check_openfx_root
    check_plugin_exists
    clean_build
    configure_cmake
    build_plugin

    plugin_binary=$(find_plugin_binary)
    bundle_path=$(locate_or_create_bundle "$plugin_binary")
    install_plugin "$bundle_path"
    verify_installation
    print_summary "$bundle_path"
}

# Run main function
main "$@"