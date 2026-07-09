#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright OpenFX and contributors to the OpenFX project.

# Build a single OpenFX ComfyUI plugin for Linux, NATIVELY (Conan + CMake).
#
# Runs directly on a Linux box (e.g. Rocky 9) — no Docker. The plugin links
# libstdc++ statically (-static-libstdc++, set via aifx_harden_ofx_exports in
# contrib/plugins/ComfyUI/CMakeLists.txt), which needs the static libstdc++
# archive; this script preflight-checks for it. Mirrors the native build flow
# in the aifx repo's tools/release-linux.sh.

set -e

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
log_info()    { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error()   { echo -e "${RED}[ERROR]${NC} $1"; }

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
OFX_ROOT="$( cd "$SCRIPT_DIR/../.." && pwd )"
REQUIRED_CMAKE="3.28"

# Defaults (AnyComfy is the fork-only plugin this script exists for).
PLUGIN_NAME="AnyComfy"
TARGET_NAME="AnyComfy"
BUILD_DIR="$OFX_ROOT/build/linux"
OUTPUT_DIR="$OFX_ROOT/build/linux"
INSTALL=false

while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--plugin) PLUGIN_NAME="$2"; shift 2 ;;
        -t|--target) TARGET_NAME="$2"; shift 2 ;;
        -o|--output) OUTPUT_DIR="$2"; shift 2 ;;
        --install)   INSTALL=true; shift ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Build a single OpenFX ComfyUI plugin for Linux natively (no Docker)."
            echo "Run this on the Linux build host (e.g. Rocky 9)."
            echo ""
            echo "Options:"
            echo "  -p, --plugin NAME   Plugin name (default: AnyComfy)"
            echo "  -t, --target NAME   CMake target name (default: AnyComfy)"
            echo "  -o, --output DIR    Install destination for --install (default: build/linux)"
            echo "  --install           Copy the built .ofx.bundle to the output dir"
            echo "  -h, --help          Show this help"
            echo ""
            echo "Example:  $0 -p AnyComfy -t AnyComfy"
            exit 0
            ;;
        *) log_error "Unknown option: $1"; exit 1 ;;
    esac
done

log_info "Building Linux plugin (native): $PLUGIN_NAME (target $TARGET_NAME)"
log_info "OpenFX root: $OFX_ROOT"

if [[ "$OSTYPE" != "linux"* ]]; then
    log_error "This is a native Linux build and must run on Linux (e.g. Rocky 9)."
    log_error "Current platform: $OSTYPE"
    exit 1
fi

# Prefer a pip-installed cmake (~/.local/bin) over an older system one.
export PATH="$HOME/.local/bin:$PATH"

# --- Toolchain checks --------------------------------------------------------
for tool in conan cmake; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        log_error "'$tool' is required but not in PATH."
        [[ "$tool" == "cmake" ]] && log_error "  Install a recent one: pip install --user 'cmake>=${REQUIRED_CMAKE}'"
        [[ "$tool" == "conan" ]] && log_error "  Install: pip install --user 'conan>=2.1'  &&  conan profile detect"
        exit 1
    fi
done

CXX_BIN="$(command -v g++ || command -v clang++ || true)"
if [[ -z "$CXX_BIN" ]]; then
    log_error "No C++ compiler (g++/clang++) found. On Rocky/RHEL: sudo dnf install gcc-c++"
    exit 1
fi

# Static C++ runtime must be present. Each .ofx links libstdc++ statically so it
# carries its own std::filesystem and cannot bind to the host's; DaVinci Resolve
# loads an older libstdc++ (via libProResRAW.so) into the global scope and a
# dynamically-linked plugin then SIGSEGVs the first time it touches std::filesystem.
# The static archive is NOT installed by default on Rocky/RHEL.
if ! echo 'int main(){}' | "$CXX_BIN" -static-libstdc++ -x c++ - -o /dev/null 2>/dev/null; then
    log_error "Static C++ runtime (libstdc++.a) missing — '-static-libstdc++' fails to link."
    log_error "  Rocky/RHEL 9:  sudo dnf config-manager --set-enabled crb && sudo dnf install libstdc++-static"
    log_error "  Ubuntu/Debian: it ships with build-essential (the g++ dev package)."
    log_error "  Required: without it the .ofx links libstdc++ dynamically and crashes in DaVinci Resolve."
    exit 1
fi

cd "$OFX_ROOT"
mkdir -p "$BUILD_DIR"

# --- Build -------------------------------------------------------------------
log_info "[1/3] Installing Conan dependencies (static deps for a portable .ofx)..."
conan install . \
    -s build_type=Release \
    -pr:b=default \
    --build=missing \
    -o '&:build_comfyui_plugins=True' \
    -o '*:shared=False' \
    -o 'expat/*:shared=True' \
    -of="$BUILD_DIR"

log_info "[2/3] Configuring CMake..."
TOOLCHAIN="$(find "$BUILD_DIR" -name 'conan_toolchain.cmake' | head -1)"
if [[ -z "$TOOLCHAIN" ]]; then
    log_error "conan_toolchain.cmake not found under $BUILD_DIR after conan install."
    exit 1
fi
cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_COMFYUI_PLUGINS=ON \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_POLICY_DEFAULT_CMP0091=NEW

log_info "[3/3] Building target $TARGET_NAME..."
cmake --build "$BUILD_DIR" --config Release --target "$TARGET_NAME" --parallel

# --- Locate + verify ---------------------------------------------------------
PLUGIN_BINARY=$(find "$BUILD_DIR" -name "${PLUGIN_NAME}.ofx" -type f | head -1)
if [ -z "$PLUGIN_BINARY" ]; then
    log_error "Plugin binary not found: ${PLUGIN_NAME}.ofx (under $BUILD_DIR)"
    exit 1
fi
log_success "Built: $PLUGIN_BINARY"

if ! file "$PLUGIN_BINARY" | grep -q "ELF.*x86-64"; then
    log_error "Binary is not an x86-64 Linux ELF:"
    file "$PLUGIN_BINARY"
    exit 1
fi
file "$PLUGIN_BINARY"

# The .ofx.bundle is <BUILD_DIR>/<Name>.ofx.bundle (its Contents/Linux-x86-64/
# holds the binary). Report/copy the bundle, not just the .ofx.
PLUGIN_BUNDLE=$(find "$BUILD_DIR" -maxdepth 1 -type d -name "${PLUGIN_NAME}.ofx.bundle" | head -1)

if [ "$INSTALL" = true ] && [ -n "$PLUGIN_BUNDLE" ]; then
    mkdir -p "$OUTPUT_DIR"
    if [ "$(cd "$(dirname "$PLUGIN_BUNDLE")" && pwd)" != "$(cd "$OUTPUT_DIR" && pwd)" ]; then
        rm -rf "$OUTPUT_DIR/$(basename "$PLUGIN_BUNDLE")"
        cp -r "$PLUGIN_BUNDLE" "$OUTPUT_DIR/"
        log_success "Installed bundle to: $OUTPUT_DIR/$(basename "$PLUGIN_BUNDLE")"
    fi
fi

log_success "Build complete!"
echo ""
echo "Linux bundle: ${PLUGIN_BUNDLE:-$PLUGIN_BINARY}"
echo "To test: copy the .ofx.bundle into /usr/OFX/Plugins or ~/OFX/Plugins"
