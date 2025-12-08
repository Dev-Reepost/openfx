#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright OpenFX and contributors to the OpenFX project.
#
# Build macOS Universal Binary OFX Plugin (arm64 + x86_64)
#
# This script builds a plugin separately for arm64 and x86_64,
# then combines them into a macOS universal binary using lipo.
# Universal binaries only work on macOS.

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

show_usage() {
    cat << EOF
Usage: $0 [options]

Build a universal binary (arm64 + x86_64) OFX plugin for macOS.

Options:
  -h, --help              Show this help message
  -p, --plugin NAME       Plugin name (default: SAMSegmentation)
  -t, --target NAME       CMake target name (default: SAMSegmentation)
  -c, --clean             Clean build directories before building
  -i, --install           Install to ~/Library/OFX/Plugins after building
  --install-dir DIR       Install to custom directory

Examples:
  $0                                  # Build SAMSegmentation as universal
  $0 -i                               # Build and install to ~/Library/OFX/Plugins
  $0 -p MyPlugin -t MyPluginTarget    # Build custom plugin
  $0 --clean --install                # Clean build and install
  $0 --install-dir ~/OFX/Plugins      # Install to custom directory

Requirements:
  - Must be run on macOS (arm64 recommended)
  - Requires Conan 2.x and CMake 3.28+
  - Dependencies must be available for both architectures

Note: This script builds each architecture separately and combines them.
      It requires Conan packages to be available for both arm64 and x86_64.
EOF
}

# Default values
PLUGIN_NAME="SAMSegmentation"
TARGET_NAME="SAMSegmentation"
CLEAN_BUILD=false
INSTALL_PLUGIN=false
INSTALL_DIR=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            show_usage
            exit 0
            ;;
        -p|--plugin)
            PLUGIN_NAME="$2"
            shift 2
            ;;
        -t|--target)
            TARGET_NAME="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN_BUILD=true
            shift
            ;;
        -i|--install)
            INSTALL_PLUGIN=true
            shift
            ;;
        --install-dir)
            INSTALL_DIR="$2"
            INSTALL_PLUGIN=true
            shift 2
            ;;
        *)
            log_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Set default install directory if not specified
if [[ "$INSTALL_PLUGIN" == true && -z "$INSTALL_DIR" ]]; then
    INSTALL_DIR="$HOME/Library/OFX/Plugins"
fi

# Verify we're on macOS
if [[ "$OSTYPE" != "darwin"* ]]; then
    log_error "This script only works on macOS"
    exit 1
fi

# Get OpenFX root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OFX_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$OFX_ROOT"

log_info "Building universal binary for: $PLUGIN_NAME"
log_info "OpenFX root: $OFX_ROOT"

# Build directories
ARM64_DIR="build/arm64"
X86_64_DIR="build/x86_64"
RELEASE_DIR="build/Release"

# Clean if requested
if [[ "$CLEAN_BUILD" == true ]]; then
    log_warning "Cleaning build directories..."
    rm -rf "$ARM64_DIR" "$X86_64_DIR"
fi

# Step 1: Build arm64 version
log_info "Step 1/4: Building arm64 version..."
mkdir -p "$ARM64_DIR"

log_info "Installing Conan dependencies for arm64..."
conan install . \
    -s build_type=Release \
    -s arch=armv8 \
    -pr:b=default \
    --build=missing \
    -o build_comfyui_plugins=True \
    -of="$ARM64_DIR"

log_info "Configuring CMake for arm64..."
# Find the conan_toolchain.cmake file
ARM64_TOOLCHAIN=$(find "$ARM64_DIR" -name "conan_toolchain.cmake" | head -1)
cmake -S . -B "$ARM64_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.4 \
    -DBUILD_COMFYUI_PLUGINS=ON \
    -DCMAKE_TOOLCHAIN_FILE="$ARM64_TOOLCHAIN" \
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW

log_info "Building $TARGET_NAME for arm64..."
cmake --build "$ARM64_DIR" --config Release --target "$TARGET_NAME" --parallel

# Step 2: Build x86_64 version
log_info "Step 2/4: Building x86_64 version..."
mkdir -p "$X86_64_DIR"

log_info "Installing Conan dependencies for x86_64..."
if ! conan install . \
    -s build_type=Release \
    -s arch=x86_64 \
    -pr:b=default \
    --build=missing \
    -o build_comfyui_plugins=True \
    -of="$X86_64_DIR" 2>&1; then
    log_error "Failed to install x86_64 dependencies via Conan"
    log_warning "This likely means Conan packages don't have x86_64 prebuilt binaries"
    log_warning "Attempting to build from source (this may take a while)..."

    conan install . \
        -s build_type=Release \
        -s arch=x86_64 \
        -pr:b=default \
        --build="*" \
        -o build_comfyui_plugins=True \
        -of="$X86_64_DIR"
