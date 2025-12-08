#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright OpenFX and contributors to the OpenFX project.

# Build OpenFX ComfyUI plugins for Linux (both ARM64 and x86_64) using Docker
# This script runs on macOS and builds Linux binaries in Docker containers

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

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
OFX_ROOT="$( cd "$SCRIPT_DIR/../.." && pwd )"

# Docker image name
DOCKER_IMAGE_ARM64="openfx-linux-builder:latest"
DOCKER_IMAGE_AMD64="openfx-linux-builder:latest"  # Use same image with platform flag

# Default values
PLUGIN_NAME="SAMSegmentation"
TARGET_NAME="SAMSegmentation"
OUTPUT_DIR="$OFX_ROOT/build/linux-universal"
INSTALL=false
CLEAN=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--plugin)
            PLUGIN_NAME="$2"
            shift 2
            ;;
        -t|--target)
            TARGET_NAME="$2"
            shift 2
            ;;
        -o|--output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --install)
            INSTALL=true
            shift
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Build OpenFX ComfyUI plugins for Linux (ARM64 + x86_64) using Docker"
            echo ""
            echo "Options:"
            echo "  -p, --plugin NAME     Plugin name (default: SAMSegmentation)"
            echo "  -t, --target NAME     CMake target name (default: SAMSegmentation)"
            echo "  -o, --output DIR      Output directory (default: build/linux-universal)"
            echo "  --install             Copy to output directory after build"
            echo "  -c, --clean           Clean build directories before building"
            echo "  -h, --help            Show this help message"
            echo ""
            echo "Example:"
            echo "  $0 -p SAMSegmentation -t SAMSegmentation --install"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

log_info "Building Linux universal plugin: $PLUGIN_NAME"
log_info "OpenFX root: $OFX_ROOT"

# Check if Docker is running
if ! docker info >/dev/null 2>&1; then
    log_error "Docker is not running. Please start Docker and try again."
    exit 1
fi

# Build Docker image if it doesn't exist
if ! docker image inspect "$DOCKER_IMAGE_ARM64" >/dev/null 2>&1; then
    log_info "Building Docker image: $DOCKER_IMAGE_ARM64"
    docker build -t "$DOCKER_IMAGE_ARM64" -f "$SCRIPT_DIR/Dockerfile.linux" "$SCRIPT_DIR"
    log_success "Docker image built"
else
    log_info "Using existing Docker image: $DOCKER_IMAGE_ARM64"
fi

# Clean if requested
if [ "$CLEAN" = true ]; then
    log_warning "Cleaning build directories..."
    rm -rf "$OFX_ROOT/build/linux-arm64" "$OFX_ROOT/build/linux-x86_64"
fi

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Step 1: Build ARM64 version
log_info "Step 1/4: Building ARM64 version..."

# Don't use --platform flag for native architecture (avoids Docker platform issues)
docker run --rm \
    -v "$OFX_ROOT:/build" \
    -w /build \
    "$DOCKER_IMAGE_ARM64" \
    bash -c "
        set -e
        echo '[INFO] Installing Conan dependencies for ARM64...'
        conan install . \
            -s build_type=Release \
            -s arch=armv8 \
            -pr:b=default \
            --build=missing \
            -o build_comfyui_plugins=True \
            -of=build/linux-arm64

        echo '[INFO] Configuring CMake for ARM64...'
        TOOLCHAIN=\$(find build/linux-arm64 -name 'conan_toolchain.cmake' | head -1)
        cmake -S . -B build/linux-arm64 \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_COMFYUI_PLUGINS=ON \
            -DCMAKE_TOOLCHAIN_FILE=\"\$TOOLCHAIN\" \
            -DCMAKE_POLICY_DEFAULT_CMP0091=NEW

        echo '[INFO] Building $TARGET_NAME for ARM64...'
        cmake --build build/linux-arm64 --config Release --target $TARGET_NAME --parallel

        echo '[SUCCESS] ARM64 build completed'
    "

if [ $? -ne 0 ]; then
    log_error "ARM64 build failed"
    exit 1
fi

log_success "ARM64 build completed"

# Step 2: Build x86_64 version
log_info "Step 2/4: Building x86_64 version (using emulation)..."
log_warning "This may take 10-15 minutes due to emulation..."

# Check if binfmt is available for x86_64 emulation
if ! docker run --rm --privileged multiarch/qemu-user-static --reset -p yes >/dev/null 2>&1; then
    log_warning "Could not set up QEMU emulation. Trying direct execution..."
fi

