// Copyright OpenFX and contributors to the OpenFX project.
// SPDX-License-Identifier: BSD-3-Clause

# ComfyUI OFX Plugins

OpenFX plugins that integrate with ComfyUI REST server for AI-powered image processing.

## Overview

This directory contains OFX plugins that connect to a ComfyUI server to perform AI operations like:
- Image segmentation (SAM2)
- Image upscaling (Real-ESRGAN)
- Inpainting (Stable Diffusion)
- Style transfer
- And more...

Based on the Flame/Flare PyBox ComfyUI project, these plugins bring the same functionality to **all** OFX-compatible hosts (Flame, Nuke, Resolve, etc.).

## Architecture

```
ComfyUI/
├── common/                          # Shared code for all plugins
│   ├── comfyui_client.h/.cpp       # REST API client
│   └── comfyui_base_plugin.h/.cpp  # Base plugin class
├── segmentation/                    # SAM2 segmentation plugin (future)
├── upscale/                         # Upscaling plugin (future)
└── CMakeLists.txt                   # Build configuration
```

## Dependencies

All dependencies are managed by **Conan** and automatically downloaded during build:

- **nlohmann/json** - JSON parsing for workflows
- **cpp-httplib** - HTTP/HTTPS client for REST API
- **websocketpp** - WebSocket support for status monitoring
- **OpenImageIO** - EXR image file I/O
- **OpenSSL** - HTTPS encryption

## Building

### Prerequisites

- CMake 3.28+
- Conan 2.1+
- C++17 compiler
- ComfyUI server (for runtime)

### Build Commands

**Option 1: Using build script** (recommended)
```bash
# From OpenFX root directory
./scripts/build-cmake.sh -o build_comfyui_plugins=True Release
```

**Option 2: Manual build**
```bash
# Install dependencies with Conan
conan install -s build_type=Release \
              -o build_comfyui_plugins=True \
              -pr:b=default \
              --build=missing .

# Configure CMake
cmake --preset conan-release \
      -DBUILD_COMFYUI_PLUGINS=ON

# Build
cmake --build build/Release --config Release --parallel

# Install plugins
cmake --build build/Release --target install --config Release
```

### Build Options

- `BUILD_COMFYUI_PLUGINS=ON/OFF` - Enable ComfyUI plugin compilation (default: OFF)

## Installation

Plugins are installed to standard OFX directories:

- **macOS**: `~/Library/OFX/Plugins/`
- **Linux**: `/usr/OFX/Plugins/` or `~/.local/share/OFX/Plugins/`
- **Windows**: `C:\Program Files\Common Files\OFX\Plugins\`

## ComfyUI Server Setup

### Requirements

1. **ComfyUI server** running and accessible
2. **Shared network storage** for image exchange
3. **Required AI models** installed on server

### Configuration

Each plugin requires configuration parameters:

- **Server Address**: Hostname/IP of ComfyUI server (e.g., `localhost` or `192.168.1.100`)
- **Server Port**: ComfyUI port (default: `8188`)
- **Shared Mount Path**: Network path accessible to both OFX host and ComfyUI server
- **Project Name**: Identifier for organizing files

### Directory Structure

```
<shared-mount>/
├── in/<project>/segmentation/       # Input images (written by OFX plugin)
├── out/<project>/segmentation/      # Output images (written by ComfyUI)
└── models/                          # AI models (optional)
```

## Development Status

### Current State (Foundation)

- ✅ Conan dependency configuration
- ✅ CMake build integration
- ✅ Base plugin architecture
- ✅ ComfyUI client stub implementation
- ✅ Directory structure

### Next Steps

1. **Implement ComfyUI Client** (cpp-httplib integration)
   - HTTP POST for workflow submission
   - WebSocket for status monitoring
   - Image retrieval

2. **Complete Base Plugin Class**
   - Workflow execution logic
   - OpenImageIO EXR I/O
   - Progress reporting
   - Error handling

3. **Create Segmentation Plugin**
   - SAM2 workflow implementation
   - Model selection UI
   - Multi-output handling (result + matte)

4. **Add Additional Plugins**
   - Upscaling
   - Inpainting
   - Style transfer

## Testing

### Unit Tests
```bash
# TODO: Add unit test framework
```

### Integration Tests
```bash
# Test with actual ComfyUI server
# 1. Start ComfyUI server
# 2. Load plugin in OFX host
# 3. Configure server connection
# 4. Process test image
```

## Documentation

- [Transposition Guide](../../docs/pybox-to-ofx-transposition.md) - Complete architecture and implementation plan
- [OpenFX Documentation](../../../Documentation/) - OFX API reference
- [ComfyUI API](https://github.com/comfyanonymous/ComfyUI) - ComfyUI server documentation

## Contributing

Follow the existing code style and patterns:
- Use SPDX license headers
- Follow OpenFX naming conventions
- Document all public APIs
- Add tests for new functionality

## License

BSD-3-Clause (same as OpenFX)

## Contact

Based on the PyBox ComfyUI projects:
- https://github.com/Dev-Reepost/flame_comfyui_client
- https://github.com/Dev-Reepost/flame_comfyui_pybox
- https://github.com/Dev-Reepost/flame_comfyui_segmentation