fi

log_info "Configuring CMake for x86_64..."
# Find the conan_toolchain.cmake file
X86_64_TOOLCHAIN=$(find "$X86_64_DIR" -name "conan_toolchain.cmake" | head -1)
cmake -S . -B "$X86_64_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=x86_64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=14.4 \
    -DBUILD_COMFYUI_PLUGINS=ON \
    -DCMAKE_TOOLCHAIN_FILE="$X86_64_TOOLCHAIN" \
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW

log_info "Building $TARGET_NAME for x86_64..."
cmake --build "$X86_64_DIR" --config Release --target "$TARGET_NAME" --parallel

# Step 3: Find the binaries
log_info "Step 3/4: Locating plugin binaries..."

# Find arm64 binary
ARM64_BINARY=$(find "$ARM64_DIR" -name "${PLUGIN_NAME}.ofx" -type f | head -1)
if [[ -z "$ARM64_BINARY" ]]; then
    log_error "Could not find arm64 binary for $PLUGIN_NAME"
    exit 1
fi
log_success "Found arm64 binary: $ARM64_BINARY"

# Find x86_64 binary
X86_64_BINARY=$(find "$X86_64_DIR" -name "${PLUGIN_NAME}.ofx" -type f | head -1)
if [[ -z "$X86_64_BINARY" ]]; then
    log_error "Could not find x86_64 binary for $PLUGIN_NAME"
    exit 1
fi
log_success "Found x86_64 binary: $X86_64_BINARY"

# Verify architectures
log_info "Verifying binary architectures..."
ARM64_INFO=$(lipo -info "$ARM64_BINARY")
X86_64_INFO=$(lipo -info "$X86_64_BINARY")

if ! echo "$ARM64_INFO" | grep -q "architecture: arm64"; then
    log_error "arm64 binary is not actually arm64!"
    echo "$ARM64_INFO"
    exit 1
fi

if ! echo "$X86_64_INFO" | grep -q "architecture: x86_64"; then
    log_error "x86_64 binary is not actually x86_64!"
    echo "$X86_64_INFO"
    exit 1
fi

log_success "Both binaries verified"

# Step 4: Combine into universal binary
log_info "Step 4/4: Creating universal binary..."

# Create output bundle structure
BUNDLE_DIR="$RELEASE_DIR/${PLUGIN_NAME}.ofx.bundle/Contents/MacOS"
mkdir -p "$BUNDLE_DIR"

UNIVERSAL_BINARY="$BUNDLE_DIR/${PLUGIN_NAME}.ofx"

# Use lipo to create universal binary
lipo -create \
    "$ARM64_BINARY" \
    "$X86_64_BINARY" \
    -output "$UNIVERSAL_BINARY"

log_success "Universal binary created: $UNIVERSAL_BINARY"

# Verify universal binary
log_info "Verifying universal binary..."
lipo -info "$UNIVERSAL_BINARY"

# Copy Info.plist from arm64 build
ARM64_BUNDLE_DIR=$(dirname "$(dirname "$ARM64_BINARY")")
if [[ -f "$ARM64_BUNDLE_DIR/Info.plist" ]]; then
    cp "$ARM64_BUNDLE_DIR/Info.plist" "$RELEASE_DIR/${PLUGIN_NAME}.ofx.bundle/Contents/"
    log_success "Copied Info.plist"
fi

# Step 5: Install if requested
if [[ "$INSTALL_PLUGIN" == true ]]; then
    log_info "Step 5/5: Installing plugin to $INSTALL_DIR..."

    mkdir -p "$INSTALL_DIR"

    # Remove existing plugin if present
    if [[ -d "$INSTALL_DIR/${PLUGIN_NAME}.ofx.bundle" ]]; then
        log_warning "Removing existing plugin at $INSTALL_DIR/${PLUGIN_NAME}.ofx.bundle"
        rm -rf "$INSTALL_DIR/${PLUGIN_NAME}.ofx.bundle"
    fi

    # Copy the bundle
    cp -r "$RELEASE_DIR/${PLUGIN_NAME}.ofx.bundle" "$INSTALL_DIR/"

    log_success "Plugin installed to: $INSTALL_DIR/${PLUGIN_NAME}.ofx.bundle"
fi

# Installation info
log_success "Build complete!"
echo
log_info "Universal plugin bundle: $RELEASE_DIR/${PLUGIN_NAME}.ofx.bundle"

if [[ "$INSTALL_PLUGIN" == false ]]; then
    log_info "To install for Flame/host apps:"
    log_info "  cp -r \"$RELEASE_DIR/${PLUGIN_NAME}.ofx.bundle\" \"$HOME/Library/OFX/Plugins/\""
    log_info "Or run: $0 --install"
fi

echo
log_info "Binary architectures:"
lipo -detailed_info "$UNIVERSAL_BINARY"