docker run --rm \
    --platform linux/amd64 \
    -v "$OFX_ROOT:/build" \
    -w /build \
    "$DOCKER_IMAGE_AMD64" \
    bash -c "
        set -e
        echo '[INFO] Installing Conan dependencies for x86_64...'
        conan install . \
            -s build_type=Release \
            -s arch=x86_64 \
            -pr:b=default \
            --build=missing \
            -o build_comfyui_plugins=True \
            -of=build/linux-x86_64

        echo '[INFO] Configuring CMake for x86_64...'
        TOOLCHAIN=\$(find build/linux-x86_64 -name 'conan_toolchain.cmake' | head -1)
        cmake -S . -B build/linux-x86_64 \
            -DCMAKE_BUILD_TYPE=Release \
            -DBUILD_COMFYUI_PLUGINS=ON \
            -DCMAKE_TOOLCHAIN_FILE=\"\$TOOLCHAIN\" \
            -DCMAKE_POLICY_DEFAULT_CMP0091=NEW

        echo '[INFO] Building $TARGET_NAME for x86_64...'
        cmake --build build/linux-x86_64 --config Release --target $TARGET_NAME --parallel

        echo '[SUCCESS] x86_64 build completed'
    "

if [ $? -ne 0 ]; then
    log_error "x86_64 build failed"
    exit 1
fi

log_success "x86_64 build completed"

# Step 3: Locate and verify binaries
log_info "Step 3/4: Locating plugin binaries..."

ARM64_BINARY=$(find "$OFX_ROOT/build/linux-arm64" -name "${PLUGIN_NAME}.ofx" -type f | head -1)
X86_64_BINARY=$(find "$OFX_ROOT/build/linux-x86_64" -name "${PLUGIN_NAME}.ofx" -type f | head -1)

if [ -z "$ARM64_BINARY" ]; then
    log_error "ARM64 binary not found: ${PLUGIN_NAME}.ofx"
    exit 1
fi

if [ -z "$X86_64_BINARY" ]; then
    log_error "x86_64 binary not found: ${PLUGIN_NAME}.ofx"
    exit 1
fi

log_success "Found ARM64 binary: $ARM64_BINARY"
log_success "Found x86_64 binary: $X86_64_BINARY"

# Verify architectures
log_info "Verifying binary architectures..."

ARM64_INFO=$(file "$ARM64_BINARY")
X86_64_INFO=$(file "$X86_64_BINARY")

if ! echo "$ARM64_INFO" | grep -q "ARM aarch64"; then
    log_error "ARM64 binary is not actually ARM64!"
    echo "$ARM64_INFO"
    exit 1
fi

if ! echo "$X86_64_INFO" | grep -q "x86-64"; then
    log_error "x86_64 binary is not actually x86_64!"
    echo "$X86_64_INFO"
    exit 1
fi

log_success "Both binaries verified"

# Show binary info
log_info "Binary information:"
echo "ARM64:   $ARM64_INFO"
echo "x86_64:  $X86_64_INFO"

# Step 4: Package binaries
if [ "$INSTALL" = true ]; then
    log_info "Step 4/4: Packaging plugins to $OUTPUT_DIR..."

    # Create architecture-specific directories
    mkdir -p "$OUTPUT_DIR/arm64"
    mkdir -p "$OUTPUT_DIR/x86_64"

    # Find and copy bundle directories
    ARM64_BUNDLE=$(dirname "$(dirname "$(dirname "$ARM64_BINARY")")")
    X86_64_BUNDLE=$(dirname "$(dirname "$(dirname "$X86_64_BINARY")")")

    BUNDLE_NAME=$(basename "$ARM64_BUNDLE")

    # Copy bundles
    cp -r "$ARM64_BUNDLE" "$OUTPUT_DIR/arm64/"
    cp -r "$X86_64_BUNDLE" "$OUTPUT_DIR/x86_64/"

    log_success "ARM64 plugin: $OUTPUT_DIR/arm64/$BUNDLE_NAME"
    log_success "x86_64 plugin: $OUTPUT_DIR/x86_64/$BUNDLE_NAME"
fi

log_success "Build complete!"
echo ""
echo "Linux ARM64 binary:  $ARM64_BINARY"
echo "Linux x86_64 binary: $X86_64_BINARY"
echo ""
echo "To deploy on Linux:"
echo "  - ARM64 systems: Copy from $OUTPUT_DIR/arm64/ to /usr/OFX/Plugins or ~/.OFX/Plugins"
echo "  - x86_64 systems: Copy from $OUTPUT_DIR/x86_64/ to /usr/OFX/Plugins or ~/.OFX/Plugins"
